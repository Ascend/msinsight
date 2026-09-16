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
#include <utility>
#include "vector"
#include "DataBaseManager.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"
#include "DbSummaryDataBase.h"
#include "ParamsParser.h"
#include "FileUtil.h"
#include "OperatorLogTestUtil.h"
#include "../../FullDbTestSuit.cpp"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;

class DbOperatorTestSuit : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "msprof_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        auto summeryDatabase =
            std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
                DataBaseManager::Instance().CreateSummaryDatabase("2", dbPath));
        summeryDatabase->OpenDb(dbPath, false);
    }

    static void TearDownTestSuite() {}
};

const std::string GROUP_OPERATOR = "Operator";
const std::string GROUP_OPERATOR_TYPE = "Operator Type";
const std::string GROUP_INPUT_SHAPE = "Input Shape";

class TestDbSummaryDataBase : public DbSummaryDataBase {
  public:
    using DbSummaryDataBase::DbSummaryDataBase;

    bool PrepareEmptySql() {
        sqlite3_stmt *stmt = nullptr;
        return PrepareSql("", stmt, "QueryDetail", "the detail query", "the profiling database", "rankId=244");
    }
};

class OperatorDatabaseLogTest : public ::testing::Test {
  protected:
    void SetUp() override {
        dbPath = FileUtil::SplicePath(::testing::TempDir(), "operator_query_log_test.db");
        if (FileUtil::CheckFilePathExist(dbPath)) {
            ASSERT_TRUE(FileUtil::RemoveFile(dbPath));
        }
        database = std::make_unique<TestDbSummaryDataBase>(mutex);
        ASSERT_TRUE(database->CreateDbIfNotExist(dbPath));
        ASSERT_TRUE(database->AttachDb(dbPath));
    }

    void TearDown() override {
        if (database) {
            database->CloseDb();
        }
        if (FileUtil::CheckFilePathExist(dbPath)) {
            EXPECT_TRUE(FileUtil::RemoveFile(dbPath));
        }
    }

    std::string dbPath;
    std::recursive_mutex mutex;
    std::unique_ptr<TestDbSummaryDataBase> database;
};

TEST_F(OperatorDatabaseLogTest, EmptySqlPrepareFailureLogsCauseOnce) {
    const auto mark = OperatorLogTestUtil::Mark();

    EXPECT_FALSE(database->PrepareEmptySql());

    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "PrepareSql",
        {"rankId=244", "sqliteCode=0", "cause=the prepared statement is empty",
            "suggestion=Check the generated query and request parameters"},
        {});
}

TEST_F(OperatorDatabaseLogTest, StatisticPrepareFailureLogsContextOnce) {
    Dic::Protocol::OperatorStatisticReqParams params = {
        false, "244", "0\nforged", GROUP_OPERATOR_TYPE, 15, 1, 10, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response;
    const auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(database->QueryOperatorStatisticInfo(params, response));

    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryStatistic", "PrepareSql",
        {"rankId=244", "deviceId=0\\nforged", "group=Operator Type", "sqliteCode=1",
            "cause=no such table: COMPUTE_TASK_INFO", "suggestion="},
        {"SELECT COUNT", "Failed to query total num", "deviceId=0\nforged"});
}

TEST_F(OperatorDatabaseLogTest, CategoryGenerateAndTableFailuresLogTheirOwnerOnce) {
    Dic::Protocol::OperatorDurationReqParams params = {"244", "0", "Unknown", 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data;
    for (const auto &group : {std::string("Unknown"), std::string(1024 * 1024, 'x')}) {
        params.group = group;
        const auto mark = OperatorLogTestUtil::Mark();
        EXPECT_FALSE(database->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data));
        EXPECT_TRUE(params.group == group);
        EXPECT_TRUE(data.empty());
        OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryCategory", "GenerateSql",
            {"rankId=244", "group=unknown,", "cause=the operator group is unknown", "suggestion="},
            {"group=Unknown", std::string(64, 'x')});
    }

    params.group = "Communication Operator Type";
    auto mark = OperatorLogTestUtil::Mark();
    EXPECT_TRUE(database->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data));
    EXPECT_TRUE(data.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryCategory", "CheckTable",
        {"rankId=244", "deviceId=0", "group=Communication Operator Type", "table=COMMUNICATION_OP",
            "cause=the communication table is missing", "suggestion="},
        {});

    Dic::Protocol::OperatorStatisticReqParams statisticParams = {
        true, "244", "0", "Communication Operator Type", 15, 1, 10, "", ""};
    std::vector<Dic::Protocol::OperatorStatisticInfoRes> statisticData;
    mark = OperatorLogTestUtil::Mark();
    EXPECT_TRUE(database->QueryAllOperatorStatisticInfo(statisticParams, statisticData));
    EXPECT_TRUE(statisticData.empty());
    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 1, 0, "QueryStatistic", "CheckTable",
        {"rankId=244", "deviceId=0", "group=Communication Operator Type", "table=COMMUNICATION_OP",
            "cause=the communication table is missing", "suggestion="},
        {});
}

TEST_F(OperatorDatabaseLogTest, ClosedDatabaseFailuresLogTheirOwnerOnce) {
    Dic::Protocol::OperatorStatisticReqParams params = {
        true, "244", "0", "Communication Operator Type", 15, 1, 10, "", ""};
    std::vector<Dic::Protocol::OperatorStatisticInfoRes> statisticData;
    database->CloseDb();
    auto mark = OperatorLogTestUtil::Mark();
    EXPECT_FALSE(database->QueryAllOperatorStatisticInfo(params, statisticData));

    OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryStatistic", "CheckTable",
        {"rankId=244", "deviceId=0", "group=Communication Operator Type", "sqliteCode=" + std::to_string(SQLITE_MISUSE),
            "cause=database is closed", "suggestion="},
        {"table is missing", "Optional data is unavailable"});

    std::vector<Dic::Protocol::OperatorDetailInfoRes> detailData;
    std::string level;
    const std::vector<std::pair<std::string, std::string>> groups = {{GROUP_OPERATOR_TYPE, "group=Operator Type,"},
        {"", "group=,"}, {"Unknown", "group=unknown,"}, {std::string(1024 * 1024, 'x'), "group=unknown,"}};
    for (const auto &group : groups) {
        params.group = group.first;
        mark = OperatorLogTestUtil::Mark();
        EXPECT_FALSE(database->QueryAllOperatorDetailInfo(params, detailData, level));
        EXPECT_TRUE(params.group == group.first);
        EXPECT_TRUE(detailData.empty());
        OperatorLogTestUtil::ExpectSingleOperatorLog(mark, 0, 1, "QueryDetail", "PrepareSql",
            {"rankId=244", "deviceId=0", group.second, "sqliteCode=" + std::to_string(SQLITE_MISUSE),
                "cause=database is closed", "suggestion="},
            {"out of memory", "Failed Check Table", "Failed to get Detail Info", "group=Unknown",
                std::string(64, 'x')});
    }
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryOperatorDurationInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorDurationReqParams params = {"2", "2", GROUP_OPERATOR_TYPE, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 8;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int unitSize = 1;
    EXPECT_EQ(data.size(), unitSize);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryOperatorDurationInfoByOpTypeAndInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorDurationReqParams params = {"2", "2", GROUP_INPUT_SHAPE, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 11;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int unitSize = 1;
    EXPECT_EQ(data.size(), unitSize);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryOperatorDurationInfoByOperator) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorDurationReqParams params = {"2", "2", GROUP_OPERATOR, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 11;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int cnt = 1;
    EXPECT_EQ(data.size(), cnt);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryOperatorStatisticInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "2", "2", GROUP_OPERATOR_TYPE, 15, 0, 10, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    bool result = db->QueryOperatorStatisticInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 8;
    int size = 8;
    EXPECT_EQ(response.total, total);
    EXPECT_EQ(response.data.size(), size);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryOperatorStatisticInfoByOpTypeAndInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "2", "2", GROUP_INPUT_SHAPE, 15, 0, 5, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    bool result = db->QueryOperatorStatisticInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 11;
    EXPECT_EQ(response.total, total);
    int size = 5;
    EXPECT_EQ(response.data.size(), size);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryAllOperatorStatisticInfoByOpTypeAndInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {true, "2", "2", GROUP_INPUT_SHAPE, 15, 0, 5, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    std::vector<Protocol::OperatorStatisticInfoRes> compareRes;
    bool result = db->QueryAllOperatorStatisticInfo(reqParams, compareRes);
    EXPECT_EQ(result, true);
    int size = 11;
    EXPECT_EQ(compareRes.size(), size);
}

TEST_F(DbOperatorTestSuit, FullDb_of_QueryAllOperatorDetailInfoWhenPmuDataNotExist) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "2", "2", GROUP_OPERATOR, 15, 0, 5, "", ""};
    Dic::Protocol::OperatorDetailInfoResponse response = {};
    bool result = db->QueryOperatorDetailInfo(reqParams, response);
    EXPECT_EQ(result, true);
}

TEST_F(DbOperatorTestSuit, QueryBandwidthContentionMatMulDataTest) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("2");
    std::vector<Dic::Module::BandwidthContentionMatMulInfo> info;
    bool result = db->QueryBandwidthContentionMatMulData(info);
    ASSERT_TRUE(result);
    ASSERT_EQ(info.size(), 0);
}
