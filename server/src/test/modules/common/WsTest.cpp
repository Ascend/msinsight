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

#include "../../TestSuit.h"
#include "../../TestSuit.h"
#include "ServerLog.h"
#include "JsonUtil.h"
#include "ProtocolManager.h"
#include "ModuleManager.h"
#include "WsSessionManager.h"
#include "SummaryProtocol.h"
#include "ParserStatusManager.h"
#include "ProtocolDefs.h"
#include "ClusterParseThreadPoolExecutor.h"
#include "ParamsParser.h"
#include "WsServer.h"
#include "ProtocolUtil.h"

using namespace Dic;

TEST_F(TestSuit, TestAllRequestHandler) {
    std::string err = "";
    std::ifstream file(TestSuit::GetTestDataFile("request.csv"));
    std::string line;
    int count = 0;
    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::unique_ptr<Protocol::Request> request = Protocol::ProtocolManager::Instance().FromJson(line, err);
        std::unique_ptr<Module::ProtocolMessage> msg = std::unique_ptr<Module::ProtocolMessage>(request.release());
        auto *reqPtr = dynamic_cast<Protocol::Request *>(msg.release());
        if (reqPtr != nullptr) {
            Module::ModuleManager::Instance().OnDispatchModuleRequest(std::unique_ptr<Protocol::Request>(reqPtr));
        }
        count++;
        EXPECT_EQ(err, "");
    }
    file.close();
    int size = 106;
    EXPECT_EQ(count, size);
}

TEST_F(TestSuit, TestAllRequestSessionErr) {
    std::string err = "";
    std::ifstream file(TestSuit::GetTestDataFile("request.csv"));
    std::string line;
    int count = 0;
    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::unique_ptr<Protocol::Request> request = Protocol::ProtocolManager::Instance().FromJson(line, err);
        std::unique_ptr<Module::ProtocolMessage> msg = std::unique_ptr<Module::ProtocolMessage>(request.release());
        auto *reqPtr = dynamic_cast<Protocol::Request *>(msg.release());
        if (reqPtr != nullptr) {
            Module::ModuleManager::Instance().OnDispatchModuleRequest(std::unique_ptr<Protocol::Request>(reqPtr));
        }
        count++;
        EXPECT_EQ(err, "");
    }
    file.close();
}
