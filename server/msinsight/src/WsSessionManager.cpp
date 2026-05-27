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
namespace Dic::Server {
WsSessionManager &WsSessionManager::Instance() {
    static WsSessionManager instance;
    return instance;
}
void WsSessionManager::AddSession(std::shared_ptr<WsSession> newSession) {
    std::unique_lock<std::mutex> lock(sessionMutex);
    if (!session) {
        newSession->Start();
        session = std::move(newSession);
    }
}
void WsSessionManager::RemoveSession() {
    std::unique_lock<std::mutex> lock(sessionMutex);
    session.reset();
}
WsSession *WsSessionManager::GetSession() {
    std::unique_lock<std::mutex> lock(sessionMutex);
    return session.get();
}
std::shared_ptr<WsSession> WsSessionManager::GetSessionPtr() {
    std::unique_lock<std::mutex> lock(sessionMutex);
    return session;
}
bool WsSessionManager::CheckSession() { return session && session->GetStatus() != WsSession::Status::CLOSED; }
} // end of namespace Dic
