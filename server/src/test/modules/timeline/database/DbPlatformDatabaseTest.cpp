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
#include "DbPlatformDataBase.h"
#include "DataBaseManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::Timeline;

namespace Dic::Protocol {
using namespace Dic::Module::Timeline;
}

class DbPlatformDatabaseTest : public ::testing::Test {
  protected:
    class MockPlatformDatabase : public Dic::Module::FullDb::DbPlatformDataBase {
      public:
        explicit MockPlatformDatabase(std::recursive_mutex &sqlMutex) : DbPlatformDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };

    static const std::string CREATE_TABLE_TITLES_SQL;
    static const std::string CREATE_TABLE_LEVELS_SQL;
    static const std::string CREATE_TABLE_METRICS_SQL;
    static const std::string CREATE_TABLE_SCALING_SQL;

    void InsertTestTitles(sqlite3 *db) {
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (1, 'Total External Traffic Ratio', 'desc', 1, 'Ratio');");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_titles_names VALUES (2, 'Socket 0', 'desc', 0, '');");
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (3, 'External Traffic Ratio', 'desc', 1, 'Ratio');");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_titles_names VALUES (4, 'Numa 0', 'desc', 0, '');");
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (5, 'DRAM read bandwidth', 'desc', 0, 'GB/s');");
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (6, 'DRAM write bandwidth', 'desc', 0, 'GB/s');");
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (7, 'CPU utilization', 'desc', 0, 'Ratio');");
        DatabaseTestCaseMockUtil::InsertData(
            db, "INSERT INTO p_titles_names VALUES (8, 'PCIe throughput', 'desc', 0, 'GB/s');");
    }

    void InsertTestLevels(sqlite3 *db) {
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_levels_hierarchy_names VALUES (1, 2, 3, 0);");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_levels_hierarchy_names VALUES (2, 2, 4, 5);");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_levels_hierarchy_names VALUES (3, 2, 4, 6);");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_levels_hierarchy_names VALUES (4, 2, 4, 7);");
        DatabaseTestCaseMockUtil::InsertData(db, "INSERT INTO p_levels_hierarchy_names VALUES (5, 2, 4, 8);");
    }
};

const std::string DbPlatformDatabaseTest::CREATE_TABLE_TITLES_SQL =
    "CREATE TABLE IF NOT EXISTS p_titles_names (id INTEGER PRIMARY KEY, name TEXT, description TEXT, "
    "summary_flag INTEGER, measurement_unit TEXT);";

const std::string DbPlatformDatabaseTest::CREATE_TABLE_LEVELS_SQL =
    "CREATE TABLE IF NOT EXISTS p_levels_hierarchy_names (id INTEGER PRIMARY KEY, title0_id INTEGER, "
    "title1_id INTEGER, title2_id INTEGER);";

const std::string DbPlatformDatabaseTest::CREATE_TABLE_METRICS_SQL =
    "CREATE TABLE IF NOT EXISTS p_metrics (id INTEGER PRIMARY KEY, ts INTEGER, value REAL, levels_id INTEGER);";

const std::string DbPlatformDatabaseTest::CREATE_TABLE_SCALING_SQL =
    "CREATE TABLE IF NOT EXISTS p_scaling_values (id INTEGER PRIMARY KEY, level_id INTEGER, max_value REAL);";

TEST_F(DbPlatformDatabaseTest, QueryLevelData) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    InsertTestTitles(db);
    InsertTestLevels(db);
    database.SetDbPtr(db);

    Dic::Module::FullDb::LevelDataMap levels;
    bool result = database.QueryLevelData(levels);

    EXPECT_TRUE(result);
    const size_t expectSize = 5;
    EXPECT_EQ(levels.size(), expectSize);

    EXPECT_EQ(levels[1].levelId, 1);
    EXPECT_EQ(levels[1].title0Id, 2);
    EXPECT_EQ(levels[1].title1Id, 3);
    EXPECT_EQ(levels[1].title2Id, 0);

    EXPECT_EQ(levels[2].levelId, 2);
    EXPECT_EQ(levels[2].title0Id, 2);
    EXPECT_EQ(levels[2].title1Id, 4);
    EXPECT_EQ(levels[2].title2Id, 5);
}

TEST_F(DbPlatformDatabaseTest, QueryLevelDataEmptyTable) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    database.SetDbPtr(db);

    Dic::Module::FullDb::LevelDataMap levels;
    bool result = database.QueryLevelData(levels);

    EXPECT_TRUE(result);
    EXPECT_EQ(levels.size(), 0);
}

TEST_F(DbPlatformDatabaseTest, QueryLevelDataTableNotExist) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);

    Dic::Module::FullDb::LevelDataMap levels;
    bool result = database.QueryLevelData(levels);

    EXPECT_FALSE(result);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformMetricsSortedByTs) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    InsertTestTitles(db);
    InsertTestLevels(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 300, 75.5, 2);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (2, 100, 50.0, 2);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (3, 200, 60.0, 3);");
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformMetric> metrics;
    bool result = database.QueryPlatformMetrics(metrics);

    EXPECT_TRUE(result);
    const size_t expectSize = 3;
    ASSERT_EQ(metrics.size(), expectSize);
    EXPECT_EQ(metrics[0].ts, 100);
    EXPECT_EQ(metrics[1].ts, 200);
    EXPECT_EQ(metrics[2].ts, 300);
    EXPECT_EQ(metrics[0].levelId, 2);
    EXPECT_DOUBLE_EQ(metrics[0].value, 50.0);
    EXPECT_EQ(metrics[0].title0Id, 2);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformMetricsEmptyTable) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformMetric> metrics;
    bool result = database.QueryPlatformMetrics(metrics);

    EXPECT_TRUE(result);
    EXPECT_EQ(metrics.size(), 0);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformMetricsTableNotExist) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformMetric> metrics;
    bool result = database.QueryPlatformMetrics(metrics);

    EXPECT_FALSE(result);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformCounterDataNoFilters) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_SCALING_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (2, 200, 60.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_scaling_values (id, level_id, max_value) VALUES (1, 5, 3.0);");
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformCounterData> dataList;
    bool result = database.QueryPlatformCounterData(1, 0, 0, dataList);

    EXPECT_TRUE(result);
    const size_t expectSize = 2;
    ASSERT_EQ(dataList.size(), expectSize);
    EXPECT_DOUBLE_EQ(dataList[0].value, 50.0);
    EXPECT_DOUBLE_EQ(dataList[1].value, 60.0);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformCounterDataFilterByLevelId) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_SCALING_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (2, 200, 60.0, 2);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (3, 300, 70.0, 2);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_scaling_values (id, level_id, max_value) VALUES (1, 2, 100.0);");
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformCounterData> dataList;
    bool result = database.QueryPlatformCounterData(2, 0, 0, dataList);

    EXPECT_TRUE(result);
    const size_t expectSize = 2;
    ASSERT_EQ(dataList.size(), expectSize);
    EXPECT_DOUBLE_EQ(dataList[0].value, 60.0);
}

TEST_F(DbPlatformDatabaseTest, QueryPlatformCounterDataFilterNonExistentLevelId) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_SCALING_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    database.SetDbPtr(db);

    std::vector<Dic::Module::FullDb::PlatformCounterData> dataList;
    bool result = database.QueryPlatformCounterData(99, 0, 0, dataList);

    EXPECT_TRUE(result);
    EXPECT_EQ(dataList.size(), 0);
}

TEST_F(DbPlatformDatabaseTest, QueryUnitCounterBasic) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    InsertTestTitles(db);
    InsertTestLevels(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (2, 200, 60.0, 1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.rankId = "0";
    params.threadName = "External Traffic Ratio";
    params.threadId = "1";
    params.startTime = 0;
    params.endTime = 0;
    const uint64_t minTimestamp = 0;
    std::vector<Dic::Protocol::UnitCounterData> dataList;
    bool result = database.QueryUnitCounter(params, minTimestamp, dataList);

    EXPECT_TRUE(result);
    const size_t expectSize = 2;
    ASSERT_EQ(dataList.size(), expectSize);
    EXPECT_EQ(dataList[0].timestamp, 100);
    EXPECT_EQ(dataList[1].timestamp, 200);
}

TEST_F(DbPlatformDatabaseTest, QueryUnitCounterDeduplicatesConsecutiveSameValues) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    InsertTestTitles(db);
    InsertTestLevels(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (2, 200, 50.0, 1);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (3, 300, 70.0, 1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.rankId = "0";
    params.threadName = "External Traffic Ratio";
    params.threadId = "1";
    const uint64_t minTimestamp = 0;
    std::vector<Dic::Protocol::UnitCounterData> dataList;
    bool result = database.QueryUnitCounter(params, minTimestamp, dataList);

    EXPECT_TRUE(result);
    const size_t expectSize = 2;
    ASSERT_EQ(dataList.size(), expectSize);
    EXPECT_EQ(dataList[0].timestamp, 100);
    EXPECT_EQ(dataList[0].valueJsonStr, "{\"Ratio\":50.000000}");
    EXPECT_EQ(dataList[1].timestamp, 300);
    EXPECT_EQ(dataList[1].valueJsonStr, "{\"Ratio\":70.000000}");
}

TEST_F(DbPlatformDatabaseTest, QueryUnitCounterInvalidThreadId) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_METRICS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_LEVELS_SQL);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_TITLES_SQL);
    InsertTestTitles(db);
    InsertTestLevels(db);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_metrics (id, ts, value, levels_id) VALUES (1, 100, 50.0, 1);");
    database.SetDbPtr(db);

    Dic::Protocol::UnitCounterParams params;
    params.rankId = "0";
    params.threadName = "External Traffic Ratio";
    params.threadId = "not_a_number";
    const uint64_t minTimestamp = 0;
    std::vector<Dic::Protocol::UnitCounterData> dataList;
    bool result = database.QueryUnitCounter(params, minTimestamp, dataList);

    EXPECT_FALSE(result);
}

TEST_F(DbPlatformDatabaseTest, QueryScalingValues) {
    std::recursive_mutex testMutex;
    MockPlatformDatabase database(testMutex);
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, CREATE_TABLE_SCALING_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_scaling_values (id, level_id, max_value) VALUES (1, 1, 10.0);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_scaling_values (id, level_id, max_value) VALUES (2, 2, 20.0);");
    DatabaseTestCaseMockUtil::InsertData(
        db, "INSERT INTO p_scaling_values (id, level_id, max_value) VALUES (3, 3, 100.0);");
    database.SetDbPtr(db);

    Dic::Module::FullDb::ScalingValueMap scalingValues;
    bool result = database.QueryScalingValuesData(scalingValues);

    EXPECT_TRUE(result);
    const size_t expectSize = 3;
    ASSERT_EQ(scalingValues.size(), expectSize);
    EXPECT_DOUBLE_EQ(scalingValues[3], 100.0);
}
