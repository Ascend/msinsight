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
#include "MemScopeProtocolRequest.h"
#include "DataBaseManager.h"
#include "MemScopeParser.h"
#include "MemScopeDatabase.h"
#include "MemScopeService.h"
#include "MemScopeTableColumn.h"
#include "../../TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic;

class MemScopeDatabaseTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "leaks_dump_20250806.dat");
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
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

TEST_F(MemScopeDatabaseTest, QueryEntireEventsTable) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEntireEventsTable(events);
    size_t expectSize = 33882;
    EXPECT_EQ(events.size(), expectSize);
}

TEST_F(MemScopeDatabaseTest, QueryDeviceIds) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    std::set<std::string> deviceIds;
    memoryDatabase->QueryDeviceIds(deviceIds);
    size_t expectSize = 1;
    EXPECT_EQ(deviceIds.size(), expectSize);
}
TEST_F(MemScopeDatabaseTest, QueryDeviceEventTypeMap) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    std::unordered_map<std::string, std::vector<std::string>> resultMap;
    memoryDatabase->QueryMallocOrFreeEventTypeWithDeviceId(resultMap);
    std::vector<std::string> eventTypes = resultMap["1"];
    auto it = std::find(eventTypes.begin(), eventTypes.end(), "HAL");
    EXPECT_TRUE(it != eventTypes.end());
    it = std::find(eventTypes.begin(), eventTypes.end(), "PTA");
    EXPECT_TRUE(it != eventTypes.end());
}
TEST_F(MemScopeDatabaseTest, QueryMinAndMaxTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    uint64_t minTimestamp = memoryDatabase->GetGlobalMinTimestamp();
    uint64_t maxTimestamp = memoryDatabase->GetGlobalMaxTimestamp();
    const uint64_t expectMin = 1754448083902750070;
    const uint64_t expectMax = 1754448132413582150;
    EXPECT_EQ(minTimestamp, expectMin);
    EXPECT_EQ(maxTimestamp, expectMax);
}

TEST_F(MemScopeDatabaseTest, QueryMemoryBlockWithNoTimeAndSizeCondition) {
    std::vector<MemScopeEvent> events;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    MemScopeMemoryBlockParams params;
    params.deviceId = "1";
    params.relativeTime = false;
    params.eventType = "PTA";
    std::vector<MemoryBlock> blocks;
    memoryDatabase->QueryMemoryBlocks(params, false, blocks);
    const size_t expectSize = 3267;
    EXPECT_EQ(blocks.size(), expectSize);
}

TEST_F(MemScopeDatabaseTest, QueryMemoryBlockWithTimeAndSizeConditionAndRelativeTime) {
    MemScopeMemoryBlockParams params;
    params.deviceId = "1";
    params.relativeTime = true;
    // 15s
    const uint64_t endTimestamp = 20000000000;
    const uint64_t maxSize = 60000;
    params.endTimestamp = endTimestamp;
    params.maxSize = maxSize;
    params.eventType = "PTA";
    params.orderBy = std::string(MemoryBlockTableColumn::START_TIMESTAMP);
    std::vector<MemoryBlock> blocks;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    memoryDatabase->QueryMemoryBlocks(params, false, blocks);
    const size_t expectSize = 269;
    EXPECT_EQ(blocks.size(), expectSize);
    MemoryBlock &firstBlock = blocks[0];
    const uint64_t expectStartTime = 13350166480;
    const uint64_t expectEndTime = 45422237070;
    EXPECT_EQ(firstBlock.startTimestamp, expectStartTime);
    EXPECT_EQ(firstBlock.endTimestamp, expectEndTime);
    EXPECT_TRUE(firstBlock.size > 0 and firstBlock.size < params.maxSize);
}

TEST_F(MemScopeDatabaseTest, QueryAllocationWithNoTimeCondition) {
    MemScopeMemoryAllocationParams params;
    params.deviceId = "1";
    params.optimized = false;
    params.eventType = "PTA";
    std::vector<MemoryAllocation> allocations;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    size_t expectSize = 6534;
    EXPECT_EQ(allocations.size(), expectSize);
}

TEST_F(MemScopeDatabaseTest, QueryAllocationWithTimeAndRelativeCondition) {
    MemScopeMemoryAllocationParams params;
    params.deviceId = "1";
    params.optimized = false;
    // 15s
    const uint64_t endTimestamp = 20000000000;
    params.endTimestamp = endTimestamp;
    params.relativeTime = true;
    params.eventType = "PTA";
    std::vector<MemoryAllocation> allocations;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    const size_t expectSize = 324;
    EXPECT_EQ(allocations.size(), expectSize);
    MemoryAllocation &firstAllocation = allocations[0];
    const uint64_t expectTimestamp = 13350166480;
    EXPECT_EQ(firstAllocation.timestamp, expectTimestamp);
    uint64_t totalSize = 37888;
    EXPECT_EQ(firstAllocation.totalSize, totalSize);
}

TEST_F(MemScopeDatabaseTest, QueryLatestAllocationWithinTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    const std::string deviceId = "1";
    const std::string eventType = "HAL";
    const uint64_t expectDuration = 20000000000;
    const uint64_t timestamp = memoryDatabase->GetGlobalMinTimestamp() + expectDuration;
    auto alloc = memoryDatabase->QueryLatestAllocationWithinTimestamp(deviceId, eventType, timestamp);
    EXPECT_TRUE(alloc.has_value());
    const uint64_t expectTotalSize = 187076614;
    EXPECT_EQ(alloc->totalSize, expectTotalSize);
}

TEST_F(MemScopeDatabaseTest, QueryNextAllocationAfterTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    const std::string deviceId = "1";
    const std::string eventType = "PTA";
    const uint64_t expectDuration = 10000000;
    const uint64_t timestamp = memoryDatabase->GetGlobalMinTimestamp() + expectDuration;
    auto alloc = memoryDatabase->QueryNextAllocationAfterTimestamp(deviceId, eventType, timestamp);
    EXPECT_TRUE(alloc.has_value());
    const uint64_t expectTotalSize = 37888;
    EXPECT_EQ(alloc->totalSize, expectTotalSize);
}

TEST_F(MemScopeDatabaseTest, QueryMemoryBlocksOwnersReleasedAfterTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    const std::string deviceId = "1";
    const std::string eventType = "PTA";
    // 10s
    const uint64_t expectDuration = 20000000000;
    const uint64_t timestamp = memoryDatabase->GetGlobalMinTimestamp() + expectDuration;
    std::set<std::string> owners;
    memoryDatabase->QueryMemoryBlocksOwnersReleasedAfterTimestamp(deviceId, eventType, timestamp, owners);
    EXPECT_FALSE(owners.empty());
    std::set<std::string> expectOwners = {"PTA@model@weight", "PTA@ops@aten", "PTA@ops@aten@leaks_mem"};
    EXPECT_EQ(owners, expectOwners);
}

TEST_F(MemScopeDatabaseTest, QueryTotalSizeUtilTimestampUsingOwner) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    const std::string deviceId = "1";
    // 10s
    const uint64_t expectDuration = 20000000000;
    const std::string eventType = "HAL";
    const uint64_t timestamp = memoryDatabase->GetGlobalMinTimestamp() + expectDuration;
    auto allocation = memoryDatabase->QueryLatestAllocationWithinTimestamp(deviceId, eventType, timestamp);
    uint64_t cannTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, "CANN");
    uint64_t ptaTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, "PTA");
    uint64_t ptaModelTotalSize =
        memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, "PTA@model");
    uint64_t ptaOpTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, "PTA@ops");
    const uint64_t expectCANNTotalSize = 187076614;
    const uint64_t expectPTATotalSize = 142264320;
    const uint64_t expectPTAModelTotalSize = 95234560;
    const uint64_t expectPTAOpTotalSize = 47029760;
    EXPECT_TRUE(allocation.has_value());
    EXPECT_EQ(allocation->totalSize, cannTotalSize);
    EXPECT_EQ(cannTotalSize, expectCANNTotalSize);
    EXPECT_EQ(ptaTotalSize, expectPTATotalSize);
    EXPECT_EQ(ptaModelTotalSize, expectPTAModelTotalSize);
    EXPECT_EQ(ptaOpTotalSize, expectPTAOpTotalSize);
}

TEST_F(MemScopeDatabaseTest, QueryPythonTraces) {
    const uint64_t startTimestamp = 1000000;
    const uint64_t endTimestamp = 20000000000;
    MemScopeThreadPythonTraceParams params;
    params.startTimestamp = startTimestamp;
    params.endTimestamp = endTimestamp;
    params.relativeTime = true;
    params.deviceId = "1";
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::vector<uint64_t> threadIds;
    const int expectThreadIdSize = 2;
    memoryDatabase->QueryThreadIds(threadIds);
    EXPECT_EQ(threadIds.size(), expectThreadIdSize);
    params.threadId = threadIds[0];
    MemScopePythonTrace trace;
    memoryDatabase->QueryPythonTrace(params, trace);
    EXPECT_FALSE(trace.slices.empty());
}
/***
 * 测试通过简易请求(仅deviceId)查询内存事件  应返回该deviceId下的所有事件
 */
TEST_F(MemScopeDatabaseTest, QueryMemoryEventsTableWithSimpleParams) {
    MemScopeEventParams simpleQueryParams;
    simpleQueryParams.deviceId = "1";
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::vector<MemScopeEvent> events;
    int64_t totalSize = memoryDatabase->QueryEventsByRequestParams(simpleQueryParams, events);
    int64_t expectTotalSize = 33882;
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), totalSize);
}

/***
* 测试通过时间范围、字段、分页、排序参数请求内存事件
*/
TEST_F(MemScopeDatabaseTest, QueryMemoryEventsTableWithComplexParams) {
    MemScopeEventParams complexParams;
    // 测试过滤后总量大于pageSize的情况，且根据Timestamp升序排序
    complexParams.deviceId = "1";
    complexParams.endTimestamp = 20000000000;
    complexParams.relativeTime = true;
    complexParams.currentPage = 1;
    complexParams.pageSize = 10;
    complexParams.orderBy = std::string(EventTableColumn::TIMESTAMP);
    int64_t expectTotalSize = 2927;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::vector<MemScopeEvent> events;
    int64_t totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), complexParams.pageSize);
    EXPECT_EQ(events[0].timestamp, 4454383900);
    // 测试过滤后总量小于pageSize的情况，且根据Ptr降序排序
    complexParams.pageSize = 3000;
    complexParams.orderBy = std::string(EventTableColumn::PTR);
    complexParams.desc = true;
    expectTotalSize = 2927;
    uint64_t expectFirstEventTimestamp = 12318467490;
    events.clear();
    totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), expectTotalSize);
    EXPECT_EQ(events[0].timestamp, expectFirstEventTimestamp);
    // 测试过滤后总量大于pageSize的情况，包括分页参数、filter、orderBy
    complexParams.pageSize = 10;
    complexParams.filters.emplace(std::string(EventTableColumn::ATTR), "PTA");
    expectTotalSize = 649;
    events.clear();
    totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), complexParams.pageSize);
    expectFirstEventTimestamp = 14011016610;
    EXPECT_EQ(events[0].timestamp, expectFirstEventTimestamp);
}

// Memory Block 数据表相关查询

/***
* 测试查询某个deviceId+eventType下的全部数据
*/
TEST_F(MemScopeDatabaseTest, QueryMemoryBlockTablesWithTimeRangeParams) {
    MemScopeMemoryBlockParams simpleQueryParams;
    simpleQueryParams.deviceId = "1";
    simpleQueryParams.eventType = "PTA";
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::vector<MemoryBlock> blocks;
    int64_t totalSize = memoryDatabase->QueryMemoryBlocks(simpleQueryParams, true, blocks);
    int64_t expectTotalSize = 3267;
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(blocks.size(), totalSize);
    blocks.clear();
    simpleQueryParams.deviceId = "1";
    simpleQueryParams.eventType = "HAL";
    expectTotalSize = 96;
    totalSize = memoryDatabase->QueryMemoryBlocks(simpleQueryParams, true, blocks);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(blocks.size(), totalSize);
}

/***
* 测试通过时间范围、字段、分页、排序参数请求内存事件
*/
TEST_F(MemScopeDatabaseTest, QueryMemoryEventsTableWithPaginationParams) {
    MemScopeEventParams complexParams;
    // 测试过滤后总量大于pageSize的情况，且根据startTimestamp升序排序
    complexParams.deviceId = "1";
    complexParams.endTimestamp = 100000000000;
    complexParams.relativeTime = true;
    complexParams.currentPage = 1;
    complexParams.pageSize = 20;
    complexParams.orderBy = std::string(EventTableColumn::TIMESTAMP);
    int64_t expectTotalSize = 33882;
    uint64_t expectFirstEventStartTimestamp = 4454383900;
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    EXPECT_TRUE(memoryDatabase != nullptr);
    std::vector<MemScopeEvent> events;
    int64_t totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), complexParams.pageSize);
    EXPECT_EQ(events[0].timestamp, expectFirstEventStartTimestamp);
    // 测试过滤后总量小于pageSize的情况，且根据Ptr降序排序
    complexParams.endTimestamp = expectFirstEventStartTimestamp + 1;
    complexParams.orderBy = std::string(EventTableColumn::PTR);
    complexParams.desc = true;
    expectTotalSize = 1;
    expectFirstEventStartTimestamp = 4454383900;
    events.clear();
    totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), expectTotalSize);
    EXPECT_EQ(events[0].timestamp, expectFirstEventStartTimestamp);
    // 测试过滤后总量大于pageSize的情况，包括分页参数、filter、orderBy
    complexParams.endTimestamp = 100000000000;
    complexParams.filters.emplace(std::string(EventTableColumn::PTR), "24");
    expectTotalSize = 1167;
    expectFirstEventStartTimestamp = 4473763530;
    events.clear();
    totalSize = memoryDatabase->QueryEventsByRequestParams(complexParams, events);
    EXPECT_EQ(totalSize, expectTotalSize);
    EXPECT_EQ(events.size(), complexParams.pageSize);
    EXPECT_EQ(events[0].timestamp, expectFirstEventStartTimestamp);
}

TEST_F(MemScopeDatabaseTest, QueryAllDeviceExtreumTimestampMap) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    EXPECT_TRUE(memoryDatabase != nullptr);
    const uint64_t expectMin = 1754448088357133970;
    const uint64_t expectMax = 1754448132413582150;
    const std::string deviceId = "1";
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> extreTsMap;
    memoryDatabase->QueryAllDeviceExtremumTimestamp(extreTsMap);
    EXPECT_EQ(extreTsMap.size(), 1);
    EXPECT_EQ(extreTsMap["1"].first, expectMin);
    EXPECT_EQ(extreTsMap["1"].second, expectMax);
}

TEST_F(MemScopeDatabaseTest, QueryEventsByGroupId) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    EXPECT_TRUE(memoryDatabase != nullptr);
    const uint64_t groupId = 1910;
    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEventsByGroupId(groupId, "1", false, events);
    const uint64_t expectTotalSize = 5;
    EXPECT_EQ(events.size(), expectTotalSize);
    MemScopeEvent &firstEvent = events[0];
    MemScopeEvent &lastEvent = events[expectTotalSize - 1];
    EXPECT_EQ(firstEvent.event, MEM_SCOPE_DUMP_EVENT::MALLOC);
    EXPECT_EQ(lastEvent.event, MEM_SCOPE_DUMP_EVENT::FREE);
}

/***
 * 测试低效显存识别
 */
TEST_F(MemScopeDatabaseTest, QueryBlocksTableWithInefficientThreshold) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    MemScopeMemoryBlockParams queryParams;
    queryParams.deviceId = "1";
    queryParams.relativeTime = true;
    queryParams.eventType = "PTA";
    queryParams.onlyInefficient = true; // 仅过滤
    queryParams.currentPage = 1;
    queryParams.pageSize = 10;

    // 提前申请场景
    queryParams.lazyUsedThreshold.perT = 10;
    std::vector<MemoryBlock> blocks;
    memoryDatabase->QueryMemoryBlocks(queryParams, true, blocks);
    EXPECT_FALSE(blocks.empty());
    for (auto &block : blocks) {
        EXPECT_TRUE(block.lazyUsed);
        uint64_t firstAccessInterval = block.firstAccessTimestamp - block.startTimestamp;
        uint64_t duration = block.endTimestamp - block.startTimestamp;
        EXPECT_GT(firstAccessInterval, duration * 0.1);
    }
    queryParams.lazyUsedThreshold.perT = 0;
    blocks.clear();
    // 延迟释放场景
    queryParams.delayedFreeThreshold.perT = 10;
    queryParams.delayedFreeThreshold.valueT = 100000000;
    memoryDatabase->QueryMemoryBlocks(queryParams, true, blocks);
    EXPECT_FALSE(blocks.empty());
    for (auto &block : blocks) {
        EXPECT_TRUE(block.delayedFree);
        uint64_t freeInterval = block.endTimestamp - block.lastAccessTimestamp;
        uint64_t duration = block.endTimestamp - block.startTimestamp;
        EXPECT_TRUE(freeInterval > 100000000 || freeInterval > duration * 0.1);
    }
    queryParams.delayedFreeThreshold.perT = 0;
    queryParams.delayedFreeThreshold.valueT = 0;
    blocks.clear();
    // 超长空闲场景
    queryParams.longIdleThreshold.perT = 10;
    memoryDatabase->QueryMemoryBlocks(queryParams, true, blocks);
    EXPECT_FALSE(blocks.empty());
    for (auto &block : blocks) {
        EXPECT_TRUE(block.longIdle);
        uint64_t maxInterval = block.maxAccessInterval;
        uint64_t duration = block.endTimestamp - block.startTimestamp;
        EXPECT_TRUE(maxInterval > duration * 0.1);
    }
}

/***
 * 该测试专用于针对以下场景：
 *  1. msleaks新增了某固化标签(如在进程占用下新增了Workspace内存池，其标签为PTA_WORKSPACE)
 *  2. 新增的标签 以原某固化标签为前缀，却并原固化标签分类的子分类(如新增的PTA_WORKSPACE, 与原固化标签PTA存在前缀关系，却并非PTA子类)
 *  3. 测试统计标签分类总Size时是否可能误将上述新增固化标签误统计入内
 */
TEST_F(MemScopeDatabaseTest, QueryTotalSizeUtilTimestampUsingOwnerWhenWorkspaceExists) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    const std::string deviceId = "1";
    // 未插入新标签内存块前的总量统计
    const uint64_t expectDuration = 20000000000;
    const uint64_t timestamp = memoryDatabase->GetGlobalMinTimestamp() + expectDuration;
    uint64_t originPtaTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, "PTA");
    uint64_t originPtaWorkspaceTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(
        deviceId, timestamp, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE);
    EXPECT_EQ(originPtaWorkspaceTotalSize, 0);
    EXPECT_GT(originPtaTotalSize, 0);
    std::string testFlag = "DT-TEST";
    // 插入一条PTA_WORKSPACE数据, 内存块在之后释放
    MemoryBlock mockPTAWorkspaceBlock(testFlag, deviceId, 1, 0, timestamp + 1, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE,
        MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE, "", 0, 0);
    memoryDatabase->InsertMemoryBlock(mockPTAWorkspaceBlock);
    memoryDatabase->FlushMemoryBlocksCache();
    uint64_t newPtaTotalSize =
        memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(deviceId, timestamp, MEM_SCOPE_ALLOC_OWNER_PTA);
    uint64_t newPtaWorkspaceTotalSize = memoryDatabase->QueryTotalSizeUntilTimestampUsingOwner(
        deviceId, timestamp, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE);
    EXPECT_EQ(newPtaTotalSize, originPtaTotalSize);
    EXPECT_EQ(newPtaWorkspaceTotalSize, 1);
    std::string clearSql = StringUtil::FormatString("DELETE FROM {} WHERE {} = '{}' AND {} = '{}'", TABLE_LEAKS_DUMP,
        EventTableColumn::PTR, testFlag, EventTableColumn::EVENT_TYPE, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE);
    EXPECT_TRUE(memoryDatabase->ExecSql(clearSql));
}

/***
* 该DT用于测试msleaks采集时开启CallStack的场景
*/
TEST_F(MemScopeDatabaseTest, TestCompatibilityOfCallStack) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");

    // 插入Callstack列模拟开启了CallStack
    std::string addCallStackSql = StringUtil::FormatString(
        "ALTER TABLE {} ADD {} TEXT DEFAULT 'N/A'", TABLE_LEAKS_DUMP, EventTableColumn::CALL_STACK_C);
    EXPECT_TRUE(memoryDatabase->ExecSql(addCallStackSql));

    // 重开一个db(由于db在open中会检查是否有callstack列，所以必须关闭重开)
    std::string path = memoryDatabase->GetDbPath();
    memoryDatabase->CloseDb();
    EXPECT_TRUE(memoryDatabase->OpenDb(path, false));
    EXPECT_TRUE(memoryDatabase->withCallStackC);
    EXPECT_FALSE(memoryDatabase->withCallStackPython);

    MemScopeEventParams queryParams;
    queryParams.deviceId = "1";
    queryParams.currentPage = 1;
    queryParams.pageSize = 10;
    std::vector<MemScopeEvent> events;
    int64_t total = memoryDatabase->QueryEventsByRequestParams(queryParams, events);
    EXPECT_TRUE(total > 0 && !events.empty());
    EXPECT_EQ(events.front().callStackC, "N/A");

    // 清理插入的CallStack列
    std::string clearCallStackSql =
        StringUtil::FormatString("ALTER TABLE {} DROP COLUMN {}", TABLE_LEAKS_DUMP, EventTableColumn::CALL_STACK_C);
    EXPECT_TRUE(memoryDatabase->ExecSql(clearCallStackSql));
}
