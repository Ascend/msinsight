/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "DataBaseManager.h"
#include "KernelDetailOverallDatabaseAccesser.h"
#include "Paginator.h"
#include "QueryKernelOverallHandler.h"

namespace Dic::Module::Timeline {
namespace {
std::string BuildKernelOverallResponseKey(const std::string &type, const std::string &acceleratorCore) {
    return std::to_string(type.size()) + ":" + type + "|" + std::to_string(acceleratorCore.size()) + ":" +
        acceleratorCore;
}
}

bool QueryKernelOverallHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<KernelOverallRequest &>(*requestPtr);
    std::unique_ptr<KernelOverallResponse> responsePtr = std::make_unique<KernelOverallResponse>();
    KernelOverallResponse &response = *responsePtr;
    SetBaseResponse(request, response);

    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string error;
    if (!request.params.CheckParams(minTimestamp, error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        ServerLog::Warn("DeviceId is empty for kernel overall statistics.");
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    request.params.deviceId = deviceId;

    if (!CalKernelData(request, response, error, database)) {
        SetTimelineError(ErrorCode::QUERY_KERNEL_OVERALL_FAILED);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}

void BuildKernelOverallResult(const std::vector<KernelDetailOverallRecord> &records, KernelOverallResponse &response,
    uint32_t current, uint32_t pageSize) {
    std::vector<KernelOverallRes> result;
    result.reserve(records.size());
    for (const auto &record : records) {
        KernelOverallRes res;
        res.key = BuildKernelOverallResponseKey(record.type, record.acceleratorCore);
        res.type = record.type;
        res.acceleratorCore = record.acceleratorCore;
        res.number = record.count;
        res.totalTime = record.totalDuration;
        res.avgTime = NumberUtil::DoubleReservedNDigits(record.avgDuration, 2);
        res.minTime = record.minDuration;
        res.maxTime = record.maxDuration;
        result.emplace_back(std::move(res));
    }

    Paginator<KernelOverallRes> paginator(result, pageSize);
    response.pageParam.total = paginator.GetTotal();
    response.details = paginator.GetPage(current);
}

bool QueryKernelOverallHandler::CalKernelData(KernelOverallRequest &request, KernelOverallResponse &response,
    std::string &error, const std::shared_ptr<VirtualTraceDatabase> &database) {
    const KernelDetailOverallDatabaseAccesser accesser(database, request.fileId);

    std::vector<KernelDetailOverallRecord> records;
    if (!accesser.GetKernelDetailOverallRecords(
            request.params.startTime, request.params.endTime, request.params.deviceId, request.params, records)) {
        error = "Failed to query kernel overall statistics.";
        return false;
    }

    BuildKernelOverallResult(records, response, request.params.page.current, request.params.page.pageSize);
    response.pageParam.current = request.params.page.current;
    response.pageParam.pageSize = request.params.page.pageSize;
    return true;
}
}
