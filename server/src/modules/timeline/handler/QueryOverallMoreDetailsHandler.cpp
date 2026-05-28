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
#include <algorithm>
#include "WsSessionManager.h"
#include "TraceTime.h"
#include "SystemViewOverallRepoInterface.h"
#include "SystemViewOverallRepoFactory.h"
#include "TraceDatabaseSqlConst.h"
#include "QueryOverallMoreDetailsHandler.h"
using namespace Dic::Server;
namespace Dic::Module::Timeline {
bool QueryOverallMoreDetailsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SystemViewOverallMoreDetailsRequest &>(*requestPtr);
    std::unique_ptr<UnitThreadsOperatorsResponse> responsePtr = std::make_unique<UnitThreadsOperatorsResponse>();
    UnitThreadsOperatorsResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string error;
    request.params.CheckParams(minTimestamp, error);
    if (!std::empty(error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (request.params.categoryList.size() <= 1) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    SystemViewOverallHelper overallHelper;

    if (request.params.categoryList[0] == OVERALL_CAT_COMPUTING) {
        GetComputingOverallMetricDetails(database, request.params, response, minTimestamp, overallHelper);
    } else if (request.params.categoryList[0] == COMMUNICATION_NOT_OVERLAP_TIME ||
        request.params.categoryList[0] == COMMUNICATION_TIME) {
        GetCommunicationOverallMetricDetails(database, request.params, response, minTimestamp, overallHelper);
    }

    if (overallHelper.needResponse) {
        response.body.rankId = request.params.rankId;
        response.body.currentPage = request.params.page.current;
        response.body.pageSize = request.params.page.pageSize;
        SendResponse(std::move(responsePtr), true);
        return true;
    } else {
        SetTimelineError(ErrorCode::QUERY_OVERALL_METRICS_DETAIL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
}

void QueryOverallMoreDetailsHandler::GetComputingOverallMetricDetails(
    const std::shared_ptr<VirtualTraceDatabase> &database, const SystemViewOverallReqParam &params,
    UnitThreadsOperatorsResponse &response, uint64_t minTimestamp, SystemViewOverallHelper &overallHelper) {
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataTypeByRank(params.rankId));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    if (!repoPtr->QueryDataForComputingOverallMetric(params, overallHelper, database)) {
        ServerLog::Error("Failed to get computing data for system view overall detail list.");
        return;
    }
    if (!overallHelper.kernelEvents.empty()) {
        overallHelper.needResponse = true;
        overallHelper.CategorizeComputingEvents(); // 分类kernelEvents
        // 按入参请求过滤kernelEvents, 去除一级目录
        std::vector<std::string> tempList(params.categoryList.begin() + 1, params.categoryList.end());
        std::vector<SameOperatorsDetails> filteredEvents =
            overallHelper.FilterComputingEventsByCategory(tempList, minTimestamp, params.name);
        response.body.count = filteredEvents.size();
        // 实现排序、分页
        GetPagedData(filteredEvents, params, response);
    }
}

// 实现排序、分页
void QueryOverallMoreDetailsHandler::GetPagedData(std::vector<SameOperatorsDetails> &filteredEvents,
    const SystemViewOverallReqParam &params, UnitThreadsOperatorsResponse &response) {
    // 排序
    std::string orderBy = params.order.orderBy;
    std::string orderType = params.order.NormalizeOrderType(params.order.orderType);
    std::sort(filteredEvents.begin(), filteredEvents.end(),
        [&orderBy, &orderType](const SameOperatorsDetails &a, const SameOperatorsDetails &b) {
            if (orderBy == "startTime") {
                if (orderType == "ASC") {
                    return a.timestamp < b.timestamp;
                } else {
                    return a.timestamp > b.timestamp;
                }
            } else if (orderBy == "duration") {
                if (orderType == "ASC") {
                    return a.duration < b.duration;
                } else {
                    return a.duration > b.duration;
                }
            }
            return false;
        });
    // 分页
    uint64_t offset = (params.page.current - 1) * params.page.pageSize;
    if (offset > filteredEvents.size()) {
        return;
    }
    uint64_t end =
        std::min(offset + static_cast<uint64_t>(params.page.pageSize), static_cast<uint64_t>(filteredEvents.size()));
    if (offset > UINT32_MAX || end > UINT32_MAX) {
        return;
    }
    response.body.sameOperatorsDetails.assign(
        filteredEvents.begin() + static_cast<uint32_t>(offset), filteredEvents.begin() + static_cast<uint32_t>(end));
}

void QueryOverallMoreDetailsHandler::GetCommunicationOverallMetricDetails(
    const std::shared_ptr<VirtualTraceDatabase> &database, const SystemViewOverallReqParam &params,
    UnitThreadsOperatorsResponse &response, uint64_t minTimestamp, SystemViewOverallHelper &overallHelper) {
    std::vector<Protocol::ThreadTraces> notOverlapData{};
    if (params.categoryList[0] == COMMUNICATION_NOT_OVERLAP_TIME) {
        uint64_t totalTime = 0;
        std::string sql = DataBaseManager::Instance().GetDataTypeByRank(params.rankId) == DataType::TEXT
            ? TextSqlConstant::GetOverlapAnalysisTextSqlByType(params)
            : TraceDatabaseSqlConst::GetOverlapAnalysisDbSqlByType(params);
        std::string type = DataBaseManager::Instance().GetDataTypeByRank(params.rankId) == DataType::TEXT
            ? "Communication(Not Overlapped)"
            : "2";
        ParamsForOAData paramsForOaData = {sql, type, minTimestamp, params.startTime, params.endTime};
        int deviceId = StringUtil::StringToInt(params.deviceId);
        if (!database->QueryOverlapAnalysisData(paramsForOaData, deviceId, notOverlapData, totalTime)) {
            ServerLog::Error("Failed to query Communication Overall Metrics due to incorrect not overlapped data.");
            return;
        }
    }
    overallHelper.needResponse = false;
    auto repoPtr = SystemViewOverallRepoFactory::Instance()->GetSystemViewOverallRepo(
        DataBaseManager::Instance().GetDataTypeByRank(params.rankId));
    if (repoPtr == nullptr) {
        // GetSystemViewOverallRepo中已有日志报错
        return;
    }
    if (!repoPtr->QueryCommunicationOpsTimeDataByGroupName(
            params, minTimestamp, notOverlapData, response.body.sameOperatorsDetails, database)) {
        ServerLog::Error("Failed to query Communication Overall Metrics due to incorrect time data query.");
        return;
    }
    if (response.body.sameOperatorsDetails.empty()) {
        return;
    }
    overallHelper.needResponse = true;
    response.body.count = response.body.sameOperatorsDetails.size();
    GetPagedData(response.body.sameOperatorsDetails, params, response);
}
}
