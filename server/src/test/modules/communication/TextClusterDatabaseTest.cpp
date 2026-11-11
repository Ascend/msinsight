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
#include "../../DatabaseTestCaseMockUtil.h"
#include "TextClusterDatabase.h"

using namespace Dic::Global::PROFILER::MockUtil;
class TextClusterDatabaseTest : public ::testing::Test {
  protected:
    class MockDatabase : public Dic::Module::TextClusterDatabase {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : Dic::Module::TextClusterDatabase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };
};

/**
 * text场景设置并行策略,只设置了dp，pp，tp,兼容老版本
 */
TEST_F(TextClusterDatabaseTest, TestQueryParallelStrategyConfigWhenNotSetCpAndEp) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const int64_t expectPP = 12;
    const int64_t expectTP = 11;
    const int64_t expectDP = 10;
    const int64_t expectCP = 1;
    const int64_t expectEP = 1;
    Dic::Module::ParallelStrategyConfig config = {"ttttt", expectPP, expectTP, expectDP};
    std::string msg;
    std::string level = "configed";
    Dic::Module::ClusterBaseInfo baseInfo;
    database.InsertClusterBaseInfo(baseInfo);
    database.UpdateParallelStrategyConfig(config, level, msg);
    Dic::Module::ParallelStrategyConfig result;
    std::string resultLevel;
    database.QueryParallelStrategyConfig(result, resultLevel);
    EXPECT_EQ(result.ppSize, expectPP);
    EXPECT_EQ(result.tpSize, expectTP);
    EXPECT_EQ(result.dpSize, expectDP);
    EXPECT_EQ(result.cpSize, expectCP);
    EXPECT_EQ(result.epSize, expectEP);
    EXPECT_EQ(resultLevel, level);
}

/**
 * text场景设置并行策略,设置了dp，pp，tp,cp,ep
 */
TEST_F(TextClusterDatabaseTest, TestQueryParallelStrategyConfigWhenSetAllConfig) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const int64_t expectPP = 12;
    const int64_t expectTP = 11;
    const int64_t expectDP = 10;
    const int64_t expectCP = 18;
    const int64_t expectEP = 17;
    Dic::Module::ParallelStrategyConfig config = {"ttttt", expectPP, expectTP, expectDP, expectCP, expectEP};
    std::string msg;
    std::string level = "configed";
    Dic::Module::ClusterBaseInfo baseInfo;
    database.InsertClusterBaseInfo(baseInfo);
    database.UpdateParallelStrategyConfig(config, level, msg);
    Dic::Module::ParallelStrategyConfig result;
    std::string resultLevel;
    database.QueryParallelStrategyConfig(result, resultLevel);
    EXPECT_EQ(result.ppSize, expectPP);
    EXPECT_EQ(result.tpSize, expectTP);
    EXPECT_EQ(result.dpSize, expectDP);
    EXPECT_EQ(result.cpSize, expectCP);
    EXPECT_EQ(result.epSize, expectEP);
    EXPECT_EQ(resultLevel, level);
}

/**
 * text场景查询并行策略,db未打开
 */
TEST_F(TextClusterDatabaseTest, TestQueryParallelStrategyConfigWhenDbNotOpen) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const int64_t expectPP = 0;
    const int64_t expectTP = 0;
    const int64_t expectDP = 0;
    const int64_t expectCP = 1;
    const int64_t expectEP = 1;
    Dic::Module::ParallelStrategyConfig result;
    std::string resultLevel;
    bool ans = database.QueryParallelStrategyConfig(result, resultLevel);
    EXPECT_EQ(ans, false);
    EXPECT_EQ(result.ppSize, expectPP);
    EXPECT_EQ(result.tpSize, expectTP);
    EXPECT_EQ(result.dpSize, expectDP);
    EXPECT_EQ(result.cpSize, expectCP);
    EXPECT_EQ(result.epSize, expectEP);
    EXPECT_EQ(resultLevel, "");
}

/**
 * text场景设置并行策略,db未打开
 */
TEST_F(TextClusterDatabaseTest, TestUpdateParallelStrategyConfigWhenDbNotOpen) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const int64_t expectPP = 12;
    const int64_t expectTP = 11;
    const int64_t expectDP = 10;
    const int64_t expectCP = 18;
    const int64_t expectEP = 17;
    Dic::Module::ParallelStrategyConfig config = {"ttttt", expectPP, expectTP, expectDP, expectCP, expectEP};
    std::string msg;
    std::string level = "configed";
    bool ans = database.UpdateParallelStrategyConfig(config, level, msg);
    EXPECT_EQ(ans, false);
}

TEST_F(TextClusterDatabaseTest, UpdateCollectTimeInfoSuccess) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    Dic::Protocol::SummaryBaseInfo baseInfo;
    baseInfo.collectStartTime = 1718682999267391232;
    baseInfo.collectDuration = 123.45;
    bool res = database.UpdateCollectTimeInfo(baseInfo);
    EXPECT_EQ(res, true);
    Dic::Protocol::SummaryBaseInfo ans;
    database.QueryBaseInfo(ans);
    EXPECT_EQ(baseInfo.collectStartTime, ans.collectStartTime);
    EXPECT_EQ(baseInfo.collectDuration, ans.collectDuration);
}

TEST_F(TextClusterDatabaseTest, MissingMetricsAreStoredAsMinusOneAndPreserveRealZero) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    ASSERT_TRUE(database.CreateTable());
    Dic::Module::CommunicationTimeInfo time;
    time.iterationId = "1";
    time.rankId = "0";
    time.opName = "hcom_test";
    time.opSuffix = "group";
    time.elapseTime = 0;
    std::vector<Dic::Module::CommunicationTimeInfo> times{time};
    database.InsertTimeInfoList(times);
    Dic::Module::CommunicationBandWidth bandwidth;
    bandwidth.iterationId = "1";
    bandwidth.rankId = "0";
    bandwidth.opName = "hcom_test";
    bandwidth.opSuffix = "group";
    bandwidth.transportType = "SDMA";
    std::vector<Dic::Module::CommunicationBandWidth> bandwidths{bandwidth};
    database.InsertBandwidthList(bandwidths);

    sqlite3_stmt *statement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(dbPtr,
                  "SELECT start_time, elapse_time, transit_time, synchronization_time, wait_time, idle_time, "
                  "synchronization_time_ratio, wait_time_ratio FROM communication_time_info",
                  -1, &statement, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    for (int column = 0; column < 8; ++column) {
        EXPECT_NE(sqlite3_column_type(statement, column), SQLITE_NULL);
        EXPECT_DOUBLE_EQ(sqlite3_column_double(statement, column), column == 1 ? 0 : -1);
    }
    sqlite3_finalize(statement);
    statement = nullptr;
    ASSERT_EQ(
        sqlite3_prepare_v2(dbPtr, "SELECT bandwidth_size, transit_size, transit_time FROM communication_bandwidth_info",
            -1, &statement, nullptr),
        SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    for (int column = 0; column < 3; ++column) {
        EXPECT_NE(sqlite3_column_type(statement, column), SQLITE_NULL);
        EXPECT_DOUBLE_EQ(sqlite3_column_double(statement, column), -1);
    }
    sqlite3_finalize(statement);

    Dic::Protocol::OperatorDetailsParam params;
    params.iterationId = "1";
    params.groupIdHash = "group";
    params.rankId = "0";
    params.currentPage = 1;
    params.pageSize = 10;
    Dic::Protocol::OperatorDetailsResBody result;
    ASSERT_TRUE(database.QueryAllOperators(params, result));
    ASSERT_EQ(result.allOperators.size(), 1);
    EXPECT_DOUBLE_EQ(result.allOperators[0].startTime, -1);
    EXPECT_DOUBLE_EQ(result.allOperators[0].elapseTime, 0);
    EXPECT_DOUBLE_EQ(result.allOperators[0].sdmaBw, -1);
    EXPECT_DOUBLE_EQ(result.allOperators[0].rdmaBw, -1);
}

TEST_F(TextClusterDatabaseTest, DurationPreservesMissingTransportAndAbsentBandwidthRows) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    ASSERT_TRUE(database.CreateTable());
    ASSERT_TRUE(database.ExecSql(
        "INSERT INTO communication_time_info "
        "(iteration_id, rank_id, op_name, op_suffix, start_time, elapse_time, transit_time, "
        "synchronization_time, wait_time, idle_time, synchronization_time_ratio, wait_time_ratio) "
        "VALUES ('1', '0', 'hcom_test', 'group', -1, 5, -1, -1, -1, -1, -1, -1);"
        "INSERT INTO communication_bandwidth_info "
        "(iteration_id, rank_id, op_name, op_suffix, transport_type, bandwidth_size, transit_time) VALUES "
        "('1', '0', 'hcom_test', 'group', 'SDMA', -1, -1),"
        "('1', '0', 'hcom_test', 'group', 'RDMA', 0, 0);"));
    Dic::Protocol::DurationListParams params;
    params.iterationId = "1";
    params.groupIdHash = "group";
    params.operatorName = "hcom_test";
    std::vector<Dic::Module::DurationDo> result;
    ASSERT_TRUE(database.QueryDurationList(params, result));
    ASSERT_EQ(result.size(), 1);
    EXPECT_DOUBLE_EQ(result[0].startTime, -1);
    EXPECT_DOUBLE_EQ(result[0].transitTime, -1);
    EXPECT_DOUBLE_EQ(result[0].synchronizationTime, -1);
    EXPECT_DOUBLE_EQ(result[0].waitTimeRatio, -1);
    EXPECT_DOUBLE_EQ(result[0].sdmaBw, -1);
    EXPECT_DOUBLE_EQ(result[0].sdmaTime, -1);
    EXPECT_DOUBLE_EQ(result[0].rdmaBw, 0);
    EXPECT_DOUBLE_EQ(result[0].rdmaTime, 0);

    ASSERT_TRUE(database.ExecSql("DELETE FROM communication_bandwidth_info;"));
    result.clear();
    ASSERT_TRUE(database.QueryDurationList(params, result));
    ASSERT_EQ(result.size(), 1);
    EXPECT_DOUBLE_EQ(result[0].elapseTime, 5);
    EXPECT_DOUBLE_EQ(result[0].sdmaBw, -1);
    EXPECT_DOUBLE_EQ(result[0].rdmaBw, -1);
}
