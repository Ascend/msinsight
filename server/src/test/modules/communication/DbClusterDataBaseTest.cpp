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
#include "DbClusterDataBase.h"
#include "KernelMfuDatabaseAccesser.h"
#include "SummaryClusterStatisticsProtocol.h"

using namespace Dic::Global::PROFILER::MockUtil;
class DbClusterDataBaseTest : public ::testing::Test {
  protected:
    class MockDatabase : public Dic::Module::FullDb::DbClusterDataBase {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : Dic::Module::FullDb::DbClusterDataBase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };
};

class SummaryClusterDatabaseTest : public DbClusterDataBaseTest {
  protected:
    void SetUp() override {
        ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
        database.SetDbPtr(db);
    }

    sqlite3 *db = nullptr;
    std::recursive_mutex sqlMutex;
    MockDatabase database{sqlMutex};
};

TEST_F(SummaryClusterDatabaseTest, AggregatesRankTraceTimesInMilliseconds) {
    ASSERT_TRUE(database.ExecSql(R"(
        CREATE TABLE ClusterStepTraceTime(step TEXT, type TEXT, "index" TEXT, computing REAL,
            communication_not_overlapped REAL, overlapped REAL, communication REAL, free REAL, preparing REAL);
        INSERT INTO ClusterStepTraceTime VALUES
            ('2', 'rank', '10', 0, 0, 0, 0, 0, -1),
            ('2', 'rank', '2', 600, 200, 200, 400, 100, 100),
            ('step3', 'rank', '2', 200, 100, 50, 150, 100, 0),
            ('2', 'stage', '0', 99999, 99999, 99999, 99999, 99999, 99999);
    )"));

    Dic::Protocol::SummaryClusterTime clusterTime;
    ASSERT_TRUE(database.QuerySummaryClusterTime("step2", clusterTime));
    EXPECT_DOUBLE_EQ(clusterTime.computingNotOverlapped, 0.4);
    EXPECT_DOUBLE_EQ(clusterTime.communicationNotOverlapped, 0.2);
    EXPECT_DOUBLE_EQ(clusterTime.computingCommunicationOverlapped, 0.2);
    EXPECT_DOUBLE_EQ(clusterTime.free, 0.1);
    EXPECT_DOUBLE_EQ(clusterTime.preparing, 0.099);

    std::vector<Dic::Protocol::SummaryRankTime> ranks;
    ASSERT_TRUE(database.QuerySummaryRankTime("2", ranks));
    ASSERT_EQ(ranks.size(), 2);
    EXPECT_EQ(ranks[0].rankId, "2");
    EXPECT_DOUBLE_EQ(ranks[0].total, 1.0);
    EXPECT_DOUBLE_EQ(ranks[0].computing, 0.6);
    EXPECT_DOUBLE_EQ(ranks[0].communication, 0.4);
    EXPECT_DOUBLE_EQ(ranks[0].computeRatio, 60.0);
    EXPECT_DOUBLE_EQ(ranks[0].communicationRatio, 40.0);
    EXPECT_EQ(ranks[1].rankId, "10");
    EXPECT_DOUBLE_EQ(ranks[1].total, 0.0);
    EXPECT_DOUBLE_EQ(ranks[1].computeRatio, 0.0);
    EXPECT_DOUBLE_EQ(ranks[1].communicationRatio, 0.0);

    ASSERT_TRUE(database.QuerySummaryRankTime("3", ranks));
    ASSERT_EQ(ranks.size(), 1);
    EXPECT_DOUBLE_EQ(ranks[0].total, 0.4);
    ASSERT_TRUE(database.QuerySummaryRankTime("All", ranks));
    ASSERT_EQ(ranks.size(), 2);
    EXPECT_DOUBLE_EQ(ranks[0].total, 1.4);
    EXPECT_DOUBLE_EQ(ranks[0].computeRatio, 0.8 / 1.4 * 100.0);
    EXPECT_DOUBLE_EQ(ranks[0].communicationRatio, 0.55 / 1.4 * 100.0);
    ASSERT_TRUE(database.QuerySummaryClusterTime("all", clusterTime));
    EXPECT_DOUBLE_EQ(clusterTime.computingNotOverlapped, 0.55);
    EXPECT_DOUBLE_EQ(clusterTime.computingCommunicationOverlapped, 0.25);
    ASSERT_TRUE(database.QuerySummaryClusterTime("99", clusterTime));
    EXPECT_DOUBLE_EQ(clusterTime.computingNotOverlapped, 0.0);
    ASSERT_TRUE(database.QuerySummaryRankTime("99", ranks));
    EXPECT_TRUE(ranks.empty());
}

TEST_F(SummaryClusterDatabaseTest, SumsCommunicationDomainsAndUsesPreferredStepKey) {
    ASSERT_TRUE(database.ExecSql(R"(
        CREATE TABLE CommTimeStatistic(step TEXT, rank_id INTEGER, group_name TEXT, comm_transit_time REAL,
            comm_wait_time REAL, comm_sync_time REAL, comm_free_time REAL);
        INSERT INTO CommTimeStatistic VALUES
            ('step2', 10, 'a', 0, NULL, 3, 4),
            ('step2', 2, 'a', 1.25, 2, 3, 4),
            ('step2', 2, 'b', 0.50, NULL, 2, 1),
            ('step2', 2, 'c', NULL, NULL, NULL, NULL),
            ('2', 2, 'a', 10, 20, 30, 40),
            ('3', 2, 'a', 5, 4, 3, 2),
            ('step', 2, 'a', 2, 3, 4, 5);
    )"));

    std::vector<Dic::Protocol::SummaryCommunicationTime> ranks;
    ASSERT_TRUE(database.QuerySummaryCommunicationTime("2", ranks));
    ASSERT_EQ(ranks.size(), 2);
    EXPECT_EQ(ranks[0].rankId, "2");
    EXPECT_DOUBLE_EQ(ranks[0].transit, 1.75);
    EXPECT_DOUBLE_EQ(ranks[0].wait, 2.0);
    EXPECT_DOUBLE_EQ(ranks[0].synchronization, 5.0);
    EXPECT_DOUBLE_EQ(ranks[0].free, 5.0);
    EXPECT_EQ(ranks[1].rankId, "10");
    EXPECT_DOUBLE_EQ(ranks[1].wait, 0.0);
    ASSERT_TRUE(database.QuerySummaryCommunicationTime("step3", ranks));
    ASSERT_EQ(ranks.size(), 1);
    EXPECT_DOUBLE_EQ(ranks[0].transit, 5.0);
    ASSERT_TRUE(database.QuerySummaryCommunicationTime("step", ranks));
    ASSERT_EQ(ranks.size(), 1);
    EXPECT_DOUBLE_EQ(ranks[0].transit, 2.0);
    ASSERT_TRUE(database.QuerySummaryCommunicationTime("all", ranks));
    ASSERT_EQ(ranks.size(), 2);
    EXPECT_DOUBLE_EQ(ranks[0].transit, 18.75);
    EXPECT_DOUBLE_EQ(ranks[0].wait, 29.0);
}

TEST_F(SummaryClusterDatabaseTest, ComputesBandwidthFromSizeAndTimeTotals) {
    ASSERT_TRUE(database.ExecSql(R"(
        CREATE TABLE TransitStatistic(step TEXT, rank_id INTEGER, group_name TEXT, band_type TEXT,
            transit_size REAL, transit_time REAL);
        INSERT INTO TransitStatistic VALUES
            ('step2', 2, 'a', 'HCCS', 100, 10),
            ('step2', 10, 'b', 'HCCS', 100, 40),
            ('step3', 2, 'a', 'HCCS', 100, 10),
            ('step2', 2, 'a', 'LOCAL', NULL, NULL),
            ('step2', 2, 'a', 'PCIE', 10, 0),
            ('step2', 2, 'a', 'RDMA', 30, 10),
            ('step2', 2, 'b', 'RDMA', NULL, 10),
            ('step2', 10, 'c', 'RDMA', 10, NULL);
    )"));

    std::vector<Dic::Protocol::SummaryTransportBandwidth> types;
    ASSERT_TRUE(database.QuerySummaryClusterBandwidth("2", types));
    ASSERT_EQ(types.size(), 4);
    EXPECT_EQ(types[0].transportType, "HCCS");
    EXPECT_DOUBLE_EQ(types[0].bandwidth, 4.0);
    EXPECT_EQ(types[1].transportType, "LOCAL");
    EXPECT_DOUBLE_EQ(types[1].bandwidth, 0.0);
    EXPECT_EQ(types[2].transportType, "PCIE");
    EXPECT_DOUBLE_EQ(types[2].bandwidth, 0.0);
    EXPECT_EQ(types[3].transportType, "RDMA");
    EXPECT_DOUBLE_EQ(types[3].bandwidth, 2.0);
    ASSERT_TRUE(database.QuerySummaryClusterBandwidth("all", types));
    ASSERT_EQ(types.size(), 4);
    EXPECT_DOUBLE_EQ(types[0].bandwidth, 5.0);
    ASSERT_TRUE(database.QuerySummaryClusterBandwidth("99", types));
    EXPECT_TRUE(types.empty());
}

TEST_F(SummaryClusterDatabaseTest, DistinguishesAbsentTablesFromInvalidTableStructure) {
    Dic::Protocol::SummaryClusterTime time;
    time.free = 1;
    std::vector<Dic::Protocol::SummaryRankTime> rankTimes(1);
    std::vector<Dic::Protocol::SummaryCommunicationTime> commTimes(1);
    std::vector<Dic::Protocol::SummaryTransportBandwidth> bandwidths(1);
    EXPECT_TRUE(database.QuerySummaryClusterTime("2", time));
    EXPECT_DOUBLE_EQ(time.free, 0.0);
    EXPECT_TRUE(database.QuerySummaryRankTime("all", rankTimes));
    EXPECT_TRUE(rankTimes.empty());
    EXPECT_TRUE(database.QuerySummaryCommunicationTime("all", commTimes));
    EXPECT_TRUE(commTimes.empty());
    EXPECT_TRUE(database.QuerySummaryClusterBandwidth("all", bandwidths));
    EXPECT_TRUE(bandwidths.empty());

    ASSERT_TRUE(database.ExecSql("CREATE TABLE TransitStatistic(band_type TEXT, step TEXT)"));
    EXPECT_FALSE(database.QuerySummaryClusterBandwidth("2", bandwidths));
}

TEST_F(SummaryClusterDatabaseTest, ReturnsEmptyDataWhenTableCheckFails) {
    ASSERT_TRUE(database.ExecSql("CREATE TABLE TransitStatistic(step TEXT, band_type TEXT, "
                                 "transit_size REAL, transit_time REAL);"
                                 "INSERT INTO TransitStatistic VALUES ('step2', 'RDMA', 100, 20);"));
    const auto denySelect = [](void *, int action, const char *, const char *, const char *, const char *) {
        return action == SQLITE_SELECT ? SQLITE_DENY : SQLITE_OK;
    };
    ASSERT_EQ(sqlite3_set_authorizer(db, denySelect, nullptr), SQLITE_OK);

    std::vector<Dic::Protocol::SummaryTransportBandwidth> bandwidths(1);
    EXPECT_TRUE(database.QuerySummaryClusterBandwidth("2", bandwidths));
    EXPECT_TRUE(bandwidths.empty());
    EXPECT_EQ(sqlite3_set_authorizer(db, nullptr, nullptr), SQLITE_OK);
}

TEST_F(DbClusterDataBaseTest, QueryCommunicationRankIdFallsBackOnlyForMissingTraceHostAndSingleHostMapping) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    ASSERT_TRUE(database.ExecSql("CREATE TABLE RankDeviceMap(rankId INTEGER, deviceId INTEGER, hostUid INTEGER);"
                                 "INSERT INTO RankDeviceMap VALUES (8, 0, 222), (8, 0, 222);"));

    const auto uniqueRank = database.QueryCommunicationRankId("", "0");
    ASSERT_TRUE(uniqueRank.has_value());
    EXPECT_EQ(uniqueRank.value(), "8");

    // A non-empty trace host cannot be verified when the cluster database has no HostInfo table.
    EXPECT_FALSE(database.QueryCommunicationRankId("hostB222", "0").has_value());

    ASSERT_TRUE(database.ExecSql("INSERT INTO RankDeviceMap VALUES (9, 1, NULL);"));
    EXPECT_FALSE(database.QueryCommunicationRankId("", "0").has_value());
    ASSERT_TRUE(database.ExecSql("DELETE FROM RankDeviceMap WHERE hostUid IS NULL;"));

    // Even when device 0 itself has one candidate, device-only fallback is unsafe in a multi-host mapping table.
    ASSERT_TRUE(database.ExecSql("INSERT INTO RankDeviceMap VALUES (9, 1, 111);"));
    EXPECT_FALSE(database.QueryCommunicationRankId("", "0").has_value());
    EXPECT_FALSE(database.QueryCommunicationRankId("hostB222", "0").has_value());
}

TEST_F(DbClusterDataBaseTest, QueryCommunicationRankIdUsesHostInfoForSameDeviceOnDifferentHosts) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    MockDatabase database(sqlMutex);
    database.SetDbPtr(dbPtr);
    ASSERT_TRUE(database.ExecSql("CREATE TABLE RankDeviceMap(rankId INTEGER, deviceId INTEGER, hostUid INTEGER);"
                                 "CREATE TABLE HostInfo(hostUid INTEGER, hostName TEXT);"
                                 "INSERT INTO RankDeviceMap VALUES (0, 0, 111), (8, 0, 222);"
                                 "INSERT INTO HostInfo VALUES (111, 'hostA'), (222, 'hostB');"));

    const auto hostBRank = database.QueryCommunicationRankId("hostB222", "0");
    ASSERT_TRUE(hostBRank.has_value());
    EXPECT_EQ(hostBRank.value(), "8");
    EXPECT_FALSE(database.QueryCommunicationRankId("hostC333", "0").has_value());
}

/**
 * DB场景设置并行策略,只设置了dp，pp，tp,兼容老版本
 */
TEST_F(DbClusterDataBaseTest, TestQueryParallelStrategyConfigWhenNotSetCpAndEp) {
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
 * DB场景设置并行策略,设置了dp，pp，tp,cp,ep
 */
TEST_F(DbClusterDataBaseTest, TestQueryParallelStrategyConfigWhenSetAllConfig) {
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
 * DB场景查询并行策略,db未打开
 */
TEST_F(DbClusterDataBaseTest, TestQueryParallelStrategyConfigWhenDbNotOpen) {
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
 * DB场景设置并行策略,db未打开
 */
TEST_F(DbClusterDataBaseTest, TestUpdateParallelStrategyConfigWhenDbNotOpen) {
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

TEST_F(DbClusterDataBaseTest, KernelMfuQueryListUsesFixedSchemaAndFilters) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    auto database = std::make_shared<MockDatabase>(sqlMutex);
    database->SetDbPtr(dbPtr);
    ASSERT_TRUE(
        database->ExecSql("CREATE TABLE OperatorMFU ("
                          "rank_id TEXT, op_name TEXT, kernel_name TEXT, \"kernel_start(ns)\" INTEGER, "
                          "\"kernel_end(ns)\" INTEGER, \"kernel_duration(ns)\" INTEGER, mfu REAL, "
                          "actual_tflops REAL, chip_peak_tflops REAL, flops REAL, flops_op_name TEXT, "
                          "input_shapes TEXT, output_shapes TEXT);"
                          "INSERT INTO OperatorMFU VALUES "
                          "('0', 'op0', 'Kernel_10%', 10, 20, 10, 0.5, 1.0, 2.0, 3.0, 'flops0', '[1]', '[2]'),"
                          "('1', 'op1', 'Kernel_20', 30, 40, 10, 0.6, 1.1, 2.1, 3.1, 'flops1', '[3]', '[4]');"));

    Dic::Protocol::KernelMfuListParams params;
    params.clusterPath = "cluster_0";
    params.rankIds = {"0", "1"};
    params.current = 1;
    params.pageSize = 10;
    std::string errorMsg;
    ASSERT_TRUE(params.CheckParams(errorMsg));

    std::vector<Dic::Protocol::KernelMfuRow> rows;
    std::vector<std::string> rankOptions;
    uint64_t count = 0;
    EXPECT_EQ(Dic::Module::Timeline::KernelMfuDatabaseAccesser::QueryList(database, params, rows, rankOptions, count),
        Dic::Module::Timeline::KernelMfuQueryStatus::SUCCESS);
    ASSERT_EQ(rows.size(), 2);
    ASSERT_EQ(rankOptions, std::vector<std::string>({"0", "1"}));
    EXPECT_EQ(count, 2);

    params.kernelName = "10%";
    rows.clear();
    rankOptions.clear();
    count = 0;
    EXPECT_EQ(Dic::Module::Timeline::KernelMfuDatabaseAccesser::QueryList(database, params, rows, rankOptions, count),
        Dic::Module::Timeline::KernelMfuQueryStatus::SUCCESS);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].rankId, "0");
    EXPECT_EQ(rows[0].kernelName, "Kernel_10%");
    ASSERT_EQ(rankOptions, std::vector<std::string>({"0", "1"}));
    EXPECT_EQ(count, 1);

    params.kernelName.clear();
    params.opName = "OP0";
    rows.clear();
    rankOptions.clear();
    count = 0;
    EXPECT_EQ(Dic::Module::Timeline::KernelMfuDatabaseAccesser::QueryList(database, params, rows, rankOptions, count),
        Dic::Module::Timeline::KernelMfuQueryStatus::SUCCESS);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].rankId, "0");
    EXPECT_EQ(rows[0].opName, "op0");
    ASSERT_EQ(rankOptions, std::vector<std::string>({"0", "1"}));
    EXPECT_EQ(count, 1);

    params.kernelName = "10%";
    rows.clear();
    rankOptions.clear();
    count = 0;
    EXPECT_EQ(Dic::Module::Timeline::KernelMfuDatabaseAccesser::QueryList(database, params, rows, rankOptions, count),
        Dic::Module::Timeline::KernelMfuQueryStatus::SUCCESS);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].rankId, "0");
    EXPECT_EQ(rows[0].opName, "op0");
    EXPECT_EQ(rows[0].kernelName, "Kernel_10%");
    EXPECT_EQ(count, 1);
}

TEST_F(DbClusterDataBaseTest, KernelMfuListRequiresAllKnownColumns) {
    std::recursive_mutex sqlMutex;
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    auto database = std::make_shared<MockDatabase>(sqlMutex);
    database->SetDbPtr(dbPtr);
    ASSERT_TRUE(database->ExecSql("CREATE TABLE OperatorMFU (rank_id TEXT);"));
    Dic::Protocol::KernelMfuListParams params;
    std::vector<Dic::Protocol::KernelMfuRow> rows;
    std::vector<std::string> rankOptions;
    uint64_t count = 0;
    EXPECT_EQ(Dic::Module::Timeline::KernelMfuDatabaseAccesser::QueryList(database, params, rows, rankOptions, count),
        Dic::Module::Timeline::KernelMfuQueryStatus::UNAVAILABLE);
}
