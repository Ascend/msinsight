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
#include "QuerySystemViewTraceHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QuerySystemViewTraceHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    SystemViewRequest &request = dynamic_cast<SystemViewRequest &>(*requestPtr.get());
    std::unique_ptr<SystemViewTraceResponse> responsePtr = std::make_unique<SystemViewTraceResponse>();
    SystemViewTraceResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SetResponseResult(response, true);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query system view trace failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        ServerLog::Error("Query system view trace failed to get deviceId.");
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QuerySystemViewTraceData(request.params, response.body, minTimestamp)) {
        SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_FAILED);
        SetResponseResult(response, false);
        ServerLog::Error("Failed to get timeline trace table response data.");
    }
    if (request.params.layer == "Python" && std::empty(response.body.systemViewDetail)) {
        request.params.layer = "MindSpore";
        if (!database->QuerySystemViewTraceData(request.params, response.body, minTimestamp)) {
            SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_FAILED);
            ServerLog::Error("Failed to get timeline trace table response data.");
            SendResponse(std::move(responsePtr), false);
            return false;
        } else {
            SetTimelineError(ErrorCode::RESET_ERROR);
            SetResponseResult(response, true);
        }
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Timeline
} // Module
} // Dic
