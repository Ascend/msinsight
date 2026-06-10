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
        // db 句柄由 fixture 统一持有并在 TearDown 中 sqlite3_close 关闭。
        // 析构时断开对该句柄的引用，避免基类 ~Database 对已关闭句柄二次关闭（heap-use-after-free）。
        ~MockDatabase() override {
            db = nullptr;
            isOpen = false;
        }
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
        database.CreateTraceTaskSummaryTable();
        database.CreateTraceIrqDetailTable();
    }

    int64_t QueryRowCount(const std::string &table) {
        std::string sql = "SELECT COUNT(*) FROM " + table;
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(dbPtr, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }
        int64_t count = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return count;
    }

    int64_t QueryInt64(const std::string &sql) {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(dbPtr, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return -1;
        }
        int64_t value = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            value = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return value;
    }

    std::string QueryText(const std::string &sql) {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(dbPtr, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return "";
        }
        std::string value;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text != nullptr) {
                value = reinterpret_cast<const char *>(text);
            }
        }
        sqlite3_finalize(stmt);
        return value;
    }

    void TearDown() override {
        if (dbPtr) {
            sqlite3_close(dbPtr);
            dbPtr = nullptr;
        }
    }

    // 声明顺序：sqlMutex 必须先于 database 构造（database 构造时引用 sqlMutex）
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    MockDatabase database{sqlMutex};
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

// ==================== 新表 trace_task_summary / trace_irq_detail ====================

TEST_F(TextTraceDatabaseFtraceTest, CreateTraceTaskSummaryTable) {
    EXPECT_EQ(database.CheckTableExist("trace_task_summary"), true);
}

TEST_F(TextTraceDatabaseFtraceTest, CreateTraceIrqDetailTable) {
    EXPECT_EQ(database.CheckTableExist("trace_irq_detail"), true);
}

TEST_F(TextTraceDatabaseFtraceTest, InsertAndReadbackTraceTaskSummary) {
    TraceTaskSummaryData data;
    data.comm = "bash";
    data.pid = 1234;
    data.cpuId = 0;
    data.runningNs = 700000;
    data.sleepingNs = 300000;
    data.runnableNs = 50000;
    data.csCount = 10;
    data.csInvoluntaryCount = 3;

    EXPECT_EQ(database.InsertTraceTaskSummary(data), true);
    database.CommitData(); // flush 剩余缓存

    EXPECT_EQ(QueryRowCount("trace_task_summary"), 1);
    EXPECT_EQ(QueryText("SELECT comm FROM trace_task_summary"), "bash");
    EXPECT_EQ(QueryInt64("SELECT pid FROM trace_task_summary"), 1234);
    EXPECT_EQ(QueryInt64("SELECT cpu_id FROM trace_task_summary"), 0);
    EXPECT_EQ(QueryInt64("SELECT running_ns FROM trace_task_summary"), 700000);
    EXPECT_EQ(QueryInt64("SELECT sleeping_ns FROM trace_task_summary"), 300000);
    EXPECT_EQ(QueryInt64("SELECT runnable_ns FROM trace_task_summary"), 50000);
    EXPECT_EQ(QueryInt64("SELECT cs_count FROM trace_task_summary"), 10);
    EXPECT_EQ(QueryInt64("SELECT cs_involuntary_count FROM trace_task_summary"), 3);
}

TEST_F(TextTraceDatabaseFtraceTest, InsertAndReadbackTraceIrqDetail) {
    TraceIrqDetailData data;
    data.comm = "kworker";
    data.pid = 99;
    data.cpuId = 2;
    data.irqType = "irq";
    data.irqName = "eth0";
    data.count = 5;
    data.timeNs = 7000;

    EXPECT_EQ(database.InsertTraceIrqDetail(data), true);
    database.CommitData();

    EXPECT_EQ(QueryRowCount("trace_irq_detail"), 1);
    EXPECT_EQ(QueryText("SELECT comm FROM trace_irq_detail"), "kworker");
    EXPECT_EQ(QueryInt64("SELECT pid FROM trace_irq_detail"), 99);
    EXPECT_EQ(QueryInt64("SELECT cpu_id FROM trace_irq_detail"), 2);
    EXPECT_EQ(QueryText("SELECT irq_type FROM trace_irq_detail"), "irq");
    EXPECT_EQ(QueryText("SELECT irq_name FROM trace_irq_detail"), "eth0");
    EXPECT_EQ(QueryInt64("SELECT count FROM trace_irq_detail"), 5);
    EXPECT_EQ(QueryInt64("SELECT time_ns FROM trace_irq_detail"), 7000);
}

// 复合主键 (comm, pid, cpu_id)：同一进程跨 CPU 应作为不同行存在
TEST_F(TextTraceDatabaseFtraceTest, TraceTaskSummarySameProcessDifferentCpuAreSeparateRows) {
    TraceTaskSummaryData cpu0;
    cpu0.comm = "app";
    cpu0.pid = 555;
    cpu0.cpuId = 0;
    cpu0.runningNs = 100;

    TraceTaskSummaryData cpu1 = cpu0;
    cpu1.cpuId = 1;
    cpu1.runningNs = 200;

    EXPECT_EQ(database.InsertTraceTaskSummary(cpu0), true);
    EXPECT_EQ(database.InsertTraceTaskSummary(cpu1), true);
    database.CommitData();

    EXPECT_EQ(QueryRowCount("trace_task_summary"), 2);
    EXPECT_EQ(QueryInt64("SELECT running_ns FROM trace_task_summary WHERE cpu_id = 0"), 100);
    EXPECT_EQ(QueryInt64("SELECT running_ns FROM trace_task_summary WHERE cpu_id = 1"), 200);
}

// 复合主键 (comm, pid, cpu_id, irq_type, irq_name)：不同中断名应作为不同行存在
TEST_F(TextTraceDatabaseFtraceTest, TraceIrqDetailDifferentIrqNameAreSeparateRows) {
    TraceIrqDetailData eth;
    eth.comm = "app";
    eth.pid = 555;
    eth.cpuId = 0;
    eth.irqType = "irq";
    eth.irqName = "eth0";
    eth.count = 1;

    TraceIrqDetailData timer = eth;
    timer.irqName = "timer";
    timer.count = 2;

    EXPECT_EQ(database.InsertTraceIrqDetail(eth), true);
    EXPECT_EQ(database.InsertTraceIrqDetail(timer), true);
    database.CommitData();

    EXPECT_EQ(QueryRowCount("trace_irq_detail"), 2);
    EXPECT_EQ(QueryInt64("SELECT count FROM trace_irq_detail WHERE irq_name = 'eth0'"), 1);
    EXPECT_EQ(QueryInt64("SELECT count FROM trace_irq_detail WHERE irq_name = 'timer'"), 2);
}

// UpdateTraceTaskSummaryCsCount：先插入一行时间统计数据，再累加更新 cs_count
TEST_F(TextTraceDatabaseFtraceTest, UpdateTraceTaskSummaryCsCount_Basic) {
    // 先插入基础数据（模拟子任务2 写入的时间统计）
    TraceTaskSummaryData data;
    data.comm = "bash";
    data.pid = 100;
    data.cpuId = 0;
    data.runningNs = 500000;
    data.sleepingNs = 200000;
    data.runnableNs = 30000;

    EXPECT_EQ(database.InsertTraceTaskSummary(data), true);
    database.CommitData();
    EXPECT_EQ(QueryRowCount("trace_task_summary"), 1);

    // 累加更新 cs_count
    EXPECT_EQ(database.UpdateTraceTaskSummaryCsCount("bash", 100, 0, 5, 2), true);
    EXPECT_EQ(QueryInt64("SELECT cs_count FROM trace_task_summary WHERE comm='bash'"), 5);
    EXPECT_EQ(QueryInt64("SELECT cs_involuntary_count FROM trace_task_summary WHERE comm='bash'"), 2);

    // 时间统计字段不应被覆盖
    EXPECT_EQ(QueryInt64("SELECT running_ns FROM trace_task_summary WHERE comm='bash'"), 500000);
    EXPECT_EQ(QueryInt64("SELECT sleeping_ns FROM trace_task_summary WHERE comm='bash'"), 200000);
}

// 多次 Update 应为累加，而非覆盖
TEST_F(TextTraceDatabaseFtraceTest, UpdateTraceTaskSummaryCsCount_Accumulate) {
    TraceTaskSummaryData data;
    data.comm = "app";
    data.pid = 200;
    data.cpuId = 1;
    data.runningNs = 100;

    EXPECT_EQ(database.InsertTraceTaskSummary(data), true);
    database.CommitData();

    EXPECT_EQ(database.UpdateTraceTaskSummaryCsCount("app", 200, 1, 3, 1), true);
    EXPECT_EQ(database.UpdateTraceTaskSummaryCsCount("app", 200, 1, 7, 2), true);

    EXPECT_EQ(QueryInt64("SELECT cs_count FROM trace_task_summary WHERE comm='app'"), 10);
    EXPECT_EQ(QueryInt64("SELECT cs_involuntary_count FROM trace_task_summary WHERE comm='app'"), 3);
}

// 对不存在的行调用 Update，不应创建新行（WHERE 条件不匹配）
TEST_F(TextTraceDatabaseFtraceTest, UpdateTraceTaskSummaryCsCount_NoMatch) {
    EXPECT_EQ(QueryRowCount("trace_task_summary"), 0);

    // 对不存在的行执行 Update，不应报错但也不应有数据
    EXPECT_EQ(database.UpdateTraceTaskSummaryCsCount("nonexist", 999, 0, 1, 0), true);
    EXPECT_EQ(QueryRowCount("trace_task_summary"), 0);
}

// QueryTraceTaskSummary：插入数据后查询，验证 totalCount 和数据行
TEST_F(TextTraceDatabaseFtraceTest, QueryTraceTaskSummary_Basic) {
    TraceTaskSummaryData d1;
    d1.comm = "bash";
    d1.pid = 100;
    d1.cpuId = 0;
    d1.runningNs = 1000;
    d1.sleepingNs = 500;

    TraceTaskSummaryData d2 = d1;
    d2.comm = "app";
    d2.pid = 200;
    d2.cpuId = 1;
    d2.runningNs = 2000;

    EXPECT_EQ(database.InsertTraceTaskSummary(d1), true);
    EXPECT_EQ(database.InsertTraceTaskSummary(d2), true);
    database.CommitData();

    auto result = database.QueryTraceTaskSummary(0, 10);
    EXPECT_EQ(result.totalCount, 2);
    EXPECT_EQ(result.data.size(), 2);
    EXPECT_EQ(result.data[0].comm, "bash");
    EXPECT_EQ(result.data[0].pid, 100);
    EXPECT_EQ(result.data[0].runningNs, 1000);
}

// QueryTraceTaskSummary：分页查询
TEST_F(TextTraceDatabaseFtraceTest, QueryTraceTaskSummary_Pagination) {
    for (int i = 0; i < 5; ++i) {
        TraceTaskSummaryData d;
        d.comm = "proc" + std::to_string(i);
        d.pid = i;
        d.cpuId = 0;
        d.runningNs = i * 100;
        EXPECT_EQ(database.InsertTraceTaskSummary(d), true);
    }
    database.CommitData();

    auto page1 = database.QueryTraceTaskSummary(0, 2);
    EXPECT_EQ(page1.totalCount, 5);
    EXPECT_EQ(page1.data.size(), 2);

    auto page3 = database.QueryTraceTaskSummary(4, 2);
    EXPECT_EQ(page3.totalCount, 5);
    EXPECT_EQ(page3.data.size(), 1);
}

// QueryTraceIrqDetail：插入后查询
TEST_F(TextTraceDatabaseFtraceTest, QueryTraceIrqDetail_Basic) {
    TraceIrqDetailData d;
    d.comm = "app";
    d.pid = 300;
    d.cpuId = 0;
    d.irqType = "irq";
    d.irqName = "eth0";
    d.count = 5;
    d.timeNs = 3000;

    EXPECT_EQ(database.InsertTraceIrqDetail(d), true);
    database.CommitData();

    auto result = database.QueryTraceIrqDetail(0, 10);
    EXPECT_EQ(result.totalCount, 1);
    EXPECT_EQ(result.data.size(), 1);
    EXPECT_EQ(result.data[0].irqName, "eth0");
    EXPECT_EQ(result.data[0].count, 5);
}

// Query 空表应返回 totalCount=0
TEST_F(TextTraceDatabaseFtraceTest, QueryEmptyResultNewTables) {
    // 新建空表测试
    auto taskResult = database.QueryTraceTaskSummary(0, 10);
    EXPECT_EQ(taskResult.totalCount, 0);

    auto irqResult = database.QueryTraceIrqDetail(0, 10);
    EXPECT_EQ(irqResult.totalCount, 0);
}
