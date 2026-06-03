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
#include <WsSessionManager.h>
#include "WsSessionImpl.h"
#include "MemScopeProtocolRequest.h"
#include "QueryMemScopeAllocationHandler.h"
#include "QueryMemScopeBlockHandler.h"
#include "QueryMemScopeMemoryDetailHandler.h"
#include "QueryMemScopePythonTraceHandler.h"
#include "QueryMemScopeEventHandler.h"
#include "DataBaseManager.h"
#include "MemScopeParser.h"
#include "TestSuit.h"
#include "TraceTime.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::MemScope;
using namespace Dic::Protocol;
using namespace Dic::Module::FullDb;
using namespace Dic;

class MemScopeRequestHandlerTest : public ::testing::Test {
  public:
    static const uint64_t SECOND = 1000000000;
    static const uint64_t INT64MAX = INT64_MAX;
    static void SetUpTestSuite() {
        std::string dbPath = TestSuit::GetTestDataFile("memscope", "host_pinned", "memscope_dump_20260527062213.db");
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        ASSERT_TRUE(memoryDatabase != nullptr);
        ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
        ASSERT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
        ASSERT_TRUE(MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces("0"));
    }
    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        memoryDatabase->CloseDb();
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsUseInvalidParamsDeviceId) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "@:1:;";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsUseInvalidParamsEventType) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationUseInvalidParamsTimestamp) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.startTimestamp = INT64_MAX;
    requestPtr->params.endTimestamp = 0;
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsUseNonExistsDeviceId) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "-1";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.eventType = "PTA";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsUseValidParamsWithoutTimeCondition) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "1";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.eventType = "PTA";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsUseValidParamsWithTimeAndRelativeCondition) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "1";
    requestPtr->params.relativeTime = true;
    requestPtr->params.eventType = "PTA";
    const uint64_t endTimestamp = 10000000000;
    requestPtr->params.endTimestamp = endTimestamp;
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryAllocationsAddsTailStepForHostPinned) {
    QueryMemScopeAllocationHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryAllocationRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryAllocationRequest>();
    requestPtr->params.deviceId = "cpu";
    requestPtr->params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    requestPtr->params.relativeTime = true;
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);

    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    params.relativeTime = true;

    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    handler.PaddingAllocations(allocations, params);

    ASSERT_FALSE(allocations.empty());
    EXPECT_EQ(allocations.back().timestamp,
        memoryDatabase->GetGlobalMaxTimestamp() - memoryDatabase->GetGlobalMinTimestamp());
}

TEST_F(MemScopeRequestHandlerTest, PaddingAllocationsKeepsFlatLineWhenRangeHasNoAllocationEvents) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    params.relativeTime = true;

    std::vector<MemoryAllocation> allAllocations;
    memoryDatabase->QueryMemoryAllocations(params, allAllocations);
    ASSERT_FALSE(allAllocations.empty());

    params.startTimestamp = 50;
    params.endTimestamp = 90;

    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    ASSERT_TRUE(allocations.empty());

    QueryMemScopeAllocationHandler handler;
    handler.PaddingAllocations(allocations, params);

    ASSERT_EQ(allocations.size(), 2);
    EXPECT_EQ(allocations[0].timestamp, params.startTimestamp);
    EXPECT_EQ(allocations[0].totalSize, 0);
    EXPECT_EQ(allocations[1].timestamp, params.endTimestamp);
    EXPECT_EQ(allocations[1].totalSize, 0);
}

TEST_F(MemScopeRequestHandlerTest, PaddingAllocationsDoesNotDuplicateExactBoundaryAllocations) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    params.relativeTime = true;

    std::vector<MemoryAllocation> allAllocations;
    memoryDatabase->QueryMemoryAllocations(params, allAllocations);
    ASSERT_GE(allAllocations.size(), 2);

    params.startTimestamp = allAllocations.front().timestamp;
    params.endTimestamp = allAllocations[1].timestamp;

    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    ASSERT_EQ(allocations.size(), 2);

    QueryMemScopeAllocationHandler handler;
    handler.PaddingAllocations(allocations, params);

    ASSERT_EQ(allocations.size(), 2);
    EXPECT_EQ(allocations[0].timestamp, params.startTimestamp);
    EXPECT_EQ(allocations[0].totalSize, allAllocations.front().totalSize);
    EXPECT_EQ(allocations[1].timestamp, params.endTimestamp);
    EXPECT_EQ(allocations[1].totalSize, allAllocations[1].totalSize);
}

TEST_F(MemScopeRequestHandlerTest, PaddingAllocationsUsesRelativeSessionBoundsWithoutTimeCondition) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    params.relativeTime = true;

    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    ASSERT_FALSE(allocations.empty());

    QueryMemScopeAllocationHandler handler;
    handler.PaddingAllocations(allocations, params);

    ASSERT_GE(allocations.size(), 2);
    EXPECT_EQ(allocations.front().timestamp, 0);
    EXPECT_EQ(allocations.front().totalSize, 0);
    EXPECT_EQ(allocations.back().timestamp,
        memoryDatabase->GetGlobalMaxTimestamp() - memoryDatabase->GetGlobalMinTimestamp());
    EXPECT_EQ(allocations.back().totalSize, allocations[allocations.size() - 2].totalSize);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseInvalidParamsDeviceId) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    requestPtr->params.deviceId = "@:1:;";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.eventType = "PTA";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseInvalidParamsTimestamp) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    requestPtr->params.endTimestamp = 0;
    requestPtr->params.startTimestamp = INT64_MAX;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseInvalidParamsSize) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    requestPtr->params.maxSize = 0;
    requestPtr->params.minSize = INT64_MAX;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseInvalidParamsExceedSize) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    requestPtr->params.maxSize = INT64_MAX;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseValidParamsWithoutTimeAndSizeCondition) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseValidParamsWithTimeCondition) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    const uint64_t endTimestamp = 10000000;
    requestPtr->params.endTimestamp = endTimestamp;
    requestPtr->params.relativeTime = true;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlocksUseValidParamsWithSizeCondition) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    const uint64_t maxSize = 100000;
    requestPtr->params.maxSize = maxSize;
    requestPtr->params.deviceId = "0";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryDetailUseInvalidParamsWithInjectDeviceId) {
    QueryMemScopeMemoryDetailHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryDetailRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryDetailRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "&";
    requestPtr->params.timestamp = 0;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryDetailUseInvalidParamsWithNonExistsDeviceId) {
    QueryMemScopeMemoryDetailHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryDetailRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryDetailRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "-1";
    const uint64_t durationSecond = 15;
    requestPtr->params.timestamp = durationSecond * SECOND;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryDetailUseInvalidParamsWithBigRelativeTimestamp) {
    QueryMemScopeMemoryDetailHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryDetailRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryDetailRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "1";
    requestPtr->params.timestamp = INT64MAX + 1;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryDetailUseValidParams) {
    QueryMemScopeMemoryDetailHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryDetailRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryDetailRequest>();
    const uint64_t durationSecond = 15;
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->params.timestamp = durationSecond * SECOND;
    requestPtr->params.relativeTime = true;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryDetailAcceptsHostPinnedEventType) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    params.relativeTime = true;
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    ASSERT_FALSE(allocations.empty());

    QueryMemScopeMemoryDetailHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryDetailRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryDetailRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "cpu";
    requestPtr->params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED;
    requestPtr->params.timestamp = allocations.front().timestamp;
    requestPtr->params.relativeTime = true;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryTraceUseInvalidParamsWithInjectDeviceId) {
    QueryMemScopePythonTraceHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopePythonTraceRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopePythonTraceRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "&";
    requestPtr->params.relativeTime = true;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryTraceUseInvalidParamsWithInvalidThreadId) {
    QueryMemScopePythonTraceHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopePythonTraceRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopePythonTraceRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "";
    requestPtr->params.threadId = INT64MAX + 1;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryTraceUseInvalidParamsWithInvalidTimestamp) {
    QueryMemScopePythonTraceHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopePythonTraceRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopePythonTraceRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "0";
    requestPtr->params.threadId = 1;
    requestPtr->params.startTimestamp = INT64MAX + 1;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryTraceUseValidParams) {
    QueryMemScopePythonTraceHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopePythonTraceRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopePythonTraceRequest>();
    requestPtr->moduleName = Protocol::MODULE_MEMORY;
    requestPtr->params.deviceId = "1";
    requestPtr->params.threadId = 3841316;
    requestPtr->params.relativeTime = true;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryBlockTable) {
    QueryMemScopeBlockHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeMemoryBlockRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeMemoryBlockRequest>();
    const uint64_t endTimestamp = 10000000;
    requestPtr->isTable = true;
    requestPtr->params.endTimestamp = endTimestamp;
    requestPtr->params.relativeTime = true;
    requestPtr->params.deviceId = "1";
    requestPtr->params.eventType = "PTA";
    requestPtr->moduleName = Protocol::MODULE_MEM_SCOPE;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemScopeRequestHandlerTest, QueryMemoryEventTable) {
    QueryMemScopeEventHandler handler;
    std::unique_ptr<Dic::Protocol::MemScopeEventRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemScopeEventRequest>();
    const uint64_t endTimestamp = 1000000000;
    requestPtr->params.endTimestamp = endTimestamp;
    requestPtr->params.relativeTime = true;
    requestPtr->params.deviceId = "1";
    requestPtr->moduleName = Protocol::MODULE_MEM_SCOPE;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}
