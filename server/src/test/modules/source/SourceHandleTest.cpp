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
#include "QueryApiInstructionsHandler.h"
#include "QueryApiLineHandler.h"
#include "QueryCodeFileHandler.h"
#include "QueryDetailsBaseInfoHandler.h"
#include "QueryDetailsLoadInfoHandler.h"
#include "QueryDetailsMemoryGraphHandler.h"
#include "QueryDetailsMemoryTableHandler.h"
#include "QueryInterCoreLoadAnalysisGraphHandler.h"
#include "QueryDetailsRooflineHandler.h"
#include "SourceProtocolRequest.h"
#include "WsSessionManager.h"
#include "../../TestSuit.h"

using namespace Dic::Server;
using namespace Dic::Protocol;

class SourceHandleTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(SourceHandleTest, QueryApiInstructionsHandlerTest) {
    Dic::Module::Source::QueryApiInstructionsHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<SourceApiInstrRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryApiLineHandler) {
    Dic::Module::Source::QueryApiLineHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<SourceApiLineRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryCodeFileHandler) {
    Dic::Module::Source::QueryCodeFileHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<SourceCodeFileRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryDetailsBaseInfoHandler) {
    Dic::Module::Source::QueryDetailsBaseInfoHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<SourceDetailBaseInfoRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryDetailsLoadInfoHandler) {
    Dic::Module::Source::QueryDetailsLoadInfoHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<SourceDetailsLoadInfoRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryDetailsMemoryGraphHandler) {
    Dic::Module::Source::QueryDetailsMemoryGraphHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<DetailsMemoryGraphRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryDetailsMemoryTableHandler) {
    Dic::Module::Source::QueryDetailsMemoryTableHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<DetailsMemoryTableRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryInterCoreLoadAnalysisGraphHandler) {
    Dic::Module::Source::QueryInterCoreLoadAnalysisGraphHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<DetailsInterCoreLoadGraphRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(SourceHandleTest, QueryDetailsRooflineHandler) {
    Dic::Module::Source::QueryDetailsRooflineHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<DetailsRooflineRequest>();
    handler.HandleRequest(std::move(requestPtr));
}
