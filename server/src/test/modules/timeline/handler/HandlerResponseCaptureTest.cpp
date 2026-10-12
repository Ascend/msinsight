/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "HandlerTest.cpp"

class HandlerResponseCaptureTest : public HandlerTest {
  protected:
    class CountingSession : public ResponseSession {
      public:
        void Start() override {
            ++startCount;
            ResponseSession::Start();
        }
        int startCount = 0;
    };
};

TEST_F(HandlerResponseCaptureTest, RestoresPreviousSessionAndRequestErrorWithoutRestarting) {
    ResponseCapture originalState;
    auto &manager = Dic::Server::WsSessionManager::Instance();
    auto &context = Dic::Server::RequestContext::GetInstance();
    auto previous = std::make_shared<CountingSession>();
    manager.RemoveSession();
    manager.AddSession(previous);
    previous->SetStatus(Dic::Server::WsSession::Status::CLOSED);
    context.SetError({.code = 123, .message = "previous request error"});

    {
        ResponseCapture capture;
        EXPECT_EQ(manager.GetSessionPtr(), capture.session);
        EXPECT_EQ(capture.session->GetStatus(), Dic::Server::WsSession::Status::STARTED);
        EXPECT_TRUE(context.GetError().message.empty());
        context.SetError({.code = 456, .message = "captured request error"});
    }

    EXPECT_EQ(manager.GetSessionPtr(), previous);
    EXPECT_EQ(previous->GetStatus(), Dic::Server::WsSession::Status::CLOSED);
    EXPECT_EQ(previous->startCount, 1);
    EXPECT_EQ(context.GetError().code, 123);
    EXPECT_EQ(context.GetError().message, "previous request error");
}

TEST_F(HandlerResponseCaptureTest, RestoresEmptySessionAndRequestError) {
    ResponseCapture originalState;
    auto &manager = Dic::Server::WsSessionManager::Instance();
    auto &context = Dic::Server::RequestContext::GetInstance();
    manager.RemoveSession();
    context.ResetError();
    const auto previousError = context.GetError();

    {
        ResponseCapture capture;
        EXPECT_EQ(manager.GetSessionPtr(), capture.session);
        context.SetError({.code = 789, .message = "temporary request error"});
    }

    EXPECT_EQ(manager.GetSessionPtr(), nullptr);
    EXPECT_EQ(context.GetError().code, previousError.code);
    EXPECT_EQ(context.GetError().message, previousError.message);
}
