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
#include "TraceTime.h"
#include "TrackInfoManager.h"
#include "QueryFlowCategoryEventsHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryFlowCategoryEventsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    FlowCategoryEventsRequest &request = dynamic_cast<FlowCategoryEventsRequest &>(*requestPtr.get());
    std::unique_ptr<FlowCategoryEventsResponse> responsePtr = std::make_unique<FlowCategoryEventsResponse>();
    FlowCategoryEventsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (renderEngine == nullptr) {
        ServerLog::Error("Query flow events Failed to render.");
        SetTimelineError(ErrorCode::QUERY_FLOW_EVENTS_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    bool result = renderEngine->QueryFlowCategoryEvents(request.params, minTimestamp, response.body.flowDetailList);
    SendResponse(std::move(responsePtr), result);
    return result;
}
} // Timeline
} // Module
} // Dic
