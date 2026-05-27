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
#include "WsSender.h"
#include "ModuleRequestHandler.h"
#include "WsSessionManager.h"
#include "ServerLog.h"
namespace Dic {
void SendEvent(std::unique_ptr<Dic::Protocol::Event> eventPtr) {
    auto &wsSessionManager = Dic::Server::WsSessionManager::Instance();
    if (!wsSessionManager.CheckSession()) {
        Server::ServerLog::Warn("SendEvent failed. Can't get session");
        return;
    }
    Dic::Server::WsSession *session = wsSessionManager.GetSession();
    session->OnEvent(std::move(eventPtr));
}
void SendResponse(
    std::unique_ptr<Protocol::Response> responsePtr, bool result, const std::string &errorMsg, const int errorCode) {
    if (!Dic::Server::WsSessionManager::Instance().CheckSession()) {
        Server::ServerLog::Warn("SendResponse failed. Can't get session");
        return;
    }
    auto session = Server::WsSessionManager::Instance().GetSession();
    Module::ModuleRequestHandler::SetResponseResult(*responsePtr, result, errorMsg, errorCode);
    session->OnResponse(std::move(responsePtr));
}
}
