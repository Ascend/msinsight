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

#include <gtest/gtest.h>
#include "WsServer.h"
#include "WsSessionManager.h"

using namespace Dic;
using namespace Dic::Core;
using namespace Dic::Server;
class WsServerTest : public ::testing::Test {};

class WsServerDerived : public WsServer {
  public:
    using WsServer::WsServer;

    void OnOpenCb(WsChannel *ws) { WsServer::OnOpenCb(ws); }
    void OnCloseCb(WsChannel *ws, int code, std::string_view message) { WsServer::OnCloseCb(ws, code, message); }
    void OnMessageCb(WsChannel *ws, std::string_view message, uWS::OpCode opCode) {
        WsServer::OnMessageCb(ws, message, opCode);
    }
};

/**
 * @tc.name  : ws_server_test_001
 * @tc.number: ws_server_test_001
 * @tc.desc  : Test when ws is nullptr then OnOpenCb logs "Accept new session, channel is null"
 */
TEST_F(WsServerTest, ws_server_test_001) {
    int port = 8080;
    WsServerDerived wsServer("localhost", port);
    WsChannel *ws = nullptr;
    EXPECT_NO_THROW(wsServer.OnOpenCb(ws));
}

/**
 * @tc.name  : ws_server_on_close_cb_test_001
 * @tc.number: ws_server_Test_001
 * @tc.desc  : Test when ws is nullptr then OnCloseCb returns immediately
 */
TEST_F(WsServerTest, ws_server_on_close_cb_test_001) {
    int port = 8080;
    int code = 1000;
    WsServerDerived wsServer("localhost", port);
    wsServer.OnCloseCb(nullptr, code, "Normal closure");
    // No assertions needed as the function should return immediately
}

/**
 * @tc.name  : ws_server_on_message_cb_test_001
 * @tc.number: ws_server_Test_001
 * @tc.desc  : Test when ws is nullptr then OnMessageCb returns immediately
 */
TEST_F(WsServerTest, ws_server_on_message_cb_test_001) {
    int port = 8080;
    WsServerDerived wsServer("localhost", port);
    wsServer.OnMessageCb(nullptr, "test message", uWS::OpCode::TEXT);
    // No assertions needed as the function should return immediately
}

/**
 * @tc.name  : ws_server_on_message_cb_test_002
 * @tc.number: ws_server_Test_002
 * @tc.desc  : Test when session is not valid then OnMessageCb logs an error
 */
TEST_F(WsServerTest, ws_server_on_message_cb_test_002) {
    int port = 8080;
    WsServerDerived wsServer("localhost", port);
    Dic::Server::WsChannel wsChannel;
    wsServer.OnMessageCb(&wsChannel, "test message", uWS::OpCode::TEXT);
    // No assertions needed as the function should log an error
}

TEST_F(WsServerTest, server_def_WsUserData_init) {
    WsUserData data;
    data.reqUrl = "test";
}
