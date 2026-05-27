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

#ifndef DATA_INSIGHT_CORE_SERVER_SESSION_MANAGER_H
#define DATA_INSIGHT_CORE_SERVER_SESSION_MANAGER_H

#include <memory>
#include <string>
#include <mutex>
#include "WsSession.h"

namespace Dic {
namespace Server {
class WsSessionManager {
  public:
    static WsSessionManager &Instance();

    void AddSession(std::shared_ptr<WsSession> newSession);
    void RemoveSession();
    WsSession *GetSession();
    std::shared_ptr<WsSession> GetSessionPtr();
    bool CheckSession();

  private:
    WsSessionManager() = default;
    ~WsSessionManager() = default;

    std::mutex sessionMutex;
    std::shared_ptr<WsSession> session = nullptr;
};
} // end of namespace Server
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_SERVER_SESSION_MANAGER_H
