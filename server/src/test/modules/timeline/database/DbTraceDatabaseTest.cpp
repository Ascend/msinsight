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
#include "DataBaseManager.h"
#include "DbSqlDefs.h"
#include "NpuInfoRepoMock.h"
#include "TraceDatabaseHelper.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
// clang-format off
using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::Timeline;
class DbTraceDatabaseTest : public ::testing::Test {
protected:
    class MockDatabase2 : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase2(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            ClearStringsCache();
            InitStringsCache();
        }
    };

    std::string taskIncludingMSTXInsert1 =
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
        "globalPid, taskType, contextId, streamId, taskId, modelId, depth) "
        "VALUES (1742699319641107170, 1742699319641107190, 0, 4294967295, 7480, 1984976, 1, 4294967295, 2, 12658, "
        "4294967295, 0),"
        "(1729733883833924932, 1729733883833924952, 0, 4000000002, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924992, 0, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string mstxTableInsert1 =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 239, 4000000001, 0),"
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 448, "
        "4754301164515056, 4754301164515056, 240, 4000000002, 0);";
    std::string stringIdsTableInsert1 =
        "INSERT INTO STRING_IDS(id, value) VALUES (239, 'compute'), (240, 'communication'), "
        "(447, 'start'), (448, 'hcom_allReduce')";

    // QueryUnitFlows使用的插入数据SQL
    // 同connectionId，只连接第一个
    std::string taskDataInsertForQueryUnitFlows =
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
        "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
        "(80, 100, 0, 19, 183022, 20366, 7166, 4294967295, 0, 39, 4294967295, 0),"
        "(90, 110, 0, 19, 183022, 20366, 7166, 4294967295, 0, 39, 4294967295, 0);";
    // 同connectionId，只连接第一个
    std::string communicationOpDataInsertForQueryUnitFlows =
        "INSERT INTO COMMUNICATION_OP (opName, startNs, endNs, connectionId, "
        "groupName, opId, relay, retry, dataType, algType, count, opType, waitNs) "
        "VALUES (6, 110, 130, 19, 8, 1, 0, 0, 4, 9, 1, 10, 726280), (8, 150, 170, 19, 8, 2, 0, 0, 4, 9, 1, 10, 2985);";
    std::string rankDeviceMapDataInsertForQueryUnitFlows =
        "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (0, 0);";
    std::string pytorchDataInsertForQueryUnitFlows =
        "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
        "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
        "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);";

    const std::map<TableName, std::string> FLOW_INSERT_DATA_SQL_MAP = {
        {TableName::DB_PYTORCH_API, "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
            "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
            "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);"},
        {TableName::DB_CANN_API, "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
            "name, depth) VALUES (50, 70, 10000, 87471303975183, 19, 7052, 0);"},
        {TableName::DB_MSTX_EVENTS, "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, "
            "message, globalTid, endGlobalTid, domainId, connectionId, depth) VALUES (1741595906437848910, "
            "1741595906438879500, 2, 4294967295, 4294967295, 342, 497262723712066, 497262723712066, 343, 19, 0);"},
        {TableName::DB_TASK, "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
            "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
            "(80, 100, 1, 19, 183022, 20366, 7166, 4294967295, 0, 39, 4294967295, 0);"},
        {TableName::DB_COMMUNICATION_OP, "INSERT INTO COMMUNICATION_OP (opName, startNs, endNs, connectionId, "
            "groupName, opId, relay, retry, dataType, algType, count, opType, waitNs) VALUES (393, "
            "1723537649878647930, 1723537649878710990, 19, 395, 1, 0, 0, 5, 396, 5, 338, 1097840);"},
    };
};
namespace Dic::Protocol {
using namespace Dic::Module::Timeline;
}

/**
 * RANK_DEVICE_MAP表和NPU_INFO都不存在的情况
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdWithRankDeviceAndNpuInfoTableNotExist)
{
    Dic::Protocol::DataBaseManager::Instance().Clear();
    std::recursive_mutex testMutex;
    Dic::Module::FullDb::DbTraceDataBase database(testMutex);
    std::vector<std::string> rankIds = database.QueryRankId();
    const uint64_t expectSize = 1;
    EXPECT_EQ(rankIds.size(), expectSize);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * RANK_DEVICE_MAP表存在的情况
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdWithRankDeviceExist)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestQueryRankIdWithRankDeviceExist";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PYTORCH,
                                                           "TestQueryRankIdWithRankDeviceExist");
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (999, 8), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    std::vector<std::string> rankIds = database.QueryRankId();
    const uint32_t expectSize = 2;
    const std::string firstRankId = "999";
    const std::string secondRankId = "276878";
    const uint32_t first = 0;
    const uint32_t second = 1;
    EXPECT_EQ(rankIds.size(), expectSize);
    EXPECT_EQ(rankIds[first], firstRankId);
    EXPECT_EQ(rankIds[second], secondRankId);
    rankIds = database.QueryRankId();
    EXPECT_EQ(rankIds.size(), expectSize);
    EXPECT_EQ(rankIds[first], firstRankId);
    EXPECT_EQ(rankIds[second], secondRankId);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * RANK_DEVICE_MAP表不存在但NPU_INFO存在的情况
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdWithNpuInfoExist)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestQueryRankIdWithNpuInfoExist";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE NPU_INFO (id INTEGER, name TEXT);";
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::MS_PROF,
                                                           "TestQueryRankIdWithNpuInfoExist");
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO NPU_INFO (id, name) VALUES (3, 'h'), (4, 'kk');";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    std::vector<std::string> rankIds = database.QueryRankId();
    const uint32_t expectSize = 2;
    const std::string firstRankId = "3";
    const std::string secondRankId = "4";
    const uint32_t first = 0;
    const uint32_t second = 1;
    EXPECT_EQ(rankIds.size(), expectSize);
    EXPECT_EQ(rankIds[first], firstRankId);
    EXPECT_EQ(rankIds[second], secondRankId);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * QueryRankIdAndDeviceMap函数RANK_DEVICE_MAP表和NPU_INFO都不存在的情况
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdAndDeviceMapWithRankDeviceAndNpuInfoTableNotExist)
{
    Dic::Protocol::DataBaseManager::Instance().Clear();
    std::recursive_mutex testMutex;
    Dic::Module::FullDb::DbTraceDataBase database(testMutex);
    std::unordered_map<std::string, std::string> rankIds = database.QueryRankIdAndDeviceMap();
    EXPECT_EQ(rankIds.size(), 0);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * QueryRankIdAndDeviceMap函数RANK_DEVICE_MAP表存在的情况
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdAndDeviceMapWithRankDeviceExist)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestQueryRankIdAndDeviceMapWithRankDeviceExist";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PYTORCH,
                                                           "TestQueryRankIdAndDeviceMapWithRankDeviceExist");
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (999, 8), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    std::unordered_map<std::string, std::string> rankIds = database.QueryRankIdAndDeviceMap();
    const uint32_t expectSize = 2;
    const std::string firstRankId = "999";
    const std::string secondRankId = "276878";
    const std::string first = "8";
    const std::string second = "7";
    EXPECT_EQ(rankIds.size(), expectSize);
    EXPECT_EQ(rankIds[firstRankId], first);
    EXPECT_EQ(rankIds[secondRankId], second);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * QueryRankIdAndDeviceMap函数RANK_DEVICE_MAP表存在但列名错误
 */
TEST_F(DbTraceDatabaseTest, TestQueryRankIdAndDeviceMapWithRankDeviceWrongColumn)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestQueryRankIdAndDeviceMapWithRankDeviceWrongColumn";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PYTORCH,
                                                           "TestQueryRankIdAndDeviceMapWithRankDeviceWrongColumn");
    std::recursive_mutex testMutex;
    MockDatabase dataBase(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, devicId INTEGER);";
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, devicId) VALUES (999, 8), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    dataBase.SetDbPtr(db);
    std::unordered_map<std::string, std::string> rankIds = dataBase.QueryRankIdAndDeviceMap();
    EXPECT_EQ(rankIds.size(), 0);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * GetDeviceId函数host存在
 */
TEST_F(DbTraceDatabaseTest, TestGetDeviceIdWithHostExist)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestGetDeviceIdWithHostExist";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PYTORCH,
                                                           "TestGetDeviceIdWithHostExist");
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (999, 8), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    std::string hostSql = "CREATE TABLE HOST_INFO (hostUid TEXT,hostName TEXT);";
    DatabaseTestCaseMockUtil::CreateTable(db, hostSql);
    insertSql =
        "INSERT INTO \"main\".\"HOST_INFO\" (\"hostUid\", \"hostName\") VALUES ('3203900921565068809', 'ubuntu');";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    std::string fileId = "ubuntu3203900921565068809 999";
    std::string deviceId = database.GetDeviceId(fileId);
    const std::string expectDeviceId = "8";
    EXPECT_EQ(deviceId, expectDeviceId);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

/**
 * GetDeviceId函数host不存在
 */
TEST_F(DbTraceDatabaseTest, TestGetDeviceIdWithHostNotExist)
{
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = "TestGetDeviceIdWithHostNotExist";
            return;
        }
    };
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PYTORCH, "TestGetDeviceIdWithHostNotExist");
    std::recursive_mutex testMutex;
    MockDatabase database(testMutex);
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::OpenDBAndCreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (999, 8), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    std::string fileId = "276878";
    std::string deviceId = database.GetDeviceId(fileId);
    const std::string expectDeviceId = "7";
    EXPECT_EQ(deviceId, expectDeviceId);
    Dic::Protocol::DataBaseManager::Instance().Clear();
}

TEST_F(DbTraceDatabaseTest, TestQueryAffinityOptimizerWhenDbNotOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    const Dic::Protocol::KernelDetailsParams params;
    const std::string optimizers;
    std::vector<Dic::Protocol::ThreadTraces> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAffinityOptimizer(params, optimizers, data, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryAffinityOptimizerWhenTableExist)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_PYTORCH_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string pyData =
        "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
        "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"type\", \"depth\") "
        "VALUES ('1723510445651039490', '1723510445651061410', 17738580008830245, 0, 268435456, NULL, NULL, NULL, "
        "NULL, NULL, 50002, 3);";
    std::string strData =
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268435456, 'originOptimizer');";
    DatabaseTestCaseMockUtil::InsertData(db, pyData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    database.SetDbPtr(db);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    const std::string optimizers = "originOptimizer";
    std::vector<Dic::Protocol::ThreadTraces> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAffinityOptimizer(params, optimizers, data, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryAffinityOptimizerWhenTableNotExist)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_PYTORCH_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    const std::string optimizers = "originOptimizer";
    std::vector<Dic::Protocol::ThreadTraces> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAffinityOptimizer(params, optimizers, data, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryAICpuOpCanBeOptimizedWhenDbNotOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    const Dic::Protocol::KernelDetailsParams params;
    const std::vector<std::string> replace;
    const std::map<std::string, Dic::Module::Timeline::AICpuCheckDataType> dataType;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAICpuOpCanBeOptimized(params, replace, dataType, data, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryAICpuOpCanBeOptimizedWhenDbOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_TASK, TableName::DB_COMPUTE_TASK_INFO};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string comData = "INSERT INTO \"main\".\"COMPUTE_TASK_INFO\" (\"name\", \"globalTaskId\", \"blockDim\", "
        "\"mixBlockDim\", \"taskType\", \"opType\", \"inputFormats\", \"inputDataTypes\", "
        "\"inputShapes\", \"outputFormats\", \"outputDataTypes\", \"outputShapes\", \"attrInfo\", "
        "\"waitNs\") VALUES (3, 1, 8, 0, 1, 2, 6, 4, 8, 6, 5, 10, 5, 1240);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'AI_CPU');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (2, 'duration');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (3, 'MIX_AIV');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (4, 'NonZero');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (5, 'N/A');";
    std::string taskData = "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
        "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
        "\"modelId\", \"depth\") VALUES (1723510445634242160, 1723510445634253160, 0, 4294967295, "
        "1, 4130085, 293, 4294967295, 0, 39, 4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, comData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    database.SetDbPtr(db);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    std::vector<std::string> replace = { "duration" };
    std::map<std::string, Dic::Module::Timeline::AICpuCheckDataType> dataType;
    Dic::Module::Timeline::AICpuCheckDataType aiCpuCheckDataType;
    aiCpuCheckDataType.input.emplace_back("lll");
    aiCpuCheckDataType.output.emplace_back("mmm");
    dataType["other"] = aiCpuCheckDataType;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAICpuOpCanBeOptimized(params, replace, dataType, data, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenSqlInject)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    requestParams.orderBy = "lll@#";
    const uint64_t minTimestamp = 0;
    const std::vector<uint64_t> traceId = {0};
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenDbNotOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    const std::vector<uint64_t> traceId = {0};
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenDbOpenHardWare)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_TASK, TableName::DB_COMPUTE_TASK_INFO};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.processes.push_back(SimpleProcess {"17738580008830245", {"0"}});
    requestParams.metaTypeList = {"Ascend Hardware"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    const std::vector<uint64_t> traceId = {0};
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenDbOpenHardWareAndOverlap_NameInvalidForOverlap)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_TASK, TableName::DB_CANN_API,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_COMMUNICATION_OP, TableName::DB_PYTORCH_API,
        TableName::DB_MSTX_EVENTS, TableName::DB_OVERLAP_ANALYSIS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string taskData =
        "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") "
        "VALUES (1742699319641107170, 1742699319641107190, 0, 72907, 7480, 1984976, 1, 4294967295, 2, 12658, "
        "4294967295, 0);";
    std::string overlapData =
        "INSERT INTO \"main\".\"OVERLAP_ANALYSIS\" (\"id\", \"deviceId\", \"startNs\", \"endNs\", \"type\") VALUES "
        "(103984, 0, 1742699321190093818, 1742699321190208301, 2);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'EVENT_WAIT');";
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.processes.push_back(SimpleProcess {"17738580008830245", {"2"}});
    requestParams.name = "EVENT_WAIT";
    requestParams.rankId = "0";
    requestParams.startTime = 1742699319641100000;
    requestParams.endTime = 1742699321190208302;
    requestParams.pageSize = 10;
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.metaTypeList = {"Ascend Hardware"};
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    uint64_t trackId = TrackInfoManager::Instance().GetTrackId("0", "17738580008830245", "2");
    const std::vector<uint64_t> traceIds = {trackId};
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceIds);
    EXPECT_EQ(result, true);
    EXPECT_EQ(responseBody.sameOperatorsDetails.size(), 1);
    EXPECT_EQ(responseBody.sameOperatorsDetails[0].pid, "Ascend Hardware");
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenHavingDeviceMSTXEvents)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_TASK, TableName::DB_CANN_API,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_COMMUNICATION_OP, TableName::DB_PYTORCH_API,
        TableName::DB_MSTX_EVENTS, TableName::DB_OVERLAP_ANALYSIS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    DatabaseTestCaseMockUtil::InsertData(db, taskIncludingMSTXInsert1);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTableInsert1);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsTableInsert1);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.processes.push_back(SimpleProcess {"Ascend Hardware", {"2", "2_239", "2_240"}});
    requestParams.name = "start";
    requestParams.rankId = "0";
    requestParams.startTime = 1720000000000000000; // 1720000000000000000
    requestParams.endTime = 1740000000000000000; // 1740000000000000000
    requestParams.pageSize = 10; // 10
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.metaTypeList = {"Ascend Hardware"};
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    uint64_t trackId1 = TrackInfoManager::Instance().GetTrackId("0", "Ascend Hardware", "2");
    uint64_t trackId2 = TrackInfoManager::Instance().GetTrackId("0", "Ascend Hardware", "2_239");
    uint64_t trackId3 = TrackInfoManager::Instance().GetTrackId("0", "Ascend Hardware", "2_240");
    const std::vector<uint64_t> traceIds = {trackId1, trackId2, trackId3};
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceIds);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.sameOperatorsDetails.size(), 1);
    EXPECT_EQ(responseBody.sameOperatorsDetails[0].pid, "Ascend Hardware");
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenCANN)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_TASK, TableName::DB_CANN_API,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_COMMUNICATION_OP, TableName::DB_PYTORCH_API,
        TableName::DB_MSTX_EVENTS, TableName::DB_OVERLAP_ANALYSIS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.rankId = "";
    requestParams.processes.push_back(SimpleProcess {"17738580008830245", {"0"}});
    requestParams.metaTypeList = {"CANN_API"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.pageSize = 10;
    requestParams.current = 1;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    std::vector<uint64_t> traceId;
    for (const auto& process : requestParams.processes) {
        for (const auto& tid : process.tidList) {
            traceId.emplace_back(TrackInfoManager::Instance().GetTrackId(requestParams.rankId, process.pid, tid));
        }
    }
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenMstx)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_TASK, TableName::DB_CANN_API,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_COMMUNICATION_OP, TableName::DB_PYTORCH_API,
        TableName::DB_MSTX_EVENTS, TableName::DB_OVERLAP_ANALYSIS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.rankId = "";
    requestParams.processes.push_back(SimpleProcess {"17738580008830245", {"0"}});
    requestParams.metaTypeList = {"MSTX_EVENTS"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    requestParams.current = 1;
    requestParams.pageSize = 10;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    std::vector<uint64_t> traceId;
    for (const auto& process : requestParams.processes) {
        for (const auto& tid : process.tidList) {
            traceId.emplace_back(TrackInfoManager::Instance().GetTrackId(requestParams.rankId, process.pid, tid));
        }
    }
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenApi)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO,
        TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, TableName::DB_TASK, TableName::DB_CANN_API,
        TableName::DB_COMMUNICATION_TASK_INFO, TableName::DB_COMMUNICATION_OP, TableName::DB_PYTORCH_API,
        TableName::DB_MSTX_EVENTS, TableName::DB_OVERLAP_ANALYSIS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string pyData =
        "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
        "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"type\", \"depth\") "
        "VALUES ('1723510445651039490', '1723510445651061410', 17738580008830245, 0, 268435456, NULL, NULL, NULL, "
        "NULL, NULL, 50002, 3);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268435456, 'FORMAT_ND');";
    DatabaseTestCaseMockUtil::InsertData(db, pyData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.rankId = "";
    requestParams.processes.push_back(SimpleProcess {"17738580008830245", {"0"}});
    requestParams.metaTypeList = {"PYTORCH_API"};
    requestParams.orderBy = "duration";
    requestParams.order = "DESC";
    const uint64_t min = 0;
    const uint64_t max = 1823510445651061410;
    requestParams.startTime = min;
    requestParams.endTime = max;
    requestParams.name = "FORMAT_ND";
    requestParams.current = 1;
    requestParams.pageSize = 10;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    std::vector<uint64_t> traceId;
    for (const auto& process : requestParams.processes) {
        for (const auto& tid : process.tidList) {
            traceId.emplace_back(TrackInfoManager::Instance().GetTrackId(requestParams.rankId, process.pid, tid));
        }
    }
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryThreadSameOperatorsDetailsWhenOsrt)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_OSRT_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string osrtData =
        "INSERT INTO OSRT_API (name, globalTid, startNs, endNs) VALUES (100, 324875, 199, 2540);";
    std::string stringIdsData = "INSERT INTO STRING_IDS (id, value) VALUES (100, 'add');";
    DatabaseTestCaseMockUtil::InsertData(db, osrtData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitThreadsOperatorsParams requestParams;
    requestParams.rankId = "";
    requestParams.processes.push_back(SimpleProcess {"324875", {"0"}});
    requestParams.metaTypeList = {"OSRT_API"};
    requestParams.orderBy = "timestamp";
    requestParams.order = "DESC";
    const uint64_t min = 0;
    const uint64_t max = 3000;
    requestParams.startTime = min;
    requestParams.endTime = max;
    requestParams.name = "add";
    requestParams.pageSize = 10; // 10
    requestParams.current = 1;
    Dic::Protocol::UnitThreadsOperatorsBody responseBody;
    const uint64_t minTimestamp = 0;
    std::vector<uint64_t> traceId;
    for (const auto& process : requestParams.processes) {
        for (const auto& tid : process.tidList) {
            traceId.emplace_back(TrackInfoManager::Instance().GetTrackId(requestParams.rankId, process.pid, tid));
        }
    }
    bool result = database.QueryThreadSameOperatorsDetails(requestParams, responseBody, minTimestamp, traceId);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.sameOperatorsDetails.size(), 1);
}

TEST_F(DbTraceDatabaseTest, TestQueryAclnnOpCountExceedThresholdWhenDbNotOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    Dic::Protocol::KernelDetailsParams params;
    params.current = 1;
    params.pageSize = 10;
    const uint64_t threshold = 0;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAclnnOpCountExceedThreshold(params, threshold, data, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryAclnnOpCountExceedThresholdWhenDbOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO, TableName::DB_TASK};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string comData = "INSERT INTO \"main\".\"COMPUTE_TASK_INFO\" (\"name\", \"globalTaskId\", \"blockDim\", "
        "\"mixBlockDim\", \"taskType\", \"opType\", \"inputFormats\", \"inputDataTypes\", "
        "\"inputShapes\", \"outputFormats\", \"outputDataTypes\", \"outputShapes\", \"attrInfo\", "
        "\"waitNs\") VALUES (1, 1, 8, 0, 1, 2, 6, 4, 8, 6, 5, 10, 5, 1240);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'aclnnuuuu8');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (2, 'duration');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (3, 'MIX_AIV');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (4, 'NonZero');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (5, 'N/A');";
    std::string taskData = "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
        "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
        "\"modelId\", \"depth\") VALUES (1723510445634242160, 1723510445634253160, 0, 4294967295, "
        "1, 4130085, 293, 4294967295, 0, 39, 4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, comData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    database.SetDbPtr(db);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    params.current = 1;
    params.pageSize = 10;
    const uint64_t threshold = 0;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 0;
    bool result = database.QueryAclnnOpCountExceedThreshold(params, threshold, data, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryFusibleOpDataWhenDbOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    std::vector<Dic::Module::Timeline::FuseableOpRule> rule;
    rule.push_back({{"Transpose"}, "", ""});
    Protocol::OperatorFusionResBody resBody;
    const uint64_t minTimestamp = 0;
    bool result0 = database.QueryFusibleOpData(params, rule, resBody, minTimestamp);
    EXPECT_EQ(result0, false);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_COMPUTE_TASK_INFO, TableName::DB_TASK};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string comData = "INSERT INTO \"main\".\"COMPUTE_TASK_INFO\" (\"name\", \"globalTaskId\", \"blockDim\", "
        "\"mixBlockDim\", \"taskType\", \"opType\", \"inputFormats\", \"inputDataTypes\", "
        "\"inputShapes\", \"outputFormats\", \"outputDataTypes\", \"outputShapes\", \"attrInfo\", "
        "\"waitNs\") VALUES (1, 1, 8, 0, 1, 2, 6, 4, 8, 6, 5, 10, 5, 1240);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'Transpose');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (2, 'Transpose');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (3, 'MIX_AIV');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (4, 'NonZero');\n"
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (5, 'N/A');";
    std::string taskData = "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
        "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
        "\"modelId\", \"depth\") VALUES (1723510445634242160, 1723510445634253160, 0, 4294967295, "
        "1, 4130085, 293, 4294967295, 0, 39, 4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, comData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    database.SetDbPtr(db);
    bool result = database.QueryFusibleOpData(params, rule, resBody, minTimestamp);
    EXPECT_EQ(result, true);
    rule[0].opList.emplace_back("Transpose2");
    bool result2 = database.QueryFusibleOpData(params, rule, resBody, minTimestamp);
    EXPECT_EQ(result2, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryAffinityAPIDataWhenDbOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_STRING_IDS, TableName::DB_PYTORCH_API};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    std::string pyData =
        "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
        "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"type\", \"depth\") "
        "VALUES ('1723510445651039490', '1723510445651061410', 17738580008830245, 0, 268435456, NULL, NULL, NULL, "
        "NULL, NULL, 50002, 3);";
    std::string strData = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268435456, 'npu::kkk');";
    DatabaseTestCaseMockUtil::InsertData(db, pyData);
    DatabaseTestCaseMockUtil::InsertData(db, strData);
    database.SetDbPtr(db);
    const Dic::Protocol::KernelDetailsParams params;
    const std::set<std::string> pattern;
    const uint64_t minTimestamp = 0;
    std::map<uint64_t, std::vector<Dic::Protocol::FlowLocation>> data;
    std::map<uint64_t, std::vector<uint32_t>> indexes;
    bool result = database.QueryAffinityAPIData(params, pattern, minTimestamp, data, indexes);
    EXPECT_EQ(result, true);
}

TEST_F(DbTraceDatabaseTest, TestQueryAffinityAPIDataWhenDbNotOpen)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    const Dic::Protocol::KernelDetailsParams params;
    const std::set<std::string> pattern;
    const uint64_t minTimestamp = 0;
    std::map<uint64_t, std::vector<Dic::Protocol::FlowLocation>> data;
    std::map<uint64_t, std::vector<uint32_t>> indexes;
    bool result = database.QueryAffinityAPIData(params, pattern, minTimestamp, data, indexes);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestGetCounterUnitsAndDataTypesWhenACCPMU)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_ACC_PMU, TableName::DB_SOC_BANDWIDTH_LEVEL, TableName::DB_NPU_MEM,
        TableName::DB_HCCS, TableName::DB_PCIE, TableName::DB_AICORE_FREQ};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    database.SetDbPtr(db);
    const std::string fileId = "ll";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsWhenConnectionIdIsEmptyThenReturnFalse)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    for (const auto &item : Dic::FULL_DB_TABLE_MAP) {
        DatabaseTestCaseMockUtil::CreateTable(db, item.second);
    }
    database.SetDbPtr(db);
    const Dic::Protocol::UnitFlowsParams requestParams;
    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    EXPECT_EQ(result, false);
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsWhenCANNAndNoDeviceDataThenReturnFalse)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", \"connectionId\", "
        "\"name\", \"depth\") VALUES (1734925661693760506, 1734925661693790778, 10000, 87471303975183, 476320, 7052, "
        "0);";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "476320";
    requestParams.metaType = "CANN_API";
    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    EXPECT_EQ(result, false);
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromCANNToAscendHardwareThenReturnTrue)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", \"connectionId\", "
        "\"name\", \"depth\") VALUES (1734925661693760506, 1734925661693790778, 10000, 87471303975183, 476320, 7052, "
        "0);";
    const std::string taskData =
        "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1734925661780577867, 1734925661780577887, 15, 476320, 183022, 20366, 7166, 4294967295, 0, 39, "
        "4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "476320";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "15";
    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.rankId, "15");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.rankId, "15");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.metaType, "CANN_API");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.metaType, "Ascend Hardware");
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromCANNToAscendHardwareAndDeviceUniqueThenReturnTrue)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", \"connectionId\", "
        "\"name\", \"depth\") VALUES (1734925661693760506, 1734925661693790778, 10000, 87471303975183, 476320, 7052, "
        "0);";
    const std::string taskData =
        "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1734925661780577867, 1734925661780577887, 15, 476320, 183022, 20366, 7166, 4294967295, 0, 39, "
        "4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    database.SetDbPtr(db);
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "476320";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "15";
    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.rankId, "15");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.rankId, "15");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.metaType, "CANN_API");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.metaType, "Ascend Hardware");
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromCANNToAscendHardwareAndRankIdAndDeviceIdNotSame)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_COMMUNICATION_OP,
        TableName::DB_CONNECTION_IDS, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", \"connectionId\", "
        "\"name\", \"depth\") VALUES (1734925661693760506, 1734925661693790778, 10000, 87471303975183, 476320, 7052, "
        "0);";
    const std::string taskData =
        "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
        "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
        "(1734925661780577867, 1734925661780577887, 15, 476320, 183022, 20366, 7166, 4294967295, 0, 39, "
        "4294967295, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (999, 15), (276878, 7);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "476320";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "999";
    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.rankId, "999");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.rankId, "999");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().from.metaType, "CANN_API");
    EXPECT_EQ(responseBody.unitAllFlows.front().flows.front().to.metaType, "Ascend Hardware");
    RestoreRepoFunc();
}

// PyTorch->CANN->Ascend Hardware连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromPyTorchToCANNToAscendHardware)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 19, 7052, 0);";
    const std::string pytorchData =
        "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
        "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
        "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    EXPECT_EQ(responseBody.unitAllFlows[1].cat, "async_npu");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    ASSERT_EQ(responseBody.unitAllFlows[1].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 80); // 80
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[0].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[0].to.timestamp, 80); // 80
    RestoreRepoFunc();
}

// PyTorch->CANN->Ascend Hardware->Communication连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromPyTorchToCANNToAscendHardwareToCommunication)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 19, 7052, 0);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    EXPECT_EQ(responseBody.unitAllFlows[1].cat, "async_npu");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 2); // 2
    ASSERT_EQ(responseBody.unitAllFlows[1].flows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 80); // 80
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.timestamp, 110); // 110
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[0].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[0].to.timestamp, 80); // 80
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[1].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[1].flows[1].to.timestamp, 110); // 110
    RestoreRepoFunc();
}

// CANN->Ascend Hardware->Communication连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线，npumonitor采集数据无PyTorch层
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromCANNToAscendHardwareToCommunication)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 19, 7052, 0);";
    // 同connectionId，只连接第一个
    const std::string taskData =
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
        "globalPid, taskType, contextId, streamId, taskId, modelId, depth) VALUES "
        "(80, 100, 0, 19, 183022, 20366, 7166, 4294967295, 0, 39, 4294967295, 0),"
        "(90, 110, 0, 19, 183022, 20366, 7166, 4294967295, 0, 39, 4294967295, 0);";
    // 同connectionId，只连接第一个
    const std::string communicationOpData =
        "INSERT INTO COMMUNICATION_OP (opName, startNs, endNs, connectionId, "
        "groupName, opId, relay, retry, dataType, algType, count, opType, waitNs) "
        "VALUES (6, 110, 130, 19, 8, 1, 0, 0, 4, 9, 1, 10, 726280), (8, 150, 170, 19, 8, 2, 0, 0, 4, 9, 1, 10, 2985);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 80); // 80
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.timestamp, 110); // 110
    RestoreRepoFunc();
}

// 增加测试用例
// CANN->Communication连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线，npumonitor采集数据无PyTorch层，可能无Ascend Hardware层
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromCANNToCommunication)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 19, 7052, 0);";
    // 同connectionId，只连接第一个
    const std::string communicationOpData =
        "INSERT INTO COMMUNICATION_OP (opName, startNs, endNs, connectionId, "
        "groupName, opId, relay, retry, dataType, algType, count, opType, waitNs) "
        "VALUES (6, 110, 130, 19, 8, 1, 0, 0, 4, 9, 1, 10, 726280), (8, 150, 170, 19, 8, 2, 0, 0, 4, 9, 1, 10, 2985);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "HostToDevice");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 50); // 50
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 110); // 110
    RestoreRepoFunc();
}

// PyTorch->Ascend Hardware->Communication连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromPyTorchToAscendHardwareToCommunication)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string pytorchData =
        "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
        "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
        "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);";
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 20, 7052, 0);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, taskDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "Ascend Hardware";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "async_npu");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 80); // 80
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.timestamp, 110); // 110
    RestoreRepoFunc();
}

// PyTorch->Ascend Hardware连线，修改前不能通过，修改后能通过，每条泳道最多有一个算子和其它泳道连线
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromPyTorchToAscendHardware)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string pytorchData =
        "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
        "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
        "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);";
    const std::string cannApiData =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, "
        "name, depth) VALUES (50, 70, 10000, 87471303975183, 20, 7052, 0);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, taskDataInsertForQueryUnitFlows);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "Ascend Hardware";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "async_npu");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 80); // 80
    RestoreRepoFunc();
}

// MSTX连线，修改前和修改后都能通过，只允许一对一
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromMSTXToAscendHardware)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);

    const std::string taskData =
        "INSERT INTO TASK (startNs, endNs, deviceId, connectionId, globalTaskId, "
        "globalPid, taskType, contextId, streamId, taskId, modelId, depth) "
        "VALUES (10, 30, 0, 4294967295, 7480, 1984976, 1, 4294967295, 2, 12658, 4294967295, 0),"
        "(20, 40, 0, 4000000002, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, 0),"
        "(25, 70, 0, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, 0),"
        "(180, 200, 0, 19, 4000000001, 20366, 7166, 4294967295, 0, 39, 4294967295, 0);";
    std::string mstxData =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(10, 20, 2, 4294967295, 4294967295, 447, 4754301164515056, 4754301164515056, 239, 4000000001, 0),"
        "(12, 20, 2, 4294967295, 4294967295, 448, 4754301164515056, 4754301164515056, 240, 4000000002, 0);";

    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, mstxData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "MSTX_EVENTS";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "MsTx");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.rankId, "0");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.rankId, "0");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 10); // 10
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 25); // 25
    RestoreRepoFunc();
}

// async_task_queue连线，修改前和修改后都能通过，只允许一对一，增加deque在前测试
// fwdbwd连线，修改前和修改后都能通过，只允许一对一
TEST_F(DbTraceDatabaseTest, TestQueryUnitFlowsFromPyTorchToPyTorchFlowTypeFwdBwd)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CONNECTION_IDS, TableName::DB_CANN_API, TableName::DB_PYTORCH_API,
        TableName::DB_TASK, TableName::DB_NPU_INFO, TableName::DB_MSTX_EVENTS,
        TableName::DB_COMMUNICATION_OP, TableName::DB_RANK_DEVICE_MAP};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);

    const std::string pytorchData =
        "INSERT INTO PYTORCH_API (startNs, endNs, globalTid, connectionId, name, "
        "sequenceNumber, fwdThreadId, inputDtypes, inputShapes, callchainId, type, depth) "
        "VALUES (20, 40, 17738580008830245, 1, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3),"
        "(30, 90, 17738580008830245, 25, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3),"
        "(150, 170, 17738580008830245, 79, 268435456, NULL, NULL, NULL, NULL, NULL, 50002, 3);";
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19), (25, 19), (79, 19);";

    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (0, 'abc')";
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, rankDeviceMapDataInsertForQueryUnitFlows);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "PYTORCH_API";
    requestParams.rankId = "0";

    Dic::Protocol::UnitFlowsBody responseBody;
    MockNpuInfoRepoFunc();
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].cat, "fwdbwd");
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1);
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.rankId, "0");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.rankId, "0");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.timestamp, 20); // 20
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.timestamp, 30); // 30
    RestoreRepoFunc();
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromMSTX_EVENTS_ToAscendHardware_HaveName)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData = "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string mstxData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_MSTX_EVENTS);
    const std::string stringIdsData =
        "INSERT INTO STRING_IDS (id, value) VALUES (7166, 'task_name_1'), (342, 'dataloader');";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, mstxData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "MSTX_EVENTS";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1); // 1
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1); // 1
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.metaType, "MSTX_EVENTS");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.name, "dataloader");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.metaType, "Ascend Hardware");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.name, "task_name_1");
}


TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromPyTorch_ToCANN_HaveName_Fail)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string cannApiData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_CANN_API);
    const std::string pytorchData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_PYTORCH_API);
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromPyTorch_ToAscendHardware_HaveName)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string pytorchData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_PYTORCH_API);
    const std::string stringIdsData =
        "INSERT INTO STRING_IDS (id, value) VALUES "
        " (7166, 'task_name_1'),"
        " (268435456, 'python_name_1');";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "PYTORCH_API";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1); // 1
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1); // 1
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.metaType, "PYTORCH_API");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.name, "python_name_1");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.metaType, "Ascend Hardware");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.name, "task_name_1");
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromPyTorch_ToCommunication_HaveName)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string pytorchData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_PYTORCH_API);
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string communicationOpData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_COMMUNICATION_OP);
    const std::string stringIdsData =
        "INSERT INTO STRING_IDS (id, value) VALUES "
        " (393, 'hcom_broadcast__550_0_1'),"
        " (268435456, 'python_name_1');";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, pytorchData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "PYTORCH_API";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1); // 1
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.metaType, "PYTORCH_API");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.name, "python_name_1");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.metaType, "HCCL");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.name, "hcom_broadcast__550_0_1");
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromCANN_ToAscendHardware_HaveName)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string cannApiData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_CANN_API);
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string stringIdsData =
        "INSERT INTO STRING_IDS (id, value) VALUES "
        " (7052, 'cann_name_1'),"
        " (7166, 'task_name_1');";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1); // 1
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 1); // 1
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.metaType, "CANN_API");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].from.name, "cann_name_1");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.metaType, "Ascend Hardware");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[0].to.name, "task_name_1");
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromCANN_ToCommunication_HaveName)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string cannApiData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_CANN_API);
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string communicationOpData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_COMMUNICATION_OP);
    const std::string stringIdsData =
        "INSERT INTO STRING_IDS (id, value) VALUES "
        " (7052, 'cann_name_1'),"
        " (393, 'hcom_broadcast__550_0_1');";
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, cannApiData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, communicationOpData);
    DatabaseTestCaseMockUtil::InsertData(db, stringIdsData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "19";
    requestParams.metaType = "CANN_API";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    ASSERT_EQ(result, true);
    ASSERT_EQ(responseBody.unitAllFlows.size(), 1); // 1
    ASSERT_EQ(responseBody.unitAllFlows[0].flows.size(), 2); // 2
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.metaType, "CANN_API");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].from.name, "cann_name_1");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.metaType, "HCCL");
    EXPECT_EQ(responseBody.unitAllFlows[0].flows[1].to.name, "hcom_broadcast__550_0_1");
}

TEST_F(DbTraceDatabaseTest, TestQueryUnitFlows_FromAscendHardware_ToCommunication_HaveName_Fail)
{
    std::recursive_mutex testMutex;
    MockDatabase2 database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    const std::vector<TableName> list{TableName::DB_CANN_API, TableName::DB_MSTX_EVENTS, TableName::DB_PYTORCH_API,
        TableName::DB_NPU_INFO, TableName::DB_TASK, TableName::DB_COMMUNICATION_OP, TableName::DB_CONNECTION_IDS,
        TableName::DB_STRING_IDS};
    DatabaseTestCaseMockUtil::CreateTablesFromList(db, list);
    const std::string connectIdsData =
        "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 19);";
    const std::string npuInfoData = "INSERT INTO NPU_INFO (id, name) VALUES (1, 'abc')";
    const std::string taskData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_TASK);
    const std::string communcationOpData = FLOW_INSERT_DATA_SQL_MAP.at(TableName::DB_COMMUNICATION_OP);
    DatabaseTestCaseMockUtil::InsertData(db, connectIdsData);
    DatabaseTestCaseMockUtil::InsertData(db, npuInfoData);
    DatabaseTestCaseMockUtil::InsertData(db, taskData);
    DatabaseTestCaseMockUtil::InsertData(db, communcationOpData);
    std::string sql = "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
    DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string insertSql = "INSERT INTO RANK_DEVICE_MAP (rankId, deviceId) VALUES (1, 1);";
    DatabaseTestCaseMockUtil::InsertData(db, insertSql);
    database.SetDbPtr(db);

    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.id = "1";
    requestParams.metaType = "ASCEND_HARDWARE";
    requestParams.rankId = "1";

    Dic::Protocol::UnitFlowsBody responseBody;
    bool result = database.QueryUnitFlows(requestParams, responseBody, 0, 0);
    EXPECT_EQ(result, false);
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenPython)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::API);
    trackQueryVec.emplace_back(item);
    params.order = "descend";
    params.isMatchCase = true;
    params.isMatchExact = true;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where value like ?)  SELECT api.ROWID as id, 'pytorch' as tid, "
        "api.globalTid as pid, api.startNs as timestamp, api.endNs as endTime, api.depth, '' as deviceId, ids.value as "
        "value from PYTORCH_API  api join ids on ids.id = api.name WHERE api.globalTid = ? AND api.startNs >= ? AND "
        "api.endNs <= ?  ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenCann)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::CANN_API);
    trackQueryVec.emplace_back(item);
    params.order = "asc";
    params.isMatchCase = true;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where value like '%'||?||'%')  SELECT cann.connectionId as id, "
        "cann.globalTid as pid, cann.type as tid, cann.startNs as timestamp, cann.endNs as endTime, cann.depth, '' as "
        "deviceId, ids.value from CANN_API  cann join ids on ids.id = cann.name WHERE globalTid = ? AND type = ? AND "
        "startNs >= ? AND endNs <= ?  ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenMstx)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::MS_TX);
    trackQueryVec.emplace_back(item);
    params.order = "asc";
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where lower(value) like lower('%'||?||'%'))  SELECT mstx.ROWID "
        "as id, mstx.globalTid as pid, mstx.domainId as tid, mstx.startNs as timestamp, mstx.endNs as endTime, mstx.depth, '' "
        "as deviceId, ids.value from MSTX_EVENTS  mstx join ids on ids.id = mstx.message WHERE globalTid = ? AND "
        "startNs >= ? AND endNs <= ?  ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenOsrt)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::OSRT_API);
    trackQueryVec.emplace_back(item);
    params.order = "asc";
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where lower(value) like lower('%'||?||'%'))  "
        "SELECT osrt.ROWID AS id, 'OSRT_API' AS tid, osrt.globalTid AS pid, osrt.startNs AS timestamp, "
        "osrt.endNs AS endTime, 0 AS depth, '' AS deviceId, ids.value AS value FROM OSRT_API  osrt "
        "JOIN ids ON ids.id = osrt.name WHERE osrt.globalTid = ? AND osrt.startNs >= ? AND osrt.endNs <= ?  "
        "ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenHardWare)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::ASCEND_HARDWARE);
    trackQueryVec.emplace_back(item);
    params.order = "asc";
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT hadware.id "
        "as id, hadware.pid as pid, hadware.tid as tid, hadware.timestamp as timestamp, hadware.endTime as endTime, "
        "hadware.depth as depth, hadware.deviceId as deviceId, ids.value  FROM (SELECT coalesce(c.name, m.message, "
        "s.name, main.taskType) as name, main.ROWID AS id, 'Ascend Hardware' as pid, main.streamId as tid,main.startNs "
        "as timestamp, main.endNs as endTime, main.depth as depth, main.deviceId as deviceId FROM TASK main left join "
        "COMPUTE_TASK_INFO c on c.globalTaskId = main.globalTaskId left join MSTX_EVENTS m on (m.connectionId = "
        "main.connectionId and  m.connectionId != 4294967295 ) left join COMMUNICATION_SCHEDULE_TASK_INFO s on "
        "main.globalTaskId = s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND main.startNs >= ? AND "
        "main.endNs <= ?) hadware  join ids on ids.id = hadware.name ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenGroup)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::HCCL);
    trackQueryVec.emplace_back(item);
    item.threadId = "999group";
    params.order = "asc";
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT main.ROWID "
        "as id, 'HCCL' as pid, ci.groupName||'_'||ci.planeId as tid, main.startNs as timestamp, main.endNs as endTime, "
        "main.depth, main.deviceId as deviceId, ids.value from TASK main join COMMUNICATION_TASK_INFO ci on "
        "ci.globalTaskId = main.globalTaskId join ids on ids.id = ci.taskType WHERE main.deviceId = ? and ci.groupName "
        "= ? AND ci.planeId = ? AND main.startNs >= ? AND main.endNs <= ? ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetLockRangeSqlWhenPlane)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    Dic::Module::Timeline::SearchAllSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::HCCL);
    trackQueryVec.emplace_back(item);
    item.threadId = "999";
    params.order = "asc";
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = Dic::Module::Timeline::TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id, value from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT main.ROWID "
        "as id, 'HCCL' as pid, ci.groupName||'_'||ci.planeId as tid, main.startNs as timestamp, main.endNs as endTime, "
        "main.depth, main.deviceId as deviceId, ids.value from TASK main join COMMUNICATION_TASK_INFO ci on "
        "ci.globalTaskId = main.globalTaskId join ids on ids.id = ci.taskType WHERE main.deviceId = ? and ci.groupName "
        "= ? AND ci.planeId = ? AND main.startNs >= ? AND main.endNs <= ? ORDER BY timestamp DESC  LIMIT ? OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenPython)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = true;
    params.isMatchExact = true;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where value like ?)  SELECT api.ROWID as id, 'pytorch' "
        "as tid, api.globalTid as pid, api.startNs as timestamp, api.endNs as endTime, api.depth, "
        "'PYTORCH_API' as metaType from PYTORCH_API  api join ids on ids.id = api.name WHERE api.globalTid = ? "
        "AND api.startNs >= ? AND api.endNs <= ?  ORDER BY timestamp ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenCann)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::CANN_API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = true;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where value like '%'||?||'%')  SELECT cann.connectionId "
        "as id, cann.globalTid as pid, cann.type as tid, cann.startNs as timestamp, cann.endNs as endTime, "
        "cann.depth, 'CANN_API' as metaType from CANN_API  cann join ids on ids.id = cann.name WHERE globalTid = ? "
        "AND type = ? AND startNs >= ? AND endNs <= ?  ORDER BY timestamp ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenMstx)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::MS_TX);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))  SELECT "
                   "mstx.ROWID as id, mstx.globalTid as pid, mstx.domainId as tid, mstx.startNs as timestamp, "
                   "mstx.endNs as endTime, mstx.depth, 'MSTX_EVENTS' as metaType from MSTX_EVENTS  mstx join ids "
                   "on ids.id = mstx.message WHERE globalTid = ? AND startNs >= ? AND endNs <= ?  ORDER BY timestamp "
                   "ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenOsrt)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::OSRT_API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))  SELECT osrt.ROWID AS id, "
        "'OSRT_API' AS tid, osrt.globalTid AS pid, osrt.startNs AS timestamp, osrt.endNs AS endTime, 0 AS depth, "
        "'OSRT_API' as metaType FROM OSRT_API  osrt JOIN ids ON ids.id = osrt.name WHERE osrt.globalTid = ? AND "
        "osrt.startNs >= ? AND osrt.endNs <= ?  ORDER BY timestamp ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenHardWare)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::ASCEND_HARDWARE);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT hadware.id as "
        "id, hadware.pid as pid, hadware.tid as tid, hadware.timestamp as timestamp, hadware.endTime as endTime, "
        "hadware.depth as depth, 'Ascend Hardware' as metaType  FROM (SELECT coalesce(c.name, m.message, s.name, "
        "main.taskType) as name, main.ROWID AS id, 'Ascend Hardware' as pid, main.streamId as tid,main.startNs as "
        "timestamp, main.endNs as endTime, main.depth as depth FROM TASK main left join COMPUTE_TASK_INFO c on "
        "c.globalTaskId = main.globalTaskId left join MSTX_EVENTS m on  (m.connectionId = main.connectionId and  "
        "m.connectionId != 4294967295 ) left join COMMUNICATION_SCHEDULE_TASK_INFO s on main.globalTaskId = "
        "s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND main.startNs >= ? AND main.endNs <= ?) "
        "hadware  join ids on ids.id = hadware.name  ORDER BY timestamp ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchSliceNameWithLockRangeSqlWhenHccl)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::HCCL);
    item.threadId = "888group";
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))  "
                   "SELECT op.opId as id, 'HCCL' as pid, op.groupName||'group' as tid, op.startNs as timestamp, "
                   "op.endNs as endTime, 0 as depth, 'HCCL' as metaType from COMMUNICATION_OP op join ids on id = "
                   "op.opName WHERE op.groupName = ? AND op.startNs >= ? AND op.endNs <= ?  ORDER BY timestamp ASC "
                   "LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenPython)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = true;
    params.isMatchExact = true;
    params.rankId = "ll Host";
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where value like ?) SELECT count(1) as count FROM (SELECT "
        "name from PYTORCH_API WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) api join ids on id = api.name ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenCann)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::CANN_API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = true;
    params.isMatchExact = false;
    params.rankId = "ll Host";
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where value like '%'||?||'%') SELECT count(1) as count FROM (SELECT "
        "name from CANN_API WHERE globalTid = ? AND type = ? AND startNs >= ? AND endNs <= ?) cann join ids on "
        "id = cann.name ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenMstx)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::MS_TX);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    params.rankId = "ll Host";
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT count(1) "
        "as count FROM (SELECT message from MSTX_EVENTS WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) mstx "
        "join ids on id = mstx.message ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenOsrt)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    item.rankId = path;
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::OSRT_API);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    params.rankId = "ll Host";
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) "
        "SELECT count(1) as count FROM (SELECT name from OSRT_API WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) "
        "osrt join ids on id = osrt.name ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenHardWare)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::ASCEND_HARDWARE);
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT count(1) as count "
        "FROM (SELECT coalesce(c.name, m.message, s.name, main.taskType) as name FROM TASK main  left join "
        "COMPUTE_TASK_INFO c on c.globalTaskId = main.globalTaskId  left join MSTX_EVENTS m on  (m.connectionId = "
        "main.connectionId and  m.connectionId != 4294967295 ) left join COMMUNICATION_SCHEDULE_TASK_INFO s on "
        "main.globalTaskId = s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND main.startNs >= ? AND "
        "main.endNs <= ?) hadware  join ids on id = hadware.name ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenHccl)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    Dic::Module::Timeline::SearchCountParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::HCCL);
    item.threadId = "888group";
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql = DbTraceDataBase::GetSearchCountWithLockSql(params, trackQueryVec);
    EXPECT_EQ(sql, "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT count(1) "
        "as count FROM (SELECT opName as name from COMMUNICATION_OP WHERE groupName = ? AND startNs >= ? AND endNs "
        "<= ?) op join ids on id = op.name ");
}

TEST_F(DbTraceDatabaseTest, GetSearchCountWithLockSqlWhenHcclPlane)
{
    std::vector<Dic::Module::Timeline::TrackQuery> trackQueryVec;
    Dic::Module::Timeline::TrackQuery item;
    std::string path = "lll";
    Dic::Module::Timeline::SearchSliceParams params;
    item.metaType = PROCESS_TYPE_ES.at(PROCESS_TYPE::HCCL);
    item.threadId = "888";
    trackQueryVec.emplace_back(item);
    params.isMatchCase = false;
    params.isMatchExact = false;
    std::string sql =
        Dic::Module::Timeline::TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQueryVec, path);
    EXPECT_EQ(sql,
        "with ids as (select id from STRING_IDS where lower(value) like lower('%'||?||'%')) SELECT main.ROWID as id, "
        "'HCCL' as pid, ci.groupName||'_'||ci.planeId as tid, main.startNs as timestamp, main.endNs as endTime, "
        "main.depth, 'HCCL' as metaType from TASK main join COMMUNICATION_TASK_INFO ci on ci.globalTaskId = "
        "main.globalTaskId join ids on ids.id = ci.taskType WHERE main.deviceId = ? and ci.groupName = ? AND "
        "ci.planeId = ? AND main.startNs >= ? AND main.endNs <= ? ORDER BY timestamp ASC LIMIT 1 OFFSET ?");
}

TEST_F(DbTraceDatabaseTest, ProcessByteAlignmentAnalyzerDataForDbTest)
{
    std::vector<Dic::Module::ByteAlignmentAnalyzerLargeOperatorInfo> largeOpInfo = {{"hcom1"}, {"hcom2"}};
    std::vector<Dic::Module::ByteAlignmentAnalyzerSmallOperatorInfo> smallOpInfo = {
        {"hcom3", "Memcpy", 2, "SDMA", "ON_CHIP"}, {"hcom1", "Memcpy", 2, "SDMA", "ON_CHIP"},
        {"hcom1", "Reduce_Inline", 2, "SDMA", "ON_CHIP"}, {"hcom2", "Memcpy", 2, "SDMA", "ON_CHIP"}};
    std::vector<Dic::Module::CommunicationLargeOperatorInfo> result;
    DbTraceDataBase::ProcessByteAlignmentAnalyzerDataForDb(result, largeOpInfo, smallOpInfo);
    ASSERT_EQ(result.size(), 2); // 2
    ASSERT_EQ(result[0].memcpyTasks.size(), 1);
    ASSERT_EQ(result[0].reduceInlineTasks.size(), 1);
    ASSERT_EQ(result[1].memcpyTasks.size(), 1);
    ASSERT_EQ(result[1].reduceInlineTasks.size(), 0);
}
// clang-format on
