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

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <gtest/gtest.h>
#include <functional>
#include "GlobalProtocolEvent.h"
#include "GlobalProtocolResponse.h"
#include "JsonUtil.h"
#include "ProtocolManager.h"
#include "TraceIdManager.h"
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
    uWS::App::WebSocketBehavior<WsUserData> CreateWsBehavior() { return WsServer::CreateWsBehavior(); }
    using WsServer::AddGetHandler;
    using WsServer::AddPostHandler;

    uWS::App &GetApp() {
        if (!wsApp) {
            wsApp = std::make_unique<uWS::App>();
        }
        return *wsApp;
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

TEST_F(WsServerTest, ws_server_uses_enlarged_backpressure_limit) {
    int port = 8080;
    WsServerDerived wsServer("localhost", port);
    auto behavior = wsServer.CreateWsBehavior();
    EXPECT_EQ(behavior.maxBackpressure, 200U * 1024U * 1024U);
}

namespace {
// Use the same event loop as the server, with a bounded timeout and an ephemeral loopback port.
class LoopbackClient {
  public:
    explicit LoopbackClient(uWS::App &app) : app(app) {
#ifdef _WIN32
        WSADATA socketData;
        EXPECT_EQ(WSAStartup(MAKEWORD(2, 2), &socketData), 0);
#endif
        context = us_create_socket_context(0, NativeLoop(), sizeof(LoopbackClient *), {});
        *static_cast<LoopbackClient **>(us_socket_context_ext(0, context)) = this;
        us_socket_context_on_open(0, context, [](us_socket_t *socket, int, char *, int) {
            auto &client = FromSocket(socket);
            client.socket = socket;
            client.Write(client.request);
            if (!client.secondChunk.empty() || client.abortRequest) {
                client.chunkTimer = us_create_timer(NativeLoop(), 0, sizeof(LoopbackClient *));
                *static_cast<LoopbackClient **>(us_timer_ext(client.chunkTimer)) = &client;
                us_timer_set(
                    client.chunkTimer,
                    [](us_timer_t *timer) {
                        auto &client = **static_cast<LoopbackClient **>(us_timer_ext(timer));
                        if (client.beforeSecondChunk) {
                            client.beforeSecondChunk();
                        }
                        if (client.abortRequest) {
                            client.Finish();
                        } else {
                            client.Write(client.secondChunk);
                        }
                    },
                    20, 0);
            }
            return socket;
        });
        us_socket_context_on_data(0, context, [](us_socket_t *socket, char *data, int length) {
            auto &client = FromSocket(socket);
            client.received.append(data, static_cast<size_t>(length));
            if (!client.waitForClose && client.ReadPayload()) {
                client.Finish();
            }
            return socket;
        });
        us_socket_context_on_close(0, context, [](us_socket_t *socket, int, void *) {
            auto &client = FromSocket(socket);
            client.socket = nullptr;
            client.Finish();
            return socket;
        });
        us_socket_context_on_end(0, context, [](us_socket_t *socket) {
            auto &client = FromSocket(socket);
            client.peerClosed = true;
            client.Finish();
            return socket;
        });
        us_socket_context_on_connect_error(0, context, [](us_socket_t *socket, int) {
            auto &client = FromSocket(socket);
            client.connectFailed = true;
            client.socket = nullptr;
            client.Finish();
            return socket;
        });
        us_socket_context_on_writable(0, context, [](us_socket_t *socket) {
            FromSocket(socket).FlushWrite();
            return socket;
        });
        us_socket_context_on_timeout(0, context, [](us_socket_t *socket) { return socket; });
    }

    ~LoopbackClient() {
        Finish();
        us_socket_context_free(0, context);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool Run(const std::string &requestData, size_t responseSize, bool websocket = false) {
        request = requestData;
        payloadSize = responseSize;
        isWebSocket = websocket;
        int port = 0;
        app.listen("127.0.0.1", 0, [&port](us_listen_socket_t *listener) {
            if (listener) {
                port = us_socket_local_port(0, reinterpret_cast<us_socket_t *>(listener));
            }
        });
        if (port <= 0) {
            ADD_FAILURE() << "Failed to get loopback listening port: " << port;
            return false;
        }
        timeoutTimer = us_create_timer(NativeLoop(), 0, sizeof(LoopbackClient *));
        *static_cast<LoopbackClient **>(us_timer_ext(timeoutTimer)) = this;
        us_timer_set(
            timeoutTimer,
            [](us_timer_t *timer) {
                auto &client = **static_cast<LoopbackClient **>(us_timer_ext(timer));
                client.timedOut = true;
                client.Finish();
            },
            3000, 0);
        socket = us_socket_context_connect(0, context, "127.0.0.1", port, nullptr, 0, 0);
        if (!socket) {
            connectFailed = true;
            Finish();
        }
        uWS::Loop::get()->run();
        if (timedOut || connectFailed) {
            ADD_FAILURE() << "Loopback request failed: timeout=" << timedOut << ", connect=" << connectFailed
                          << ", pending write=" << pendingWrite.size() << ", socket error=" << socketError
                          << ", received=" << received;
        }
        return !timedOut && !connectFailed;
    }

    std::string received;
    std::string payload;
    std::string secondChunk;
    std::function<void()> beforeSecondChunk;
    bool abortRequest = false;
    bool waitForClose = false;
    bool peerClosed = false;

  private:
    static us_loop_t *NativeLoop() { return reinterpret_cast<us_loop_t *>(uWS::Loop::get()); }

    static LoopbackClient &FromSocket(us_socket_t *socket) {
        return **static_cast<LoopbackClient **>(us_socket_context_ext(0, us_socket_context(0, socket)));
    }

    void Write(const std::string &data) {
        pendingWrite.append(data);
        FlushWrite();
    }

    void FlushWrite() {
        if (!socket || pendingWrite.empty()) {
            return;
        }
        int written = us_socket_write(0, socket, pendingWrite.data(), static_cast<int>(pendingWrite.size()), 0);
#ifdef _WIN32
        socketError = WSAGetLastError();
#endif
        pendingWrite.erase(0, static_cast<size_t>(written));
    }

    bool ReadPayload() {
        auto headerEnd = received.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            return false;
        }
        size_t payloadStart = headerEnd + 4;
        if (isWebSocket) {
            if (received.size() < payloadStart + 2) {
                return false;
            }
            const auto opcode = static_cast<unsigned char>(received[payloadStart]) & 0x0f;
            EXPECT_EQ(opcode, uWS::OpCode::TEXT);
            const auto length = static_cast<unsigned char>(received[payloadStart + 1]) & 0x7f;
            if (length == 126) {
                if (received.size() < payloadStart + 4) {
                    return false;
                }
                payloadSize = (static_cast<unsigned char>(received[payloadStart + 2]) << 8) |
                    static_cast<unsigned char>(received[payloadStart + 3]);
                payloadStart += 4;
            } else {
                EXPECT_LT(length, 126);
                payloadSize = length;
                payloadStart += 2;
            }
        }
        if (received.size() < payloadStart + payloadSize) {
            return false;
        }
        payload = received.substr(payloadStart, payloadSize);
        return true;
    }

    void Finish() {
        if (finished) {
            return;
        }
        finished = true;
        if (socket) {
            us_socket_close(0, socket, 0, nullptr);
            socket = nullptr;
        }
        app.close();
        if (timeoutTimer) {
            us_timer_close(timeoutTimer);
            timeoutTimer = nullptr;
        }
        if (chunkTimer) {
            us_timer_close(chunkTimer);
            chunkTimer = nullptr;
        }
    }

    uWS::App &app;
    us_socket_context_t *context = nullptr;
    us_socket_t *socket = nullptr;
    us_timer_t *timeoutTimer = nullptr;
    us_timer_t *chunkTimer = nullptr;
    std::string request;
    std::string pendingWrite;
    size_t payloadSize = 0;
    bool isWebSocket = false;
    bool finished = false;
    bool timedOut = false;
    bool connectFailed = false;
    int socketError = 0;
};

class RecordingApiHandler : public ApiHandler {
  public:
    explicit RecordingApiHandler(API_TYPE type) : ApiHandler(type) {}
    bool run(std::string_view data, std::string &result) override {
        ++calls;
        input = std::string(data);
        result = "handler-result";
        return true;
    }
    int calls = 0;
    std::string input;
};

class WsSessionImplDerived : public WsSessionImpl {
  public:
    using WsSessionImpl::WsSessionImpl;
    using WsSessionImpl::OnHandleResponseQueue;
    using WsSessionImpl::responseQueue;
    void OnChannelClosed() {
        channel = nullptr;
        SetStatus(Status::CLOSED);
    }
};

void RunDeferredCallbacks() {
    // Keep the loop alive long enough to process defer(), without creating worker threads.
    auto timer = us_create_timer(reinterpret_cast<us_loop_t *>(uWS::Loop::get()), 0, 0);
    us_timer_set(timer, [](us_timer_t *timer) { us_timer_close(timer); }, 1, 0);
    uWS::Loop::get()->run();
}

std::unique_ptr<Dic::Protocol::TokenHeartCheckResponse> MakeResponse() {
    auto response = std::make_unique<Dic::Protocol::TokenHeartCheckResponse>();
    response->moduleName = "global";
    response->id = 7;
    response->requestId = 6;
    response->result = true;
    return response;
}
} // namespace

TEST_F(WsServerTest, AddGetHandlerPassesQueryAndReturnsCorsHeaders) {
    WsServerDerived server("127.0.0.1", 0);
    auto handler = std::make_shared<RecordingApiHandler>(API_TYPE::GET);
    LoopbackClient client(server.GetApp());
    server.AddGetHandler("/ut/get", handler);
    ASSERT_TRUE(client.Run("GET /ut/get?name=test&count=2 HTTP/1.1\r\nHost: localhost\r\n\r\n", 14));
    EXPECT_EQ(handler->calls, 1);
    EXPECT_EQ(handler->input, "name=test&count=2");
    EXPECT_EQ(client.payload, "handler-result");
    EXPECT_NE(client.received.find("Access-Control-Allow-Origin: *\r\n"), std::string::npos);
    EXPECT_NE(client.received.find("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"), std::string::npos);
    EXPECT_NE(client.received.find("Access-Control-Allow-Headers: Content-Type\r\n"), std::string::npos);
}

TEST_F(WsServerTest, AddGetHandlerPassesEmptyQuery) {
    WsServerDerived server("127.0.0.1", 0);
    auto handler = std::make_shared<RecordingApiHandler>(API_TYPE::GET);
    LoopbackClient client(server.GetApp());
    server.AddGetHandler("/ut/get", handler);
    ASSERT_TRUE(client.Run("GET /ut/get HTTP/1.1\r\nHost: localhost\r\n\r\n", 14));
    EXPECT_EQ(handler->calls, 1);
    EXPECT_TRUE(handler->input.empty());
    EXPECT_EQ(client.payload, "handler-result");
}

TEST_F(WsServerTest, AddPostHandlerAccumulatesBodyBeforeCallingHandler) {
    WsServerDerived server("127.0.0.1", 0);
    auto handler = std::make_shared<RecordingApiHandler>(API_TYPE::POST);
    LoopbackClient client(server.GetApp());
    server.AddPostHandler("/ut/post", handler);
    client.secondChunk = "-second";
    client.beforeSecondChunk = [handler]() { EXPECT_EQ(handler->calls, 0); };
    ASSERT_TRUE(client.Run("POST /ut/post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 12\r\n\r\nfirst", 14));
    EXPECT_EQ(handler->calls, 1);
    EXPECT_EQ(handler->input, "first-second");
    EXPECT_EQ(client.payload, "handler-result");
    EXPECT_NE(client.received.find("Access-Control-Allow-Origin: *\r\n"), std::string::npos);
    EXPECT_NE(client.received.find("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"), std::string::npos);
    EXPECT_NE(client.received.find("Access-Control-Allow-Headers: Content-Type\r\n"), std::string::npos);
}

TEST_F(WsServerTest, AddPostHandlerAcceptsEmptyBody) {
    WsServerDerived server("127.0.0.1", 0);
    auto handler = std::make_shared<RecordingApiHandler>(API_TYPE::POST);
    LoopbackClient client(server.GetApp());
    server.AddPostHandler("/ut/post", handler);
    ASSERT_TRUE(client.Run("POST /ut/post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n", 14));
    EXPECT_EQ(handler->calls, 1);
    EXPECT_TRUE(handler->input.empty());
    EXPECT_EQ(client.payload, "handler-result");
}

TEST_F(WsServerTest, AddPostHandlerDoesNotRunForAbortedBody) {
    WsServerDerived server("127.0.0.1", 0);
    auto handler = std::make_shared<RecordingApiHandler>(API_TYPE::POST);
    LoopbackClient client(server.GetApp());
    server.AddPostHandler("/ut/post", handler);
    client.abortRequest = true;
    ASSERT_TRUE(client.Run("POST /ut/post HTTP/1.1\r\nHost: localhost\r\nContent-Length: 12\r\n\r\nfirst", 14));
    RunDeferredCallbacks();
    EXPECT_EQ(handler->calls, 0);
    EXPECT_TRUE(client.payload.empty());
}

class WsSessionImplTest : public ::testing::Test {
  protected:
    void SetUp() override { previousTraceId = TraceIdManager::GetTraceId(); }
    void TearDown() override {
        RunDeferredCallbacks();
        TraceIdManager::SetTraceId(previousTraceId);
    }

    void ExpectWebSocketPayload(const std::function<void(WsSessionImplDerived &)> &action, const std::string &expected,
        bool waitForClose = false, unsigned int maxBackpressure = 1024) {
        uWS::App app;
        std::shared_ptr<WsSessionImplDerived> session;
        uWS::App::WebSocketBehavior<WsUserData> behavior;
        behavior.maxBackpressure = maxBackpressure;
        behavior.open = [&](WsChannel *channel) {
            session = std::make_shared<WsSessionImplDerived>(channel);
            action(*session);
        };
        behavior.close = [&](WsChannel *, int, std::string_view) {
            if (session) {
                session->OnChannelClosed();
            }
        };
        app.ws<WsUserData>("/ut/ws", std::move(behavior));
        LoopbackClient client(app);
        client.waitForClose = waitForClose;
        ASSERT_TRUE(client.Run("GET /ut/ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                               "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                               "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n",
            expected.size(), true));
        ASSERT_NE(session, nullptr);
        EXPECT_NE(client.received.find("101 Switching Protocols"), std::string::npos);
        if (waitForClose) {
            EXPECT_TRUE(client.peerClosed);
        } else {
            EXPECT_EQ(client.payload, expected);
        }
    }

    std::string previousTraceId;
};

TEST_F(WsSessionImplTest, OnHandleResponseQueueReturnsWhenQueueIsDisabled) {
    WsSessionImplDerived session(nullptr);
    auto response = MakeResponse();
    session.responseQueue << std::move(response);
    WsSessionImplDerived::OnHandleResponseQueue(session);
    // The production queue switch is const false; entering the loop would alter production behavior.
    EXPECT_EQ(session.responseQueue.Size(), 1);
    session.SetStatus(WsSession::Status::CLOSED);
    WsSessionImplDerived::OnHandleResponseQueue(session);
    EXPECT_EQ(session.responseQueue.Size(), 1);
}

TEST_F(WsSessionImplTest, OnResponseIgnoresNullResponse) {
    WsSessionImplDerived session(nullptr);
    session.OnResponse(nullptr);
    EXPECT_TRUE(session.responseQueue.Empty());
    EXPECT_EQ(session.GetStatus(), WsSession::Status::INIT);
}

TEST_F(WsSessionImplTest, OnResponseSerializesAndSendsResponse) {
    auto response = MakeResponse();
    std::string error;
    auto json = Dic::Protocol::ProtocolManager::Instance().ToJson(*response, error);
    ASSERT_TRUE(json.has_value());
    const auto expected = StringUtil::ToUtf8Str(JsonUtil::JsonDump(*json));
    ExpectWebSocketPayload([](WsSessionImplDerived &session) { session.OnResponse(MakeResponse()); }, expected);
}

TEST_F(WsSessionImplTest, SendBaseResponseIgnoresNullResponse) {
    WsSessionImplDerived session(nullptr);
    session.SendBaseResponse(nullptr);
    EXPECT_EQ(session.GetStatus(), WsSession::Status::INIT);
}

TEST_F(WsSessionImplTest, SendBaseResponseSendsBaseFieldsWithoutModuleSerializer) {
    Dic::Protocol::Response response(std::string("ut/base"));
    response.moduleName = "unregistered-ut-module";
    response.id = 9;
    response.requestId = 8;
    response.error = Dic::Protocol::ErrorMessage{4003, "invalid parameter"};
    response.resultCallbackId = 10;
    document_t json(kObjectType);
    Dic::Protocol::ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    ExpectWebSocketPayload(
        [response](WsSessionImplDerived &session) {
            session.SendBaseResponse(std::make_unique<Dic::Protocol::Response>(response));
        },
        StringUtil::ToUtf8Str(JsonUtil::JsonDump(json)));
}

TEST_F(WsSessionImplTest, OnEventIgnoresNullEvent) {
    WsSessionImplDerived session(nullptr);
    session.OnEvent(nullptr);
    EXPECT_EQ(session.GetStatus(), WsSession::Status::INIT);
}

TEST_F(WsSessionImplTest, OnEventSerializesAndSendsEvent) {
    Dic::Protocol::ReadFileFailEvent event;
    event.moduleName = "global";
    event.id = 11;
    event.body.filePath = "ut-input.json";
    event.body.error = "read failed";
    std::string error;
    auto json = Dic::Protocol::ProtocolManager::Instance().ToJson(event, error);
    ASSERT_TRUE(json.has_value());
    ExpectWebSocketPayload(
        [event](WsSessionImplDerived &session) {
            session.OnEvent(std::make_unique<Dic::Protocol::ReadFileFailEvent>(event));
        },
        StringUtil::ToUtf8Str(JsonUtil::JsonDump(*json)));
}

TEST_F(WsSessionImplTest, SendReturnsFalseWithoutChannel) {
    WsSessionImplDerived session(nullptr);
    EXPECT_FALSE(session.Send("ut-message"));
}

TEST_F(WsSessionImplTest, SendReturnsFalseWhenSessionIsClosed) {
    ExpectWebSocketPayload(
        [](WsSessionImplDerived &session) {
            session.SetStatus(WsSession::Status::CLOSED);
            EXPECT_FALSE(session.Send("must-not-send"));
            session.SetStatus(WsSession::Status::INIT);
            EXPECT_TRUE(session.Send("ut-message"));
        },
        "ut-message");
}

TEST_F(WsSessionImplTest, SendReturnsTrueWhenChannelSendSucceeds) {
    ExpectWebSocketPayload(
        [](WsSessionImplDerived &session) { EXPECT_TRUE(session.Send("ut-message")); }, "ut-message");
}

TEST_F(WsSessionImplTest, SendReturnsFalseWhenChannelDropsMessage) {
    ExpectWebSocketPayload(
        [](WsSessionImplDerived &session) {
            auto channel = const_cast<WsChannel *>(session.GetChannel());
            auto data = static_cast<uWS::WebSocketData *>(us_socket_ext(0, reinterpret_cast<us_socket_t *>(channel)));
            // Model an existing backpressure buffer larger than the configured one-byte limit.
            data->buffer.append("xx", 2);
            EXPECT_FALSE(session.Send("must-not-send"));
            data->buffer.clear();
            EXPECT_TRUE(session.Send("ut-message"));
        },
        "ut-message", false, 1);
}

TEST_F(WsSessionImplTest, SendResponseReturnsWhenSerializationFails) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    Dic::Protocol::Response response(std::string("ut/unknown"));
    TraceIdManager::SetTraceId("response-request");
    session->SendResponse(response);
    TraceIdManager::SetTraceId("before-dispatch");
    RunDeferredCallbacks();
    EXPECT_EQ(TraceIdManager::GetTraceId(), "before-dispatch");
}

TEST_F(WsSessionImplTest, OnResponseReturnsWhenSerializationFails) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    session->OnResponse(std::make_unique<Dic::Protocol::Response>(std::string("ut/unknown")));
    EXPECT_TRUE(session->responseQueue.Empty());
}

TEST_F(WsSessionImplTest, SendResponseRestoresTraceIdInDeferredCallback) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    TraceIdManager::SetTraceId("response-request");
    session->SendResponse(*MakeResponse());
    TraceIdManager::SetTraceId("before-dispatch");
    RunDeferredCallbacks();
    EXPECT_EQ(TraceIdManager::GetTraceId(), "response-request");
}

TEST_F(WsSessionImplTest, SendResponseSkipsClosedSessionInDeferredCallback) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    TraceIdManager::SetTraceId("response-request");
    session->SendResponse(*MakeResponse());
    session->SetStatus(WsSession::Status::CLOSED);
    TraceIdManager::SetTraceId("before-dispatch");
    RunDeferredCallbacks();
    EXPECT_EQ(TraceIdManager::GetTraceId(), "before-dispatch");
}

TEST_F(WsSessionImplTest, SendResponseDoesNotKeepExpiredSessionAlive) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    std::weak_ptr<WsSessionImplDerived> observer = session;
    TraceIdManager::SetTraceId("response-request");
    session->SendResponse(*MakeResponse());
    session.reset();
    EXPECT_TRUE(observer.expired());
    TraceIdManager::SetTraceId("before-dispatch");
    RunDeferredCallbacks();
    EXPECT_EQ(TraceIdManager::GetTraceId(), "before-dispatch");
}

TEST_F(WsSessionImplTest, OnEventReturnsWhenSerializationFails) {
    auto session = std::make_shared<WsSessionImplDerived>(nullptr);
    TraceIdManager::SetTraceId("event-request");
    session->OnEvent(std::make_unique<Dic::Protocol::Event>(std::string("ut/unknown")));
    TraceIdManager::SetTraceId("before-dispatch");
    RunDeferredCallbacks();
    EXPECT_EQ(TraceIdManager::GetTraceId(), "before-dispatch");
}

TEST_F(WsSessionImplTest, StopClosesSessionWithoutChannel) {
    WsSessionImplDerived session(nullptr);
    session.SetStatus(WsSession::Status::STARTED);
    session.Stop();
    EXPECT_EQ(session.GetStatus(), WsSession::Status::CLOSED);
    EXPECT_EQ(session.GetChannel(), nullptr);
    EXPECT_GE(session.GetStopTime(), session.GetCreateTime());
    EXPECT_FALSE(session.Send("after-stop"));
    session.Stop();
    EXPECT_EQ(session.GetChannel(), nullptr);
}

TEST_F(WsSessionImplTest, StopClosesAndClearsChannel) {
    ExpectWebSocketPayload(
        [](WsSessionImplDerived &session) {
            // Flush the upgrade response before closing the WebSocket on its event loop.
            auto self = session.shared_from_this();
            uWS::Loop::get()->defer([self]() {
                ASSERT_NE(self->GetChannel(), nullptr);
                self->Stop();
                EXPECT_EQ(self->GetStatus(), WsSession::Status::CLOSED);
                EXPECT_EQ(self->GetChannel(), nullptr);
                EXPECT_GE(self->GetStopTime(), self->GetCreateTime());
            });
        },
        "", true);
}
