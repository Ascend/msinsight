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
#include "TextTraceDatabase.h"
#include "DatabaseTestCaseMockUtil.h"

using namespace Dic::Module::Timeline;

class TextTraceDatabaseFtraceTest : public ::testing::Test {
  protected:
    class MockDatabase : public TextTraceDatabase {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };

    void SetUp() override {
        Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
        database.SetDbPtr(dbPtr);
        database.CreateFtraceTable();
    }

    void TearDown() override {
        if (dbPtr) {
            sqlite3_close(dbPtr);
            dbPtr = nullptr;
        }
    }

    sqlite3 *dbPtr = nullptr;
    MockDatabase database{sqlMutex};
    std::recursive_mutex sqlMutex;
};

TEST_F(TextTraceDatabaseFtraceTest, CreateFtraceTable) {
    std::recursive_mutex testMutex;
    MockDatabase testDatabase(testMutex);
    testDatabase.SetDbPtr(dbPtr);

    bool success = database.CheckTableExist("ftrace_analysis");
    EXPECT_EQ(success, true);
}

TEST_F(TextTraceDatabaseFtraceTest, InsertAndQueryFtraceTimeStatistics) {
    // 插入耗时统计数据
    FtraceStatisticsData timeData;
    timeData.trackId = 1;
    timeData.dataType = FtraceDataType::TIME;
    timeData.data["running"] = "1000";
    timeData.data["sleeping"] = "2000";
    timeData.data["runnable"] = "500";
    timeData.data["uninterruptibleSleep"] = "100";

    bool insertResult = database.InsertFtraceStatList({timeData});
    EXPECT_EQ(insertResult, true);

    // 查询验证
    auto result = database.QueryFtraceStatistics(FtraceDataType::TIME, 0, 10);
    EXPECT_EQ(result.totalCount, 1);
    EXPECT_EQ(result.data[0].trackId, 1);
    EXPECT_EQ(result.data[0].data["running"], "1000");
    EXPECT_EQ(result.data[0].data["sleeping"], "2000");
    EXPECT_EQ(result.data[0].data["runnable"], "500");
    EXPECT_EQ(result.data[0].data["uninterruptibleSleep"], "100");
}

TEST_F(TextTraceDatabaseFtraceTest, InsertAndQueryFtraceIrqStatistics) {
    // 插入中断统计数据
    FtraceStatisticsData irqData;
    irqData.trackId = 2;
    irqData.dataType = FtraceDataType::IRQ;
    irqData.data["softIrqCount"] = "10";
    irqData.data["softIrqDuration"] = "5000";
    irqData.data["hardIrqCount"] = "5";
    irqData.data["hardIrqDuration"] = "2000";

    bool insertResult = database.InsertFtraceStatList({irqData});
    EXPECT_EQ(insertResult, true);

    // 查询验证
    auto result = database.QueryFtraceStatistics(FtraceDataType::IRQ, 0, 10);
    EXPECT_EQ(result.totalCount, 1);
    EXPECT_EQ(result.data[0].trackId, 2);
    EXPECT_EQ(result.data[0].data["softIrqCount"], "10");
    EXPECT_EQ(result.data[0].data["softIrqDuration"], "5000");
    EXPECT_EQ(result.data[0].data["hardIrqCount"], "5");
    EXPECT_EQ(result.data[0].data["hardIrqDuration"], "2000");
}

TEST_F(TextTraceDatabaseFtraceTest, InsertAndQueryFtraceSchedStatistics) {
    // 插入上下文切换统计数据
    FtraceStatisticsData schedData;
    schedData.trackId = 3;
    schedData.dataType = FtraceDataType::SCHED;
    schedData.data["contextSwitchCount"] = "100";
    schedData.data["contextSwitchDuration"] = "50000";

    bool insertResult = database.InsertFtraceStatList({schedData});
    EXPECT_EQ(insertResult, true);

    // 查询验证
    auto result = database.QueryFtraceStatistics(FtraceDataType::SCHED, 0, 10);
    EXPECT_EQ(result.totalCount, 1);
    EXPECT_EQ(result.data[0].trackId, 3);
    EXPECT_EQ(result.data[0].data["contextSwitchCount"], "100");
    EXPECT_EQ(result.data[0].data["contextSwitchDuration"], "50000");
}

TEST_F(TextTraceDatabaseFtraceTest, InsertMultipleDataTypesForSameTrackId) {
    // 为同一个trackId插入不同类型的统计数据
    FtraceStatisticsData timeData;
    timeData.trackId = 10;
    timeData.dataType = FtraceDataType::TIME;
    timeData.data["running"] = "3000";

    FtraceStatisticsData irqData;
    irqData.trackId = 10;
    irqData.dataType = FtraceDataType::IRQ;
    irqData.data["softIrqCount"] = "20";

    FtraceStatisticsData schedData;
    schedData.trackId = 10;
    schedData.dataType = FtraceDataType::SCHED;
    schedData.data["contextSwitchCount"] = "200";

    bool insertResult = database.InsertFtraceStatList({timeData, irqData, schedData});
    EXPECT_EQ(insertResult, true);

    // 分别查询每种类型
    auto timeResult = database.QueryFtraceStatistics(FtraceDataType::TIME, 0, 10);
    auto irqResult = database.QueryFtraceStatistics(FtraceDataType::IRQ, 0, 10);
    auto schedResult = database.QueryFtraceStatistics(FtraceDataType::SCHED, 0, 10);

    EXPECT_EQ(timeResult.totalCount, 1);
    EXPECT_EQ(timeResult.data[0].trackId, 10);
    EXPECT_EQ(timeResult.data[0].data["running"], "3000");

    EXPECT_EQ(irqResult.totalCount, 1);
    EXPECT_EQ(irqResult.data[0].trackId, 10);
    EXPECT_EQ(irqResult.data[0].data["softIrqCount"], "20");

    EXPECT_EQ(schedResult.totalCount, 1);
    EXPECT_EQ(schedResult.data[0].trackId, 10);
    EXPECT_EQ(schedResult.data[0].data["contextSwitchCount"], "200");
}

TEST_F(TextTraceDatabaseFtraceTest, QueryWithPagination) {
    // 插入多条数据
    for (uint64_t i = 0; i < 25; ++i) {
        FtraceStatisticsData data;
        data.trackId = i;
        data.dataType = FtraceDataType::TIME;
        data.data["running"] = std::to_string(i * 100);
        database.InsertFtraceStatList({data});
    }

    // 查询第一页 (0-9)
    auto page1 = database.QueryFtraceStatistics(FtraceDataType::TIME, 0, 10);
    EXPECT_EQ(page1.totalCount, 25);
    EXPECT_EQ(page1.data[0].trackId, 0);
    EXPECT_EQ(page1.data[9].trackId, 9);

    // 查询第二页 (10-19)
    auto page2 = database.QueryFtraceStatistics(FtraceDataType::TIME, 10, 10);
    EXPECT_EQ(page2.totalCount, 25);
    EXPECT_EQ(page2.data[0].trackId, 10);
    EXPECT_EQ(page2.data[9].trackId, 19);

    // 查询第三页 (20-24，只剩5条)
    auto page3 = database.QueryFtraceStatistics(FtraceDataType::TIME, 20, 10);
    EXPECT_EQ(page3.totalCount, 25);
    EXPECT_EQ(page3.data[0].trackId, 20);
    EXPECT_EQ(page3.data[4].trackId, 24);
}

TEST_F(TextTraceDatabaseFtraceTest, QueryEmptyResult) {
    auto result = database.QueryFtraceStatistics(FtraceDataType::TIME, 0, 10);
    EXPECT_EQ(result.totalCount, 0);
}

TEST_F(TextTraceDatabaseFtraceTest, QueryDifferentDataTypes) {
    // 插入三种类型的数据
    FtraceStatisticsData timeData;
    timeData.trackId = 1;
    timeData.dataType = FtraceDataType::TIME;
    timeData.data["running"] = "1000";

    FtraceStatisticsData irqData;
    irqData.trackId = 2;
    irqData.dataType = FtraceDataType::IRQ;
    irqData.data["softIrqCount"] = "10";

    FtraceStatisticsData schedData;
    schedData.trackId = 3;
    schedData.dataType = FtraceDataType::SCHED;
    schedData.data["contextSwitchCount"] = "100";

    database.InsertFtraceStatList({timeData, irqData, schedData});

    // TIME类型查询应该只有1条
    auto timeResult = database.QueryFtraceStatistics(FtraceDataType::TIME, 0, 10);
    EXPECT_EQ(timeResult.totalCount, 1);

    // IRQ类型查询应该只有1条
    auto irqResult = database.QueryFtraceStatistics(FtraceDataType::IRQ, 0, 10);
    EXPECT_EQ(irqResult.totalCount, 1);

    // SCHED类型查询应该只有1条
    auto schedResult = database.QueryFtraceStatistics(FtraceDataType::SCHED, 0, 10);
    EXPECT_EQ(schedResult.totalCount, 1);
}
