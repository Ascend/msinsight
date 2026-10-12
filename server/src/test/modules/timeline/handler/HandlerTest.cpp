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
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include "WsSession.h"
#define private public
#include "WsSessionManager.h"
#undef private
#include "TextTraceDatabase.h"
#include "WsSessionImpl.h"
#include "RequestContext.h"
#include <vector>
class HandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

  protected:
    class ResponseSession : public Dic::Server::WsSession {
      public:
        void Start() override { status = Status::STARTED; }
        Status GetStatus() const override { return status; }
        void OnResponse(std::unique_ptr<Dic::Protocol::Response> response) override {
            responses.emplace_back(std::move(response));
        }
        void SendBaseResponse(std::unique_ptr<Dic::Protocol::Response> response) override {
            OnResponse(std::move(response));
        }
        void OnEvent(std::unique_ptr<Dic::Protocol::Event>) override {}
        void SetStatus(Status value) override { status = value; }
        void WaitForExit(int = 10000) override {}
        std::vector<std::unique_ptr<Dic::Protocol::Response>> responses;
        Status status = Status::INIT;
    };

    // Opt-in capture: existing suites retain their own session setup and teardown.
    class ResponseCapture {
      public:
        ResponseCapture() : session(std::make_shared<ResponseSession>()) {
            previousError = Dic::Server::RequestContext::GetInstance().GetError();
            Dic::Server::RequestContext::GetInstance().ResetError();
            session->Start();
            auto &manager = Dic::Server::WsSessionManager::Instance();
            std::unique_lock<std::mutex> lock(manager.sessionMutex);
            previous = std::exchange(manager.session, session);
        }
        ~ResponseCapture() {
            auto &manager = Dic::Server::WsSessionManager::Instance();
            {
                std::unique_lock<std::mutex> lock(manager.sessionMutex);
                // AddSession would call Start again and spawn workers on an existing real session.
                manager.session = std::move(previous);
            }
            Dic::Server::RequestContext::GetInstance().SetError(previousError);
        }
        ResponseCapture(const ResponseCapture &) = delete;
        ResponseCapture &operator=(const ResponseCapture &) = delete;
        std::shared_ptr<ResponseSession> session;

      private:
        std::shared_ptr<Dic::Server::WsSession> previous;
        Dic::Protocol::ErrorMessage previousError;
    };
    class MockDatabase : public Dic::Module::Timeline::TextTraceDatabase {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };
};
