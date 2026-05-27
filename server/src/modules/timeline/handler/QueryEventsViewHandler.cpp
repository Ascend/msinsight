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
#include "PythonStackHelper.h"
#include "TraceTime.h"
#include "QueryEventsViewHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryEventsViewHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    EventsViewRequest &request = dynamic_cast<EventsViewRequest &>(*requestPtr.get());

    std::unique_ptr<EventsViewResponse> responsePtr = std::make_unique<EventsViewResponse>();
    EventsViewResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query events view failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    EventsViewParams queryParams = request.params;
    PythonStackHelper::RestoreEventsViewParams(queryParams);
    if (!queryParams.tid.empty()) {
        queryParams.threadIdList.emplace_back(queryParams.tid);
    }
    if (!database->QueryEventsViewData(queryParams, responsePtr->body, minTimestamp)) {
        ServerLog::Warn("Failed to get events view table response data.");
        SetTimelineError(ErrorCode::QUERY_EVENTS_VIEW_DATA_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
