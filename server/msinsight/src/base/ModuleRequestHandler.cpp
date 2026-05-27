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

#include "ModuleRequestHandler.h"
#include "../utils/pch.h"
#include "../WsSessionManager.h"
#include "../utils/IdBuilder.h"
#include "RequestContext.h"
#include "RequestFilter.h"

namespace Dic {
namespace Module {
using namespace Dic::Protocol;
using namespace Dic::Server;

bool ModuleRequestHandler::HandleRequestEntrance(std::unique_ptr<Request> requestPtr) {
    RequestContext::GetInstance().ResetError();
    unsigned int id = requestPtr->id;
    auto &requestFilter = RequestFilter::Instance();
    std::string key = GetRequestKey(*requestPtr);
    if (requestFilter.IsNeedFilter(GetRequestKey(*requestPtr), id)) {
        ServerLog::Warn("Request is outdated, intercepted,id:", id, ", key:", key);
        WsSession &session = *Server::WsSessionManager::Instance().GetSession();
        std::unique_ptr<Response> responsePtr = std::make_unique<Response>();
        SetBaseResponse(*requestPtr, *responsePtr);
        SetCommonError(Common::ErrorCode::OUTDATED_REQUEST_ERROR);
        SetResponseResult(*responsePtr, false);
        session.SendBaseResponse(std::move(responsePtr));
        return false;
    }
    ServerLog::Info("Start handle request, module = ", moduleName, ", command = ", command, ", id = ", id);
    bool res = HandleRequest(std::move(requestPtr));
    ServerLog::Info("End handle request, module = ", moduleName, ", command = ", command, ", id = ", id);
    requestFilter.ClearKey(key, id);
    return res;
}

const std::string ModuleRequestHandler::GetError() { return error; }

void ModuleRequestHandler::SetBaseResponse(const Request &request, Response &response) {
    response.type = ProtocolMessage::Type::RESPONSE;
    response.id = static_cast<uint32_t>(IdBuilder::ResponseIdBuilder().Build());
    response.requestId = request.id;
    response.command = request.command;
    response.moduleName = request.moduleName;
    if (request.resultCallbackId.has_value()) {
        response.resultCallbackId = request.resultCallbackId;
    }
}

void ModuleRequestHandler::SetResponseResult(
    Response &response, bool result, const std::string &errorMsg, const int errorCode) {
    response.result = result;

    if (!errorMsg.empty()) {
        ServerLog::Error(errorMsg);
    }
    response.error = {.code = errorCode, .message = errorMsg};

    SetResponseErrorFromRequestContext(response);
}

void ModuleRequestHandler::SetResponseErrorFromRequestContext(Response &response) {
    ErrorMessage error = RequestContext::GetInstance().GetError();
    if (!error.message.empty()) {
        response.error = {.code = error.code, .message = error.message};
    }
}

void ModuleRequestHandler::SetRequestContextError(ErrorMessage error) { RequestContext::GetInstance().SetError(error); }

void ModuleRequestHandler::ResetRequestContextError() { RequestContext::GetInstance().ResetError(); }

bool ModuleRequestHandler::IsAsync() { return async; }

std::string ModuleRequestHandler::GetRequestKey(Request &request) { return std::to_string(request.id); }
} // end of namespace Module
} // end of namespace Dic
