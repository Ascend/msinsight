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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "QueryThreadTracesSummaryHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryThreadTracesSummaryHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitThreadTracesSummaryRequest &request = dynamic_cast<UnitThreadTracesSummaryRequest &>(*requestPtr.get());
    std::unique_ptr<UnitThreadTracesSummaryResponse> responsePtr = std::make_unique<UnitThreadTracesSummaryResponse>();
    UnitThreadTracesSummaryResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.cardId);
    if (database == nullptr) {
        ServerLog::Error("Query thread traces summary failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    bool result = database->QueryThreadTracesSummary(request.params, response.body, minTimestamp);
    if (!result) {
        SetTimelineError(ErrorCode::QUERY_THREAD_TRACES_SUMMARY_FAILED);
        SetResponseResult(response, result);
    }
    SendResponse(std::move(responsePtr), result);
    return result;
}

} // Timeline
} // Module
} // Dic
