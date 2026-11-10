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
#include <memory>
#include <string>
#include "DatabaseTestCaseMockUtil.h"
#include "DataEngine.h"
#include "DataBaseManager.h"
#include "FtraceTimeStatisticsParseUnit.h"
#include "FtraceSchedStatisticsParseUnit.h"
#include "FtraceIrqStatisticsParseUnit.h"
#include "FtraceIrqAggregationParseUnit.h"
#include "RenderEngineInterface.h"
#include "ThreadPool.h"
#define private public
#include "RenderEngine.h"
#undef private
#include "FtraceStatisticsSliceReader.h"

namespace Dic::Module::FullDb {
namespace {
using namespace Timeline;
using Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil;
using ThreadInfoMap = std::unordered_map<uint64_t, std::pair<std::string, std::string>>;

#ifdef _WIN32
class ThreadPoolTestEnvironment : public testing::Environment {
  public:
    void TearDown() override {
        // Join singleton workers before Windows enters DLL static destruction under the loader lock.
        Dic::ThreadPool::Instance().ShutDown();
    }
};

testing::Environment *const threadPoolTestEnvironment =
    testing::AddGlobalTestEnvironment(new ThreadPoolTestEnvironment());
#endif

class StatisticsDataEngine : public DataEngine {
  public:
    std::vector<CompeteSliceDomain> slices;
    ThreadInfoMap threads;
    unsigned threadQueryCount = 0;
    unsigned sliceQueryCount = 0;

    bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &query, std::vector<CompeteSliceDomain> &result) override {
        ++sliceQueryCount;
        result = slices;
        return true;
    }

    void QueryAllThreadInfo(const ThreadQuery &query, ThreadInfoMap &result) override {
        ++threadQueryCount;
        result = threads;
    }
};

class InMemoryTraceDatabase : public TextTraceDatabase {
  public:
    explicit InMemoryTraceDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
    void Attach(sqlite3 *database) {
        db = database;
        isOpen = true;
        path = ":memory:";
    }
};

template <typename Unit> class TestableStatisticsUnit : public Unit {
  public:
    using Unit::PreCheck;
    using Unit::HandleParseProcess;
};

class FtraceStatisticsParseUnitTest : public testing::Test {
  protected:
    void SetUp() override {
        previousDataEngine = RenderEngine::Instance()->dataEngine;
        dataEngine = std::make_shared<StatisticsDataEngine>();
        RenderEngine::Instance()->SetDataEngineInterface(dataEngine);
        DatabaseTestCaseMockUtil::OpenDB(rawDatabase);
        ASSERT_NE(rawDatabase, nullptr);
        database = std::make_shared<InMemoryTraceDatabase>(sqlMutex);
        database->Attach(rawDatabase);
    }

    void TearDown() override {
        RenderEngine::Instance()->SetDataEngineInterface(previousDataEngine);
        database.reset();
        rawDatabase = nullptr;
    }

    CompeteSliceDomain Slice(uint64_t trackId, const std::string &name, uint64_t duration, const std::string &args) {
        CompeteSliceDomain slice;
        slice.trackId = trackId;
        slice.name = name;
        slice.duration = duration;
        slice.args = args;
        return slice;
    }

    template <typename Unit> void Parse() {
        TestableStatisticsUnit<Unit> unit;
        std::string error;
        ASSERT_TRUE(unit.PreCheck({"statistics-ut"}, database, error)) << error;
        ASSERT_TRUE(unit.HandleParseProcess({"statistics-ut"}, database, error)) << error;
    }

    int64_t QueryInteger(const std::string &sql) {
        sqlite3_stmt *statement = nullptr;
        const int prepared = sqlite3_prepare_v2(rawDatabase, sql.c_str(), -1, &statement, nullptr);
        EXPECT_EQ(prepared, SQLITE_OK) << sqlite3_errmsg(rawDatabase);
        if (prepared != SQLITE_OK) {
            return -1;
        }
        const int step = sqlite3_step(statement);
        EXPECT_EQ(step, SQLITE_ROW);
        const int64_t value = step == SQLITE_ROW ? sqlite3_column_int64(statement, 0) : -1;
        EXPECT_EQ(sqlite3_finalize(statement), SQLITE_OK);
        return value;
    }

    void CreateStreamingCache() {
        ASSERT_EQ(sqlite3_exec(rawDatabase, R"(
CREATE TABLE msprof_ftrace_cache(marker TEXT);
CREATE TABLE process(pid TEXT PRIMARY KEY,process_name TEXT);
CREATE TABLE thread(track_id INTEGER PRIMARY KEY,pid TEXT);
CREATE TABLE slice(track_id INTEGER,name TEXT,duration INTEGER,args TEXT);
INSERT INTO process VALUES('task','Process Scheduling'),('cpu','CPU Scheduling');
INSERT INTO thread VALUES(1,'task'),(2,'cpu');
)",
                      nullptr, nullptr, nullptr),
            SQLITE_OK);
    }

    std::recursive_mutex sqlMutex;
    sqlite3 *rawDatabase = nullptr;
    std::shared_ptr<InMemoryTraceDatabase> database;
    std::shared_ptr<StatisticsDataEngine> dataEngine;
    std::shared_ptr<DataEngineInterface> previousDataEngine;
};

TEST_F(FtraceStatisticsParseUnitTest, TimeAggregatesStatesAndSeparatesCpu) {
    dataEngine->threads = {{1, {"process", "worker:42"}}, {2, {"process", "worker:42"}}};
    dataEngine->slices = {
        Slice(1, "Running", 10, R"({"cpu":"0"})"),
        Slice(1, "Running", 20, R"({"cpu":"0"})"),
        Slice(1, "Sleeping", 40, R"({"cpu":"0"})"),
        Slice(1, "Runnable", 5, R"({"cpu":"0"})"),
        Slice(2, "Running", 70, R"({"cpu":"1"})"),
    };
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 2);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE cpu_id=0"), 30);
    EXPECT_EQ(QueryInteger("SELECT sleeping_ns FROM trace_task_summary WHERE cpu_id=0"), 40);
    EXPECT_EQ(QueryInteger("SELECT runnable_ns FROM trace_task_summary WHERE cpu_id=0"), 5);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE cpu_id=1"), 70);
}

TEST_F(FtraceStatisticsParseUnitTest, TimeParsesPidAfterLastColonAndSkipsMissingTrack) {
    dataEngine->threads = {{1, {"process", "app:worker:42"}}};
    dataEngine->slices = {
        Slice(1, "Running", 10, R"({"cpu":"0"})"),
        Slice(99, "Running", 100, R"({"cpu":"0"})"),
    };
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 1);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE comm='app:worker' AND pid=42"), 10);
}

class FtraceTimeInvalidTidTest : public FtraceStatisticsParseUnitTest,
                                 public testing::WithParamInterface<std::string> {};

TEST_P(FtraceTimeInvalidTidTest, SkipsInvalidTid) {
    dataEngine->threads = {{1, {"process", GetParam()}}};
    dataEngine->slices = {Slice(1, "Running", 10, R"({"cpu":"0"})")};
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 0);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidTid, FtraceTimeInvalidTidTest, testing::Values("", "worker", ":42", "worker:", "worker:abc"));

class FtraceTimeInvalidCpuTest : public FtraceStatisticsParseUnitTest,
                                 public testing::WithParamInterface<std::string> {};

TEST_P(FtraceTimeInvalidCpuTest, UsesUnknownCpuForMissingOrInvalidValue) {
    dataEngine->threads = {{1, {"process", "worker:42"}}};
    dataEngine->slices = {Slice(1, "Running", 10, GetParam())};
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 1);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE cpu_id=-1"), 10);
}

INSTANTIATE_TEST_SUITE_P(InvalidCpu, FtraceTimeInvalidCpuTest,
    testing::Values("{}", R"({"cpu":""})", R"({"cpu":"invalid"})", R"({"cpu":"999999999999999999999"})"));

TEST_F(FtraceStatisticsParseUnitTest, SchedAccumulatesTotalAndInvoluntarySwitches) {
    ASSERT_TRUE(database->CreateTraceTaskSummaryTable());
    ASSERT_TRUE(database->ExecSql("INSERT INTO trace_task_summary(comm,pid,cpu_id,cs_count,cs_involuntary_count)"
                                  " VALUES('worker',42,-1,4,1),('other',43,-1,0,0);"));
    dataEngine->threads = {{1, {"process", "worker:42"}}, {2, {"process", "other:43"}}};
    dataEngine->slices = {
        Slice(1, "sched_switch", 0, R"({"prev_comm":"worker","prev_pid":"42","prev_state":"R"})"),
        Slice(1, "sched_switch", 0, R"({"prev_comm":"worker","prev_pid":"42","prev_state":"S"})"),
        Slice(2, "sched_switch", 0, R"({"prev_comm":"other","prev_pid":"43","prev_state":"R"})"),
    };
    Parse<FtraceSchedStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE comm='worker'"), 6);
    EXPECT_EQ(QueryInteger("SELECT cs_involuntary_count FROM trace_task_summary WHERE comm='worker'"), 2);
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE comm='other'"), 1);
    EXPECT_EQ(QueryInteger("SELECT cs_involuntary_count FROM trace_task_summary WHERE comm='other'"), 1);
}

class FtraceSchedFilteredInputTest : public FtraceStatisticsParseUnitTest,
                                     public testing::WithParamInterface<std::string> {};

TEST_P(FtraceSchedFilteredInputTest, SkipsInvalidOrUnmatchedSwitch) {
    ASSERT_TRUE(database->CreateTraceTaskSummaryTable());
    ASSERT_TRUE(database->ExecSql("INSERT INTO trace_task_summary(comm,pid,cpu_id) VALUES('worker',42,-1);"));
    dataEngine->threads = {{1, {"process", "worker:42"}}};
    dataEngine->slices = {Slice(1, "sched_switch", 0, GetParam())};
    Parse<FtraceSchedStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT SUM(cs_count) FROM trace_task_summary"), 0);
}

INSTANTIATE_TEST_SUITE_P(FilteredSwitch, FtraceSchedFilteredInputTest,
    testing::Values("{}", R"({"prev_comm":"worker"})", R"({"prev_pid":"42"})",
        R"({"prev_comm":"worker","prev_pid":"abc"})", R"({"prev_comm":"migration/0","prev_pid":"42"})",
        R"({"prev_comm":"swapper/0","prev_pid":"42"})", R"({"prev_comm":"kworker/0:1","prev_pid":"42"})",
        R"({"prev_comm":"missing","prev_pid":"42"})"));

TEST_F(FtraceStatisticsParseUnitTest, SchedUsesUnknownCpuForNonNumericCpuTid) {
    ASSERT_TRUE(database->CreateTraceTaskSummaryTable());
    ASSERT_TRUE(database->ExecSql("INSERT INTO trace_task_summary(comm,pid,cpu_id) VALUES('CPU 3',42,-1);"));
    dataEngine->threads = {{1, {"process", "CPU 3:42"}}};
    dataEngine->slices = {Slice(1, "sched_switch", 0, R"({"prev_comm":"CPU 3","prev_pid":"42","prev_state":"R"})")};
    Parse<FtraceSchedStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE cpu_id=-1"), 1);
}

TEST_F(FtraceStatisticsParseUnitTest, IrqAggregatesByCpuTypeAndName) {
    dataEngine->threads = {{1, {"process", "CPU 0"}}, {2, {"process", "CPU 1"}}};
    dataEngine->slices = {
        Slice(1, "irq", 10, R"({"task":"app:worker:42","name":"timer"})"),
        Slice(1, "irq", 20, R"({"task":"app:worker:42","name":"timer"})"),
        Slice(2, "irq", 40, R"({"task":"app:worker:42","name":"timer"})"),
        Slice(1, "softirq", 50, R"({"task":"app:worker:42","action":"NET_RX"})"),
        Slice(1, "irq", 70, R"({"task":"app:worker:42","name":"disk"})"),
    };
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 4);
    EXPECT_EQ(QueryInteger("SELECT count FROM trace_irq_detail WHERE cpu_id=0 AND irq_name='timer'"), 2);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE cpu_id=0 AND irq_name='timer'"), 30);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE cpu_id=1"), 40);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE irq_type='softirq' AND irq_name='NET_RX'"), 50);
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail WHERE comm='app:worker' AND pid=42"), 4);
}

class FtraceIrqFilteredInputTest : public FtraceStatisticsParseUnitTest,
                                   public testing::WithParamInterface<std::string> {};

TEST_P(FtraceIrqFilteredInputTest, SkipsInvalidIdleOrKernelTask) {
    dataEngine->threads = {{1, {"process", "CPU 0"}}};
    dataEngine->slices = {Slice(1, "irq", 10, GetParam())};
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
}

INSTANTIATE_TEST_SUITE_P(FilteredIrq, FtraceIrqFilteredInputTest,
    testing::Values("{}", R"({"task":"<idle>","name":"timer"})", R"({"task":"worker","name":"timer"})",
        R"({"task":":42","name":"timer"})", R"({"task":"worker:","name":"timer"})",
        R"({"task":"worker:abc","name":"timer"})", R"({"task":"migration/0:42","name":"timer"})",
        R"({"task":"swapper/0:42","name":"timer"})", R"({"task":"kworker/0:1:42","name":"timer"})",
        R"({"task":"worker:42"})"));

TEST_F(FtraceStatisticsParseUnitTest, SoftirqRequiresActionInsteadOfName) {
    dataEngine->slices = {Slice(1, "softirq", 10, R"({"task":"worker:42","name":"timer"})")};
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
}

class FtraceIrqUnknownCpuTest : public FtraceStatisticsParseUnitTest,
                                public testing::WithParamInterface<std::string> {};

TEST_P(FtraceIrqUnknownCpuTest, UsesUnknownCpuForInvalidTid) {
    dataEngine->threads = {{1, {"process", GetParam()}}};
    dataEngine->slices = {Slice(1, "irq", 10, R"({"task":"worker:42","name":"timer"})")};
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE cpu_id=-1"), 10);
}

INSTANTIATE_TEST_SUITE_P(UnknownCpu, FtraceIrqUnknownCpuTest, testing::Values("worker:42", "CPU invalid"));

TEST_F(FtraceStatisticsParseUnitTest, IrqUsesUnknownCpuWhenTrackIsMissing) {
    dataEngine->slices = {Slice(99, "irq", 10, R"({"task":"worker:42","name":"timer"})")};
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE cpu_id=-1"), 10);
}

TEST_F(FtraceStatisticsParseUnitTest, EmptySlicesDoNotQueryThreadsOrInsertStatistics) {
    Parse<FtraceTimeStatisticsParseUnit>();
    Parse<FtraceSchedStatisticsParseUnit>();
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(dataEngine->threadQueryCount, 0U);
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 0);
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofTimeUsesEventIdentityAcrossRenameAndMissingTrack) {
    dataEngine->threads = {{1, {"process", "obsolete:42"}}};
    dataEngine->slices = {
        Slice(1, "Running", 10, R"({"source":"msprof_ftrace","cpu":63,"tid":42,"comm":"worker:long-original"})"),
        Slice(1, "Runnable", 3, R"({"source":"msprof_ftrace","cpu":63,"tid":42,"comm":"worker:renamed"})"),
        Slice(99, "Running", 20, R"({"source":"msprof_ftrace","cpu":"64","tid":"43","comm":"kworker/0:1"})"),
        Slice(100, "Sleeping", 7, R"({"source":"msprof_ftrace","tid":44,"comm":"task-without-cpu"})"),
    };
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 4);
    EXPECT_EQ(
        QueryInteger("SELECT running_ns FROM trace_task_summary WHERE comm='worker:long-original' AND cpu_id=63"), 10);
    EXPECT_EQ(QueryInteger("SELECT runnable_ns FROM trace_task_summary WHERE comm='worker:renamed' AND pid=42"), 3);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE comm='kworker/0:1' AND cpu_id=64"), 20);
    EXPECT_EQ(QueryInteger("SELECT sleeping_ns FROM trace_task_summary WHERE pid=44 AND cpu_id=-1"), 7);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofTimeDoesNotGuessTidFromDisplayLabel) {
    dataEngine->threads = {{1, {"process", "display-only:42"}}};
    dataEngine->slices = {
        Slice(1, "Running", 100, R"({"source":"msprof_ftrace","cpu":1,"comm":"worker"})"),
        Slice(1, "Running", 200, R"({"source":"msprof_ftrace","cpu":1,"tid":"18446744073709551616","comm":"worker"})"),
    };
    Parse<FtraceTimeStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 0);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofSchedUsesCpuAndExplicitPreemptionWithoutTrackLookup) {
    dataEngine->slices = {
        Slice(99, "sched_switch", 0,
            R"({"source":"msprof_ftrace","cpu":63,"prev_comm":"kworker/0:1","prev_pid":42,"prev_state":"R"})"),
        Slice(99, "sched_switch", 0,
            R"({"source":"msprof_ftrace","cpu":63,"prev_comm":"kworker/0:1","prev_pid":42,"prev_state":"R","preempted":false})"),
        Slice(99, "sched_switch", 0,
            R"({"source":"msprof_ftrace","cpu":63,"prev_comm":"kworker/0:1","prev_pid":42,"prev_state":"R+","preempted":true})"),
        Slice(99, "sched_switch", 0,
            R"({"source":"msprof_ftrace","cpu":64,"prev_comm":"swapper","prev_pid":0,"prev_state":"R"})"),
    };
    Parse<FtraceSchedStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 2);
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE pid=42 AND cpu_id=63"), 3);
    EXPECT_EQ(QueryInteger("SELECT cs_involuntary_count FROM trace_task_summary WHERE pid=42 AND cpu_id=63"), 1);
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE pid=0 AND cpu_id=64"), 1);
    EXPECT_EQ(QueryInteger("SELECT cs_involuntary_count FROM trace_task_summary WHERE pid=0"), 0);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofSchedPreservesUnknownCpuInsteadOfUsingTrackLabel) {
    dataEngine->threads = {{1, {"process", "CPU 0"}}};
    dataEngine->slices = {Slice(
        1, "sched_switch", 0, R"({"source":"msprof_ftrace","prev_comm":"worker","prev_pid":42,"prev_state":"R"})")};
    Parse<FtraceSchedStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE pid=42 AND cpu_id=-1"), 1);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofIrqKeepsKernelIdleAndUnknownTaskWithEventCpu) {
    dataEngine->threads = {{1, {"process", "CPU 0"}}};
    dataEngine->slices = {
        Slice(
            1, "softirq", 10, R"({"source":"msprof_ftrace","cpu":63,"tid":42,"comm":"kworker/0:1","action":"NET_RX"})"),
        Slice(1, "softirq", 20,
            R"({"source":"msprof_ftrace","cpu":64,"tid":0,"comm":"swapper","task":"<idle>","action":"NET_RX"})"),
        Slice(1, "softirq", 30,
            R"({"source":"msprof_ftrace","cpu":65,"comm":"<unknown>","task":"<unknown>","action":"NET_RX"})"),
        Slice(1, "softirq", 40, R"({"source":"msprof_ftrace","tid":43,"comm":"worker:long-name","action":"NET_RX"})"),
    };
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 4);
    EXPECT_EQ(QueryInteger("SELECT SUM(count) FROM trace_irq_detail"), 4);
    EXPECT_EQ(QueryInteger("SELECT SUM(time_ns) FROM trace_irq_detail"), 100);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE comm='kworker/0:1' AND cpu_id=63"), 10);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE pid=0 AND cpu_id=64"), 20);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE pid=-1 AND comm='<unknown>' AND cpu_id=65"), 30);
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE pid=43 AND cpu_id=-1"), 40);
    Parse<FtraceIrqAggregationParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT SUM(soft_irq_count) FROM trace_task_summary"), 4);
    EXPECT_EQ(QueryInteger("SELECT SUM(soft_irq_duration) FROM trace_task_summary"), 100);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofIrqWithUnknownActionRemainsInCpuTotals) {
    dataEngine->slices = {Slice(99, "softirq", 10, R"({"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker"})")};
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT count FROM trace_irq_detail WHERE irq_name='<unknown>' AND cpu_id=8"), 1);
}

TEST_F(FtraceStatisticsParseUnitTest, StructuredArgsRejectObjectsArraysAndOverflowWithoutThrowing) {
    dataEngine->threads = {{1, {"process", "worker:42"}}};
    dataEngine->slices = {
        Slice(1, "softirq", 100, R"([])"),
        Slice(1, "softirq", 200, R"({"task":{"nested":42},"name":true})"),
        Slice(1, "softirq", 300, R"({"task":"worker:18446744073709551616","action":"NET_RX"})"),
    };
    Parse<FtraceIrqStatisticsParseUnit>();
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
}

TEST_F(FtraceStatisticsParseUnitTest, TimeUnitReportsFinalFlushFailureAndRetainsPendingRows) {
    TestableStatisticsUnit<FtraceTimeStatisticsParseUnit> unit;
    std::string error;
    ASSERT_TRUE(unit.PreCheck({"statistics-ut"}, database, error));
    ASSERT_EQ(sqlite3_exec(rawDatabase,
                  "CREATE TRIGGER reject_summary BEFORE INSERT ON trace_task_summary "
                  "BEGIN SELECT RAISE(ABORT,'forced statistics failure'); END",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    dataEngine->slices = {Slice(1, "Running", 10, R"({"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker"})")};

    EXPECT_FALSE(unit.HandleParseProcess({"statistics-ut"}, database, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 0);
    ASSERT_EQ(sqlite3_exec(rawDatabase, "DROP TRIGGER reject_summary", nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_TRUE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 10);
}

TEST_F(FtraceStatisticsParseUnitTest, IrqUnitReportsFinalFlushFailureAndRetainsPendingRows) {
    TestableStatisticsUnit<FtraceIrqStatisticsParseUnit> unit;
    std::string error;
    ASSERT_TRUE(unit.PreCheck({"statistics-ut"}, database, error));
    ASSERT_EQ(sqlite3_exec(rawDatabase,
                  "CREATE TRIGGER reject_irq BEFORE INSERT ON trace_irq_detail "
                  "BEGIN SELECT RAISE(ABORT,'forced statistics failure'); END",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    dataEngine->slices = {
        Slice(1, "softirq", 7, R"({"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker","action":"NET_RX"})")};

    EXPECT_FALSE(unit.HandleParseProcess({"statistics-ut"}, database, error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
    ASSERT_EQ(sqlite3_exec(rawDatabase, "DROP TRIGGER reject_irq", nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_TRUE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT time_ns FROM trace_irq_detail WHERE pid=42 AND cpu_id=8"), 7);
}

TEST_F(FtraceStatisticsParseUnitTest, SummaryBatchBoundaryReturnsWriteFailureWithoutDroppingRows) {
    ASSERT_TRUE(database->CreateTraceTaskSummaryTable());
    ASSERT_EQ(sqlite3_exec(rawDatabase, "PRAGMA query_only=ON", nullptr, nullptr, nullptr), SQLITE_OK);
    TraceTaskSummaryData row;
    row.comm = "worker";
    row.cpuId = 8;
    row.runningNs = 1;
    for (int index = 0; index < Timeline::CACHE_SIZE - 1; ++index) {
        row.pid = index;
        ASSERT_TRUE(database->InsertTraceTaskSummary(row));
    }
    row.pid = Timeline::CACHE_SIZE - 1;
    EXPECT_FALSE(database->InsertTraceTaskSummary(row));
    EXPECT_FALSE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), 0);
    ASSERT_EQ(sqlite3_exec(rawDatabase, "PRAGMA query_only=OFF", nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_TRUE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_task_summary"), Timeline::CACHE_SIZE);
}

TEST_F(FtraceStatisticsParseUnitTest, IrqBatchBoundaryReturnsWriteFailureWithoutDroppingRows) {
    ASSERT_TRUE(database->CreateTraceIrqDetailTable());
    ASSERT_EQ(sqlite3_exec(rawDatabase, "PRAGMA query_only=ON", nullptr, nullptr, nullptr), SQLITE_OK);
    TraceIrqDetailData row;
    row.comm = "worker";
    row.cpuId = 8;
    row.irqType = "softirq";
    row.irqName = "NET_RX";
    row.count = 1;
    row.timeNs = 2;
    for (int index = 0; index < Timeline::CACHE_SIZE - 1; ++index) {
        row.pid = index;
        ASSERT_TRUE(database->InsertTraceIrqDetail(row));
    }
    row.pid = Timeline::CACHE_SIZE - 1;
    EXPECT_FALSE(database->InsertTraceIrqDetail(row));
    EXPECT_FALSE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), 0);
    ASSERT_EQ(sqlite3_exec(rawDatabase, "PRAGMA query_only=OFF", nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_TRUE(database->CommitTraceStatisticsChecked());
    EXPECT_EQ(QueryInteger("SELECT COUNT(*) FROM trace_irq_detail"), Timeline::CACHE_SIZE);
}

TEST_F(FtraceStatisticsParseUnitTest, IrqAggregationReportsUpdateFailureInsteadOfSuccessfulZeroStatistics) {
    dataEngine->slices = {
        Slice(1, "softirq", 7, R"({"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker","action":"NET_RX"})")};
    Parse<FtraceIrqStatisticsParseUnit>();
    ASSERT_EQ(sqlite3_exec(rawDatabase,
                  "CREATE TRIGGER reject_irq_update BEFORE UPDATE ON trace_task_summary "
                  "BEGIN SELECT RAISE(ABORT,'forced aggregate failure'); END",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    TestableStatisticsUnit<FtraceIrqAggregationParseUnit> unit;
    std::string error;
    ASSERT_TRUE(unit.PreCheck({"statistics-ut"}, database, error));
    EXPECT_FALSE(unit.HandleParseProcess({"statistics-ut"}, database, error));
    EXPECT_EQ(QueryInteger("SELECT soft_irq_duration FROM trace_task_summary WHERE pid=42"), 0);
    ASSERT_EQ(sqlite3_exec(rawDatabase, "DROP TRIGGER reject_irq_update", nullptr, nullptr, nullptr), SQLITE_OK);
    ASSERT_TRUE(unit.HandleParseProcess({"statistics-ut"}, database, error));
    EXPECT_EQ(QueryInteger("SELECT soft_irq_duration FROM trace_task_summary WHERE pid=42"), 7);
}

TEST_F(FtraceStatisticsParseUnitTest, MsprofMarkedCacheStreamsAllStatisticsWithoutMaterializingDataEngineSlices) {
    CreateStreamingCache();
    ASSERT_EQ(sqlite3_exec(rawDatabase, R"(
INSERT INTO slice VALUES(1,'Running',10,'{"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker"}');
INSERT INTO slice VALUES(2,'sched_switch',0,
 '{"source":"msprof_ftrace","cpu":8,"prev_pid":42,"prev_comm":"worker","prev_state":"R"}');
INSERT INTO slice VALUES(2,'softirq',2,
 '{"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"worker","action":"NET_RX"}');
)",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    dataEngine->slices = {
        Slice(1, "Running", 999, R"({"source":"msprof_ftrace","cpu":8,"tid":42,"comm":"unwanted-fake"})")};
    Parse<FtraceTimeStatisticsParseUnit>();
    Parse<FtraceSchedStatisticsParseUnit>();
    Parse<FtraceIrqStatisticsParseUnit>();
    Parse<FtraceIrqAggregationParseUnit>();
    EXPECT_EQ(dataEngine->sliceQueryCount, 0u);
    EXPECT_EQ(dataEngine->threadQueryCount, 0u);
    EXPECT_EQ(QueryInteger("SELECT running_ns FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 10);
    EXPECT_EQ(QueryInteger("SELECT cs_count FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 1);
    EXPECT_EQ(QueryInteger("SELECT soft_irq_duration FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 2);
}

TEST_F(FtraceStatisticsParseUnitTest, StreamingStatisticsReaderVisitsRowsAndPropagatesSqliteStepErrors) {
    CreateStreamingCache();
    ASSERT_EQ(sqlite3_exec(rawDatabase, R"(
WITH RECURSIVE numbers(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM numbers WHERE n<2048)
INSERT INTO slice SELECT 2,'softirq',n,'{}' FROM numbers;
INSERT INTO slice VALUES(1,'softirq',123,'{}'),(2,'unselected',456,'{}');
)",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    uint64_t count = 0;
    uint64_t duration = 0;
    std::string error;
    ASSERT_TRUE(FtraceStatistics::ForEachSlice(
        *database, "unused", "CPU Scheduling", {"softirq"},
        [&](const CompeteSliceDomain &slice) {
            ++count;
            duration += slice.duration;
            return true;
        },
        error))
        << error;
    EXPECT_EQ(count, 2048u);
    EXPECT_EQ(duration, 2048u * 2049u / 2u);
    count = 0;
    EXPECT_FALSE(FtraceStatistics::ForEachSlice(
        *database, "unused", "CPU Scheduling", {"softirq"},
        [&](const CompeteSliceDomain &) {
            if (++count == 10) {
                sqlite3_interrupt(rawDatabase);
            }
            return true;
        },
        error));
    EXPECT_EQ(count, 10u);
    EXPECT_FALSE(error.empty());
}
} // namespace
} // namespace Dic::Module::FullDb
