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

#ifndef DATA_INSIGHT_CORE_WS_SERVER_H
#define DATA_INSIGHT_CORE_WS_SERVER_H

#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include "ServerDefs.h"
#include "BaseServer.h"
#include "ApiHandler.h"

namespace Dic {
namespace Server {
class WsServer : public BaseServer {
  public:
    WsServer(const std::string &host, int port, bool strictMode = true);

    ~WsServer() noexcept override = default;

    bool Start() override;
    bool Stop() override;
    bool IsStart() const;

  protected:
    static void ListenThreadFunc(WsServer &server);

    void StartListen();
    uWS::App::WebSocketBehavior<WsUserData> CreateWsBehavior();
    void ListenCb(us_listen_socket_t *listenSocket);
    void OnOpenCb(WsChannel *ws);
    void OnCloseCb(WsChannel *ws, int code, std::string_view message);
    void OnMessageCb(WsChannel *ws, std::string_view message, uWS::OpCode opCode);
    void LoadHandlers();
    void AddPostHandler(const std::string &key, std::shared_ptr<Core::ApiHandler> handler);
    void AddGetHandler(const std::string &key, std::shared_ptr<Core::ApiHandler> handler);

    static void PreLoadEventDir();
    std::unique_ptr<uWS::App> wsApp = nullptr;
    volatile bool listenStart = false;
    std::unique_ptr<std::thread> listenThreadPtr;
    std::mutex resultMutex;
    std::condition_variable resultCv;
    const int resultTimeout = 2000;
    bool strictMode = true;
};
} // end of namespace Server
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_WS_SERVER_H
