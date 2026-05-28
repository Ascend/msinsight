/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#include <atomic>
#include "WsSessionManager.h"
#include "TraceTime.h"
#include "SystemViewOverallRepoInterface.h"
#include "SystemViewOverallRepoFactory.h"
#include "SystemViewOverallCacheManager.h"
#include "QuerySystemViewOverallHandler.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
bool QuerySystemViewOverallHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SystemViewOverallRequest &>(*requestPtr);
    std::unique_ptr<SystemViewOverallResponse> responsePtr = std::make_unique<SystemViewOverallResponse>();
    SystemViewOverallResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (!database->CheckValueFromStatusInfoTable(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS)) {
        response.isLoading = true;
        SetTimelineError(ErrorCode::OVERLAP_ANALYSIS_PARSE_NOT_FINISH);
        SendResponse(std::move(responsePtr), false);
        return true;
    }
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string error;
    if (!request.params.CheckParams(minTimestamp, error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }
    // query cache, while not time range analysis
    std::vector<SystemViewOverallRes> overallDetails =
        SystemViewOverallCacheManager::Instance().GetOverallData(request.fileId);
    // startTime = endTime代表未开启按时间范围分析，不等则代表开启按时间范围分析
    if (!overallDetails.empty() && request.params.startTime == request.params.endTime) {
        response.details = overallDetails;
    } else if (CalOverallData(request, response, error, database)) {
        if (request.params.startTime == request.params.endTime) { // set cache while not time range analysis
            SystemViewOverallCacheManager::Instance().SetOverallData(request.fileId, response.details);
        }
    } else {
        SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_OVERALL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    response.pageParam.total = response.details.size();
    response.pageParam.current = request.params.page.current;
    response.pageParam.pageSize = request.params.page.pageSize;
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QuerySystemViewOverallHandler::CalOverallData(SystemViewOverallRequest &request,
    SystemViewOverallResponse &response, std::string &error, const std::shared_ptr<VirtualTraceDatabase> &database) {
    SystemViewOverallHelper overallHelper;
    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        error = "Failed to get deviceId for system view overall statistics.";
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    request.params.deviceId = deviceId;
    // 执行一层级Overlap查询，结果存放在overallHelper.overlapInfos, 并显式返回e2eTime
    overallHelper.e2eTime = GetOverlapAnalysisData(overallHelper, database, request, response.details);
    if (overallHelper.e2eTime == 0.0) {
        error = "Failed to query system view overall. Overlap information query incomplete or missing.";
        SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_OVERALL_FAILED);
        return false;
    }
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(request.fileId));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_OVERALL_FAILED);
        return false;
    }
    // Computing拆解
    if (!repoPtr->QueryDataForComputingOverallMetric(request.params, overallHelper, database)) {
        ServerLog::Warn(
            "Failed to get computing data for system view overall."); // Computing部分失败，但是继续剩下的查询
    }
    if (!overallHelper.kernelEvents.empty()) {
        // 若kernel details含有效pmu数据，则进行Computing Overall统计，否则跳过Computing拆解
        overallHelper.CategorizeComputingEvents();
        overallHelper.AggregateComputingOverallMetrics(response.details);
    }
    // Communication拆解
    repoPtr->QueryCommunicationOverlapOverallInfos(request.params, overallHelper, response.details, database);
    return true;
}

double QuerySystemViewOverallHandler::GetOverlapAnalysisData(SystemViewOverallHelper &overallHelper,
    const std::shared_ptr<VirtualTraceDatabase> &database, const SystemViewOverallRequest &request,
    std::vector<SystemViewOverallRes> &responseBody) {
    double e2eTime{};
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataType(request.fileId));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return e2eTime;
    }
    overallHelper.overlapInfos = repoPtr->QueryOverlapAnalysisDataForOverallMetric(request.params, database);
    // Overlap: CommunicationIndex = 0, ComputingIndex = 1, FreeIndex = 2按字典序排序
    uint32_t expectOverlapSize = 3;
    if (overallHelper.overlapInfos.size() < expectOverlapSize) {
        return e2eTime;
    }
    const size_t communicationIndex = 0;
    const size_t computingIndex = 1;
    const size_t freeIndex = 2;
    double communicationNotOverlapped = overallHelper.overlapInfos[communicationIndex].duration;
    double computingTime = overallHelper.overlapInfos[computingIndex].duration;
    double freeTime = overallHelper.overlapInfos[freeIndex].duration;
    e2eTime = communicationNotOverlapped + computingTime + freeTime;
    if (std::fabs(e2eTime - 0.0) < std::numeric_limits<double>::epsilon()) {
        return e2eTime;
    }
    double computingRatio = NumberUtil::DoubleReservedNDigits(PERCENTAGE_RATIO_SCALE * computingTime / e2eTime, TWO);
    double communicationRatio =
        NumberUtil::DoubleReservedNDigits(PERCENTAGE_RATIO_SCALE * communicationNotOverlapped / e2eTime, TWO);
    double freeRatio = NumberUtil::DoubleReservedNDigits(PERCENTAGE_RATIO_SCALE * freeTime / e2eTime, TWO);
    double e2eRatio = PERCENTAGE_RATIO_SCALE;
    // 组装一层级数据
    Protocol::SystemViewOverallRes tmpRes = {.totalTime = computingTime,
        .ratio = computingRatio,
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = UINT32_MAX,
        .name = COMPUTING_TIME,
        .children = {},
        .level = 1,
        .id = std::to_string(overallHelper.idCounter++)};
    responseBody.emplace_back(tmpRes);
    tmpRes = {.totalTime = communicationNotOverlapped,
        .ratio = communicationRatio,
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = COMMUNICATION_NOT_OVERLAP_TIME,
        .children = {},
        .level = 1,
        .id = std::to_string(overallHelper.idCounter++)};
    responseBody.emplace_back(tmpRes);
    tmpRes = {.totalTime = freeTime,
        .ratio = freeRatio,
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = FREE_TIME,
        .children = {},
        .level = 1,
        .id = std::to_string(overallHelper.idCounter++)};
    responseBody.emplace_back(tmpRes);
    tmpRes = {.totalTime = NumberUtil::DoubleReservedNDigits(e2eTime, TWO),
        .ratio = e2eRatio,
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = E2E_TIME,
        .children = {},
        .level = 1,
        .id = std::to_string(overallHelper.idCounter++)};
    responseBody.emplace_back(tmpRes);
    return e2eTime;
}
} // namespace Dic::Module::Timeline
