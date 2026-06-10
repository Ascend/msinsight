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

    uint64_t totalCount = 0;
    auto result = database.QueryTraceTaskSummary({}, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 2);
    EXPECT_EQ(result.size(), 2);
    // 默认排序为 running_ns DESC
    EXPECT_EQ(result[0].comm, "app");
    EXPECT_EQ(result[0].pid, 200);
    EXPECT_EQ(result[0].cpuId, 1);
    EXPECT_EQ(result[0].runningNs, 2000);
    EXPECT_EQ(result[1].comm, "bash");
    EXPECT_EQ(result[1].pid, 100);
    EXPECT_EQ(result[1].cpuId, 0);
    EXPECT_EQ(result[1].runningNs, 1000);
    EXPECT_EQ(result[1].sleepingNs, 500);
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

    uint64_t totalCount = 0;
    auto page1 = database.QueryTraceTaskSummary({}, 0, 2, totalCount);
    EXPECT_EQ(totalCount, 5);
    EXPECT_EQ(page1.size(), 2);
    EXPECT_EQ(page1[0].runningNs, 400);
    EXPECT_EQ(page1[1].runningNs, 300);

    auto page3 = database.QueryTraceTaskSummary({}, 4, 2, totalCount);
    EXPECT_EQ(totalCount, 5);
    EXPECT_EQ(page3.size(), 1);
    EXPECT_EQ(page3[0].runningNs, 0);
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
    uint64_t totalCount = 999;
    auto taskResult = database.QueryTraceTaskSummary({}, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 0);

    auto irqResult = database.QueryTraceIrqDetail(0, 10);
    EXPECT_EQ(irqResult.totalCount, 0);
}

// ==================== QueryTraceTaskSummary 测试（Handler 查询路径）====================

// 基础查询：插入数据后查询，验证 totalCount 和行数据
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_Basic) {
    TraceTaskSummaryData d1;
    d1.comm = "bash";
    d1.pid = 100;
    d1.cpuId = 0;
    d1.runningNs = 1000;
    d1.sleepingNs = 500;
    d1.runnableNs = 100;
    d1.csCount = 5;
    d1.csInvoluntaryCount = 2;
    EXPECT_TRUE(database.InsertTraceTaskSummary(d1));
    database.CommitData();

    uint64_t totalCount = 0;
    auto rows = database.QueryTraceTaskSummary({}, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 1);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].comm, "bash");
    EXPECT_EQ(rows[0].pid, 100);
    EXPECT_EQ(rows[0].cpuId, 0);
    EXPECT_EQ(rows[0].runningNs, 1000);
    EXPECT_EQ(rows[0].sleepingNs, 500);
    EXPECT_EQ(rows[0].runnableNs, 100);
    EXPECT_EQ(rows[0].csCount, 5);
    EXPECT_EQ(rows[0].csInvoluntaryCount, 2);
}

// 排序测试：验证 orderBy 字段映射和升降序
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_Sorting) {
    TraceTaskSummaryData d1;
    d1.comm = "alpha";
    d1.pid = 100;
    d1.cpuId = 0;
    d1.runningNs = 1000;
    TraceTaskSummaryData d2 = d1;
    d2.comm = "beta";
    d2.pid = 200;
    d2.runningNs = 3000;
    TraceTaskSummaryData d3 = d1;
    d3.comm = "gamma";
    d3.pid = 300;
    d3.runningNs = 2000;
    EXPECT_TRUE(database.InsertTraceTaskSummary(d1));
    EXPECT_TRUE(database.InsertTraceTaskSummary(d2));
    EXPECT_TRUE(database.InsertTraceTaskSummary(d3));
    database.CommitData();

    // 按 running 降序
    uint64_t totalCount = 0;
    SystemViewFtraceStatParams descParams;
    descParams.orderBy = "running";
    descParams.order = "descend";
    auto rowsDesc = database.QueryTraceTaskSummary(descParams, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 3);
    EXPECT_EQ(rowsDesc[0].runningNs, 3000);
    EXPECT_EQ(rowsDesc[1].runningNs, 2000);
    EXPECT_EQ(rowsDesc[2].runningNs, 1000);

    // 按 running 升序
    SystemViewFtraceStatParams ascParams;
    ascParams.orderBy = "running";
    ascParams.order = "ascend";
    auto rowsAsc = database.QueryTraceTaskSummary(ascParams, 0, 10, totalCount);
    EXPECT_EQ(rowsAsc[0].runningNs, 1000);
    EXPECT_EQ(rowsAsc[1].runningNs, 2000);
    EXPECT_EQ(rowsAsc[2].runningNs, 3000);

    // 按 comm 排序
    SystemViewFtraceStatParams commParams;
    commParams.orderBy = "comm";
    commParams.order = "ascend";
    auto rowsComm = database.QueryTraceTaskSummary(commParams, 0, 10, totalCount);
    EXPECT_EQ(rowsComm[0].comm, "alpha");
    EXPECT_EQ(rowsComm[1].comm, "beta");
    EXPECT_EQ(rowsComm[2].comm, "gamma");
}

// 筛选测试：cpu/comm/pid 模糊匹配
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_Filtering) {
    TraceTaskSummaryData d1;
    d1.comm = "bash";
    d1.pid = 100;
    d1.cpuId = 0;
    d1.runningNs = 1000;
    TraceTaskSummaryData d2 = d1;
    d2.comm = "python3";
    d2.pid = 200;
    d2.cpuId = 1;
    d2.runningNs = 2000;
    TraceTaskSummaryData d3 = d1;
    d3.comm = "bash_worker";
    d3.pid = 101;
    d3.cpuId = 0;
    d3.runningNs = 3000;
    EXPECT_TRUE(database.InsertTraceTaskSummary(d1));
    EXPECT_TRUE(database.InsertTraceTaskSummary(d2));
    EXPECT_TRUE(database.InsertTraceTaskSummary(d3));
    database.CommitData();

    uint64_t totalCount = 0;

    // 按 comm 模糊匹配 "bash"
    SystemViewFtraceStatParams commParams;
    commParams.filters = {{"comm", "bash"}};
    auto rows = database.QueryTraceTaskSummary(commParams, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 2); // bash 和 bash_worker
    EXPECT_EQ(rows.size(), 2);

    // 按 cpu 筛选 "0"
    SystemViewFtraceStatParams cpuParams;
    cpuParams.filters = {{"cpu", "0"}};
    rows = database.QueryTraceTaskSummary(cpuParams, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 2); // cpuId=0 的 bash 和 bash_worker

    // 按 pid 模糊匹配 "10"
    SystemViewFtraceStatParams pidParams;
    pidParams.filters = {{"pid", "10"}};
    rows = database.QueryTraceTaskSummary(pidParams, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 2); // pid=100 和 pid=101
}

// IRQ 聚合测试：验证 softirq/hardirq 的 count 和 duration 聚合
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_IrqAggregation) {
    TraceTaskSummaryData d1;
    d1.comm = "app";
    d1.pid = 500;
    d1.cpuId = 0;
    d1.runningNs = 5000;
    EXPECT_TRUE(database.InsertTraceTaskSummary(d1));
    database.CommitData();

    TraceIrqDetailData si1;
    si1.comm = "app";
    si1.pid = 500;
    si1.cpuId = 0;
    si1.irqType = "softirq";
    si1.irqName = "timer";
    si1.count = 3;
    si1.timeNs = 300;
    TraceIrqDetailData si2 = si1;
    si2.irqName = "net";
    si2.count = 2;
    si2.timeNs = 200;
    TraceIrqDetailData hi1;
    hi1.comm = "app";
    hi1.pid = 500;
    hi1.cpuId = 0;
    hi1.irqType = "irq";
    hi1.irqName = "eth0";
    hi1.count = 1;
    hi1.timeNs = 100;
    EXPECT_TRUE(database.InsertTraceIrqDetail(si1));
    EXPECT_TRUE(database.InsertTraceIrqDetail(si2));
    EXPECT_TRUE(database.InsertTraceIrqDetail(hi1));
    database.CommitData();

    // IRQ 聚合：将 trace_irq_detail 数据 UPDATE 到 trace_task_summary
    EXPECT_TRUE(database.AggregateIrqToTaskSummary());

    uint64_t totalCount = 0;
    auto rows = database.QueryTraceTaskSummary({}, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 1);
    EXPECT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].softIrqCount, 5); // 3 + 2
    EXPECT_EQ(rows[0].softIrqDuration, 500); // 300 + 200
    EXPECT_EQ(rows[0].hardIrqCount, 1);
    EXPECT_EQ(rows[0].hardIrqDuration, 100);
}

// 分页测试
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_Pagination) {
    for (int i = 0; i < 5; ++i) {
        TraceTaskSummaryData d;
        d.comm = "proc" + std::to_string(i);
        d.pid = i;
        d.cpuId = 0;
        d.runningNs = i * 1000;
        EXPECT_TRUE(database.InsertTraceTaskSummary(d));
    }
    database.CommitData();

    uint64_t totalCount = 0;
    SystemViewFtraceStatParams pageParams;
    pageParams.orderBy = "running";
    pageParams.order = "ascend";
    auto page1 = database.QueryTraceTaskSummary(pageParams, 0, 2, totalCount);
    EXPECT_EQ(totalCount, 5);
    EXPECT_EQ(page1.size(), 2);
    EXPECT_EQ(page1[0].runningNs, 0);
    EXPECT_EQ(page1[1].runningNs, 1000);

    auto page3 = database.QueryTraceTaskSummary(pageParams, 4, 2, totalCount);
    EXPECT_EQ(totalCount, 5);
    EXPECT_EQ(page3.size(), 1);
    EXPECT_EQ(page3[0].runningNs, 4000);
}

// 空结果测试
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_EmptyResult) {
    uint64_t totalCount = 999;
    auto rows = database.QueryTraceTaskSummary({}, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 0);
    EXPECT_TRUE(rows.empty());
}

// 不支持的 columnName 应被忽略
TEST_F(TextTraceDatabaseFtraceTest, QueryForHandler_IgnoreUnknownFilter) {
    TraceTaskSummaryData d1;
    d1.comm = "test";
    d1.pid = 1;
    d1.cpuId = 0;
    d1.runningNs = 100;
    EXPECT_TRUE(database.InsertTraceTaskSummary(d1));
    database.CommitData();

    uint64_t totalCount = 0;
    SystemViewFtraceStatParams unknownParams;
    unknownParams.filters = {{"unknown_col", "value"}};
    auto rows = database.QueryTraceTaskSummary(unknownParams, 0, 10, totalCount);
    EXPECT_EQ(totalCount, 1); // 未知 filter 被忽略，返回所有行
    EXPECT_EQ(rows.size(), 1);
}

// 幂等性保证：ParseUnit 框架通过 status_info 表保证重复导入跳过 Parse，不会导致 IRQ 翻倍
