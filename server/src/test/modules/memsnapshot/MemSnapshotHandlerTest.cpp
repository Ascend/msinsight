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

#include <gtest/gtest.h>
#include "WsSessionManager.h"
#include "WsSessionImpl.h"
#include "MemSnapshotProtocolRequest.h"
#include "QueryMemSnapshotBlockHandler.h"
#include "QueryMemSnapshotEventHandler.h"
#include "QueryMemSnapshotAllocationHandler.h"
#include "QueryMemSnapshotDetailHandler.h"
#include "QueryMemSnapshotLeakStatsHandler.h"
#include "DataBaseManager.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::MemSnapshot;
using namespace Dic::Protocol;
using namespace Dic;

class MemSnapshotHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        testDbPath = TestSuit::GetTestDataFile("snapshot", "snapshot_with_multi_devices.pkl.db");
        auto snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(testDbPath);
        ASSERT_TRUE(snapshotDb != nullptr);
        ASSERT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));
    }

    static void TearDownTestSuite() { DataBaseManager::Instance().Clear(DatabaseType::MEM_SNAPSHOT); }

  protected:
    static std::string testDbPath;
};

std::string MemSnapshotHandlerTest::testDbPath;

// ============== QueryMemSnapshotBlockHandler Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryBlocksTableWithValidParams) {
    QueryMemSnapshotBlockHandler handler;
    std::unique_ptr<MemSnapshotBlocksRequest> requestPtr = std::make_unique<MemSnapshotBlocksRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "BLOCK";
    requestPtr->params.currentPage = 1;
    requestPtr->params.pageSize = 10;
    requestPtr->params.orderBy = "id";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryBlocksViewWithValidParams) {
    QueryMemSnapshotBlockHandler handler;
    std::unique_ptr<MemSnapshotBlocksRequest> requestPtr = std::make_unique<MemSnapshotBlocksRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = false;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "BLOCK";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryBlocksWithInvalidEventIdxRange) {
    QueryMemSnapshotBlockHandler handler;
    std::unique_ptr<MemSnapshotBlocksRequest> requestPtr = std::make_unique<MemSnapshotBlocksRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "BLOCK";
    requestPtr->params.startEventIdx = 5000;
    requestPtr->params.endEventIdx = 100;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

// ============== QueryMemSnapshotEventHandler Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryEventsTableWithValidParams) {
    QueryMemSnapshotEventHandler handler;
    std::unique_ptr<MemSnapshotEventsRequest> requestPtr = std::make_unique<MemSnapshotEventsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.currentPage = 1;
    requestPtr->params.pageSize = 10;
    requestPtr->params.orderBy = "id";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventsViewWithValidParams) {
    QueryMemSnapshotEventHandler handler;
    std::unique_ptr<MemSnapshotEventsRequest> requestPtr = std::make_unique<MemSnapshotEventsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = false;
    requestPtr->params.pageSize = 10;
    requestPtr->params.currentPage = 1;
    requestPtr->params.deviceId = "0";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventsWithInvalidEventIdxRange) {
    QueryMemSnapshotEventHandler handler;
    std::unique_ptr<MemSnapshotEventsRequest> requestPtr = std::make_unique<MemSnapshotEventsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.startEventIdx = 10000;
    requestPtr->params.endEventIdx = 100;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventsWithFilters) {
    QueryMemSnapshotEventHandler handler;
    std::unique_ptr<MemSnapshotEventsRequest> requestPtr = std::make_unique<MemSnapshotEventsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.currentPage = 1;
    requestPtr->params.pageSize = 10;
    requestPtr->params.orderBy = "id";
    requestPtr->params.filters["action"] = "alloc";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

// ============== QueryMemSnapshotAllocationHandler Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryAllocationsWithValidParams) {
    QueryMemSnapshotAllocationHandler handler;
    std::unique_ptr<MemSnapshotAllocationsRequest> requestPtr = std::make_unique<MemSnapshotAllocationsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "BLOCK";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

// ============== QueryMemSnapshotLeakStatsHandler Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryLeakStatsWithValidParams) {
    QueryMemSnapshotLeakStatsHandler handler;
    std::unique_ptr<MemSnapshotLeakStatsRequest> requestPtr = std::make_unique<MemSnapshotLeakStatsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.deviceId = "0";
    requestPtr->params.startEventIdx = 100;
    requestPtr->params.endEventIdx = 1000;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryLeakStatsWithInvalidEventIdxRange) {
    QueryMemSnapshotLeakStatsHandler handler;
    std::unique_ptr<MemSnapshotLeakStatsRequest> requestPtr = std::make_unique<MemSnapshotLeakStatsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.deviceId = "0";
    requestPtr->params.startEventIdx = 5000;
    requestPtr->params.endEventIdx = 100;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

// ============== QueryMemSnapshotDetailHandler Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryBlockDetailWithValidParams) {
    QueryMemSnapshotDetailHandler handler;
    std::unique_ptr<MemSnapshotDetailRequest> requestPtr = std::make_unique<MemSnapshotDetailRequest>();
    requestPtr->params.deviceId = "0";
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.type = "block";
    requestPtr->params.id = 1;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventDetailWithValidParams) {
    QueryMemSnapshotDetailHandler handler;
    std::unique_ptr<MemSnapshotDetailRequest> requestPtr = std::make_unique<MemSnapshotDetailRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.type = "event";
    requestPtr->params.id = 1;
    requestPtr->params.deviceId = "0";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryDetailWithInvalidType) {
    QueryMemSnapshotDetailHandler handler;
    std::unique_ptr<MemSnapshotDetailRequest> requestPtr = std::make_unique<MemSnapshotDetailRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.type = "invalid_type";
    requestPtr->params.id = 1;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryBlockDetailWithNonExistentId) {
    QueryMemSnapshotDetailHandler handler;
    std::unique_ptr<MemSnapshotDetailRequest> requestPtr = std::make_unique<MemSnapshotDetailRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.type = "block";
    requestPtr->params.id = -99999;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventDetailWithNonExistentId) {
    QueryMemSnapshotDetailHandler handler;
    std::unique_ptr<MemSnapshotDetailRequest> requestPtr = std::make_unique<MemSnapshotDetailRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->params.type = "event";
    requestPtr->params.id = -99999;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

// ============== Combined Filter Tests ==============

TEST_F(MemSnapshotHandlerTest, QueryBlocksWithMultipleFilters) {
    QueryMemSnapshotBlockHandler handler;
    std::unique_ptr<MemSnapshotBlocksRequest> requestPtr = std::make_unique<MemSnapshotBlocksRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "BLOCK";
    requestPtr->params.currentPage = 1;
    requestPtr->params.pageSize = 10;
    requestPtr->params.orderBy = "id";
    requestPtr->params.startEventIdx = 100;
    requestPtr->params.endEventIdx = 5000;
    requestPtr->params.minSize = 1024;
    requestPtr->params.maxSize = 1048576;
    requestPtr->params.filters["state"] = "allocated";

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemSnapshotHandlerTest, QueryEventsWithEventIdxRange) {
    QueryMemSnapshotEventHandler handler;
    std::unique_ptr<MemSnapshotEventsRequest> requestPtr = std::make_unique<MemSnapshotEventsRequest>();
    requestPtr->moduleName = MODULE_MEM_SCOPE;
    requestPtr->projectName = testDbPath;
    requestPtr->isTable = true;
    requestPtr->params.deviceId = "0";
    requestPtr->params.currentPage = 1;
    requestPtr->params.pageSize = 10;
    requestPtr->params.orderBy = "id";
    requestPtr->params.startEventIdx = 100;
    requestPtr->params.endEventIdx = 1000;

    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}
