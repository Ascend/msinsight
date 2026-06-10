/*
* -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <cstdio>
#include <gtest/gtest.h>
#include "DataBaseManager.h"
#include "MemSnapshotDatabase.h"
#include "MemSnapshotDefs.h"
#include "MemSnapshotTableColumn.h"
#include "MemSnapshotParser.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "sqlite3.h"
#include "../TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::MemSnapshot;
using namespace Dic;

class MemSnapshotDatabaseTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        // 准备测试数据
        testDbPath = TestSuit::GetTestDataFile("snapshot", "snapshot_with_multi_devices.pkl.db");

        // 获取数据库实例
        snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(testDbPath);
        ASSERT_TRUE(snapshotDb != nullptr);

        // 打开数据库
        ASSERT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));
    }

    static void TearDownTestSuite() {
        if (snapshotDb != nullptr) {
            snapshotDb->CloseDb();
        }
        DataBaseManager::Instance().Clear(DatabaseType::MEM_SNAPSHOT);
    }

  protected:
    static std::string testDbPath;
    static std::shared_ptr<MemSnapshotDatabase> snapshotDb;
};

std::string MemSnapshotDatabaseTest::testDbPath;
std::shared_ptr<MemSnapshotDatabase> MemSnapshotDatabaseTest::snapshotDb = nullptr;

// 测试数据库打开和关闭
TEST_F(MemSnapshotDatabaseTest, OpenAndCloseDb) {
    EXPECT_TRUE(snapshotDb->IsOpen());

    // 测试重复打开
    EXPECT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));

    // 关闭数据库
    snapshotDb->CloseDb();
    EXPECT_FALSE(snapshotDb->IsOpen());

    // 重新打开
    EXPECT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));
    EXPECT_TRUE(snapshotDb->IsOpen());
}

// 测试表存在性检查
TEST_F(MemSnapshotDatabaseTest, CheckAllTableExist) { EXPECT_TRUE(snapshotDb->CheckAllTableExist()); }

// 测试devices初始化信息
TEST_F(MemSnapshotDatabaseTest, CheckInitDevices) {
    const auto deviceIds = snapshotDb->GetDeviceIds();
    EXPECT_EQ(deviceIds.size(), 2);
    if (deviceIds.size() == 2) {
        EXPECT_EQ(deviceIds[0], "0");
        EXPECT_EQ(deviceIds[1], "1");
    }
    EXPECT_EQ(snapshotDb->GetDeviceMaxEntryId("0"), 8131);
    EXPECT_EQ(snapshotDb->GetDeviceMaxEntryId("1"), 9699);
}

// 测试查询所有内存块
TEST_F(MemSnapshotDatabaseTest, QueryAllBlocks) {
    std::vector<Block> blocks;
    bool result = snapshotDb->QueryAllBlocks(blocks, "0");
    EXPECT_TRUE(result);
    EXPECT_EQ(blocks.size(), 3219);
    result = snapshotDb->QueryAllBlocks(blocks, "1");
    EXPECT_TRUE(result);
    EXPECT_EQ(blocks.size(), 6435);
}

// 测试根据ID查询内存块
TEST_F(MemSnapshotDatabaseTest, QueryBlockById) {
    // 先查询所有块获取一个有效的ID
    const auto expectBlockId = 1;
    auto block = snapshotDb->QueryBlockById(expectBlockId, "0");

    EXPECT_TRUE(block.has_value());
    EXPECT_EQ(block->id, expectBlockId);
    EXPECT_EQ(block->size, 41943552);
    EXPECT_EQ(block->state, BLOCK_STATE_ACTIVE_ALLOC);

    block = snapshotDb->QueryBlockById(expectBlockId, "1");

    EXPECT_TRUE(block.has_value());
    EXPECT_EQ(block->id, expectBlockId);
    EXPECT_EQ(block->size, 37888);
    EXPECT_EQ(block->state, BLOCK_STATE_ACTIVE_ALLOC);

    // 测试查询不存在的ID
    auto nonExistentBlock = snapshotDb->QueryBlockById(-1000, "0");
    EXPECT_FALSE(nonExistentBlock.has_value());
    nonExistentBlock = snapshotDb->QueryBlockById(-1000, "1");
    EXPECT_FALSE(nonExistentBlock.has_value());
}

// 测试字典映射功能
TEST_F(MemSnapshotDatabaseTest, GetRealValueInTableDictionaryMap) {
    // 测试存在的映射
    std::string realValue = snapshotDb->GetRealValueInTableDictionaryMap("block", "state", 1);

    // 测试不存在的表
    std::string nonExistentTableValue = snapshotDb->GetRealValueInTableDictionaryMap("non_existent_table", "state", 1);
    EXPECT_EQ(nonExistentTableValue, "1");

    // 测试不存在的列
    std::string nonExistentColumnValue =
        snapshotDb->GetRealValueInTableDictionaryMap("block", "non_existent_column", 1);
    EXPECT_EQ(nonExistentColumnValue, "1");

    // 测试不存在的键
    std::string nonExistentKeyValue = snapshotDb->GetRealValueInTableDictionaryMap("block", "state", 9999);
    EXPECT_EQ(nonExistentKeyValue, "9999");
}

// 测试数据库重置功能
TEST_F(MemSnapshotDatabaseTest, Reset) {
    // 调用重置方法
    MemSnapshotDatabase::Reset();

    // 验证数据库已关闭
    EXPECT_FALSE(snapshotDb->IsOpen());

    // 重新获取并打开数据库
    snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(testDbPath);
    ASSERT_TRUE(snapshotDb != nullptr);
    EXPECT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));
}

// 测试查询内存记录
TEST_F(MemSnapshotDatabaseTest, QueryMemoryRecords) {
    MemSnapshotAllocationParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";

    std::vector<MemoryRecord> records;
    snapshotDb->QueryMemoryRecords(params, records);

    EXPECT_EQ(records.size(), 8132);

    // 验证第一条记录的字段
    if (!records.empty()) {
        EXPECT_EQ(records[0].id, 0);
        EXPECT_EQ(records[0].allocated, 94482944);
        EXPECT_EQ(records[0].reserved, 155189248);
        EXPECT_EQ(records[0].active, 94482944);
    }

    // 任意事件发生时刻，均满足allocated <= active <= reserved, 此处间隔1000个事件做一次验证
    size_t checkIdx = 1000;
    while (checkIdx < records.size()) {
        EXPECT_LE(records[checkIdx].allocated, records[checkIdx].active);
        EXPECT_LE(records[checkIdx].active, records[checkIdx].reserved);
        checkIdx += 1000;
    }
}

// 测试查询blocks表
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTable) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "id";

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_EQ(totalCount, 3219);
    EXPECT_EQ(blocks.size(), 10);

    if (!blocks.empty()) {
        // 验证第一条block的字段
        EXPECT_EQ(blocks[0].id, -320);
        EXPECT_EQ(blocks[0].address, 20697531023360);
        EXPECT_EQ(blocks[0].size, 4096.5);
        EXPECT_EQ(blocks[0].requestedSize, 4096);

        // 验证最后一个block的字段
        EXPECT_EQ(blocks[blocks.size() - 1].id, -311);
        EXPECT_EQ(blocks[blocks.size() - 1].address, 20697475301376);
        EXPECT_EQ(blocks[blocks.size() - 1].size, 2048.5);
        EXPECT_EQ(blocks[blocks.size() - 1].requestedSize, 2048);
    }
}

// 测试查询blocks表带事件索引范围
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithEventIdxRange) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.startEventIdx = 100;
    params.endEventIdx = 1000;
    params.orderBy = "allocEventId";

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_EQ(totalCount, 764);
    EXPECT_EQ(blocks.size(), 10);

    for (const auto &block : blocks) {
        EXPECT_TRUE((block.allocEventId < 0 || static_cast<uint64_t>(block.allocEventId) >= params.endEventIdx) &&
            (block.freeEventId < 0 || static_cast<uint64_t>(block.freeEventId) >= params.startEventIdx));
    }
}

// 测试查询blocks表带过滤条件
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithFilters) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "id";
    params.filters["state"] = "allocated";

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);
    EXPECT_EQ(totalCount, 3219);
    EXPECT_EQ(blocks.size(), params.pageSize);

    for (const auto &block : blocks) {
        EXPECT_EQ(block.state, BLOCK_STATE_ACTIVE_ALLOC);
    }
}

// 测试查询blocks表带降序排序
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithDescOrder) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = BlockTableColumn::ADDRESS;
    params.desc = true;

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_EQ(totalCount, 3219);
    EXPECT_EQ(blocks.size(), params.pageSize);

    // 验证降序排序
    for (size_t i = 1; i < blocks.size(); ++i) {
        EXPECT_GE(blocks[i - 1].address, blocks[i].address);
    }
}

// 测试查询潜在泄漏blocks表
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableOnlyUnreleasedInRange) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.startEventIdx = 100;
    params.endEventIdx = 1000;
    params.orderBy = "allocEventId";
    params.onlyUnreleasedInRange = true;

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_EQ(totalCount, 146);
    ASSERT_EQ(blocks.size(), params.pageSize);
    for (const auto &block : blocks) {
        EXPECT_GE(block.allocEventId, params.startEventIdx);
        EXPECT_LE(block.allocEventId, params.endEventIdx);
        EXPECT_TRUE(block.freeEventId < 0 || static_cast<uint64_t>(block.freeEventId) > params.endEventIdx);
    }
}

// 测试查询潜在泄漏blocks表与普通过滤条件叠加
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableOnlyUnreleasedInRangeWithFilters) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.startEventIdx = 100;
    params.endEventIdx = 1000;
    params.orderBy = "allocEventId";
    params.onlyUnreleasedInRange = true;
    params.filters["state"] = BLOCK_STATE_ACTIVE_ALLOC;
    params.rangeFilters["allocEventId"] = {100, 500};

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_EQ(totalCount, 78);
    ASSERT_EQ(blocks.size(), params.pageSize);
    for (const auto &block : blocks) {
        EXPECT_EQ(block.state, BLOCK_STATE_ACTIVE_ALLOC);
        EXPECT_GE(block.allocEventId, 100);
        EXPECT_LE(block.allocEventId, 500);
        EXPECT_TRUE(block.freeEventId < 0 || static_cast<uint64_t>(block.freeEventId) > params.endEventIdx);
    }
}

// 测试查询潜在泄漏聚合统计与表格展示值一致
TEST_F(MemSnapshotDatabaseTest, QueryPotentialLeakStats) {
    MemSnapshotLeakStatsParams params;
    params.deviceId = "0";
    params.startEventIdx = 100;
    params.endEventIdx = 1000;

    MemSnapshotLeakStatsDTO stats;
    EXPECT_TRUE(snapshotDb->QueryPotentialLeakStats(params, stats));
    EXPECT_EQ(stats.totalSize, 108473.0);
    EXPECT_EQ(stats.maxSize, 9216.5);
    EXPECT_EQ(stats.minSize, 0.5);
}

TEST_F(MemSnapshotDatabaseTest, QueryPotentialLeakStatsMatchesDisplayedBlockTableSizeSum) {
    const std::string precisionDbPath = testDbPath + ".precision_test.db";
    sqlite3 *precisionDb = nullptr;
    ASSERT_EQ(sqlite3_open(precisionDbPath.c_str(), &precisionDb), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(precisionDb,
                  "CREATE TABLE dictionary (`table` TEXT, `column` TEXT, `key` TEXT, `value` TEXT);"
                  "CREATE TABLE block_0 (`id` INTEGER PRIMARY KEY, `address` INTEGER, `size` INTEGER, "
                  "`requestedSize` INTEGER, `state` INTEGER DEFAULT 99, `allocEventId` INTEGER, `freeEventId` INTEGER);"
                  "CREATE TABLE trace_entry_0 (`id` INTEGER PRIMARY KEY, `action` INTEGER, `address` INTEGER, `size` "
                  "INTEGER, "
                  "`stream` INTEGER, `allocated` INTEGER, `active` INTEGER, `reserved` INTEGER, `callstack` TEXT);"
                  "INSERT INTO dictionary VALUES ('block_0', 'state', '1', 'active_allocated');",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    sqlite3_stmt *insertStmt = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(precisionDb, "INSERT INTO block_0 VALUES (?, ?, 1, 1, 1, ?, -1);", -1, &insertStmt, nullptr),
        SQLITE_OK);
    for (int id = 1; id <= 512; ++id) {
        sqlite3_bind_int(insertStmt, 1, id);
        sqlite3_bind_int(insertStmt, 2, id);
        sqlite3_bind_int(insertStmt, 3, id);
        ASSERT_EQ(sqlite3_step(insertStmt), SQLITE_DONE);
        sqlite3_reset(insertStmt);
        sqlite3_clear_bindings(insertStmt);
    }
    sqlite3_finalize(insertStmt);
    sqlite3_close(precisionDb);

    auto precisionSnapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(precisionDbPath);
    ASSERT_TRUE(precisionSnapshotDb != nullptr);
    ASSERT_TRUE(precisionSnapshotDb->OpenDbReadOnly(precisionDbPath));

    MemSnapshotLeakStatsParams statsParams;
    statsParams.deviceId = "0";
    statsParams.startEventIdx = 1;
    statsParams.endEventIdx = 512;

    MemSnapshotBlockParams blockParams;
    blockParams.deviceId = "0";
    blockParams.startEventIdx = statsParams.startEventIdx;
    blockParams.endEventIdx = statsParams.endEventIdx;
    blockParams.onlyUnreleasedInRange = true;
    blockParams.currentPage = 1;
    blockParams.pageSize = 100000;

    MemSnapshotLeakStatsDTO stats;
    std::vector<BlockTableItemDTO> blocks;
    EXPECT_TRUE(precisionSnapshotDb->QueryPotentialLeakStats(statsParams, stats));
    EXPECT_EQ(precisionSnapshotDb->QueryBlocksTable(blockParams, blocks), 512);

    double displayedTotalSize = 0;
    for (const auto &block : blocks) {
        displayedTotalSize += block.size;
    }
    EXPECT_NEAR(stats.totalSize, displayedTotalSize, 1e-9);

    precisionSnapshotDb->CloseDb();
    DataBaseManager::Instance().Clear(DatabaseType::MEM_SNAPSHOT);
    std::remove(precisionDbPath.c_str());
    snapshotDb = DataBaseManager::Instance().GetMemSnapshotDatabase(testDbPath);
    ASSERT_TRUE(snapshotDb != nullptr);
    ASSERT_TRUE(snapshotDb->OpenDbReadOnly(testDbPath));
}

// 测试查询无匹配潜在泄漏聚合统计
TEST_F(MemSnapshotDatabaseTest, QueryPotentialLeakStatsWithoutMatches) {
    MemSnapshotLeakStatsParams params;
    params.deviceId = "0";
    params.startEventIdx = 0;
    params.endEventIdx = 0;

    MemSnapshotLeakStatsDTO stats;
    EXPECT_TRUE(snapshotDb->QueryPotentialLeakStats(params, stats));
    EXPECT_EQ(stats.totalSize, 0);
    EXPECT_EQ(stats.maxSize, 0);
    EXPECT_EQ(stats.minSize, 0);
}

// 测试查询trace entries表
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTable) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = TraceEntryTableColumn::ID;

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_EQ(totalCount, 8132);
    EXPECT_EQ(entries.size(), params.pageSize);

    // 验证第一条entry的字段
    if (!entries.empty()) {
        EXPECT_EQ(entries[0].id, 0);
        EXPECT_EQ(entries[0].action, TRACE_ENTRY_ACTION_SEG_MAP);
        EXPECT_EQ(entries[0].address, 20697552257024);
    }
}

// 测试查询trace entries表带事件索引范围
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTableWithEventIdxRange) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 15;
    params.startEventIdx = 100;
    params.endEventIdx = 500;
    params.orderBy = TraceEntryTableColumn::ID;

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_EQ(totalCount, 401);
    EXPECT_EQ(entries.size(), params.pageSize);

    // 验证所有返回的entry都在指定事件索引范围内
    for (const auto &entry : entries) {
        EXPECT_GE(entry.id, params.startEventIdx);
        EXPECT_LE(entry.id, params.endEventIdx);
    }
}

// 测试查询trace entries表带过滤条件
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTableWithFilters) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "id";
    params.filters["action"] = "alloc";

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_EQ(totalCount, 2899);
    EXPECT_EQ(entries.size(), params.pageSize);
    for (const auto &entry : entries) {
        EXPECT_EQ(entry.action, TRACE_ENTRY_ACTION_ALLOC);
    }
}

// 测试查询trace entries表带升序排序
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTableWithAscOrder) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = TraceEntryTableColumn::ADDRESS;
    params.desc = false;

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_EQ(totalCount, 8132);
    EXPECT_EQ(entries.size(), params.pageSize);

    // 验证升序排序
    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_LE(entries[i - 1].address, entries[i].address);
    }
}

// 测试查询trace entries表第10页
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTableSecondPage) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 10;
    params.pageSize = 100;
    params.orderBy = TraceEntryTableColumn::RESERVED;

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_EQ(totalCount, 8132);
    EXPECT_EQ(entries.size(), params.pageSize);

    // 验证升序排序
    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_LE(entries[i - 1].reserved, entries[i].reserved);
    }
}

// 测试根据ID查询trace entry
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntryById) {
    const auto expectEntryId = 1;
    const auto entry = snapshotDb->QueryTraceEntryById(expectEntryId, "0");

    EXPECT_TRUE(entry.has_value());
    EXPECT_EQ(entry->id, expectEntryId);

    // 测试查询不存在的ID
    auto nonExistentEntry = snapshotDb->QueryTraceEntryById(-1000, "0");
    EXPECT_FALSE(nonExistentEntry.has_value());
    nonExistentEntry = snapshotDb->QueryTraceEntryById(-1000, "1");
    EXPECT_FALSE(nonExistentEntry.has_value());
}

// 测试查询内存块的freeRequested事件
TEST_F(MemSnapshotDatabaseTest, QueryFreeRequestedTraceEntryByBlock) {
    // 先查询一个有freeEventId的block
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 100;
    params.orderBy = "id";

    std::vector<BlockTableItemDTO> blocks;
    snapshotDb->QueryBlocksTable(params, blocks);

    // 找一个有freeEventId的block
    bool foundBlockWithFree = false;
    for (const auto &block : blocks) {
        if (block.freeEventId > 0) {
            foundBlockWithFree = true;
            Block tmpBlock{.address = block.address, .allocEventId = block.allocEventId};
            auto freeRequestedEntry = snapshotDb->QueryFreeRequestedTraceEntryByBlock(tmpBlock, "0");
            // 如果存在freeRequested事件，验证其属性
            if (freeRequestedEntry.has_value()) {
                EXPECT_GT(freeRequestedEntry->id, block.allocEventId);
                EXPECT_EQ(freeRequestedEntry->address, block.address);
            }
            break;
        }
    }
    EXPECT_TRUE(foundBlockWithFree || !blocks.empty());
}

// 测试查询blocks表带Size范围过滤
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithSizeRange) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "size";
    params.minSize = 1024;
    params.maxSize = 1048576;

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_GT(totalCount, 0);
    EXPECT_LE(blocks.size(), params.pageSize);

    for (const auto &block : blocks) {
        EXPECT_GE(block.size, params.minSize / 1024);
        EXPECT_LE(block.size, params.maxSize / 1024);
    }
}

// 测试查询blocks表带rangeFilters
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithRangeFilters) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "id";
    params.rangeFilters["size"] = {1024, 1048576};

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_GT(totalCount, 0);
    for (const auto &block : blocks) {
        EXPECT_GE(block.size, 1024);
        EXPECT_LE(block.size, 1048576);
    }
}

// 测试查询blocks表搜索、范围搜索、排序、分页完整
TEST_F(MemSnapshotDatabaseTest, QueryBlocksTableWithMultipleFiltersCombined) {
    MemSnapshotBlockParams params;
    params.deviceId = "0";
    params.eventType = "BLOCK";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "size"; // 根据size排序
    params.startEventIdx = 100; // 设置虚拟时间戳区间最小
    params.endEventIdx = 5000; // 设置虚拟时间戳区间最大
    params.filters["state"] = "allocated"; // 状态为active_allocated
    params.rangeFilters["allocEventId"] = {0, 9999999}; // 模拟内存泄漏场景，过滤内存块的起点在采集时间之前
    params.rangeFilters["freeEventId"] = {
        -1, -1}; // 模拟内存泄漏场景，过滤内存块的终点在采集时间之后（即未在采集区间释放）
    params.rangeFilters["size"] = {1024, 1024 * 1024}; // 过滤大小在1MB到1GB之间的内存块

    std::vector<BlockTableItemDTO> blocks;
    int64_t totalCount = snapshotDb->QueryBlocksTable(params, blocks);

    EXPECT_GE(totalCount, 0);
    EXPECT_EQ(blocks.size(), std::min(totalCount, params.pageSize)); // 分页结果
    double preBlockSize = 0;
    for (const auto &block : blocks) {
        // 测试排序
        EXPECT_GE(block.size, preBlockSize);
        preBlockSize = block.size;
        // 测试满足虚拟时间戳区间
        EXPECT_TRUE(block.allocEventId < 0 || static_cast<uint64_t>(block.allocEventId) <= params.endEventIdx);
        EXPECT_TRUE(block.freeEventId < 0 || static_cast<uint64_t>(block.freeEventId) >= params.startEventIdx);
        // 测试状态为active_allocated
        EXPECT_EQ(block.state, BLOCK_STATE_ACTIVE_ALLOC);
        // 申请事件id在[0, 9999999]之间
        EXPECT_TRUE(0 <= block.allocEventId && static_cast<uint64_t>(block.allocEventId) <= 9999999);
        // 释放事件id为-1（即未在采集区间释放）
        EXPECT_EQ(block.freeEventId, -1);
        // 大小在[1MB, 1GB]之间
        EXPECT_TRUE(1024 <= block.size && block.size <= 1024 * 1024);
    }
}

// 测试查询trace entries表带rangeFilters
TEST_F(MemSnapshotDatabaseTest, QueryTraceEntriesTableWithRangeFilters) {
    MemSnapshotEventParams params;
    params.deviceId = "0";
    params.currentPage = 1;
    params.pageSize = 10;
    params.orderBy = "id";
    params.rangeFilters["size"] = {1024, 1048576};

    std::vector<TraceEntryTableItemDTO> entries;
    int64_t totalCount = snapshotDb->QueryTraceEntriesTable(params, entries);

    EXPECT_GT(totalCount, 0);
    for (const auto &entry : entries) {
        EXPECT_GE(entry.size, 1024);
        EXPECT_LE(entry.size, 1048576);
    }
}

// 测试查询segment事件直到指定事件ID
TEST_F(MemSnapshotDatabaseTest, QuerySegmentEventsUntil) {
    const int64_t eventId = 1000;
    std::vector<TraceEntry> events;

    bool result = snapshotDb->QuerySegmentEventsUntil(eventId, "0", events);
    EXPECT_TRUE(result);
    EXPECT_GT(events.size(), 0);

    // action值: 0=segment_map, 1=segment_unmap, 2=segment_alloc, 3=segment_free
    for (const auto &event : events) {
        EXPECT_TRUE(event.action == TRACE_ENTRY_ACTION_SEG_MAP || event.action == TRACE_ENTRY_ACTION_SEG_UNMAP ||
            event.action == TRACE_ENTRY_ACTION_SEG_ALLOC || event.action == TRACE_ENTRY_ACTION_SEG_FREE);
    }
}

// 测试查询segment事件直到最大事件ID
TEST_F(MemSnapshotDatabaseTest, QuerySegmentEventsUntilMaxEventId) {
    const int64_t maxEventId = snapshotDb->GetDeviceMaxEntryId("0");
    std::vector<TraceEntry> events;

    bool result = snapshotDb->QuerySegmentEventsUntil(maxEventId, "0", events);
    EXPECT_TRUE(result);

    // 验证所有事件ID都小于等于最大事件ID
    for (const auto &event : events) {
        EXPECT_LE(event.id, maxEventId);
    }
}

// 测试查询活跃的内存块
TEST_F(MemSnapshotDatabaseTest, QueryActiveBlocksByEventId) {
    const int64_t eventId = 1000;
    std::vector<Block> blocks;

    bool result = snapshotDb->QueryActiveBlocksByEventId(eventId, "0", blocks);
    EXPECT_TRUE(result);
    EXPECT_GT(blocks.size(), 0);

    // 验证所有返回的块在指定事件ID时刻都是活跃的
    // 活跃条件: allocEventId <= eventId 且 (freeEventId > eventId 或 freeEventId < 0)
    for (const auto &block : blocks) {
        EXPECT_LE(block.allocEventId, eventId);
        EXPECT_TRUE(block.freeEventId > eventId || block.freeEventId < 0);
    }
}
