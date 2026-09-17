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
#include "DbTraceDataBase.h"
#include "TraceTime.h"
#include "../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;

namespace {
class ThreadStateMockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
  public:
    explicit ThreadStateMockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
    ~ThreadStateMockDatabase() override {
        if (isOpen && db != nullptr) {
            sqlite3_close(db);
            isOpen = false;
        }
    }

    void SetDbPtr(sqlite3 *dbPtr) {
        isOpen = true;
        db = dbPtr;
        path = ":memory:";
        InitStringsCache();
    }
};

void CreateThreadStateTables(sqlite3 *db) {
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE HOST_CORE_PROCESS (id INTEGER, pid INTEGER, name TEXT, start_ts INTEGER, end_ts INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(db,
        "CREATE TABLE HOST_CORE_THREAD (id INTEGER, process_id INTEGER, tid INTEGER, name TEXT, "
        "start_ts INTEGER, end_ts INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(db, "CREATE TABLE HOST_CORE_METRIC_DESC (id INTEGER, name TEXT);");
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE HOST_CORE_METRIC (tid_id INTEGER, desc_id INTEGER, ts INTEGER, value NUMERIC);");
}

void InsertCompleteThreadStateData(sqlite3 *db) {
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO HOST_CORE_PROCESS VALUES (1, 1000, 'Process_1000', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO HOST_CORE_THREAD VALUES (1, 1, 10001, 'Thread_10001', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO HOST_CORE_METRIC_DESC VALUES "
        "(1, 'Active time'), (2, 'Wait time'), (3, 'Preemption time'), (4, 'Unknown time');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO HOST_CORE_METRIC VALUES "
        "(1, 1, 1000000000, 300000000), (1, 2, 1001000000, 100000000),"
        "(1, 3, 1002000000, 50000000), (1, 4, 1003000000, 50000000);");
}
} // namespace

TEST(DbTraceDatabaseThreadStateTest, MetadataContainsThreadStateOnlyWhenMetricExists) {
    std::recursive_mutex testMutex;
    ThreadStateMockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadStateTables(db);
    InsertCompleteThreadStateData(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO HOST_CORE_THREAD VALUES (2, 1, 10002, 'Thread_Without_State', 1000000000, 3000000000);");
    database.SetDbPtr(db);

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    EXPECT_TRUE(database.QueryUnitsMetadata("Threading", metaData));
    ASSERT_EQ(metaData.size(), 1);
    ASSERT_EQ(metaData[0]->children.size(), 1);
    const auto &thread = metaData[0]->children[0];
    EXPECT_EQ(thread->metaData.threadId, "10001");
    ASSERT_EQ(thread->children.size(), 1);
    EXPECT_EQ(thread->children[0]->metaData.metricGroup, "thread_state");
    EXPECT_EQ(thread->children[0]->metaData.bucketWidthNs, 500000000);
}

TEST(DbTraceDatabaseThreadStateTest, CounterNormalizesFourStates) {
    std::recursive_mutex testMutex;
    ThreadStateMockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadStateTables(db);
    InsertCompleteThreadStateData(db);
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "THREADING_ANALYSIS";
    params.metricGroup = "thread_state";
    params.pid = "1000";
    params.threadId = "10001";
    params.startTime = 0;
    params.endTime = 1000000000;
    std::vector<Dic::Protocol::UnitCounterData> data;
    EXPECT_TRUE(database.QueryUnitCounter(params, 1000000000, data));
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(data[0].timestamp, 0);
    EXPECT_NE(data[0].valueJsonStr.find("\"Active\":60.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"Sync Wait\":20.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"Preemption\":10.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"Unknown\":10.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"activeSeconds\":0.300000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"bucketWidthNs\":500000000"), std::string::npos);
}

TEST(DbTraceDatabaseThreadStateTest, CounterSkipsIncompleteBucket) {
    std::recursive_mutex testMutex;
    ThreadStateMockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadStateTables(db);
    InsertCompleteThreadStateData(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO HOST_CORE_METRIC VALUES "
        "(1, 1, 1500000000, 200000000), (1, 2, 1501000000, 100000000),"
        "(1, 3, 1502000000, 100000000);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "THREADING_ANALYSIS";
    params.metricGroup = "thread_state";
    params.pid = "1000";
    params.threadId = "10001";
    params.startTime = 0;
    params.endTime = 2000000000;
    std::vector<Dic::Protocol::UnitCounterData> data;
    EXPECT_TRUE(database.QueryUnitCounter(params, 1000000000, data));
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(data[0].timestamp, 0);
}

class ThreadStateReviewTest : public testing::Test {
  protected:
    std::recursive_mutex mutex;
    ThreadStateMockDatabase database{mutex};
    sqlite3 *db = nullptr;
    const uint64_t origin = 1000000000;

    void SetUp() override {
        Dic::Module::Timeline::TraceTime::Instance().Reset();
        Dic::Module::Timeline::TraceTime::Instance().UpdateTime(origin, origin + 3000000000);
        DatabaseTestCaseMockUtil::OpenDB(db);
        CreateThreadStateTables(db);
        InsertCompleteThreadStateData(db);
        database.SetDbPtr(db);
    }

    void TearDown() override { Dic::Module::Timeline::TraceTime::Instance().Reset(); }

    void Exec(const std::string &sql) {
        ASSERT_EQ(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
    }

    void AddBucket(int thread, uint64_t ts, uint64_t duration) {
        for (int state = 1; state <= 4; ++state) {
            Exec("INSERT INTO HOST_CORE_METRIC VALUES (" + std::to_string(thread) + "," + std::to_string(state) + "," +
                std::to_string(ts) + "," + std::to_string(duration / 4) + ")");
        }
    }

    std::vector<Dic::Protocol::UnitCounterData> Query(const std::string &tid = "10001", uint64_t start = 1000000000) {
        Dic::Protocol::UnitCounterParams params;
        params.metaType = "THREADING_ANALYSIS";
        params.metricGroup = "thread_state";
        params.pid = "1000";
        params.threadId = tid;
        params.endTime = 3000000000;
        std::vector<Dic::Protocol::UnitCounterData> data;
        EXPECT_TRUE(database.QueryUnitCounter(params, start, data));
        return data;
    }
};

TEST_F(ThreadStateReviewTest, WidthIsPerThreadAndDoesNotUsePartialFirstBucket) {
    Exec("UPDATE HOST_CORE_METRIC SET value = value / 10");
    AddBucket(1, 1500000000, 500000000);
    AddBucket(1, 2000000000, 500000000);
    Exec("INSERT INTO HOST_CORE_THREAD VALUES (2, 1, 10002, 'Worker', 1000000000, 3000000000)");
    AddBucket(2, 1000000000, 100000000);
    AddBucket(2, 1100000000, 100000000);
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("Threading", metadata));
    ASSERT_EQ(metadata.size(), 1);
    ASSERT_EQ(metadata[0]->children.size(), 2);
    EXPECT_EQ(metadata[0]->children[0]->children[0]->metaData.bucketWidthNs, 500000000);
    EXPECT_EQ(metadata[0]->children[1]->children[0]->metaData.bucketWidthNs, 100000000);
    for (const auto &sample : Query()) {
        EXPECT_NE(sample.valueJsonStr.find("\"bucketWidthNs\":500000000"), std::string::npos);
    }
    for (const auto &sample : Query("10002")) {
        EXPECT_NE(sample.valueJsonStr.find("\"bucketWidthNs\":100000000"), std::string::npos);
    }
}

TEST_F(ThreadStateReviewTest, MissingSamplingIntervalDoesNotEnlargeWidth) {
    AddBucket(1, 2000000000, 500000000);
    const auto samples = Query();
    ASSERT_EQ(samples.size(), 2);
    EXPECT_EQ(samples[1].timestamp, 1000000000);
    EXPECT_NE(samples[0].valueJsonStr.find("\"bucketWidthNs\":500000000"), std::string::npos);
}

TEST_F(ThreadStateReviewTest, IncompleteFirstBucketDoesNotAffectLaterValidBuckets) {
    Exec("DELETE FROM HOST_CORE_METRIC WHERE desc_id = 4");
    AddBucket(1, 1500000000, 500000000);
    AddBucket(1, 2000000000, 500000000);
    const auto samples = Query();
    ASSERT_EQ(samples.size(), 2);
    EXPECT_EQ(samples[0].timestamp, 500000000);
    EXPECT_NE(samples[0].valueJsonStr.find("\"bucketWidthNs\":500000000"), std::string::npos);
}

TEST_F(ThreadStateReviewTest, MixedNullBucketIsHiddenButLlcRemainsAvailable) {
    Exec("INSERT INTO HOST_CORE_METRIC VALUES (1, 1, 1000000000, NULL);"
         "INSERT INTO HOST_CORE_METRIC_DESC VALUES (5, 'LLC Hits');"
         "INSERT INTO HOST_CORE_METRIC VALUES (1, 5, 1000000000, 100)");
    EXPECT_TRUE(Query().empty());
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("Threading", metadata));
    ASSERT_EQ(metadata.size(), 1);
    const auto &children = metadata[0]->children[0]->children;
    ASSERT_EQ(children.size(), 1);
    EXPECT_EQ(children[0]->metaData.metricGroup, "llc_cache");
}

TEST_F(ThreadStateReviewTest, FourStatesAcrossDifferentBucketsDoNotCreateLane) {
    Exec("UPDATE HOST_CORE_METRIC SET ts = ts + desc_id * 100000000");
    EXPECT_TRUE(Query().empty());
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("Threading", metadata));
    EXPECT_TRUE(metadata.empty());
}

TEST_F(ThreadStateReviewTest, ZeroAndNegativeBucketsDoNotCreateLane) {
    for (const auto value : {0, -1}) {
        Exec("UPDATE HOST_CORE_METRIC SET value = " + std::to_string(value));
        EXPECT_TRUE(Query().empty());
        std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
        ASSERT_TRUE(database.QueryUnitsMetadata("Threading", metadata));
        EXPECT_TRUE(metadata.empty());
    }
}

TEST_F(ThreadStateReviewTest, MetadataAndCounterUseSessionOriginNotThreadStart) {
    const uint64_t sessionOrigin = origin + 7000000;
    Dic::Module::Timeline::TraceTime::Instance().Reset();
    Dic::Module::Timeline::TraceTime::Instance().UpdateTime(sessionOrigin, sessionOrigin + 3000000000);
    Exec("UPDATE HOST_CORE_METRIC SET ts = ts + 17000000");
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("Threading", metadata));
    ASSERT_EQ(metadata.size(), 1);
    const auto samples = Query("10001", sessionOrigin);
    ASSERT_EQ(samples.size(), 1);
    EXPECT_EQ(samples[0].timestamp, 10000000);
    EXPECT_EQ(metadata[0]->children[0]->children[0]->metaData.bucketWidthNs, 500000000);
}

TEST_F(ThreadStateReviewTest, NoHostCoreTablesPreservesStandardHostLaneAndTime) {
    Exec("DROP TABLE HOST_CORE_METRIC; DROP TABLE HOST_CORE_METRIC_DESC;"
         "DROP TABLE HOST_CORE_THREAD; DROP TABLE HOST_CORE_PROCESS");
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, {TableName::DB_PYTORCH_API});
    Exec("INSERT INTO PYTORCH_API(startNs, endNs, globalTid, name, depth) "
         "VALUES (1000000000, 3000000000, 13073176077431292, 1, 0)");
    EXPECT_FALSE(database.IsThreadingAnalysisDatabase());
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    database.QueryUnitsMetadata("Standard", metadata);
    ASSERT_EQ(metadata.size(), 1);
    ASSERT_EQ(metadata[0]->children.size(), 1);
    ASSERT_EQ(metadata[0]->children[0]->children.size(), 1);
    EXPECT_EQ(metadata[0]->children[0]->children[0]->metaData.threadName, "PyTorch");
    uint64_t start = 900000000;
    uint64_t end = 4000000000;
    EXPECT_TRUE(database.QueryExtremumTimestamp(start, end));
    EXPECT_EQ(start, 900000000);
    EXPECT_EQ(end, 4000000000);
}

TEST_F(ThreadStateReviewTest, StateIsAppendedAfterExistingHostThreadChildren) {
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, {TableName::DB_PYTORCH_API});
    Exec("INSERT INTO PYTORCH_API(startNs, endNs, globalTid, name, depth) "
         "VALUES (1000000000, 3000000000, 13073176077431292, 1, 0);"
         "ALTER TABLE HOST_CORE_THREAD ADD COLUMN global_tid INTEGER;"
         "UPDATE HOST_CORE_THREAD SET global_tid = 13073176077431292");
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metadata;
    ASSERT_TRUE(database.QueryUnitsMetadata("Composite", metadata));
    ASSERT_EQ(metadata.size(), 1);
    ASSERT_EQ(metadata[0]->children.size(), 1);
    const auto &children = metadata[0]->children[0]->children;
    ASSERT_EQ(children.size(), 2);
    EXPECT_EQ(children[0]->metaData.threadName, "PyTorch");
    EXPECT_EQ(children[1]->metaData.metricGroup, "thread_state");
}
