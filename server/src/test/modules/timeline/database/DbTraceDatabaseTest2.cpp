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
#include "DbTraceDataBase.h"
#include "CollectionTimeService.h"
#include "TraceDatabaseHelper.h"
#include "NpuInfoRepoMock.h"
#include "../../../DatabaseTestCaseMockUtil.h"
using namespace Dic::Global::PROFILER::MockUtil;
class DbTraceDatabaseTest2 : public ::testing::Test {
  protected:
    const std::string pytorchDataSql =
        "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", "
        "\"connectionId\", \"name\", \"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", "
        "\"callchainId\", \"depth\") VALUES ('1718180918997274130', '1718180918997289000', 8785587534247538, 0, "
        "268435456, NULL, NULL, NULL, NULL, NULL, 8);";
    const std::string cannDataSql =
        "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", "
        "\"globalTid\", \"connectionId\", \"name\", \"depth\") VALUES (1729478236911261506, "
        "1729478236911265550, 20000, 1237912654215057, 250011, 5413, 0);";
    const std::string mstxDataSql =
        "INSERT INTO \"main\".\"MSTX_EVENTS\" (\"startNs\", \"endNs\", \"eventType\", "
        "\"rangeId\", \"category\", \"message\", \"globalTid\", \"endGlobalTid\", \"domainId\", "
        "\"connectionId\", \"depth\") VALUES (947741767895850870, 947741768895903230, 2, "
        "4294967295, 4294967295, 8, 16884049020452276, 16884049020452276, 65535, 4000000001, 0);";
    const std::string numeApiDataSql = "INSERT INTO \"main\".\"ENUM_API_TYPE\" (\"id\", \"name\") "
                                       "VALUES (20000, 'acl');";
};
class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
  public:
    explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
    ~MockDatabase() override {
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
        return;
    }
    void SetMetaVersion(const std::string &version) { metaVersion = version; }
};

namespace {
void CreateThreadingAnalysisTables(sqlite3 *db) {
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE p_process (id INTEGER, pid INTEGER, name TEXT, start_ts INTEGER, end_ts INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(db,
        "CREATE TABLE p_thread (id INTEGER, process_id INTEGER, tid INTEGER, name TEXT, "
        "start_ts INTEGER, end_ts INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(db, "CREATE TABLE p_core_metric_desc (id INTEGER, name TEXT);");
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE p_core_metric (tid_id INTEGER, desc_id INTEGER, ts INTEGER, value NUMERIC);");
}

void InsertThreadingAnalysisData(sqlite3 *db) {
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_process VALUES (1, 1000, 'Process_1000', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_thread VALUES "
        "(1, 1, 10001, 'Thread_10001', 1000000000, 3000000000),"
        "(2, 1, 10002, 'Thread_10002', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_core_metric_desc VALUES "
        "(5, 'LLC Hits'), (6, 'LLC Misses');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_core_metric VALUES "
        "(1, 5, 1000000000, 900), (1, 6, 1000000000, 100),"
        "(1, 5, 1500000000, 800), (1, 6, 1500000000, 200),"
        "(2, 5, 1004000000, 720), (2, 6, 1004000000, 80);");
}
} // namespace

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisSchemaRangeAndMetadata) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    InsertThreadingAnalysisData(db);
    database.SetDbPtr(db);

    EXPECT_TRUE(database.IsThreadingAnalysisDatabase());
    uint64_t minTimestamp = 0;
    uint64_t maxTimestamp = 0;
    EXPECT_TRUE(database.QueryExtremumTimestamp(minTimestamp, maxTimestamp));
    EXPECT_EQ(minTimestamp, 1000000000);
    EXPECT_EQ(maxTimestamp, 3000000000);

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    EXPECT_TRUE(database.QueryUnitsMetadata("Threading", metaData));
    ASSERT_EQ(metaData.size(), 1);
    EXPECT_EQ(metaData[0]->type, "process");
    EXPECT_EQ(metaData[0]->metaData.metaType, "THREADING_ANALYSIS");
    EXPECT_EQ(metaData[0]->metaData.processId, "1000");
    EXPECT_EQ(metaData[0]->metaData.processName, "Process_1000");
    ASSERT_EQ(metaData[0]->children.size(), 2);
    EXPECT_EQ(metaData[0]->children[0]->type, "thread");
    EXPECT_EQ(metaData[0]->children[0]->metaData.threadId, "10001");
    EXPECT_EQ(metaData[0]->children[0]->metaData.threadName, "Thread_10001");
    ASSERT_EQ(metaData[0]->children[0]->children.size(), 1);
    const auto &llcCache = metaData[0]->children[0]->children[0];
    EXPECT_EQ(llcCache->type, "counter");
    EXPECT_EQ(llcCache->metaData.threadName, "LLC Cache");
    EXPECT_EQ(llcCache->metaData.metricGroup, "llc_cache");
    EXPECT_EQ(llcCache->metaData.threadId, "10001");
    EXPECT_EQ(llcCache->metaData.bucketWidthNs, 500000000);
    ASSERT_EQ(llcCache->metaData.dataType.size(), 2);
    EXPECT_EQ(llcCache->metaData.dataType[0], "LLC Hits");
    EXPECT_EQ(llcCache->metaData.dataType[1], "LLC Misses");
}

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisMetadataOmitsThreadsWithoutLlcSamples) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_process VALUES "
        "(1, 1000, 'Process_1000', 1000000000, 3000000000),"
        "(2, 2000, 'Process_2000', 1000000000, 3000000000);"
        "INSERT INTO p_thread VALUES "
        "(1, 1, 10001, 'Llc_Thread', 1000000000, 3000000000),"
        "(2, 1, 10002, 'Empty_Thread', 1000000000, 3000000000),"
        "(3, 2, 20001, 'Empty_Process_Thread', 1000000000, 3000000000);"
        "INSERT INTO p_core_metric_desc VALUES (1, 'LLC Hits'), (2, 'LLC Misses');"
        "INSERT INTO p_core_metric VALUES "
        "(1, 1, 1000000000, 80), (1, 2, 1000000000, 20);");
    database.SetDbPtr(db);

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    EXPECT_TRUE(database.QueryUnitsMetadata("Threading", metaData));
    ASSERT_EQ(metaData.size(), 1);
    EXPECT_EQ(metaData[0]->metaData.processId, "1000");
    ASSERT_EQ(metaData[0]->children.size(), 1);
    EXPECT_EQ(metaData[0]->children[0]->metaData.threadId, "10001");
    ASSERT_EQ(metaData[0]->children[0]->children.size(), 1);
    EXPECT_EQ(metaData[0]->children[0]->children[0]->metaData.metricGroup, "llc_cache");
}

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisLlcUsesDeclaredHostGlobalTidAsParent) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, {TableName::DB_PYTORCH_API});
    DatabaseTestCaseMockUtil::InsertData(db,
        "ALTER TABLE p_thread ADD COLUMN global_tid INTEGER;"
        "INSERT INTO PYTORCH_API(startNs, endNs, globalTid, name, depth) VALUES "
        "(1000000000, 1100000000, 13073176077431292, 1, 0);"
        "INSERT INTO p_process VALUES (1, 3043836, 'Process 3043836', 1000000000, 3000000000);"
        "INSERT INTO p_thread VALUES "
        "(1, 1, 3043836, 'Thread 3043836', 1000000000, 3000000000, 13073176077431292);"
        "INSERT INTO p_core_metric_desc VALUES (1, 'LLC Hits'), (2, 'LLC Misses');"
        "INSERT INTO p_core_metric VALUES "
        "(1, 1, 1000000000, 900), (1, 2, 1000000000, 100);");
    database.SetDbPtr(db);

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    EXPECT_TRUE(database.QueryUnitsMetadata("Combined", metaData));
    ASSERT_EQ(metaData.size(), 1);
    EXPECT_EQ(metaData[0]->metaData.metaType, "PROCESS");
    ASSERT_EQ(metaData[0]->children.size(), 1);
    const auto &hostThread = metaData[0]->children[0];
    EXPECT_EQ(hostThread->metaData.processId, "13073176077431292");
    EXPECT_EQ(hostThread->metaData.threadId, "3043836");
    ASSERT_EQ(hostThread->children.size(), 2);
    EXPECT_EQ(hostThread->children[0]->metaData.threadName, "PyTorch");
    EXPECT_EQ(hostThread->children[1]->metaData.metricGroup, "llc_cache");
}

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisLlcBucketWidthUsesAnotherThreadWithMultipleSamples) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_process VALUES (1, 1000, 'Process_1000', 1000000000, 3000000000);"
        "INSERT INTO p_thread VALUES "
        "(1, 1, 10001, 'Single_Sample', 1000000000, 3000000000),"
        "(2, 1, 10002, 'Multiple_Samples', 1000000000, 3000000000);"
        "INSERT INTO p_core_metric_desc VALUES (5, 'LLC Hits'), (6, 'LLC Misses');"
        "INSERT INTO p_core_metric VALUES "
        "(1, 5, 1000000000, 900), (1, 6, 1000000000, 100),"
        "(2, 5, 1100000000, 800), (2, 6, 1100000000, 200),"
        "(2, 5, 1600000000, 700), (2, 6, 1600000000, 300);");
    database.SetDbPtr(db);

    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    EXPECT_TRUE(database.QueryUnitsMetadata("Threading", metaData));
    ASSERT_EQ(metaData.size(), 1);
    ASSERT_EQ(metaData[0]->children.size(), 2);
    EXPECT_EQ(metaData[0]->children[0]->children[0]->metaData.bucketWidthNs, 500000000);
    EXPECT_EQ(metaData[0]->children[1]->children[0]->metaData.bucketWidthNs, 500000000);
}

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisLlcCounterNormalization) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    InsertThreadingAnalysisData(db);
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "THREADING_ANALYSIS";
    params.metricGroup = "llc_cache";
    params.pid = "1000";
    params.threadId = "10001";
    params.startTime = 0;
    params.endTime = 400000000;
    std::vector<Dic::Protocol::UnitCounterData> data;
    EXPECT_TRUE(database.QueryUnitCounter(params, 1000000000, data));
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(data[0].timestamp, 0);
    EXPECT_NE(data[0].valueJsonStr.find("\"hits\":900.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"misses\":100.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"totalAccesses\":1000.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"hitRate\":90.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"missRate\":10.000000"), std::string::npos);
    EXPECT_NE(data[0].valueJsonStr.find("\"bucketWidthNs\":500000000"), std::string::npos);
    EXPECT_EQ(data[0].valueJsonStr.find("Active"), std::string::npos);
}

TEST_F(DbTraceDatabaseTest2, TestThreadingAnalysisCounterIsolatedByProcessAndThread) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    CreateThreadingAnalysisTables(db);
    InsertThreadingAnalysisData(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_process VALUES (2, 2000, 'Process_2000', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_thread VALUES (3, 2, 10001, 'Reused_Thread_10001', 1000000000, 3000000000);");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO p_core_metric VALUES "
        "(3, 5, 1000000000, 1), (3, 6, 1000000000, 999);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "THREADING_ANALYSIS";
    params.metricGroup = "llc_cache";
    params.pid = "1000";
    params.threadId = "10001";
    params.startTime = 0;
    params.endTime = 400000000;
    std::vector<Dic::Protocol::UnitCounterData> llcData;
    EXPECT_TRUE(database.QueryUnitCounter(params, 1000000000, llcData));
    ASSERT_EQ(llcData.size(), 1);
    EXPECT_NE(llcData[0].valueJsonStr.find("\"hits\":900.000000"), std::string::npos);
    EXPECT_NE(llcData[0].valueJsonStr.find("\"misses\":100.000000"), std::string::npos);
}
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenTaskNotExist) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), 0);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenStreamTrackExist) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{
        TableName::DB_COMMUNICATION_OP, TableName::DB_TASK, TableName::DB_COMMUNICATION_TASK_INFO};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 2;
    const uint8_t expectHardWareCount = 1;
    const std::string expectStreamName = "Stream 2";
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[0]->children.size(), expectHardWareCount);
    EXPECT_EQ(metaData[0]->children[0]->metaData.threadName, expectStreamName);
    EXPECT_EQ(metaData[0]->children[0]->metaData.cardId, fileId);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenStreamExistMSTXWithoutDomain) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_MSTX_EVENTS, TableName::DB_TASK};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924992, 7, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 65535, 4000000001, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t expectHardWareCount = 2;
    const uint64_t first = 1;
    database.QueryUnitsMetadata(fileId, metaData);
    ASSERT_EQ(metaData.size(), expectProcessCount);
    ASSERT_EQ(metaData[first]->children.size(), expectHardWareCount);
    EXPECT_EQ(metaData[first]->children[0]->metaData.threadName, "Stream 2 MSTX");
    EXPECT_EQ(metaData[first]->children[0]->metaData.cardId, fileId);
    EXPECT_EQ(metaData[first]->children[1]->metaData.threadName, "Stream 2");
    EXPECT_EQ(metaData[first]->children[1]->metaData.cardId, fileId);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenStreamExistMSTXWithDomain) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_MSTX_EVENTS, TableName::DB_TASK, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924992, 7, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 239, 4000000001, 0);";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (239, 'compute'), (447, 'start')";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t expectHardWareCount = 2;
    const uint64_t first = 1;
    database.QueryUnitsMetadata(fileId, metaData);
    ASSERT_EQ(metaData.size(), expectProcessCount);
    ASSERT_EQ(metaData[first]->children.size(), expectHardWareCount);
    EXPECT_EQ(metaData[first]->children[0]->metaData.threadName, "Stream 2 MSTX domain compute");
    EXPECT_EQ(metaData[first]->children[0]->metaData.cardId, fileId);
    EXPECT_EQ(metaData[first]->children[1]->metaData.threadName, "Stream 2");
    EXPECT_EQ(metaData[first]->children[1]->metaData.cardId, fileId);
}

/**
 * 只存在task表不存在commucation相关表就一个泳道都没有
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenTaskExistCommucationNotExist) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 1;
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
}

/**
 * 查询hccl的plane泳道
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenPlaneTrackExist) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_TASK,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 21412, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string commucationInfoInsertSql =
        "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", \"planeId\", "
        "\"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", \"transportType\", \"size\", "
        "\"dataType\", \"linkType\", \"opId\") VALUES (6, 21412, 7, 0, 8, 9223372036854775807, 65535, 0, 0, 0, 4, "
        "65535, 0, 1);";
    std::string commucationOpInsertSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (6, 1729773871230644118, 1729773871230661178, 144529, 8, 1, 0, 0, 4, 9, 1, 10, 726280, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, commucationInfoInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, commucationOpInsertSql);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t first = 0;
    const uint8_t second = 1;
    const uint8_t three = 2;
    const std::string expectGroupName = "Group 0 Communication";
    const std::string expectPlaneName = "Plane 0";
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[second]->children.size(), three);
    EXPECT_EQ(metaData[second]->children[first]->metaData.threadName, expectGroupName);
    EXPECT_EQ(metaData[second]->children[second]->metaData.threadName, expectPlaneName);
}

/**
 * 查询hccl的plane泳道(deviceId唯一)
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenDeviceUnique) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_TASK,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 0, 4294967295, 21412, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string commucationOpInsertSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (6, 1729773871230644118, 1729773871230661178, 144529, 8, 1, 0, 0, 4, 9, 1, 10, 726280, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, commucationOpInsertSql);
    MockNpuInfoRepoFunc();
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "0";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t zero = 0;
    const uint8_t one = 1;
    const std::string expectGroupName = "Group 0 Communication";
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[one]->children.size(), one);
    EXPECT_EQ(metaData[one]->children[zero]->metaData.threadName, expectGroupName);
    RestoreRepoFunc();
}

/**
 * 过滤plane为4294967295的泳道
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenPlaneTrackIsWrong) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_TASK,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 21412, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string commucationInfoInsertSql =
        "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", \"planeId\", "
        "\"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", \"transportType\", \"size\", "
        "\"dataType\", \"linkType\", \"opId\") VALUES (6, 21412, 7, 4294967295, 8, 9223372036854775807, 65535, 0, 0, "
        "0, 4, "
        "65535, 0, 1);";
    std::string commucationOpInsertSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (6, 1729773871230644118, 1729773871230661178, 144529, 8, 1, 0, 0, 4, 9, 1, 10, 726280, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, commucationInfoInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, commucationOpInsertSql);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t first = 0;
    const uint8_t second = 1;
    const std::string expectGroupName = "Group 0 Communication";
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[second]->children.size(), second);
    EXPECT_EQ(metaData[second]->children[first]->metaData.threadName, expectGroupName);
}

/**
 * 查询hccl的plane泳道
 * metaVersion 1.1.0
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenPlaneTrackExistVersion_1_1_0) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    database.SetMetaVersion("1.1.0");
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_TASK,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 21412, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string commucationInfoInsertSql =
        "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", \"planeId\", "
        "\"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", \"transportType\", \"size\", "
        "\"dataType\", \"linkType\", \"opId\") VALUES (6, 21412, 7, 0, 8, 9223372036854775807, 65535, 0, 0, 0, 4, "
        "65535, 0, 1);";
    std::string commucationOpInsertSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (6, 1729773871230644118, 1729773871230661178, 144529, 8, 1, 0, 0, 4, 9, 1, 10, 726280, 7);";
    const std::string groupNameValue = "90.90.97.96%enp194s0f0_60008_8_1735556595505601";
    const std::string stringIdsInsertSql = "INSERT INTO \"STRING_IDS\" (\"id\",\"value\") "
                                           "VALUES (8, '90.90.97.96%enp194s0f0_60008_8_1735556595505601');";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, commucationInfoInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, commucationOpInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsInsertSql);
    database.SetDbPtr(db);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t two = 2;
    const std::string expectPlaneName = "Plane 0";
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[1]->children.size(), two);
    EXPECT_EQ(metaData[1]->children[0]->metaData.groupNameValue, groupNameValue);
    EXPECT_EQ(metaData[1]->children[1]->metaData.threadName, expectPlaneName);
    EXPECT_EQ(metaData[1]->children[1]->metaData.groupNameValue, "");
}

/**
 * 过滤plane为4294967295的泳道
 * metaVersion 1.1.0
 */
TEST_F(DbTraceDatabaseTest2, TestQueryUnitsMetadataWhenPlaneTrackIsWrongVersion_1_1_0) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    database.SetMetaVersion("1.1.0");
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_TASK,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 21412, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string commucationInfoInsertSql =
        "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", \"planeId\", "
        "\"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", \"transportType\", \"size\", "
        "\"dataType\", \"linkType\", \"opId\") VALUES (6, 21412, 7, 4294967295, 8, 9223372036854775807, 65535, 0, 0, "
        "0, 4, "
        "65535, 0, 1);";
    std::string commucationOpInsertSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (6, 1729773871230644118, 1729773871230661178, 144529, 8, 1, 0, 0, 4, 9, 1, 10, 726280, 7);";
    const std::string groupNameValue = "90.90.97.96%enp194s0f0_60008_8_1735556595505601";
    const std::string stringIdsInsertSql = "INSERT INTO \"STRING_IDS\" (\"id\",\"value\") "
                                           "VALUES (8, '90.90.97.96%enp194s0f0_60008_8_1735556595505601');";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, commucationInfoInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, commucationOpInsertSql);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsInsertSql);
    database.SetDbPtr(db);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    std::string fileId = "7";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    const uint8_t expectProcessCount = 3;
    const uint8_t first = 0;
    const uint8_t second = 1;
    database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[second]->children.size(), second);
    EXPECT_EQ(metaData[second]->children[first]->metaData.groupNameValue, groupNameValue);
}

/**
 * 测试pytorch，cann，mstx都存在的情况下的泳道信息
 */
TEST_F(DbTraceDatabaseTest2, TestQueryHostMetadataWhenAllHostExistThenhaveThreeTrack) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_PYTORCH_API, TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS,
        TableName::DB_ENUM_API_TYPE, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, cannDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, mstxDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, numeApiDataSql);
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    database.QueryHostMetadata("2", metaData);
    const uint64_t expectSize = 3;
    const uint64_t first = 0;
    const uint64_t second = 1;
    const uint64_t third = 2;
    EXPECT_EQ(metaData.size(), expectSize);
    EXPECT_EQ(metaData[first]->metaData.processName, "Process 288224");
    EXPECT_EQ(metaData[first]->children[first]->metaData.metaType, "CANN_API");
    EXPECT_EQ(metaData[first]->children[first]->metaData.threadId, "292753");
    EXPECT_EQ(metaData[first]->children[first]->children[first]->metaData.threadId, "");
    EXPECT_EQ(metaData[first]->children[first]->children[first]->metaData.processId, "1237912654215057");
    EXPECT_EQ(metaData[second]->metaData.processName, "Process 3931124");
    EXPECT_EQ(metaData[second]->children[first]->metaData.metaType, "CANN_API");
    EXPECT_EQ(metaData[second]->children[first]->metaData.threadId, "3931572");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->metaData.threadId, "3931572");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->metaData.processId, "16884049020452276");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->children[first]->metaData.metaType, "MSTX_EVENTS");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->children[first]->metaData.threadId, "65535");
    EXPECT_EQ(
        metaData[second]->children[first]->children[first]->children[first]->metaData.processId, "16884049020452276");
    EXPECT_EQ(metaData[third]->metaData.processName, "Process 2045554");
    EXPECT_EQ(metaData[third]->children[first]->metaData.metaType, "CANN_API");
    EXPECT_EQ(metaData[third]->children[first]->metaData.threadId, "2045554");
    EXPECT_EQ(metaData[third]->children[first]->children[first]->metaData.threadId, "pytorch");
    EXPECT_EQ(metaData[third]->children[first]->children[first]->metaData.processId, "8785587534247538");
}

/**
 * 测试cann，mstx都存在但pytorch不存在的情况下的泳道信息
 */
TEST_F(DbTraceDatabaseTest2, TestQueryHostMetadataWhenPytorchNotExistThenhaveTwoTrack) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{
        TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_ENUM_API_TYPE, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    DatabaseTestCaseMockUtil::InsertData(db, cannDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, mstxDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, numeApiDataSql);
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    database.QueryHostMetadata("hhh", metaData);
    const uint64_t expectSize = 2;
    const uint64_t first = 0;
    const uint64_t second = 1;
    EXPECT_EQ(metaData.size(), expectSize);
    EXPECT_EQ(metaData[first]->metaData.processName, "Process 288224");
    EXPECT_EQ(metaData[first]->children[first]->metaData.metaType, "CANN_API");
    EXPECT_EQ(metaData[first]->children[first]->metaData.threadId, "292753");
    EXPECT_EQ(metaData[first]->children[first]->children[first]->metaData.threadId, "");
    EXPECT_EQ(metaData[first]->children[first]->children[first]->metaData.processId, "1237912654215057");
    EXPECT_EQ(metaData[second]->metaData.processName, "Process 3931124");
    EXPECT_EQ(metaData[second]->children[first]->metaData.metaType, "CANN_API");
    EXPECT_EQ(metaData[second]->children[first]->metaData.threadId, "3931572");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->metaData.threadId, "3931572");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->metaData.processId, "16884049020452276");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->children[first]->metaData.metaType, "MSTX_EVENTS");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->children[first]->metaData.threadId, "65535");
    EXPECT_EQ(
        metaData[second]->children[first]->children[first]->children[first]->metaData.processId, "16884049020452276");
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenDbNotOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    const Dic::Protocol::SystemViewParams requestParams{};
    Dic::Protocol::SystemViewBody responseBody;
    const uint64_t minTimestamp = 0;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenSqlInject) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "####@";
    Dic::Protocol::SystemViewBody responseBody;
    const uint64_t minTimestamp = 0;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenHardware) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMPUTE_TASK_INFO, TableName::DB_STRING_IDS,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO STRING_IDS (id, value) VALUES (1, 'NormalTask'), (2, 'SharedTask');"
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, taskType) VALUES "
        "(10, 30, 0, 100, 1, 1), (40, 80, 0, 200, 2, 2), "
        "(90, 120, 0, 300, 3, 2), (130, 140, 0, NULL, 4, 1);"
        "INSERT INTO MSTX_EVENTS (connectionId) VALUES (200), (NULL);");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.order = "ascend";
    requestParams.current = 1;
    requestParams.pageSize = 10;
    requestParams.layer = "Ascend Hardware";
    requestParams.rankId = "0";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
    ASSERT_EQ(responseBody.systemViewDetail.size(), 2);
    EXPECT_EQ(responseBody.systemViewDetail[0].name, "NormalTask");
    EXPECT_EQ(responseBody.systemViewDetail[0].numberCalls, 2);
    EXPECT_DOUBLE_EQ(responseBody.systemViewDetail[0].totalTime, 0.03);
    EXPECT_DOUBLE_EQ(responseBody.systemViewDetail[0].time, 50.0);
    EXPECT_EQ(responseBody.systemViewDetail[1].name, "SharedTask");
    EXPECT_EQ(responseBody.systemViewDetail[1].numberCalls, 1);
    EXPECT_DOUBLE_EQ(responseBody.systemViewDetail[1].totalTime, 0.03);
    EXPECT_DOUBLE_EQ(responseBody.systemViewDetail[1].time, 50.0);
    EXPECT_EQ(responseBody.total, 2);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenHardwareKeepsInvalidConnectionId) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMPUTE_TASK_INFO, TableName::DB_STRING_IDS,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO STRING_IDS (id, value) VALUES (1, 'InvalidConnectionTask');"
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, taskType) VALUES "
        "(10, 30, 0, 4294967295, 1, 1);"
        "INSERT INTO MSTX_EVENTS (connectionId) VALUES (4294967295);");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.order = "ascend";
    requestParams.current = 1;
    requestParams.pageSize = 10;
    requestParams.layer = "Ascend Hardware";
    requestParams.rankId = "0";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
    ASSERT_EQ(responseBody.systemViewDetail.size(), 1);
    EXPECT_EQ(responseBody.systemViewDetail[0].name, "InvalidConnectionTask");
    EXPECT_EQ(responseBody.systemViewDetail[0].numberCalls, 1);
    EXPECT_EQ(responseBody.total, 1);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewTraceDataWhenHardwareKeepsMstx) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMPUTE_TASK_INFO, TableName::DB_STRING_IDS,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO STRING_IDS (id, value) VALUES (1, 'NormalTask'), (2, 'MstxTask');"
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, taskType) VALUES "
        "(10, 30, 0, 100, 1, 1), (40, 80, 0, 200, 2, 2);"
        "INSERT INTO MSTX_EVENTS (connectionId) VALUES (200);");
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "startTime";
    requestParams.order = "ascend";
    requestParams.current = 1;
    requestParams.pageSize = 10;
    requestParams.layer = "Ascend Hardware";
    requestParams.rankId = "0";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewTraceBody responseBody;
    bool result = database.QuerySystemViewTraceData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
    ASSERT_EQ(responseBody.systemViewDetail.size(), 2);
    EXPECT_EQ(responseBody.systemViewDetail[0].name, "NormalTask");
    EXPECT_EQ(responseBody.systemViewDetail[1].name, "MstxTask");
    EXPECT_EQ(responseBody.total, 2);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenHCCL) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_STRING_IDS,
        TableName::DB_COMMUNICATION_TASK_INFO};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "HCCL";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenCommunication) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_STRING_IDS,
        TableName::DB_COMMUNICATION_TASK_INFO};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "COMMUNICATION";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenCANN) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_CANN_API, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "CANN";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenPython) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_PYTORCH_API, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "Python";
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenOverlap) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_OVERLAP_ANALYSIS, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string overlapData = "INSERT INTO \"main\".\"OVERLAP_ANALYSIS\" (\"id\", \"deviceId\", \"startNs\", "
                              "\"endNs\", \"type\") VALUES (5, 0, 1723510445657911820, 1723510445657974180, 1);";
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (0, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    DatabaseTestCaseMockUtil::InsertData(db, overlapData);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "Overlap Analysis";
    requestParams.searchName = "Communication";
    requestParams.rankId = "0";
    const uint64_t cur = 1;
    const uint64_t size = 100;
    requestParams.pageSize = size;
    requestParams.current = cur;
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQuerySystemViewDataWhenOverlapSearchFree) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_OVERLAP_ANALYSIS, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string overlapData = "INSERT INTO \"main\".\"OVERLAP_ANALYSIS\" (\"id\", \"deviceId\", \"startNs\", "
                              "\"endNs\", \"type\") VALUES (6, 0, 1723510445657974180, 1723510445658074180, 3);";
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (0, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    DatabaseTestCaseMockUtil::InsertData(db, overlapData);
    database.SetDbPtr(db);
    Dic::Protocol::SystemViewParams requestParams;
    requestParams.orderBy = "name";
    requestParams.layer = "Overlap Analysis";
    requestParams.searchName = "F";
    requestParams.rankId = "0";
    const uint64_t cur = 1;
    const uint64_t size = 100;
    requestParams.pageSize = size;
    requestParams.current = cur;
    const uint64_t minTimestamp = 0;
    Dic::Protocol::SystemViewBody responseBody;
    bool result = database.QuerySystemViewData(requestParams, responseBody, minTimestamp);
    EXPECT_EQ(result, true);
    ASSERT_EQ(responseBody.systemViewDetail.size(), 1);
    EXPECT_EQ(responseBody.systemViewDetail[0].name, "Free");
}

TEST_F(DbTraceDatabaseTest2, TestQueryFlowCategoryListWhenDbOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    std::string table = "CREATE TABLE connectionCats (id INTEGER, cat TEXT);";
    std::string tableData = "INSERT INTO \"main\".\"connectionCats\" (\"id\", \"cat\") VALUES (1, '612484');";
    DatabaseTestCaseMockUtil::CreateTable(db, table);
    DatabaseTestCaseMockUtil::InsertData(db, tableData);
    database.SetDbPtr(db);
    std::vector<std::string> categories;
    const std::string rankId;
    bool result = database.QueryFlowCategoryList(categories, rankId);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQueryCommunicationStatisticsData) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    const Dic::Protocol::SummaryStatisticParams requestParams;
    Dic::Protocol::SummaryStatisticsBody responseBody;
    bool result = database.QueryCommunicationStatisticsData(requestParams, responseBody);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryCommunicationKernelInfoWhenDbOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string opData =
        "INSERT INTO \"main\".\"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (322, 1723510445656562660, 1723510445656625680, 149336, 324, 1, 0, 0, 5, 325, 8192, 326, 1412060, 0);";
    std::string infoData = "INSERT INTO \"main\".\"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", "
                           "\"taskType\", \"planeId\", \"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", "
                           "\"dstRank\", \"transportType\", \"size\", \"dataType\", \"linkType\", \"opId\") VALUES "
                           "(1, 6901, 323, 0, 324, 9223372036854775807, 65535, 0, 0, 0, 65536, 65535, 0, 1);";
    std::string taskData = "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
                           "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
                           "\"modelId\", \"depth\") VALUES (1723510445634242160, 1723510445634242160, 0, 4294967295, "
                           "6901, 4130085, 293, 4294967295, 0, 39, 4294967295, 0);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'device');";
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (0, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, opData);
    DatabaseTestCaseMockUtil::InsertData(db, infoData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    const std::string name = "device";
    const std::string rankId = "0";
    Dic::Protocol::CommunicationKernelBody body;
    bool result = database.QueryCommunicationKernelInfo(name, rankId, body);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest2, TestQueryCommunicationKernelInfoWhenUniqueDevice) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string opData =
        "INSERT INTO \"main\".\"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\", "
        "\"deviceId\") "
        "VALUES (1, 1723510445656562660, 1723510445656625680, 149336, 324, 1, 0, 0, 5, 325, 8192, 326, 1412060, 0);";
    std::string taskData = "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
                           "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
                           "\"modelId\", \"depth\") VALUES (1723510445634242160, 1723510445634242160, 0, 4294967295, "
                           "6901, 4130085, 293, 4294967295, 0, 39, 4294967295, 0);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'device');";
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (0, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, opData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    const std::string name = "device";
    const std::string rankId = "0";
    MockNpuInfoRepoFunc();
    Dic::Protocol::CommunicationKernelBody body;
    bool result = database.QueryCommunicationKernelInfo(name, rankId, body);
    EXPECT_EQ(result, true);
    EXPECT_EQ(body.depth, 0);
    EXPECT_EQ(body.pid, "HCCL");
    EXPECT_EQ(body.threadId, "324group");
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest2, TestQueryCommunicationKernelInfoWhenDbNotOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    const std::string name = "device";
    const std::string rankId = "0";
    Dic::Protocol::CommunicationKernelBody body;
    bool result = database.QueryCommunicationKernelInfo(name, rankId, body);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryHostInfoWhenTableIsWrong) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    std::string table = "CREATE TABLE HOST_INFO (hostUi INTEGER,hostame TEXT);";
    DatabaseTestCaseMockUtil::CreateTable(db, table);
    std::string data =
        "INSERT INTO \"main\".\"HOST_INFO\" (\"hostUi\", \"hostame\") VALUES (4973977386493930762, 'ubuntu2204');";
    DatabaseTestCaseMockUtil::InsertData(db, data);
    database.SetDbPtr(db);
    std::string result = database.QueryHostInfo();
    EXPECT_EQ(std::empty(result), true);
}

TEST_F(DbTraceDatabaseTest2, TestQueryHostInfoWhenTimeTableIsExist) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    std::string table = "CREATE TABLE HOST_INFO (hostUid INTEGER,hostName TEXT);";
    std::string table2 = "CREATE TABLE SESSION_TIME_INFO (startTimeNs INTEGER,endTimeNs INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, table);
    DatabaseTestCaseMockUtil::CreateTable(db, table2);
    std::string data =
        "INSERT INTO \"main\".\"HOST_INFO\" (\"hostUid\", \"hostName\") VALUES (4973977386493930762, 'ubuntu2204');";
    std::string data2 = "INSERT INTO \"main\".\"SESSION_TIME_INFO\" (\"startTimeNs\", \"endTimeNs\") VALUES "
                        "(1723510445508818000, 1723510450298869000);";
    DatabaseTestCaseMockUtil::InsertData(db, data);
    DatabaseTestCaseMockUtil::InsertData(db, data2);
    database.SetDbPtr(db);
    std::string result = database.QueryHostInfo();
    EXPECT_EQ(std::empty(result), false);
    Dic::Module::FullDb::CollectionTimeService::Instance().Reset();
}

TEST_F(DbTraceDatabaseTest2, TestQueryFwdBwdDataByFlowWhenTableNotRight) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_PYTORCH_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string connTable = "CREATE TABLE CONNECTION_IDS (id INTEGER, connectionId INTEGER);";
    std::string catTable = "CREATE TABLE connectionCats(connectionId INT,cat);";
    std::string apiTypeTable = "CREATE TABLE ENUM_API_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    DatabaseTestCaseMockUtil::CreateTable(db, connTable);
    DatabaseTestCaseMockUtil::CreateTable(db, catTable);
    DatabaseTestCaseMockUtil::CreateTable(db, apiTypeTable);
    database.SetDbPtr(db);
    const std::string rankId;
    const uint64_t offset = 0;
    const Dic::Protocol::ExtremumTimestamp range;
    std::vector<Dic::Protocol::ThreadTraces> fwdBwdData;
    bool result = database.QueryFwdBwdDataByFlow(rankId, offset, range, fwdBwdData);
    EXPECT_EQ(result, false);
    EXPECT_EQ(fwdBwdData.size(), 0);
}

TEST_F(DbTraceDatabaseTest2, TestQueryFwdBwdFromMstxSuccess) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string mstxInfoSql = "CREATE TABLE StepTaskInfo (name TEXT, startNs INTEGER, endNs INTEGER, type INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, mstxInfoSql);
    database.SetDbPtr(db);
    std::vector<Protocol::ThreadTraces> traceList;
    bool res = database.QueryFwdBwdFromMstx(traceList);
    const int expectSize = 0;
    EXPECT_EQ(res, true);
    EXPECT_EQ(traceList.size(), expectSize);
}

TEST_F(DbTraceDatabaseTest2, TestQueryP2PCommunicationOpHaveConnectionIdSucceess) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_COMMUNICATION_OP, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::vector<Protocol::ThreadTraces> traceList;
    bool res = database.QueryP2PCommunicationOpHaveConnectionId(traceList);
    const int expectSize = 0;
    EXPECT_EQ(res, true);
    EXPECT_EQ(traceList.size(), expectSize);
}

TEST_F(DbTraceDatabaseTest2, TestQueryP2PCommunicationOpDataWhenDbNotOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    const std::string rankId;
    const uint64_t offset = 0;
    const Dic::Protocol::ExtremumTimestamp range;
    std::vector<Dic::Protocol::ThreadTraces> fwdBwdData;
    bool result = database.QueryP2PCommunicationOpData(rankId, offset, range, fwdBwdData);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryConnectionIdWhenDbNotOpen) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    const Dic::Protocol::UnitFlowsParams requestParams;
    EXPECT_THROW(
        Dic::Protocol::TraceDatabaseHelper::QueryConnectionId(stmt, requestParams), Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryConnectionIdWhenHccl) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.metaType = "HCCL";
    EXPECT_THROW(
        Dic::Protocol::TraceDatabaseHelper::QueryConnectionId(stmt, requestParams), Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryConnectionIdWhenCANN) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.metaType = "CANN_API";
    EXPECT_THROW(
        Dic::Protocol::TraceDatabaseHelper::QueryConnectionId(stmt, requestParams), Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryConnectionIdWhenApi) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.metaType = "PYTORCH_API";
    EXPECT_THROW(
        Dic::Protocol::TraceDatabaseHelper::QueryConnectionId(stmt, requestParams), Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryConnectionIdWhenMstx) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.metaType = "MSTX_EVENTS";
    EXPECT_THROW(
        Dic::Protocol::TraceDatabaseHelper::QueryConnectionId(stmt, requestParams), Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenSoc) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "SOC_BANDWIDTH_LEVEL";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenAcc) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "ACC_PMU";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenNPU) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "NPU_MEM";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenNPUQuerySuccess) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_PYTORCH_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string stringTable = "CREATE TABLE STRING_IDS (id INTEGER, value VARCHAR);";
    std::string npuTable = "CREATE TABLE NPU_MEM (type INTEGER,ddr NUMERIC,hbm NUMERIC,timestampNs INTEGER,"
                           "deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, stringTable);
    DatabaseTestCaseMockUtil::CreateTable(db, npuTable);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES "
        "(1, 'app');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO \"main\".\"NPU_MEM\" (\"type\", \"ddr\", \"hbm\", "
        "\"timestampNs\", \"deviceId\") VALUES (1, 0, 28036571136, 1725542118206101090, 0);");
    database.SetDbPtr(db);

    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "NPU_MEM";
    requestParams.threadId = "app/HBM";
    const uint64_t expectStartTime = 1725542118206101090;
    const uint64_t rangeTime = 1000000;
    requestParams.startTime = expectStartTime - rangeTime;
    requestParams.endTime = expectStartTime + rangeTime;
    const uint64_t minTimestamp = 0;
    const std::string rankId = "0";

    auto resultSet =
        Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId);
    resultSet->Next();
    auto startTime = resultSet->GetUint64("startTime");
    auto args = resultSet->GetString("args");
    EXPECT_EQ(startTime, expectStartTime);
    EXPECT_EQ(args, "{\"Byte\":28036571136}");
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenCPUFreqQuerySuccess) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE CPU_FREQ (timestampNs NUMERIC, cpuId NUMERIC, freq NUMERIC);");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO CPU_FREQ (timestampNs, cpuId, freq) VALUES "
        "(1010, 1, 1200000), (1015, 0, 999999), (1020, 1, 1200000);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "CPU_FREQ";
    requestParams.threadId = "CPU 1";
    requestParams.rankId = "7";
    requestParams.startTime = 5;
    requestParams.endTime = 30;
    const uint64_t minTimestamp = 1000;
    std::vector<Dic::Protocol::UnitCounterData> dataList;

    ASSERT_TRUE(database.QueryUnitCounter(requestParams, minTimestamp, dataList));
    ASSERT_EQ(dataList.size(), 2);
    EXPECT_EQ(dataList[0].timestamp, 10);
    EXPECT_EQ(dataList[0].valueJsonStr, "{\"Frequency(KHz)\":1200000}");
    EXPECT_EQ(dataList[1].timestamp, 20);
    EXPECT_EQ(dataList[1].valueJsonStr, "{\"Frequency(KHz)\":1200000}");
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenQOSQuerySuccess) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    std::string stringTable = "CREATE TABLE STRING_IDS (id INTEGER, value VARCHAR);";
    std::string qosTable =
        "CREATE TABLE QOS (deviceId NUMERIC,eventName NUMERIC,bandwidth NUMERIC,timestampNs NUMERIC)";
    DatabaseTestCaseMockUtil::CreateTable(db, stringTable);
    DatabaseTestCaseMockUtil::CreateTable(db, qosTable);
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES "
        "(2, 'QoS 0:OTHERS');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO \"main\".\"QOS\" (\"deviceId\", \"eventName\", \"bandwidth\", "
        "\"timestampNs\") VALUES (0, 2, 3611295744, 1750410162487673272);");
    database.SetDbPtr(db);

    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "QOS";
    requestParams.threadId = "QoS 0:OTHERS/Bandwidth";
    const uint64_t expectStartTime = 1750410162487673272;
    const uint64_t rangeTime = 1000000;
    requestParams.startTime = expectStartTime - rangeTime;
    requestParams.endTime = expectStartTime + rangeTime;
    const uint64_t minTimestamp = 0;
    const std::string rankId = "0";

    auto resultSet =
        Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId);
    resultSet->Next();
    auto startTime = resultSet->GetUint64("startTime");
    auto args = resultSet->GetString("args");
    EXPECT_EQ(startTime, expectStartTime);
    EXPECT_EQ(args, "{\"Bandwidth(Byte/s)\":3611295744}");
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenAICoreFreqHasTwoDiesAtSameTimestamp) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE AICORE_FREQ (deviceId NUMERIC,timestampNs NUMERIC,freq NUMERIC,dieId NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO AICORE_FREQ (deviceId,timestampNs,freq,dieId) VALUES (1,1000,1650,0),(1,1000,1200,1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "AICORE_FREQ";
    params.startTime = 0;
    params.endTime = 2000;
    auto stmt = database.CreatPreparedStatement();
    params.threadId = "AI Core Freq Die 0";
    auto die0 = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");
    ASSERT_TRUE(die0->Next());
    EXPECT_EQ(die0->GetString("args"), "{\"Frequency(Mhz)\":1650}");
    EXPECT_FALSE(die0->Next());
    die0.reset();

    params.threadId = "AI Core Freq Die 1";
    auto die1 = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");
    ASSERT_TRUE(die1->Next());
    EXPECT_EQ(die1->GetString("args"), "{\"Frequency(Mhz)\":1200}");
    EXPECT_FALSE(die1->Next());
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenAICoreFreqLegacySchemaHasNoDieId) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE AICORE_FREQ (deviceId NUMERIC,timestampNs NUMERIC,freq NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO AICORE_FREQ (deviceId,timestampNs,freq) VALUES "
        "(1,1000,1650),(1,2000,1200),(2,1500,800);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "AICORE_FREQ";
    params.threadId = "AI Core Freq";
    params.startTime = 0;
    params.endTime = 3000;
    auto stmt = database.CreatPreparedStatement();
    auto result = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");

    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetUint64("startTime"), 1000);
    EXPECT_EQ(result->GetString("args"), "{\"Frequency(Mhz)\":1650}");
    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetUint64("startTime"), 2000);
    EXPECT_EQ(result->GetString("args"), "{\"Frequency(Mhz)\":1200}");
    EXPECT_FALSE(result->Next());
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenAICoreFreqUsesLegacySentinel) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(
        db, "CREATE TABLE AICORE_FREQ (deviceId NUMERIC,timestampNs NUMERIC,freq NUMERIC,dieId NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO AICORE_FREQ (deviceId,timestampNs,freq,dieId) VALUES "
        "(1,1000,1500,-1),(1,2000,900,-1),(2,1500,1200,-1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "AICORE_FREQ";
    params.threadId = "AI Core Freq";
    params.startTime = 0;
    params.endTime = 3000;
    auto stmt = database.CreatPreparedStatement();
    auto result = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");

    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetUint64("startTime"), 1000);
    EXPECT_EQ(result->GetString("args"), "{\"Frequency(Mhz)\":1500}");
    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetUint64("startTime"), 2000);
    EXPECT_EQ(result->GetString("args"), "{\"Frequency(Mhz)\":900}");
    EXPECT_FALSE(result->Next());
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenQOSHasTwoDiesAtSameTimestamp) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, "CREATE TABLE STRING_IDS (id INTEGER, value VARCHAR);");
    DatabaseTestCaseMockUtil::CreateTable(db,
        "CREATE TABLE QOS (deviceId NUMERIC,eventName NUMERIC,bandwidth NUMERIC,timestampNs NUMERIC,dieId NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO STRING_IDS (id, value) VALUES (2, 'QoS 0:OTHERS');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO QOS (deviceId,eventName,bandwidth,timestampNs,dieId) VALUES "
        "(1,2,100,1000,0),(1,2,200,1000,1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "QOS";
    params.startTime = 0;
    params.endTime = 2000;
    auto stmt = database.CreatPreparedStatement();
    params.threadId = "QoS 0:OTHERS/Die 0/Bandwidth";
    auto die0 = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");
    ASSERT_TRUE(die0->Next());
    EXPECT_EQ(die0->GetString("args"), "{\"Bandwidth(Byte/s)\":100}");
    EXPECT_FALSE(die0->Next());
    die0.reset();

    params.threadId = "QoS 0:OTHERS/Die 1/Bandwidth";
    auto die1 = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");
    ASSERT_TRUE(die1->Next());
    EXPECT_EQ(die1->GetString("args"), "{\"Bandwidth(Byte/s)\":200}");
    EXPECT_FALSE(die1->Next());
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenQOSUsesLegacyDieSentinel) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, "CREATE TABLE STRING_IDS (id INTEGER, value VARCHAR);");
    DatabaseTestCaseMockUtil::CreateTable(db,
        "CREATE TABLE QOS (deviceId NUMERIC,eventName NUMERIC,bandwidth NUMERIC,timestampNs NUMERIC,dieId NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO STRING_IDS (id, value) VALUES (2, 'QoS 0:OTHERS');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO QOS (deviceId,eventName,bandwidth,timestampNs,dieId) VALUES "
        "(1,2,100,1000,-1),(1,2,200,2000,-1),(2,2,300,1500,-1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "QOS";
    params.threadId = "QoS 0:OTHERS/Bandwidth";
    params.startTime = 0;
    params.endTime = 3000;
    auto stmt = database.CreatPreparedStatement();
    auto result = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");

    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetString("args"), "{\"Bandwidth(Byte/s)\":100}");
    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetString("args"), "{\"Bandwidth(Byte/s)\":200}");
    EXPECT_FALSE(result->Next());
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenSimple) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "SAMPLE_PMU_TIMELINE";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenRoce) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "RoCE";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenRoH) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "RoH";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenNIC) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "NIC";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenHCCS) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "HCCS";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenPCIE) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "PCIE";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenAICORE) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::UnitCounterParams requestParams;
    requestParams.metaType = "AICORE_FREQ";
    const uint64_t minTimestamp = 0;
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, requestParams, minTimestamp, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryThreadsByPidWhenApi) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    const uint64_t startTime = 0;
    const uint64_t endTime = 0;
    Dic::Protocol::Metadata metaData;
    metaData.metaType = "PYTORCH_API";
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryThreadsByPid(stmt, startTime, endTime, metaData, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryThreadsByPidWhenOVERLAP) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    const uint64_t startTime = 0;
    const uint64_t endTime = 0;
    Dic::Protocol::Metadata metaData;
    metaData.metaType = "OVERLAP_ANALYSIS";
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryThreadsByPid(stmt, startTime, endTime, metaData, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryThreadsByPidWhenMstx) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    const uint64_t startTime = 0;
    const uint64_t endTime = 0;
    Dic::Protocol::Metadata metaData;
    metaData.metaType = "MSTX_EVENTS";
    const std::string rankId;
    EXPECT_THROW(Dic::Protocol::TraceDatabaseHelper::QueryThreadsByPid(stmt, startTime, endTime, metaData, rankId),
        Dic::Module::DatabaseException);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenApi) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "PYTORCH_API";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenHardWare) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "Ascend Hardware";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenHardWareAndTidIsNotEmpty) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "Ascend Hardware";
    params.tid = "ppp";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4StreamWithoutMSTX) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    const std::vector<TableName> list{TableName::DB_TASK};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924972, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924972, 1729733883833924992, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924932, 1729733883833924952, 7, 4294967295, 82550, 511284, 221, 4294967295, 3, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924972, 7, 4294967295, 82550, 511284, 221, 4294967295, 3, 40, 4294967295, "
        "0),"
        "(1729733883833924972, 1729733883833924992, 7, 4294967295, 82550, 511284, 221, 4294967295, 3, 40, 4294967295, "
        "0);";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.currentPage = 1;
    params.pageSize = 10; // 10
    params.metaType = "Ascend Hardware";
    params.threadIdList = {"2"};
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "7";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 3); // 3

    auto stmt2 = database.CreatPreparedStatement();
    params.threadIdList = {"3"};
    body.eventDetailList.clear();
    res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt2, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 3); // 3
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4StreamWithMSTXWithInvalidDomain) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_STRING_IDS, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924972, 1729733883833924992, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0)";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 65535, 4000000001, 0)";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (447, 'start')";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.currentPage = 1;
    params.pageSize = 10; // 10
    params.metaType = "Ascend Hardware";
    params.threadIdList = {"2_65535"};
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "7";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(dynamic_cast<DeviceEventDetail *>(body.eventDetailList[0].get())->threadName, "Stream 2 MSTX");

    auto stmt2 = database.CreatPreparedStatement();
    params.threadIdList = {"2"};
    body.eventDetailList.clear();
    res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt2, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 1);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4StreamWithMSTXWithValidDomain) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_STRING_IDS, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert =
        "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1729733883833924932, 1729733883833924952, 7, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924972, 1729733883833924992, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0)";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 2967, 4000000001, 0)";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (447, 'start'), (2967, 'cat')";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);
    // 初始化所有全量查询功能需要的表，DbTraceDataBase在OpenDb()时会调用到以下逻辑
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!database.CheckTableExist(item.first)) {
            database.ExecSql(item.second);
        }
    }

    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.currentPage = 1;
    params.pageSize = 10; // 10
    params.metaType = "Ascend Hardware";
    params.threadIdList = {"2_2967"};
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "7";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(dynamic_cast<DeviceEventDetail *>(body.eventDetailList[0].get())->threadName, "Stream 2 MSTX domain cat");

    auto stmt2 = database.CreatPreparedStatement();
    params.threadIdList = {"2"};
    body.eventDetailList.clear();
    res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt2, params, body, minTimestamp, deviceId);
    ASSERT_TRUE(res);
    ASSERT_EQ(body.eventDetailList.size(), 1);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenCANN) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_ENUM_API_TYPE, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string insertStringIdsSql = "INSERT INTO \"STRING_IDS\" (\"id\", \"value\") "
                                     "VALUES (5413, 'cann_test');";
    DatabaseTestCaseMockUtil::InsertData(db, cannDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, numeApiDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertStringIdsSql);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "CANN_API";
    params.currentPage = 1;
    params.pageSize = 10;
    params.processName = "CANN";
    params.pid = "1237912654215057";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, true);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "cann_test");
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenCANNWithHccl) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_ENUM_API_TYPE, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string insertEnumSql = "INSERT INTO \"ENUM_API_TYPE\" (\"id\", \"name\") "
                                "VALUES (20000, 'hccl');";
    std::string insertStringIdsSql = "INSERT INTO \"STRING_IDS\" (\"id\", \"value\") "
                                     "VALUES (5413, 'cann_test');";
    DatabaseTestCaseMockUtil::InsertData(db, cannDataSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertEnumSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertStringIdsSql);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "CANN_API";
    params.currentPage = 1;
    params.pageSize = 10;
    params.processName = "Thread";
    params.threadName = "hccl";
    params.pid = "1237912654215057";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, true);
    EXPECT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "cann_test");
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4HcclFiltersCommunicationOpByDeviceId) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string insertTaskSql = "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
                                "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", "
                                "\"taskId\", \"modelId\", \"depth\") VALUES (1737098043298003143, "
                                "1737098043298003743, 0, 19076, 3453, 13877, 262, 0, 5, 2950, 4294967295, 1);";
    std::string insertInfoSql = "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", "
                                "\"planeId\", \"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", "
                                "\"transportType\", \"size\", \"dataType\", \"linkType\", \"opId\") VALUES (400, "
                                "3453, 401, 0, 402, 0, 65535, 0, 4294967295, 0, 4, 2, 0, 1);";
    std::string insertOpSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", "
        "\"opType\", \"waitNs\", \"deviceId\") VALUES (400, 1737098043298003143, 1737098043314228587, 19076, "
        "402, 1, 0, 0, 5, 1167, 2048, 235, 4865418, 0), "
        "(400, 1737098043298003143, 1737098043314228587, 19077, "
        "402, 2, 0, 0, 5, 1167, 2048, 235, 4865418, 1);";
    std::string insertStringIdsSql = "INSERT INTO \"STRING_IDS\" (\"id\", \"value\") "
                                     "VALUES (400, 'hcom_broadcast__559_0_1');";
    DatabaseTestCaseMockUtil::InsertData(db, insertTaskSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertInfoSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertOpSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertStringIdsSql);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "HCCL";
    params.currentPage = 1;
    params.pageSize = 10;
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, true);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "hcom_broadcast__559_0_1");
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenHcclAndTidNotEmpty) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_COMMUNICATION_OP,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string insertTaskSql = "INSERT INTO \"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
                                "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", "
                                "\"taskId\", \"modelId\", \"depth\") VALUES (1737098043298003143, "
                                "1737098043298003743, 0, 19076, 3453, 13877, 262, 0, 5, 2950, 4294967295, 1);";
    std::string insertInfoSql = "INSERT INTO \"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", "
                                "\"planeId\", \"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", "
                                "\"transportType\", \"size\", \"dataType\", \"linkType\", \"opId\") VALUES (400, "
                                "3453, 401, 0, 402, 0, 65535, 0, 4294967295, 0, 4, 2, 0, 1);";
    std::string insertOpSql =
        "INSERT INTO \"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", "
        "\"opType\", \"waitNs\", \"deviceId\") VALUES (400, 1737098043298003143, 1737098043314228587, 19076, "
        "402, 1, 0, 0, 5, 1167, 2048, 235, 4865418, 0);";
    std::string insertStringIdsSql = "INSERT INTO \"STRING_IDS\" (\"id\", \"value\") "
                                     "VALUES (400, 'hcom_broadcast__559_0_1');";
    DatabaseTestCaseMockUtil::InsertData(db, insertTaskSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertInfoSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertOpSql);
    DatabaseTestCaseMockUtil::InsertData(db, insertStringIdsSql);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "HCCL";
    params.tid = "402group";
    params.currentPage = 1;
    params.pageSize = 10;
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, true);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "hcom_broadcast__559_0_1");
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenOverlap) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "OVERLAP_ANALYSIS";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenOverlapAndTidNotEmpty) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "OVERLAP_ANALYSIS";
    params.tid = "kkkkkkkk";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4PytorchWhenOther) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "unknown";
    params.tid = "kkkkkkkk";
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId;
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4MSTX) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_MSTX_EVENTS, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 65535, 4000000001, 0)";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (447, 'start')";
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "MSTX_EVENTS";
    params.pid = "4754301164515056";
    params.tid = "65535";
    params.currentPage = 1;
    params.pageSize = 10;
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    ASSERT_EQ(res, true);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "start");
}

TEST_F(DbTraceDatabaseTest2, TestQueryEventsView4OSRT) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_OSRT_API, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string osrtTableInsert = "INSERT INTO OSRT_API (name, globalTid, startNs, endNs) VALUES "
                                  "(12, 4785999999999, 1000, 20000)";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (12, 'futex')";
    DatabaseTestCaseMockUtil::InsertData(db, osrtTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);
    auto stmt = database.CreatPreparedStatement();
    Dic::Protocol::EventsViewParams params;
    params.metaType = "OSRT_API";
    params.pid = "4785999999999";
    params.tid = "OSRT_API";
    params.currentPage = 1;
    params.pageSize = 10;
    Dic::Protocol::EventsViewBody body;
    const uint64_t minTimestamp = 0;
    const std::string deviceId = "0";
    bool res = Dic::Protocol::TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, deviceId);
    ASSERT_EQ(res, true);
    ASSERT_EQ(body.eventDetailList.size(), 1);
    EXPECT_EQ(body.eventDetailList[0]->name, "futex");
}

TEST_F(DbTraceDatabaseTest2, TestIsValidGroupNameValueSuccess) {
    const std::string groupNameValue = "10.170.22.98%enp67s0f5_60000_0_1708156014257149";
    const bool res = Dic::Protocol::TraceDatabaseHelper::IsValidHCCLGroupNameValue(groupNameValue);
    EXPECT_EQ(res, true);
}

TEST_F(DbTraceDatabaseTest2, TestIsValidGroupNameValueFail) {
    const std::string groupNameValue = "0";
    const bool res = Dic::Protocol::TraceDatabaseHelper::IsValidHCCLGroupNameValue(groupNameValue);
    EXPECT_EQ(res, false);
}

TEST_F(DbTraceDatabaseTest2, TestGetHostPath) {
#ifdef _WIN32
    std::string filePath1 = R"(D:\GUI_TEST_DATA\32B\actor worker\ma-job_ascend_pt\ASCEND_PROFILER_OUTPUT\a.db)";
    std::string filePath2 = R"(D:\GUI_TEST_DATA\32B\actor worker\ma-job_ascend_ms\ASCEND_PROFILER_OUTPUT\a.db)";
    std::string filePath3 = R"(D:\GUI_TEST_DATA\32B\actor worker\PROF_000001_11\ASCEND_PROFILER_OUTPUT\a.db)";
    std::string filePath4 = R"(D:\GUI_TEST_DATA\deepseek_32B\actor worker\ascend_pytorch_profiler_3.db)";
#else
    std::string filePath1 = "/home/GUI_TEST_DATA/32B/actor worker/ma-job_ascend_pt/ASCEND_PROFILER_OUTPUT/a.db";
    std::string filePath2 = "/home/GUI_TEST_DATA/32B/actor worker/ma-job_ascend_ms/ASCEND_PROFILER_OUTPUT/a.db";
    std::string filePath3 = "/home/GUI_TEST_DATA/32B/actor worker/PROF_000001_11/ASCEND_PROFILER_OUTPUT/a.db";
    std::string filePath4 = "/home/GUI_TEST_DATA/deepseek_32B/actor worker/ascend_pytorch_profiler_3.db";
#endif
    auto result1 = Dic::Module::FullDb::DbTraceDataBase::GetHostPath(filePath1);
    auto result2 = Dic::Module::FullDb::DbTraceDataBase::GetHostPath(filePath2);
    auto result3 = Dic::Module::FullDb::DbTraceDataBase::GetHostPath(filePath3);
    auto result4 = Dic::Module::FullDb::DbTraceDataBase::GetHostPath(filePath4);
#ifdef _WIN32
    EXPECT_EQ(result1, R"(D:\GUI_TEST_DATA\32B\actor worker\)");
    EXPECT_EQ(result2, R"(D:\GUI_TEST_DATA\32B\actor worker\)");
    EXPECT_EQ(result3, R"(D:\GUI_TEST_DATA\32B\actor worker\)");
#else
    EXPECT_EQ(result1, "/home/GUI_TEST_DATA/32B/actor worker/");
    EXPECT_EQ(result2, "/home/GUI_TEST_DATA/32B/actor worker/");
    EXPECT_EQ(result3, "/home/GUI_TEST_DATA/32B/actor worker/");
#endif
    EXPECT_EQ(result4, "");
}

TEST_F(DbTraceDatabaseTest2, TestBuildOverlapInfoListWithFreeTimeAfterComputingAndCommunication) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert = "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
                                  "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
                                  "0);";
    std::string stringIdsTableInsert = "INSERT INTO STRING_IDS(id, value) VALUES (221, 'MsTx')";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert);

    std::vector<OVERLAP_INFO> timeInfoList{{10, 20, 0}};
    std::vector<OVERLAP_INFO> overlapInfoList = database.BuildOverlapInfoList(timeInfoList, "7"); // deviceId = 7
    ASSERT_EQ(overlapInfoList.size(), 1);
    EXPECT_EQ(overlapInfoList[0].startNs, 20); // 20
    EXPECT_EQ(overlapInfoList[0].endNs, 30); // 30
    EXPECT_EQ(overlapInfoList[0].type, 3); // 3
}

TEST_F(DbTraceDatabaseTest2, TestQueryGroupedAscendHardwareThreads_InvalidModelId) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert = "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
                                  "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, 0),"
                                  "(20, 30, 7, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 1, 0);";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 65535, 4000000001, 0)";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);

    std::vector<Dic::Protocol::ThreadGroup> groups;
    const bool result = database.QueryGroupedAscendHardwareThreadsByModelId(groups);
    EXPECT_EQ(result, true);
    EXPECT_EQ(groups.size(), 0);
}

TEST_F(DbTraceDatabaseTest2, TestQueryGroupedAscendHardwareThreads_ValidModelId) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_TASK, TableName::DB_MSTX_EVENTS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    std::string taskTableInsert = "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
                                  "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 1, 0),"
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 2, 40, 1, 0),"
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 3, 40, 2, 0),"
                                  "(20, 30, 7, 4294967295, 82550, 511284, 221, 4294967295, 4, 40, 2, 0),"
                                  "(20, 30, 7, 4000000001, 82550, 511284, 221, 4294967295, 5, 40, 3, 0);";
    std::string mstxTableInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 65535, 4000000001, 0)";
    DatabaseTestCaseMockUtil::InsertData(db, taskTableInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert);

    std::vector<Dic::Protocol::ThreadGroup> groups;
    const bool result = database.QueryGroupedAscendHardwareThreadsByModelId(groups);
    EXPECT_EQ(result, true);
    ASSERT_EQ(groups.size(), 2);
    EXPECT_EQ(groups[0].threadIds.size(), 1);
    EXPECT_EQ(groups[0].threadIds[0], "2");
    EXPECT_EQ(groups[1].threadIds.size(), 2);
    EXPECT_EQ(groups[1].threadIds[0], "3");
    EXPECT_EQ(groups[1].threadIds[1], "4");
}

TEST_F(DbTraceDatabaseTest2, TestQueryUnitCounterWhenSIORetainsNameIdentity) {
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY, value TEXT NOT NULL)");
    DatabaseTestCaseMockUtil::CreateTable(db,
        "CREATE TABLE SIO (deviceId NUMERIC,name NUMERIC,timestampNs NUMERIC,rxReq NUMERIC,rxRsp NUMERIC,"
        "rxSnp NUMERIC,rxDat NUMERIC,txReq NUMERIC,txRsp NUMERIC,txSnp NUMERIC,txDat NUMERIC)");
    DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO STRING_IDS VALUES (14,'SIO0'),(16,'SIO1');");
    DatabaseTestCaseMockUtil::InsertData(db,
        "INSERT INTO SIO VALUES (1,14,1000,10,20,30,40,50,60,70,80),"
        "(1,16,1000,11,21,31,41,51,61,71,81),(2,16,1000,12,22,32,42,52,62,72,82);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.metaType = "SIO";
    params.threadId = "snp_tx/SIO1";
    params.startTime = 0;
    params.endTime = 2000;
    auto stmt = database.CreatPreparedStatement();
    auto result = Dic::Protocol::TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, 0, "1");
    ASSERT_TRUE(result->Next());
    EXPECT_EQ(result->GetString("args"), "{\"Bandwidth(Byte/s)\":71}");
    EXPECT_FALSE(result->Next());
}
