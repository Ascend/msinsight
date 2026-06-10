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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#include "QuerySystemViewFtraceStatHandler.h"

namespace Dic::Module::Timeline {
bool QuerySystemViewFtraceStatHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SystemViewFtraceStatRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<SystemViewFtraceStatResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);

    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    auto textDb = std::dynamic_pointer_cast<TextTraceDatabase>(database);
    if (textDb == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::string warnMsg;
    if (!request.params.CheckParams(warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    uint64_t offset = (request.params.current - 1) * request.params.pageSize;
    uint64_t totalCount = 0;

    auto rows = textDb->QueryTraceTaskSummary(request.params, offset, request.params.pageSize, totalCount);

    for (const auto &data : rows) {
        std::unordered_map<std::string, std::string> row;
        row["cpu"] = std::to_string(data.cpuId);
        row["comm"] = data.comm;
        row["pid"] = std::to_string(data.pid);
        row["runnable"] = std::to_string(data.runnableNs);
        row["running"] = std::to_string(data.runningNs);
        row["sleeping"] = std::to_string(data.sleepingNs);
        row["context_switch_count"] = std::to_string(data.csCount);
        row["soft_irq_count"] = std::to_string(data.softIrqCount);
        row["soft_irq_duration"] = std::to_string(data.softIrqDuration);
        row["hard_irq_count"] = std::to_string(data.hardIrqCount);
        row["hard_irq_duration"] = std::to_string(data.hardIrqDuration);
        response.data.push_back(row);
    }

    response.pageParam.current = request.params.current;
    response.pageParam.pageSize = request.params.pageSize;
    response.pageParam.total = totalCount;

    SendResponse(std::move(responsePtr), true);
    return true;
}

} // namespace Dic::Module::Timeline
