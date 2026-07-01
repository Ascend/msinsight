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
#include "ParamsParser.h"
#include "WsServer.h"
#include "ServerLog.h"
#include "WsSessionManager.h"

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
