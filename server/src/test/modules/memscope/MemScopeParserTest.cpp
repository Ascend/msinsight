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
#include "MemScopeParser.h"
#include "../../TestSuit.h"

using namespace Dic::Module;

class MemScopeParserTest : public ::testing::Test {
  public:
    const static uint64_t SECOND = 1000000000;

    static void SetUpTestSuite() {
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "leaks_dump_20250806.dat");
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
    }

    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        memoryDatabase->CloseDb();
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(MemScopeParserTest, BuildBlockEventAttrFromEventWithEmptyAttr) {
    MemScopeEvent event;
    auto attrs = BuildEventAttrsFromJson<MemoryEventBaseAttrs>("");
    EXPECT_FALSE(attrs.has_value());
}

TEST_F(MemScopeParserTest, BuildBlockEventAttrFromEventWithInvalidJsonAttr) {
    MemScopeEvent event;
    auto attrs = BuildEventAttrsFromJson<MemoryEventBaseAttrs>("{]");
    EXPECT_FALSE(attrs.has_value());
}

TEST_F(MemScopeParserTest, BuildBlockEventAttrFromEventWithValidJsonAttr) {
    MemScopeEvent event;
    event.event = MEM_SCOPE_DUMP_EVENT::MALLOC;
    auto eventAttr = BuildEventAttrsFromJson<MallocFreeEventAttrs>(
        R"({"addr": "20617055174656", "size": "7849984", "total": "132120576","used": "116313600", "owner": "PTA@init_model"})");
    ASSERT_TRUE(eventAttr.has_value());
    const int64_t expectSize = 7849984;
    EXPECT_EQ(eventAttr->size, expectSize);
    EXPECT_EQ(eventAttr->owner, "PTA@init_model");
}

TEST_F(MemScopeParserTest, TestParseEventsToAllocationsAndBlocks) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEntireEventsTable(events);
    const size_t expectEventsSize = 33882;
    EXPECT_EQ(events.size(), expectEventsSize);
    EXPECT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
    EXPECT_TRUE(memoryDatabase->CreateMemoryAllocationAndBlockTable());
    const size_t expectBlockSize = 3267;
    const size_t expectAllocationSize = 6534;
    auto context = MemScopeParser::BuildParseContext(memoryDatabase);
    EXPECT_TRUE(context.has_value());
    MemScopeParser::ParseEventsToBlockAndAllocations(*context);
    // 校验blocks
    MemScopeMemoryBlockParams blockParams;
    blockParams.deviceId = "1";
    blockParams.relativeTime = false;
    blockParams.eventType = "PTA";
    std::vector<MemoryBlock> blocks;
    memoryDatabase->QueryMemoryBlocks(blockParams, false, blocks);
    // 校验allocations
    MemScopeMemoryAllocationParams allocationParams;
    allocationParams.deviceId = "1";
    allocationParams.optimized = false;
    allocationParams.eventType = "PTA";
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(allocationParams, allocations);
    EXPECT_EQ(blocks.size(), expectBlockSize);
    EXPECT_EQ(allocations.size(), expectAllocationSize);
}

TEST_F(MemScopeParserTest, TestParseLeaksDump) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    EXPECT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
    EXPECT_TRUE(memoryDatabase->CreateMemoryAllocationAndBlockTable());
    memoryDatabase->UpdateParseStatus(NOT_FINISH_STATUS);
    EXPECT_TRUE(MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces("0"));
    std::vector<string> traceTables = memoryDatabase->GetPythonTraceTables();
    EXPECT_EQ(traceTables.size(), 1);
    std::string traceTable = traceTables[0];
    std::vector<uint64_t> threadIds;
    memoryDatabase->QueryThreadIds(threadIds);
    EXPECT_FALSE(threadIds.empty());
    MemScopeThreadPythonTraceParams params;
    params.threadId = threadIds[0];
    MemScopePythonTrace trace;
    memoryDatabase->QueryPythonTracesUsingTableName(traceTable, params, trace);
    EXPECT_FALSE(trace.slices.empty());
    EXPECT_EQ(trace.slices.front().depth, 0);
}

/***
 * 测试解析出的内存块数据首末次访问事件是否正确
 */
TEST_F(MemScopeParserTest, TestMemoryBlockFirstLastAccessTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    const uint64_t groupId = 1910;
    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEventsByGroupId(groupId, "1", true, events);
    EventGroup eventGroup(events);
    EXPECT_FALSE(events.empty());
    auto firstEvent = events.front();
    // 根据首次事件的deviceId、eventType和timestamp查询出对应解析出的内存块
    std::vector<MemoryBlock> blocks;
    MemScopeMemoryBlockParams queryParams;
    queryParams.deviceId = firstEvent.deviceId;
    queryParams.eventType = firstEvent.eventType;
    queryParams.relativeTime = true;
    queryParams.startTimestamp = firstEvent.timestamp - 1;
    queryParams.endTimestamp = firstEvent.timestamp + 1;
    queryParams.onlyAllocOrFreeInTimeRange = true;
    // 仅查询在时间范围内申请或释放的，理应只有一个内存块
    memoryDatabase->QueryMemoryBlocks(queryParams, false, blocks);
    EXPECT_EQ(blocks.size(), 1);
    MemoryBlock block = blocks[0];
    EXPECT_EQ(block.firstAccessTimestamp, eventGroup.accessEvents.front().timestamp);
    EXPECT_EQ(block.lastAccessTimestamp, eventGroup.accessEvents.back().timestamp);
}
