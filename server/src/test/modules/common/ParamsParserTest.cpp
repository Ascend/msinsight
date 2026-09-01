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

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <gtest/gtest.h>
#include "ParamsParser.h"
#include "WsServer.h"
#include "ServerLog.h"
#include "WsSessionManager.h"
#include "WsSender.h"
#include "RequestFilter.h"
#include "ModuleRequestHandler.h"

class ParamsParserTest : public ::testing::Test {};

TEST_F(ParamsParserTest, testParamsParser) {
    Dic::Server::ParamsParser::Instance();
    Dic::Server::ParamsParser::Instance().Parse(
        {"exe", "--wsPort=9000", "--wsHost=127.0.0.1", "--logPath=./", "--logSize=10", "--logLevel=INFO"});
    int64_t expectPort = 9000;
    int64_t expectLogSize = 10;
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().wsPort, expectPort);
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().logSize, expectLogSize);
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().host, "127.0.0.1");
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().logLevel, "INFO");
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().logPath, "./");
}

TEST_F(ParamsParserTest, testParamsParserErr) {
    Dic::Server::ParamsParser::Instance();
    bool result = Dic::Server::ParamsParser::Instance().Parse({"exe", "--wsPort=9200"});
    EXPECT_EQ(result, false);
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetError(), "ERROR: Port error, port range is 9000-9100.");
    result = Dic::Server::ParamsParser::Instance().Parse({"exe", "--errtest=err"});
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetError(), "ERROR: --errtest=err has not been supported.");
    EXPECT_EQ(result, false);
    result = Dic::Server::ParamsParser::Instance().Parse({"exe", "--wsHost=err"});
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetError(), "ERROR: Host is not valid.");
    EXPECT_EQ(result, false);
    result = Dic::Server::ParamsParser::Instance().Parse({"exe"});
    EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetError(), "ERROR: Startup parameter count is not enough.");
    EXPECT_EQ(result, false);
}

TEST_F(ParamsParserTest, testWsSession) {
    Dic::Server::WsChannel *ws = nullptr;
    std::unique_ptr<Dic::Server::WsSessionImpl> session = std::make_unique<Dic::Server::WsSessionImpl>(ws);
    int waitTime = 10;
    session->WaitForExit(waitTime);
    EXPECT_EQ(session->GetChannel(), nullptr);
    EXPECT_NE(session->GetCreateTime(), 0);
    EXPECT_EQ(session->GetStartTime(), 0);
    EXPECT_EQ(session->GetStopTime(), 0);
    uint32_t deadTime = 100;
    session->SetDeadTime(deadTime);
    EXPECT_EQ(session->GetDeadTime(), deadTime);
    session->OnRequestMessage("data");
    Dic::Protocol::Event event1(R"({"key":"val"})");
    session->SetBundleName("BundleName");
    EXPECT_EQ(session->GetBundleName(), "BundleName");
    session->SetDeviceKey("deviceKey");
    EXPECT_EQ(session->GetDeviceKey(), "deviceKey");
    session->SendEvent(event1);
    Dic::Server::WsSessionManager::Instance().AddSession(std::move(session));
    auto getSession = Dic::Server::WsSessionManager::Instance().GetSession();
    Dic::Protocol::Event event("1");
    EXPECT_EQ(getSession == nullptr, false);
    auto curSession = Dic::Server::WsSessionManager::Instance().GetSession();
    if (curSession != nullptr) {
        curSession->SetStatus(Dic::Server::WsSession::Status::CLOSED);
        curSession->WaitForExit();
        Dic::Server::WsSessionManager::Instance().RemoveSession();
    }
}

TEST_F(ParamsParserTest, testServerStart) {
    std::vector<std::string> args = {"exe", "--wsPort=9003", "--wsHost=127.0.0.1", "--logSize=10", "--logLevel=INFO"};
    Dic::Server::ParamsParser::Instance().Parse(args);
    const Dic::Server::ParamsOption &option = Dic::Server::ParamsParser::Instance().GetOption();
    Dic::Server::ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, std::to_string(option.wsPort));
    Dic::Server::WsServer server(option.host, option.wsPort);
    server.Start();
    const int checkInterval = 1000;
    if (server.IsStart()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(checkInterval));
    }
    EXPECT_EQ(server.IsStart(), true);
    server.Stop();
    EXPECT_EQ(server.IsStart(), false);
}

TEST_F(ParamsParserTest, testParamsParserWsHostIPv6) {
    // 覆盖 IPv6 各类标准缩写形式：全写、::、::1、含中间 ::、前/后/中段缩写
    const std::vector<std::string> validHosts = {"::", "::1", "::ffff:0:0", "1::", "1::8", "1:2:3:4:5:6:7:8",
        "2001:db8::1", "fe80::1", "1:2::7:8", "1::6:7:8", "::7:8"};
    for (const auto &host : validHosts) {
        bool result = Dic::Server::ParamsParser::Instance().Parse({"exe", "--wsPort=9000", "--wsHost=" + host});
        EXPECT_EQ(result, true) << "expected valid host: " << host;
        EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetOption().host, host);
    }
}

TEST_F(ParamsParserTest, testParamsParserWsHostInvalid) {
    // IPv4-mapped、zone id、段数越界、段位超长、非 IP 字符串应被拒绝
    // IPv4 段值越界（如 256、999）也应在参数校验阶段被拒绝，避免延迟到监听阶段才失败
    const std::vector<std::string> invalidHosts = {"192.168.0", "1.2.3.4.5", "ggg", "fe80::1%eth0", "::ffff:1.2.3.4",
        "1:2:3:4:5:6:7:8:9", "gggg::1", "999.999.999.999", "256.1.1.1", "1.2.3.256", "01.02.03.04", "1.2.3.4.5.6"};
    for (const auto &host : invalidHosts) {
        bool result = Dic::Server::ParamsParser::Instance().Parse({"exe", "--wsPort=9000", "--wsHost=" + host});
        EXPECT_EQ(result, false) << "expected invalid host: " << host;
        EXPECT_EQ(Dic::Server::ParamsParser::Instance().GetError(), "ERROR: Host is not valid.");
    }
}

// issue #499：会话移除/关闭后，发送响应与事件不得崩溃
TEST_F(ParamsParserTest, testWsSenderAfterSessionRemoved) {
    Dic::Server::WsSessionManager::Instance().RemoveSession(); // 清理单例
    auto responsePtr = std::make_unique<Dic::Protocol::Response>("test-command");
    EXPECT_NO_THROW(Dic::SendResponse(std::move(responsePtr), false, "session gone"));
    auto eventPtr = std::make_unique<Dic::Protocol::Event>("test-event");
    EXPECT_NO_THROW(Dic::SendEvent(std::move(eventPtr)));

    Dic::Server::WsChannel *ws = nullptr;
    auto session = std::make_shared<Dic::Server::WsSessionImpl>(ws);
    Dic::Server::WsSessionManager::Instance().AddSession(session);
    EXPECT_NE(Dic::Server::WsSessionManager::Instance().GetSession(), nullptr);
    session->SetStatus(Dic::Server::WsSession::Status::CLOSED);
    session->WaitForExit(200);
    Dic::Server::WsSessionManager::Instance().RemoveSession();
    EXPECT_EQ(Dic::Server::WsSessionManager::Instance().GetSession(), nullptr);

    auto responsePtr2 = std::make_unique<Dic::Protocol::Response>("test-command-2");
    EXPECT_NO_THROW(Dic::SendResponse(std::move(responsePtr2), false, "session closed"));
    auto eventPtr2 = std::make_unique<Dic::Protocol::Event>("test-event-2");
    EXPECT_NO_THROW(Dic::SendEvent(std::move(eventPtr2)));
}

// issue #499：工作线程仍在运行时会话被移除（模拟 WaitForExit 超时路径），不得产生 UAF
// 验证手段：释放全部外部引用后用 weak_ptr 探测对象存活——修复后 worker 闭包持有
// shared_ptr（std::thread 构造时即拷贝，与线程实际开始运行无关），对象必须存活；
// 随后标记 CLOSED 令线程退出，最后一个引用释放后对象应被析构。
// 时序上无调度依赖：CLOSED 之前线程必然不退出，析构必然在线程退出后发生。
TEST_F(ParamsParserTest, testWsSessionRemoveWhileWorkerRunning) {
    Dic::Server::WsSessionManager::Instance().RemoveSession(); // 清理单例

    Dic::Server::WsChannel *ws = nullptr;
    std::weak_ptr<Dic::Server::WsSessionImpl> observer;
    {
        auto session = std::make_shared<Dic::Server::WsSessionImpl>(ws);
        observer = session;
        Dic::Server::WsSessionManager::Instance().AddSession(session); // 启动 OnHandleMsgBuffer 线程
    } // 释放局部引用；manager 仍持有

    // 释放 manager 的最后一个外部引用。此刻线程闭包的 shared_ptr 已在
    // std::thread 构造时拷贝完成：若 Fix B 缺失（std::ref 悬垂）对象立即析构；
    // 若 Fix B 生效，线程未观察到 CLOSED 前绝不退出，对象必然存活。
    Dic::Server::WsSessionManager::Instance().RemoveSession();
    std::shared_ptr<Dic::Server::WsSessionImpl> pinned = observer.lock();
    ASSERT_NE(pinned, nullptr); // 确定性断言：与线程调度无关

    // 放行 worker：经存活引用标记 CLOSED
    pinned->SetStatus(Dic::Server::WsSession::Status::CLOSED);
    pinned.reset();

    // 等待 worker 观察到 CLOSED 并退出（10ms/轮），线程持有的最后一个
    // shared_ptr 释放后对象应被析构——必然事件，轮询等待
    const int maxWaitMs = 2000;
    for (int waited = 0; waited < maxWaitMs && !observer.expired(); waited += 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(observer.expired()); // 线程退出后对象被完整释放，无泄漏
}

// issue #499：旧会话移除后新会话建立，CheckSession 并发读写不得崩溃且状态正确
TEST_F(ParamsParserTest, testCheckSessionConcurrentWithRemove) {
    Dic::Server::WsSessionManager::Instance().RemoveSession(); // 清理单例

    Dic::Server::WsChannel *ws = nullptr;
    auto session = std::make_shared<Dic::Server::WsSessionImpl>(ws);
    Dic::Server::WsSessionManager::Instance().AddSession(session);
    EXPECT_TRUE(Dic::Server::WsSessionManager::Instance().CheckSession());

    // 并发压力：多线程反复 CheckSession，主线程 Remove（曾是无锁读 shared_ptr 的数据竞争）
    std::atomic<bool> stop{false};
    std::vector<std::thread> checkers;
    for (int i = 0; i < 4; i++) {
        checkers.emplace_back([&stop]() {
            while (!stop.load()) {
                Dic::Server::WsSessionManager::Instance().CheckSession();
            }
        });
    }
    session->SetStatus(Dic::Server::WsSession::Status::CLOSED);
    Dic::Server::WsSessionManager::Instance().RemoveSession();
    EXPECT_FALSE(Dic::Server::WsSessionManager::Instance().CheckSession());

    // 移除后建立新会话（模拟重连），旧线程的 CheckSession 不得观察到已析构的旧会话
    auto session2 = std::make_shared<Dic::Server::WsSessionImpl>(ws);
    Dic::Server::WsSessionManager::Instance().AddSession(session2);
    EXPECT_TRUE(Dic::Server::WsSessionManager::Instance().CheckSession());

    stop.store(true);
    for (auto &t : checkers) {
        t.join();
    }
    // 收尾：让 worker 退出并移除会话，避免污染后续用例
    session2->SetStatus(Dic::Server::WsSession::Status::CLOSED);
    session2->WaitForExit(200);
    Dic::Server::WsSessionManager::Instance().RemoveSession();
}

// issue #499：会话已移除时，过期请求分支不得空指针解引用
TEST_F(ParamsParserTest, testOutdatedRequestWithoutSessionNotCrash) {
    class FakeHandler : public Dic::Module::ModuleRequestHandler {
        bool HandleRequest(std::unique_ptr<Dic::Protocol::Request>) override { return true; }
    };
    Dic::Server::WsSessionManager::Instance().RemoveSession(); // 清理单例
    Dic::Server::RequestFilter::Instance().SetRequestId("1", 2); // 使 id=1 命中过期过滤
    FakeHandler handler;
    auto request = std::make_unique<Dic::Protocol::Request>(std::string("cmd"));
    request->id = 1;
    EXPECT_FALSE(handler.HandleRequestEntrance(std::move(request)));
    Dic::Server::RequestFilter::Instance().ClearKey("1", 2); // 清理单例状态
}
