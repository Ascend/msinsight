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
#include <cstdlib>
#include "CommunicationStatisticsProtocol.h"
#include "CommunicationProtocol.h"
#include "DbClusterDataBase.h"
#include "TextClusterDatabase.h"
#include "ProtocolDefs.h"

using namespace Dic;
using namespace Dic::Protocol;
using namespace Dic::Module::Communication;

class CommunicationStatisticsTest : public testing::Test {
  protected:
    class StatisticsDatabase : public Module::FullDb::DbClusterDataBase {
      public:
        explicit StatisticsDatabase(std::recursive_mutex &mutex) : DbClusterDataBase(mutex) {}
        bool Open() {
            isOpen = sqlite3_open(":memory:", &db) == SQLITE_OK;
            path = ":memory:";
            return isOpen;
        }
        bool OpenReadOnly(const std::string &filePath) {
            isOpen = sqlite3_open_v2(filePath.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK;
            path = filePath;
            return isOpen;
        }
    };

    void SetUp() override {
        ASSERT_TRUE(database.Open());
        ASSERT_TRUE(database.ExecSql(
            "CREATE TABLE CommTimeStatistic(step TEXT, rank_id INTEGER, group_name TEXT, comm_total_time REAL);"
            "CREATE TABLE TransitStatistic(step TEXT, rank_id INTEGER, group_name TEXT, band_type TEXT, "
            "transit_size REAL, transit_time REAL, bandwidth REAL);"
            "CREATE TABLE CommOpStatistic(step TEXT, group_name TEXT, op_name TEXT, min_time REAL, max_time REAL);"
            "CREATE TABLE CommunicationGroupMapping(group_name TEXT, group_id TEXT);"
            "INSERT INTO CommunicationGroupMapping VALUES ('3985310405473756128','group_name_19'),('other','other_id');"
            "INSERT INTO CommTimeStatistic VALUES "
            "('step13',0,'3985310405473756128',313.8778),('step13',2,'3985310405473756128',72.66366),"
            "('step14',0,'3985310405473756128',999),('step13',1,'other',5);"
            "INSERT INTO TransitStatistic VALUES "
            "('step13',0,'3985310405473756128','HCCS',10,1,10),"
            "('step13',2,'3985310405473756128','HCCS',90,3,30),"
            "('step13',0,'3985310405473756128','SDMA',10,1,10),"
            "('step14',0,'3985310405473756128','HCCS',999,1,999),"
            "('step13',0,'other','HCCS',1,1,1);"
            "INSERT INTO CommOpStatistic VALUES "
            "('step13','3985310405473756128','hcom_batchSendRecv__128_3_1',0.43614,71.42468),"
            "('step13','3985310405473756128','hcom_batchSendRecv__128_4_1',2,2),"
            "('step14','3985310405473756128','ignored',0,999),('step13','other','ignored',0,999);"));
        protocol.Register();
    }

    void TearDown() override { protocol.UnRegister(); }

    static constexpr const char *testGroupName = "3985310405473756128";

    StatisticsResult<GroupStatistic> QueryGroupRangeRatio(const std::string &step = "step13") {
        return database.QueryGroupRangeRatio(step);
    }

    StatisticsResult<GroupStatistic> QueryGroupBandwidth(
        const std::string &step = "step13", const std::string &transportType = "HCCS") {
        return database.QueryGroupBandwidth(step, transportType);
    }

    StatisticsResult<OperatorFluctuation> QueryOperatorFluctuation(
        const std::string &step = "step13", const std::string &groupName = testGroupName) {
        return database.QueryOperatorFluctuation(step, groupName);
    }

    RankStatisticsResult QueryRankStatistics(const std::string &step = "step13",
        const std::string &groupName = testGroupName, int64_t currentPage = 1, int64_t pageSize = 20) {
        return database.QueryRankStatistics(step, groupName, {currentPage, pageSize});
    }

    template <typename Result> static void ExpectEmptyResult(const Result &result) {
        EXPECT_EQ(result.status, StatisticsStatus::SUCCESS);
        EXPECT_TRUE(result.rows.empty());
    }

    template <typename Test> static void ForEachStatisticsRequest(Test test) {
        test(CommunicationGroupRangeRatioRequest{});
        test(CommunicationGroupBandwidthRequest{});
        test(CommunicationOperatorFluctuationRequest{});
        test(CommunicationRankStatisticsRequest{});
    }

    bool AddSortingFixture() {
        return database.ExecSql("INSERT INTO TransitStatistic VALUES "
                                "('step13',3,'3985310405473756128','SDMA',7,1,7),"
                                "('step13',5,'3985310405473756128','HCCS',0,1,0),"
                                "('step13',10,'3985310405473756128','HCCS',30,1,30);");
    }

    RankStatisticsResult QueryRankSort(const std::string &orderBy, const std::string &order,
        const std::string &bandType = "", int64_t currentPage = 1, int64_t pageSize = 20) {
        return database.QueryRankStatistics("step13", testGroupName, {currentPage, pageSize, orderBy, order, bandType});
    }

    document_t RequestJson(const std::string &command) {
        const std::string text = "{\"id\":1,\"moduleName\":\"communication\",\"type\":\"request\","
                                 "\"resultCallbackId\":3,\"command\":\"" +
            command +
            "\",\"params\":{\"clusterPath\":\"/cluster\","
            "\"iterationId\":\"13\",\"groupName\":\"3985310405473756128\",\"transportType\":\"HCCS\"}}";
        document_t json;
        json.Parse(text.c_str());
        return json;
    }

    std::recursive_mutex mutex;
    StatisticsDatabase database{mutex};
    CommunicationProtocol protocol;
};

TEST_F(CommunicationStatisticsTest, GroupRangeRatioUsesMaximumOverMinimumAndReturnsBothIdentifiers) {
    auto result = QueryGroupRangeRatio();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].groupName, "3985310405473756128");
    EXPECT_EQ(items[0].groupId, "group_name_19");
    ASSERT_TRUE(items[0].value.has_value());
    EXPECT_NEAR(*items[0].value, 313.8778 / 72.66366, 1e-12);
    EXPECT_DOUBLE_EQ(*items[1].value, 1);
}

TEST_F(CommunicationStatisticsTest, GroupRangeRatioKeepsZeroDenominatorsAndEqualValues) {
    ASSERT_TRUE(database.ExecSql("UPDATE CommTimeStatistic SET comm_total_time=0 WHERE group_name='other';"
                                 "UPDATE CommTimeStatistic SET comm_total_time=8 "
                                 "WHERE group_name='3985310405473756128';"));
    auto result = QueryGroupRangeRatio();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    ASSERT_EQ(items.size(), 2);
    EXPECT_DOUBLE_EQ(*items[0].value, 1);
    EXPECT_FALSE(items[1].value.has_value());
    ASSERT_TRUE(database.ExecSql("UPDATE CommTimeStatistic SET comm_total_time=0 WHERE rank_id=2;"));
    result = QueryGroupRangeRatio();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_FALSE(result.rows[1].value.has_value());
}

TEST_F(CommunicationStatisticsTest, GroupBandwidthDividesSumsInsteadOfAveragingBandwidth) {
    auto result = QueryGroupBandwidth();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].groupName, "other");
    ASSERT_TRUE(items[0].value.has_value());
    EXPECT_DOUBLE_EQ(*items[0].value, 1);
    EXPECT_EQ(items[1].groupId, "group_name_19");
    EXPECT_EQ(items[1].groupName, testGroupName);
    ASSERT_TRUE(items[1].value.has_value());
    EXPECT_DOUBLE_EQ(*items[1].value, 25);
    auto sdma = QueryGroupBandwidth("step13", "SDMA");
    ASSERT_EQ(sdma.status, StatisticsStatus::SUCCESS);
    const auto &sdmaItems = sdma.rows;
    ASSERT_EQ(sdmaItems.size(), 1);
    EXPECT_DOUBLE_EQ(*sdmaItems[0].value, 10);
}

TEST_F(CommunicationStatisticsTest, GroupBandwidthKeepsZeroDenominatorAsNull) {
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET transit_time=0 WHERE band_type='HCCS';"));
    auto result = QueryGroupBandwidth();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    for (const auto &item : result.rows) {
        EXPECT_FALSE(item.value.has_value());
    }
}

TEST_F(CommunicationStatisticsTest, OperatorFluctuationFiltersExactGroupAndKeepsCompleteNames) {
    auto result = QueryOperatorFluctuation();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].opName, "hcom_batchSendRecv__128_3_1");
    EXPECT_NEAR(items[0].rangeTime, 70.98854, 1e-12);
    EXPECT_DOUBLE_EQ(items[1].rangeTime, 0);
    auto empty = QueryOperatorFluctuation("step13", "group_name_19");
    ASSERT_EQ(empty.status, StatisticsStatus::SUCCESS);
    EXPECT_TRUE(empty.rows.empty());
}

TEST_F(CommunicationStatisticsTest, RankStatisticsPreservesSourceValuesZeroAndUnknownTypes) {
    ASSERT_TRUE(database.ExecSql("INSERT INTO TransitStatistic VALUES "
                                 "('step13',10,'3985310405473756128','NEW_TRANSPORT',100,2,0);"));
    auto result = QueryRankStatistics();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    ASSERT_EQ(items.size(), 3);
    EXPECT_EQ(items[0].rankId, 0);
    EXPECT_EQ(items[1].rankId, 2);
    EXPECT_EQ(items[2].rankId, 10);
    ASSERT_EQ(items[0].bandwidths.size(), 2);
    EXPECT_EQ(items[0].bandwidths[0].bandType, "HCCS");
    EXPECT_DOUBLE_EQ(items[0].bandwidths[0].bandwidth, 10);
    ASSERT_EQ(items[1].bandwidths.size(), 1);
    EXPECT_DOUBLE_EQ(items[1].bandwidths[0].bandwidth, 30);
    ASSERT_EQ(items[2].bandwidths.size(), 1);
    EXPECT_EQ(items[2].bandwidths[0].bandType, "NEW_TRANSPORT");
    EXPECT_DOUBLE_EQ(items[2].bandwidths[0].bandwidth, 0);
}

TEST_F(CommunicationStatisticsTest, MissingMappingsAndConflictsAreErrorsOnlyForAffectedGroups) {
    ASSERT_TRUE(
        database.ExecSql("INSERT INTO CommunicationGroupMapping VALUES ('unrelated','one'),('unrelated','two');"));
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::SUCCESS);
    ASSERT_TRUE(database.ExecSql("DELETE FROM CommunicationGroupMapping WHERE group_name='other';"));
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("INSERT INTO CommunicationGroupMapping VALUES ('other','one'),('other','two');"));
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::SUCCESS);
}

TEST_F(CommunicationStatisticsTest, DuplicateSourceKeysAreRejected) {
    ASSERT_TRUE(database.ExecSql(
        "INSERT INTO CommTimeStatistic SELECT * FROM CommTimeStatistic WHERE rank_id=2;"
        "INSERT INTO TransitStatistic SELECT * FROM TransitStatistic WHERE step='step13' AND rank_id=2;"
        "INSERT INTO CommOpStatistic SELECT * FROM CommOpStatistic WHERE op_name LIKE '%128_3_1';"));
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryOperatorFluctuation().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, NullNegativeTextAndInvalidRanksAreNotCoercedIntoZero) {
    for (const auto &value : {"NULL", "-1", "'bad'", "1e999"}) {
        ASSERT_TRUE(database.ExecSql(
            "UPDATE CommTimeStatistic SET comm_total_time=" + std::string(value) + " WHERE step='step13';"));
        EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::INVALID_DATA);
    }
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET rank_id=-1 WHERE step='step13';"));
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("UPDATE CommOpStatistic SET min_time=max_time+1;"));
    EXPECT_EQ(QueryOperatorFluctuation().status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, NullSizeIsNotSilentlySkippedByAggregation) {
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET transit_size=NULL WHERE rank_id=2;"));
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::SUCCESS);
}

TEST_F(CommunicationStatisticsTest, OverflowIsReportedInsteadOfSerializingInfinity) {
    ASSERT_TRUE(
        database.ExecSql("UPDATE TransitStatistic SET transit_size=1e308, transit_time=1e-308 WHERE step='step13';"));
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("UPDATE CommTimeStatistic SET comm_total_time=1e308 WHERE rank_id=0;"
                                 "UPDATE CommTimeStatistic SET comm_total_time=1e-308 WHERE rank_id=2;"));
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, EachQueryRequiresOnlyItsOwnTablesAndColumns) {
    ASSERT_TRUE(database.ExecSql("DROP TABLE CommunicationGroupMapping; DROP TABLE CommTimeStatistic;"
                                 "DROP TABLE CommOpStatistic;"));
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(QueryGroupRangeRatio().status, StatisticsStatus::UNSUPPORTED);
    EXPECT_EQ(QueryGroupBandwidth().status, StatisticsStatus::UNSUPPORTED);
    EXPECT_EQ(QueryOperatorFluctuation().status, StatisticsStatus::UNSUPPORTED);
    ASSERT_TRUE(database.ExecSql("DROP TABLE TransitStatistic; CREATE TABLE TransitStatistic(step TEXT);"));
    EXPECT_EQ(QueryRankStatistics().status, StatisticsStatus::UNSUPPORTED);
}

TEST_F(CommunicationStatisticsTest, EmptyResultsSucceedAndTextDatabasesAreUnsupported) {
    ExpectEmptyResult(QueryGroupRangeRatio("step999"));
    ExpectEmptyResult(QueryGroupBandwidth("step999"));
    ExpectEmptyResult(QueryOperatorFluctuation("step999"));
    ExpectEmptyResult(QueryRankStatistics("step999"));

    Module::TextClusterDatabase text(mutex);
    EXPECT_EQ(text.QueryGroupRangeRatio("step13").status, StatisticsStatus::UNSUPPORTED);
    EXPECT_EQ(text.QueryGroupBandwidth("step13", "HCCS").status, StatisticsStatus::UNSUPPORTED);
    EXPECT_EQ(text.QueryOperatorFluctuation("step13", testGroupName).status, StatisticsStatus::UNSUPPORTED);
    EXPECT_EQ(text.QueryRankStatistics("step13", testGroupName).status, StatisticsStatus::UNSUPPORTED);
}

TEST_F(CommunicationStatisticsTest, FilterValuesAreBoundRatherThanInterpolated) {
    auto result = QueryRankStatistics("step13", "' OR 1=1 --");
    EXPECT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_TRUE(result.rows.empty());
    auto bandwidth = QueryGroupBandwidth("step13", "HCCS' OR 1=1 --");
    EXPECT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_TRUE(result.rows.empty());
}

TEST_F(CommunicationStatisticsTest, AllCommandsParseAndNormalizeStepWithoutMutatingParams) {
    ForEachStatisticsRequest([&](const auto &prototype) {
        auto json = RequestJson(prototype.command);
        std::string error;
        auto request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr) << error;
        using RequestType = std::decay_t<decltype(prototype)>;
        auto *statistics = dynamic_cast<RequestType *>(request.get());
        ASSERT_NE(statistics, nullptr);
        EXPECT_EQ(statistics->command, prototype.command);
        EXPECT_EQ(statistics->params.clusterPath, "/cluster");
        EXPECT_EQ(statistics->params.iterationId, "13");
        EXPECT_EQ(statistics->params.GetStep(), "step13");
        statistics->params.iterationId = "step13";
        EXPECT_TRUE(statistics->params.CheckParams(error));
        EXPECT_EQ(statistics->params.GetStep(), "step13");
        EXPECT_EQ(statistics->params.iterationId, "step13");
    });
}

TEST_F(CommunicationStatisticsTest, GroupNameIsAnExactStringNotDisplayIdOrNumericHash) {
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.groupName, "3985310405473756128");
    json["params"]["groupName"].SetInt64(3985310405473756128LL);
    auto invalidRequest = protocol.FromJson(json, error);
    ASSERT_NE(invalidRequest, nullptr);
    EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*invalidRequest).params.CheckParams(error));
    json["params"].RemoveMember("groupName");
    json["params"].AddMember("groupIdHash", "3985310405473756128", json.GetAllocator());
    invalidRequest = protocol.FromJson(json, error);
    ASSERT_NE(invalidRequest, nullptr);
    EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*invalidRequest).params.CheckParams(error));
}

TEST_F(CommunicationStatisticsTest, SnakeCaseGroupNameDoesNotReplaceRequiredCamelCaseParameter) {
    auto check = [&](const auto &prototype) {
        auto json = RequestJson(prototype.command);
        json["params"].RemoveMember("groupName");
        json["params"].AddMember("group_name", json_t(testGroupName, json.GetAllocator()), json.GetAllocator());
        std::string error;
        auto request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr);
        using RequestType = std::decay_t<decltype(prototype)>;
        const auto &params = dynamic_cast<const RequestType &>(*request).params;
        EXPECT_TRUE(params.groupName.empty());
        EXPECT_FALSE(params.CheckParams(error));
    };
    check(CommunicationOperatorFluctuationRequest{});
    check(CommunicationRankStatisticsRequest{});
}

TEST_F(CommunicationStatisticsTest, MissingWrongTypeAndEmptyRequiredParamsAreRejected) {
    ForEachStatisticsRequest([&](const auto &prototype) {
        using RequestType = std::decay_t<decltype(prototype)>;
        const auto &command = prototype.command;
        std::vector<std::string> fields{"clusterPath", "iterationId"};
        if (command == REQ_RES_COMMUNICATION_GROUP_BANDWIDTH) {
            fields.push_back("transportType");
        } else if (command != REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO) {
            fields.push_back("groupName");
        }
        for (const auto &field : fields) {
            for (int mode = 0; mode < 3; ++mode) {
                auto json = RequestJson(command);
                if (mode == 0) {
                    json["params"].RemoveMember(field.c_str());
                } else if (mode == 1) {
                    json["params"][field.c_str()].SetInt(13);
                } else {
                    json["params"][field.c_str()].SetString("");
                }
                std::string error;
                auto request = protocol.FromJson(json, error);
                ASSERT_NE(request, nullptr);
                EXPECT_FALSE(dynamic_cast<const RequestType &>(*request).params.CheckParams(error));
                EXPECT_FALSE(error.empty());
            }
        }
    });
}

TEST_F(CommunicationStatisticsTest, MalformedParamsInvalidIterationsAndEmbeddedNullsAreRejected) {
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    std::string error;
    json["params"].SetArray();
    EXPECT_EQ(protocol.FromJson(json, error), nullptr);
    for (const auto &iteration : {"step", "stepstep13", "13;", "-1", "1.5"}) {
        auto requestJson = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
        requestJson["params"]["iterationId"].SetString(iteration, requestJson.GetAllocator());
        auto request = protocol.FromJson(requestJson, error);
        ASSERT_NE(request, nullptr);
        EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error));
    }
    auto requestJson = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    requestJson["params"]["groupName"].SetString("one\0two", 7, requestJson.GetAllocator());
    auto request = protocol.FromJson(requestJson, error);
    ASSERT_NE(request, nullptr);
    EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error));
}

TEST_F(CommunicationStatisticsTest, ResponsesSerializeAllQueryShapes) {
    auto checkGroupResponse = [&](auto &response, auto result, const char *metricKey, const char *firstGroupName,
                                  const char *firstGroupId) {
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        response.items = std::move(result.rows);
        response.result = true;
        std::string error;
        auto output = protocol.ToJson(response, error);
        ASSERT_TRUE(output.has_value()) << error;
        const auto &body = (*output)["body"];
        ASSERT_TRUE(body["items"].IsArray());
        ASSERT_FALSE(body["items"].Empty());
        const auto &item = body["items"][0];
        EXPECT_STREQ(item["groupName"].GetString(), firstGroupName);
        EXPECT_STREQ(item["groupId"].GetString(), firstGroupId);
        EXPECT_TRUE(item[metricKey].IsDouble());
        EXPECT_FALSE(body.HasMember("total"));
        EXPECT_FALSE(body.HasMember("currentPage"));
        EXPECT_FALSE(body.HasMember("pageSize"));
    };
    CommunicationGroupRangeRatioResponse range;
    checkGroupResponse(range, QueryGroupRangeRatio(), "rangeRatio", testGroupName, "group_name_19");
    CommunicationGroupBandwidthResponse bandwidth;
    checkGroupResponse(bandwidth, QueryGroupBandwidth(), "bandwidth", "other", "other_id");

    CommunicationOperatorFluctuationResponse operators;
    auto operatorResult = QueryOperatorFluctuation();
    ASSERT_EQ(operatorResult.status, StatisticsStatus::SUCCESS);
    operators.result = true;
    operators.items = std::move(operatorResult.rows);
    std::string error;
    auto operatorJson = protocol.ToJson(operators, error);
    ASSERT_TRUE(operatorJson.has_value()) << error;
    const auto &operatorBody = (*operatorJson)["body"];
    ASSERT_TRUE(operatorBody["items"].IsArray());
    ASSERT_FALSE(operatorBody["items"].Empty());
    const auto &operatorItem = operatorBody["items"][0];
    EXPECT_TRUE(operatorItem.HasMember("opName"));
    EXPECT_TRUE(operatorItem.HasMember("rangeTime"));
    EXPECT_FALSE(operatorItem.HasMember("rangeRatio"));
    EXPECT_FALSE(operatorBody.HasMember("total"));

    CommunicationRankStatisticsResponse ranks;
    auto rankResult = QueryRankStatistics();
    ASSERT_EQ(rankResult.status, StatisticsStatus::SUCCESS);
    ranks.result = true;
    ranks.items = std::move(rankResult.rows);
    ranks.total = rankResult.total;
    auto rankJson = protocol.ToJson(ranks, error);
    ASSERT_TRUE(rankJson.has_value()) << error;
    const auto &rankBody = (*rankJson)["body"];
    ASSERT_TRUE(rankBody["items"].IsArray());
    ASSERT_FALSE(rankBody["items"].Empty());
    EXPECT_EQ(rankBody["total"].GetUint64(), 2);
    EXPECT_EQ(rankBody["currentPage"].GetInt64(), 1);
    EXPECT_EQ(rankBody["pageSize"].GetInt64(), 20);
    const auto &rankItem = rankBody["items"][0];
    EXPECT_TRUE(rankItem["rankId"].IsInt64());
    EXPECT_TRUE(rankItem["bandwidths"].IsArray());
    EXPECT_FALSE(rankItem.HasMember("sdmaBandwidth"));
}

TEST_F(CommunicationStatisticsTest, NullableGroupValuesSerializeAsJsonNullNotZero) {
    auto check = [&](auto &response, const char *metricKey) {
        response.result = true;
        response.items = {{testGroupName, "group_name_19", std::nullopt}};
        std::string error;
        auto json = protocol.ToJson(response, error);
        ASSERT_TRUE(json.has_value());
        EXPECT_TRUE((*json)["body"]["items"][0][metricKey].IsNull());
    };
    CommunicationGroupRangeRatioResponse range;
    check(range, "rangeRatio");
    CommunicationGroupBandwidthResponse bandwidth;
    check(bandwidth, "bandwidth");
}

TEST_F(CommunicationStatisticsTest, FailedResponseCannotLeakPartialRows) {
    CommunicationGroupRangeRatioResponse response;
    response.items = {{"hash", "id", 3}};
    response.result = false;
    auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    EXPECT_TRUE((*json)["body"]["items"].Empty());
}

TEST_F(CommunicationStatisticsTest, OptionalCallbackIdIsPreservedOrOmittedAndWrongTypesAreRejected) {
    auto json = RequestJson(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO);
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    ASSERT_TRUE(request->resultCallbackId.has_value());
    EXPECT_EQ(*request->resultCallbackId, 3);
    json.RemoveMember("resultCallbackId");
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_FALSE(request->resultCallbackId.has_value());
    json.AddMember("resultCallbackId", "invalid", json.GetAllocator());
    EXPECT_EQ(protocol.FromJson(json, error), nullptr);
}

TEST_F(CommunicationStatisticsTest, ReferenceSampleMatchesDesignThroughReadOnlyQueries) {
    const char *referencePath = std::getenv("MSINSIGHT_CLUSTER_STATISTICS_DB");
    if (referencePath == nullptr) {
        GTEST_SKIP() << "Set MSINSIGHT_CLUSTER_STATISTICS_DB to run the optional reference sample acceptance test.";
    }
    StatisticsDatabase reference(mutex);
    ASSERT_TRUE(reference.OpenReadOnly(referencePath));
    auto range = reference.QueryGroupRangeRatio("step13");
    ASSERT_EQ(range.status, StatisticsStatus::SUCCESS);
    const auto &groups = range.rows;
    ASSERT_EQ(groups.size(), 12);
    bool found = false;
    for (const auto &group : groups) {
        if (group.groupName == testGroupName) {
            found = true;
            EXPECT_EQ(group.groupId, "group_name_19");
            ASSERT_TRUE(group.value.has_value());
            EXPECT_NEAR(*group.value, 4.319597994375731, 1e-12);
        }
    }
    EXPECT_TRUE(found);
    auto bandwidth = reference.QueryGroupBandwidth("step13", "HCCS");
    ASSERT_EQ(bandwidth.status, StatisticsStatus::SUCCESS);
    const auto &bandwidthGroups = bandwidth.rows;
    ASSERT_EQ(bandwidthGroups.size(), 10);
    found = false;
    for (const auto &group : bandwidthGroups) {
        if (group.groupName == testGroupName) {
            found = true;
            ASSERT_TRUE(group.value.has_value());
            EXPECT_NEAR(*group.value, 20.905927651991874, 1e-12);
        }
    }
    EXPECT_TRUE(found);
    auto operators = reference.QueryOperatorFluctuation("step13", testGroupName);
    ASSERT_EQ(operators.status, StatisticsStatus::SUCCESS);
    const auto &operatorItems = operators.rows;
    ASSERT_EQ(operatorItems.size(), 3);
    EXPECT_EQ(operatorItems[0].opName, "hcom_batchSendRecv__128_4_1");
    EXPECT_NEAR(operatorItems[0].rangeTime, 157.74916, 1e-12);
    EXPECT_EQ(operatorItems[2].opName, "hcom_batchSendRecv__128_3_1");
    EXPECT_NEAR(operatorItems[2].rangeTime, 70.98854, 1e-12);
    auto ranks = reference.QueryRankStatistics("step13", testGroupName);
    ASSERT_EQ(ranks.status, StatisticsStatus::SUCCESS);
    const auto &rankItems = ranks.rows;
    ASSERT_EQ(rankItems.size(), 2);
    EXPECT_EQ(rankItems[0].rankId, 0);
    ASSERT_EQ(rankItems[0].bandwidths.size(), 2);
    EXPECT_EQ(rankItems[0].bandwidths[0].bandType, "HCCS");
    EXPECT_NEAR(rankItems[0].bandwidths[0].bandwidth, 20.9093147885042, 1e-12);
    EXPECT_EQ(rankItems[0].bandwidths[1].bandType, "SDMA");
    BandwidthDataParam bandwidthParams;
    bandwidthParams.iterationId = "13";
    bandwidthParams.rankId = "0";
    bandwidthParams.groupIdHash = testGroupName;
    bandwidthParams.operatorName = "Total Op Info";
    BandwidthDataResBody bandwidthBody;
    ASSERT_TRUE(reference.QueryBandwidthData(bandwidthParams, bandwidthBody));
    ASSERT_EQ(bandwidthBody.items.size(), 2);
    EXPECT_EQ(bandwidthBody.items[0].transportType, "HCCS");
    EXPECT_DOUBLE_EQ(bandwidthBody.items[0].transitSize, 16.777216);
    EXPECT_DOUBLE_EQ(bandwidthBody.items[0].transitTime, 0.80238);
    EXPECT_NEAR(bandwidthBody.items[0].bandwidth, 20.9093147885042, 1e-12);
    DistributionDataParam distributionParams;
    distributionParams.iterationId = "13";
    distributionParams.rankId = "0";
    distributionParams.groupIdHash = testGroupName;
    distributionParams.operatorName = "Total Op Info";
    distributionParams.transportType = "HCCS";
    DistributionResBody distributionBody;
    ASSERT_TRUE(reference.QueryDistributionData(distributionParams, distributionBody));
    document_t distributionJson;
    distributionJson.Parse(distributionBody.distributionData.c_str());
    ASSERT_FALSE(distributionJson.HasParseError());
    EXPECT_DOUBLE_EQ(distributionJson["8.388608"][0].GetDouble(), 2);
    EXPECT_DOUBLE_EQ(distributionJson["8.388608"][1].GetDouble(), 0.80238);
    std::vector<Module::GroupInfoDo> mapping;
    ASSERT_TRUE(reference.GetGroups(mapping));
    bool mapped = false;
    for (const auto &group : mapping) {
        if (group.groupIdHash == testGroupName) {
            mapped = true;
            EXPECT_EQ(group.groupId, "group_name_19");
        }
    }
    EXPECT_TRUE(mapped);
}

TEST_F(CommunicationStatisticsTest, RankPaginationCountsRanksAndKeepsAllTypesTogether) {
    ASSERT_TRUE(database.ExecSql("INSERT INTO TransitStatistic VALUES "
                                 "('step13',10,'3985310405473756128','NEW_TRANSPORT',0,1,0);"));
    auto first = QueryRankStatistics("step13", testGroupName, 1, 1);
    ASSERT_EQ(first.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(first.total, 3);
    const auto &firstItems = first.rows;
    ASSERT_EQ(firstItems.size(), 1);
    EXPECT_EQ(firstItems[0].rankId, 0);
    EXPECT_EQ(firstItems[0].bandwidths.size(), 2);
    auto last = QueryRankStatistics("step13", testGroupName, 2, 2);
    ASSERT_EQ(last.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(last.total, 3);
    const auto &lastItems = last.rows;
    ASSERT_EQ(lastItems.size(), 1);
    EXPECT_EQ(lastItems[0].rankId, 10);
    EXPECT_EQ(lastItems[0].bandwidths[0].bandType, "NEW_TRANSPORT");
    EXPECT_DOUBLE_EQ(lastItems[0].bandwidths[0].bandwidth, 0);
    auto middle = QueryRankStatistics("step13", testGroupName, 2, 1);
    ASSERT_EQ(middle.status, StatisticsStatus::SUCCESS);
    ASSERT_EQ(middle.rows.size(), 1);
    EXPECT_EQ(middle.rows[0].rankId, 2);
}

TEST_F(CommunicationStatisticsTest, RankPaginationEmptyAndOutOfRangePagesRetainFilteredTotal) {
    auto beyond = QueryRankStatistics("step13", testGroupName, 3, 1);
    ASSERT_EQ(beyond.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(beyond.total, 2);
    EXPECT_TRUE(beyond.rows.empty());
    auto maximum = QueryRankStatistics("step13", testGroupName, 10000000000LL, 1000);
    ASSERT_EQ(maximum.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(maximum.total, 2);
    EXPECT_TRUE(maximum.rows.empty());
    auto missing = QueryRankStatistics("step999", testGroupName, 2, 1);
    ASSERT_EQ(missing.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(missing.total, 0);
    EXPECT_TRUE(missing.rows.empty());
    auto other = QueryRankStatistics("step13", "other", 1, 1);
    ASSERT_EQ(other.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(other.total, 1);
}

TEST_F(CommunicationStatisticsTest, RankDefaultPaginationLimitsReturnedRanks) {
    for (int64_t rank = 3; rank < 26; ++rank) {
        ASSERT_TRUE(database.ExecSql("INSERT INTO TransitStatistic VALUES ('step13'," + std::to_string(rank) +
            ",'3985310405473756128','HCCS',1,1,1);"));
    }
    auto result = QueryRankStatistics();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(result.total, 25);
    EXPECT_EQ(result.rows.size(), 20);
}

TEST_F(CommunicationStatisticsTest, RankPaginationStillRejectsInvalidDataOutsideTheRequestedPage) {
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET bandwidth=-1 WHERE rank_id=2;"));
    EXPECT_EQ(QueryRankStatistics("step13", testGroupName, 1, 1).status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET bandwidth=1 WHERE rank_id=2;"
                                 "INSERT INTO TransitStatistic VALUES "
                                 "('step13',2,'3985310405473756128','HCCS',1,1,1);"));
    EXPECT_EQ(QueryRankStatistics("step13", testGroupName, 1, 1).status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, RankPaginationParsesDefaultsAndExplicitIntegerValues) {
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    auto defaults = dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params;
    EXPECT_EQ(defaults.options.currentPage, 1);
    EXPECT_EQ(defaults.options.pageSize, 20);
    json["params"].AddMember("currentPage", 2, json.GetAllocator());
    json["params"].AddMember("pageSize", 1, json.GetAllocator());
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    const auto &params = dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params;
    EXPECT_TRUE(params.CheckParams(error));
    EXPECT_EQ(params.options.currentPage, 2);
    EXPECT_EQ(params.options.pageSize, 1);
    json["params"].RemoveMember("currentPage");
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.options.currentPage, 1);
    EXPECT_EQ(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.options.pageSize, 1);
    json["params"].RemoveMember("pageSize");
    json["params"].AddMember("currentPage", 2, json.GetAllocator());
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_EQ(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.options.currentPage, 2);
    EXPECT_EQ(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.options.pageSize, 20);
}

TEST_F(CommunicationStatisticsTest, InvalidRankPaginationValuesReachBusinessValidation) {
    for (const auto &field : {"currentPage", "pageSize"}) {
        for (const auto &value : {"0", "-1", "1.5", "\"1\"", "null", "true", "[]", "{}", "10000000001",
                 "9223372036854775807", "18446744073709551615"}) {
            auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
            document_t invalid;
            invalid.Parse(value);
            json["params"].AddMember(
                json_t(field, json.GetAllocator()), json_t(invalid, json.GetAllocator()), json.GetAllocator());
            std::string error;
            auto request = protocol.FromJson(json, error);
            ASSERT_NE(request, nullptr);
            EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error))
                << field << "=" << value;
        }
    }
    for (const auto &options : {RankStatisticsOptions{0, 1}, RankStatisticsOptions{1, 0},
             RankStatisticsOptions{1, 1001}, RankStatisticsOptions{10000000001LL, 1}}) {
        EXPECT_EQ(
            database.QueryRankStatistics("step13", testGroupName, options).status, StatisticsStatus::INVALID_DATA);
    }
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    json["params"].AddMember("pageSize", 1001, json.GetAllocator());
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error));
}

TEST_F(CommunicationStatisticsTest, FailedRankResponseNeverLeaksPaginationTotal) {
    CommunicationRankStatisticsResponse response;
    response.result = false;
    response.total = 10;
    response.items = {{0, {{"HCCS", 1}}}};
    auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    EXPECT_TRUE((*json)["body"]["items"].Empty());
    EXPECT_EQ((*json)["body"]["total"].GetUint64(), 0);
}

TEST_F(CommunicationStatisticsTest, RankIdSortingIsNumericAndAppliedBeforePagination) {
    ASSERT_TRUE(AddSortingFixture());
    auto ascending = QueryRankSort("rankId", "ascend");
    ASSERT_EQ(ascending.status, StatisticsStatus::SUCCESS);
    const auto &ascendingItems = ascending.rows;
    ASSERT_EQ(ascendingItems.size(), 5);
    const std::vector<int64_t> ascendingRanks{0, 2, 3, 5, 10};
    for (size_t index = 0; index < ascendingRanks.size(); ++index) {
        EXPECT_EQ(ascendingItems[index].rankId, ascendingRanks[index]);
    }
    const std::vector<int64_t> descendingRanks{10, 5, 3, 2, 0};
    for (int64_t page = 1; page <= 3; ++page) {
        auto result = QueryRankSort("rankId", "descend", "", page, 2);
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        EXPECT_EQ(result.total, 5);
        const auto &items = result.rows;
        EXPECT_EQ(items.size(), page == 3 ? 1 : 2);
        for (size_t index = 0; index < items.size(); ++index) {
            EXPECT_EQ(items[index].rankId, descendingRanks[static_cast<size_t>((page - 1) * 2) + index]);
        }
    }
}

TEST_F(CommunicationStatisticsTest, BandwidthSortingKeepsZeroTiesAndMissingValuesInDeterministicOrder) {
    ASSERT_TRUE(AddSortingFixture());
    for (const auto &order : {"ascend", "descend"}) {
        auto result = QueryRankSort("bandwidth", order, "HCCS");
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        EXPECT_EQ(result.total, 5);
        const auto &items = result.rows;
        const std::vector<int64_t> expected = std::string(order) == "ascend" ? std::vector<int64_t>{5, 0, 2, 10, 3}
                                                                             : std::vector<int64_t>{2, 10, 0, 5, 3};
        ASSERT_EQ(items.size(), expected.size());
        for (size_t index = 0; index < items.size(); ++index) {
            EXPECT_EQ(items[index].rankId, expected[index]);
            if (items[index].rankId == 0) {
                EXPECT_EQ(items[index].bandwidths.size(), 2);
            }
            if (items[index].rankId == 5) {
                EXPECT_DOUBLE_EQ(items[index].bandwidths[0].bandwidth, 0);
            }
        }
        EXPECT_EQ(items.back().bandwidths[0].bandType, "SDMA");
    }
}

TEST_F(CommunicationStatisticsTest, BandwidthSortingPrecedesPaginationAndRetainsTotalOnEmptyPages) {
    ASSERT_TRUE(AddSortingFixture());
    const std::vector<int64_t> expected{2, 10, 0, 5, 3};
    for (int64_t page = 1; page <= 4; ++page) {
        auto result = QueryRankSort("bandwidth", "descend", "HCCS", page, 2);
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        EXPECT_EQ(result.total, 5);
        const auto &items = result.rows;
        EXPECT_EQ(items.size(), page < 3 ? 2 : page == 3 ? 1 : 0);
        for (size_t index = 0; index < items.size(); ++index) {
            EXPECT_EQ(items[index].rankId, expected[static_cast<size_t>((page - 1) * 2) + index]);
        }
    }
}

TEST_F(CommunicationStatisticsTest, BandwidthSortingSupportsNewTypesAndBindsUnknownTypeValues) {
    ASSERT_TRUE(AddSortingFixture());
    ASSERT_TRUE(database.ExecSql("INSERT INTO TransitStatistic VALUES "
                                 "('step13',20,'3985310405473756128','NEW_TRANSPORT',4,1,4);"));
    auto custom = QueryRankSort("bandwidth", "descend", "NEW_TRANSPORT", 1, 1);
    ASSERT_EQ(custom.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(custom.total, 6);
    ASSERT_EQ(custom.rows.size(), 1);
    EXPECT_EQ(custom.rows[0].rankId, 20);
    for (const auto &type : {"UNKNOWN", "hccs", "HCCS' OR 1=1 --"}) {
        auto result = QueryRankSort("bandwidth", "descend", type);
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        EXPECT_EQ(result.total, 6);
        const auto &items = result.rows;
        ASSERT_EQ(items.size(), 6);
        const std::vector<int64_t> expected{0, 2, 3, 5, 10, 20};
        for (size_t index = 0; index < items.size(); ++index) {
            EXPECT_EQ(items[index].rankId, expected[index]);
        }
    }
    auto empty = database.QueryRankStatistics("step999", testGroupName, {1, 1, "bandwidth", "descend", "HCCS"});
    EXPECT_EQ(empty.status, StatisticsStatus::SUCCESS);
    EXPECT_EQ(empty.total, 0);
    EXPECT_TRUE(empty.rows.empty());
}

TEST_F(CommunicationStatisticsTest, BandwidthSortingDoesNotHideInvalidRanksMetricsOrDuplicates) {
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET bandwidth=-1 WHERE rank_id=2;"));
    EXPECT_EQ(QueryRankSort("bandwidth", "descend", "HCCS", 1, 1).status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET bandwidth=30 WHERE rank_id=2;"
                                 "INSERT INTO TransitStatistic VALUES "
                                 "('step13',2,'3985310405473756128','HCCS',1,1,1);"));
    EXPECT_EQ(QueryRankSort("bandwidth", "descend", "HCCS", 1, 1).status, StatisticsStatus::INVALID_DATA);
    ASSERT_TRUE(database.ExecSql("DELETE FROM TransitStatistic WHERE rank_id=2;"
                                 "INSERT INTO TransitStatistic VALUES "
                                 "('step13',NULL,'3985310405473756128','HCCS',1,1,1);"));
    EXPECT_EQ(QueryRankSort("bandwidth", "descend", "HCCS", 1, 1).status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, RankSortingParsesDefaultsAndOptionalParameters) {
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    auto defaults = dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.options;
    EXPECT_EQ(defaults.orderBy, "rankId");
    EXPECT_EQ(defaults.order, "ascend");
    EXPECT_TRUE(defaults.bandType.empty());
    json["params"].AddMember("order", "descend", json.GetAllocator());
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    auto params = dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params;
    EXPECT_TRUE(params.CheckParams(error));
    EXPECT_EQ(params.options.order, "descend");
    json["params"].RemoveMember("order");
    json["params"].AddMember("orderBy", "bandwidth", json.GetAllocator());
    json["params"].AddMember("bandType", "HCCS", json.GetAllocator());
    request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    params = dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params;
    EXPECT_TRUE(params.CheckParams(error));
    EXPECT_EQ(params.options.orderBy, "bandwidth");
    EXPECT_EQ(params.options.order, "ascend");
    EXPECT_EQ(params.options.bandType, "HCCS");
}

TEST_F(CommunicationStatisticsTest, InvalidRankSortFieldsReturnBusinessValidationErrors) {
    const std::vector<std::pair<std::string, std::string>> invalidValues{{"orderBy", "\"\""},
        {"orderBy", "\"rank_id\""}, {"orderBy", "\"HCCS\""}, {"orderBy", "\"rankId; DROP TABLE TransitStatistic\""},
        {"orderBy", "1"}, {"orderBy", "null"}, {"order", "\"\""}, {"order", "\"asc\""}, {"order", "\"ASC\""},
        {"order", "\"descend; DROP TABLE TransitStatistic\""}, {"order", "true"}, {"order", "null"}, {"bandType", "1"},
        {"bandType", "null"}, {"bandType", "[]"}, {"bandType", "{}"}};
    for (const auto &field : invalidValues) {
        auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
        document_t invalid;
        invalid.Parse(field.second.c_str());
        json["params"].AddMember(json_t(field.first.c_str(), json.GetAllocator()), json_t(invalid, json.GetAllocator()),
            json.GetAllocator());
        std::string error;
        auto request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr);
        EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error))
            << field.first << "=" << field.second;
    }
    for (const auto &type : {std::string{}, std::string(4097, 'A'), std::string("HCCS\0SDMA", 9)}) {
        auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
        json["params"].AddMember("orderBy", "bandwidth", json.GetAllocator());
        json["params"].AddMember(
            "bandType", json_t(type.c_str(), type.size(), json.GetAllocator()), json.GetAllocator());
        std::string error;
        auto request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr);
        EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error));
    }
    auto json = RequestJson(REQ_RES_COMMUNICATION_RANK_STATISTICS);
    json["params"].AddMember("orderBy", "bandwidth", json.GetAllocator());
    std::string error;
    auto request = protocol.FromJson(json, error);
    ASSERT_NE(request, nullptr);
    EXPECT_FALSE(dynamic_cast<CommunicationRankStatisticsRequest &>(*request).params.CheckParams(error));
    EXPECT_EQ(QueryRankSort("rankId; DROP TABLE TransitStatistic", "ascend").status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryRankSort("rankId", "ascend; DROP TABLE TransitStatistic").status, StatisticsStatus::INVALID_DATA);
    EXPECT_EQ(QueryRankSort("bandwidth", "ascend").status, StatisticsStatus::INVALID_DATA);
}

TEST_F(CommunicationStatisticsTest, GroupQueriesRequireIterationAndFilterSelectedStep) {
    auto check = [&](const auto &prototype) {
        using RequestType = std::decay_t<decltype(prototype)>;
        auto json = RequestJson(prototype.command);
        json["params"].RemoveMember("iterationId");
        std::string error;
        auto request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr);
        EXPECT_FALSE(dynamic_cast<const RequestType &>(*request).params.CheckParams(error));
        json["params"].AddMember("iterationId", "invalid", json.GetAllocator());
        request = protocol.FromJson(json, error);
        ASSERT_NE(request, nullptr);
        EXPECT_FALSE(dynamic_cast<const RequestType &>(*request).params.CheckParams(error));
    };
    check(CommunicationGroupRangeRatioRequest{});
    check(CommunicationGroupBandwidthRequest{});
    ExpectEmptyResult(QueryGroupRangeRatio("step999"));
    ExpectEmptyResult(QueryGroupBandwidth("step999"));
}

TEST_F(CommunicationStatisticsTest, GroupRatioTakesExtremaOnlyWithinSelectedStep) {
    ASSERT_TRUE(database.ExecSql("DELETE FROM CommTimeStatistic; INSERT INTO CommTimeStatistic VALUES "
                                 "('step1',0,'other',100),('step1',1,'other',1),"
                                 "('step2',0,'other',2),('step2',1,'other',100);"));
    for (const auto &sample : {std::pair<std::string, double>{"step1", 100}, {"step2", 50}}) {
        const auto result = QueryGroupRangeRatio(sample.first);
        ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
        const auto &items = result.rows;
        ASSERT_EQ(items.size(), 1);
        EXPECT_DOUBLE_EQ(*items[0].value, sample.second);
    }
}

TEST_F(CommunicationStatisticsTest, GroupAndOperatorResultsSortWithStableTiesAndNullsLast) {
    ASSERT_TRUE(database.ExecSql("INSERT INTO CommunicationGroupMapping VALUES ('z','z_id'),('a','a_id');"
                                 "INSERT INTO CommTimeStatistic VALUES ('step13',0,'z',1),('step13',1,'z',100),"
                                 "('step13',0,'a',1),('step13',1,'a',100);"
                                 "UPDATE CommTimeStatistic SET comm_total_time=0 WHERE group_name='other';"
                                 "INSERT INTO TransitStatistic VALUES ('step13',0,'z','HCCS',1000,1,1000),"
                                 "('step13',0,'a','HCCS',1000,1,1000);"
                                 "UPDATE TransitStatistic SET transit_time=0 WHERE group_name='other';"
                                 "INSERT INTO CommOpStatistic VALUES ('step13','3985310405473756128','z',0,100),"
                                 "('step13','3985310405473756128','a',0,100);"));
    const auto ranges = QueryGroupRangeRatio();
    ASSERT_EQ(ranges.status, StatisticsStatus::SUCCESS);
    ASSERT_EQ(ranges.rows.size(), 4);
    EXPECT_EQ(ranges.rows[0].groupName, "a");
    EXPECT_EQ(ranges.rows[1].groupName, "z");
    EXPECT_FALSE(ranges.rows.back().value.has_value());

    const auto bandwidths = QueryGroupBandwidth();
    ASSERT_EQ(bandwidths.status, StatisticsStatus::SUCCESS);
    ASSERT_EQ(bandwidths.rows.size(), 4);
    EXPECT_EQ(bandwidths.rows[0].groupName, testGroupName);
    EXPECT_EQ(bandwidths.rows[1].groupName, "a");
    EXPECT_EQ(bandwidths.rows[2].groupName, "z");
    EXPECT_EQ(bandwidths.rows[3].groupName, "other");
    EXPECT_FALSE(bandwidths.rows[3].value.has_value());

    auto result = QueryOperatorFluctuation();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    const auto &items = result.rows;
    EXPECT_EQ(items[0].opName, "a");
    EXPECT_EQ(items[1].opName, "z");
    EXPECT_DOUBLE_EQ(items[0].rangeTime, 100);
}

TEST_F(CommunicationStatisticsTest, TotalOperatorBandwidthUsesTransitAndDoesNotMutateRequestOrRound) {
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET transit_time=0.0000123456789 WHERE rank_id=0;"));
    BandwidthDataParam params;
    params.iterationId = "13";
    params.rankId = "0";
    params.groupIdHash = "3985310405473756128";
    params.operatorName = "Total Op   Info";
    BandwidthDataResBody body;
    ASSERT_TRUE(database.QueryBandwidthData(params, body));
    ASSERT_EQ(body.items.size(), 2);
    EXPECT_EQ(body.items[0].transportType, "HCCS");
    EXPECT_DOUBLE_EQ(body.items[0].transitTime, 0.0000123456789);
    EXPECT_DOUBLE_EQ(body.items[0].bandwidth, 10);
    EXPECT_EQ(params.iterationId, "13");
    params.iterationId = "step13";
    ASSERT_TRUE(database.QueryBandwidthData(params, body));
    EXPECT_EQ(body.items.size(), 2);
    params.operatorName = "ordinary_operator";
    ASSERT_TRUE(database.QueryBandwidthData(params, body));
    EXPECT_TRUE(body.items.empty());
}

TEST_F(CommunicationStatisticsTest, TotalOperatorDistributionConvertsPacketsToLegacyShape) {
    ASSERT_TRUE(database.ExecSql("ALTER TABLE TransitStatistic ADD COLUMN package_distribution TEXT DEFAULT '[]';"
                                 "UPDATE TransitStatistic SET transit_time=0.123456789, package_distribution="
                                 "'[{\"package_size\":8.388608,\"count\":2},{\"package_size\":1,\"count\":3}]' "
                                 "WHERE step='step13' AND rank_id=0 AND band_type='HCCS';"));
    DistributionDataParam params;
    params.iterationId = "13";
    params.rankId = "0";
    params.groupIdHash = "3985310405473756128";
    params.operatorName = "Total Op Info";
    params.transportType = "HCCS";
    DistributionResBody body;
    ASSERT_TRUE(database.QueryDistributionData(params, body));
    document_t json;
    json.Parse(body.distributionData.c_str());
    ASSERT_FALSE(json.HasParseError());
    EXPECT_DOUBLE_EQ(json["8.388608"][0].GetDouble(), 2);
    EXPECT_DOUBLE_EQ(json["8.388608"][1].GetDouble(), 0.123456789);
    EXPECT_DOUBLE_EQ(json["1"][0].GetDouble(), 3);
    EXPECT_DOUBLE_EQ(json["1"][1].GetDouble(), 0.123456789);
    EXPECT_EQ(params.iterationId, "13");
    params.transportType = "RDMA";
    ASSERT_TRUE(database.QueryDistributionData(params, body));
    EXPECT_TRUE(body.distributionData.empty());
    params.transportType = "HCCS";
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET transit_time=NULL;"));
    EXPECT_FALSE(database.QueryDistributionData(params, body));
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET transit_time=1, package_distribution='invalid';"));
    EXPECT_FALSE(database.QueryDistributionData(params, body));
}

TEST_F(CommunicationStatisticsTest, TotalOperatorFallsBackOnlyWhenTransitTableIsMissing) {
    ASSERT_TRUE(database.ExecSql(
        "CREATE TABLE ClusterCommunicationBandwidth(step TEXT,rank_id INTEGER,"
        "group_name TEXT,hccl_op_name TEXT,band_type TEXT,transit_size REAL,transit_time REAL,bandwidth REAL,"
        "large_packet_ratio REAL,package_size REAL,count INTEGER,total_duration REAL);"
        "INSERT INTO ClusterCommunicationBandwidth VALUES ('step13',0,'3985310405473756128',"
        "'Total Op Info','RDMA',1,0.0000123456789,123456.789123,0.123456789,1,2,0.123456789);"));
    BandwidthDataParam bandwidth;
    bandwidth.iterationId = "13";
    bandwidth.rankId = "0";
    bandwidth.groupIdHash = "3985310405473756128";
    bandwidth.operatorName = "Total Op Info";
    BandwidthDataResBody bandBody;
    DistributionDataParam distribution;
    distribution.iterationId = "13";
    distribution.rankId = "0";
    distribution.groupIdHash = bandwidth.groupIdHash;
    distribution.operatorName = bandwidth.operatorName;
    distribution.transportType = "RDMA";
    DistributionResBody distBody;
    ASSERT_TRUE(database.QueryBandwidthData(bandwidth, bandBody));
    EXPECT_EQ(bandBody.items[0].transportType, "HCCS");
    EXPECT_FALSE(database.QueryDistributionData(distribution, distBody));
    ASSERT_TRUE(database.ExecSql(
        "DELETE FROM TransitStatistic; ALTER TABLE TransitStatistic ADD COLUMN package_distribution TEXT;"));
    ASSERT_TRUE(database.QueryBandwidthData(bandwidth, bandBody));
    EXPECT_TRUE(bandBody.items.empty());
    ASSERT_TRUE(database.QueryDistributionData(distribution, distBody));
    EXPECT_TRUE(distBody.distributionData.empty());
    ASSERT_TRUE(database.ExecSql("DROP TABLE TransitStatistic;"));
    ASSERT_TRUE(database.QueryBandwidthData(bandwidth, bandBody));
    ASSERT_EQ(bandBody.items.size(), 1);
    EXPECT_EQ(bandBody.items[0].transportType, "RDMA");
    EXPECT_DOUBLE_EQ(bandBody.items[0].transitTime, 0.0000123456789);
    EXPECT_DOUBLE_EQ(bandBody.items[0].bandwidth, 123456.789123);
    ASSERT_TRUE(database.QueryDistributionData(distribution, distBody));
    EXPECT_EQ(distBody.distributionData, "{\"1.0\":[2,0.123456789]}");
    bandwidth.operatorName = "Total Op   Info";
    bandwidth.iterationId = "step13";
    bandBody.items.clear();
    ASSERT_TRUE(database.QueryBandwidthData(bandwidth, bandBody));
    EXPECT_EQ(bandBody.items.size(), 1);
    EXPECT_EQ(bandwidth.iterationId, "step13");
    distribution.operatorName = "Total Op   Info";
    distribution.iterationId = "step13";
    ASSERT_TRUE(database.QueryDistributionData(distribution, distBody));
    EXPECT_FALSE(distBody.distributionData.empty());
}

TEST_F(CommunicationStatisticsTest, InvalidTransitRowsDoNotFallBackOrLeakPartialBandwidth) {
    BandwidthDataParam params;
    params.iterationId = "13";
    params.rankId = "0";
    params.groupIdHash = "3985310405473756128";
    params.operatorName = "Total Op Info";
    BandwidthDataResBody body;
    ASSERT_TRUE(database.ExecSql("UPDATE TransitStatistic SET bandwidth=-1 WHERE band_type='SDMA';"));
    EXPECT_FALSE(database.QueryBandwidthData(params, body));
    EXPECT_TRUE(body.items.empty());
}

TEST_F(CommunicationStatisticsTest, MatrixGroupSerializesMappedIdsInCamelCaseForBothSides) {
    MatrixGroupResponse response;
    response.result = true;
    GroupInfo group;
    group.groupId.compare = "group_name_19";
    group.groupId.baseline = "baseline_group";
    response.body.groupList.push_back(group);
    std::string error;
    auto json = protocol.ToJson(response, error);
    ASSERT_TRUE(json.has_value());
    const auto &item = (*json)["body"]["data"][0];
    EXPECT_STREQ(item["groupId"]["compare"].GetString(), "group_name_19");
    EXPECT_STREQ(item["groupId"]["baseline"].GetString(), "baseline_group");
    EXPECT_FALSE(item.HasMember("group_id"));
}

TEST_F(CommunicationStatisticsTest, MatrixGroupsReadDisplayIdFromMapping) {
    ASSERT_TRUE(database.ExecSql("ALTER TABLE CommunicationGroupMapping ADD COLUMN rank_set TEXT DEFAULT '(0,2)';"));
    std::vector<Module::GroupInfoDo> groups;
    ASSERT_TRUE(database.GetGroups(groups));
    ASSERT_EQ(groups.size(), 2);
    EXPECT_EQ(groups[0].groupId, "group_name_19");
    EXPECT_EQ(groups[0].groupIdHash, "3985310405473756128");
}

TEST_F(CommunicationStatisticsTest, GroupBandwidthKeepsFiniteRatiosWhenIntermediateSumsExceedDoubleRange) {
    ASSERT_TRUE(
        database.ExecSql("DELETE FROM TransitStatistic; INSERT INTO TransitStatistic VALUES "
                         "('step13',0,'other','HCCS',1e308,1e308,1),('step13',1,'other','HCCS',1e308,1e308,1);"));
    const auto result = QueryGroupBandwidth();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_DOUBLE_EQ(*result.rows[0].value, 1);
}

TEST_F(CommunicationStatisticsTest, GroupBandwidthPreservesSmallContributionsWithoutPerStepRounding) {
    ASSERT_TRUE(database.ExecSql("DELETE FROM TransitStatistic; INSERT INTO TransitStatistic VALUES "
                                 "('step13',0,'other','HCCS',10000000000000000,1,0),"
                                 "('step13',1,'other','HCCS',1,0,0),('step13',2,'other','HCCS',1,0,0);"));
    const auto result = QueryGroupBandwidth();
    ASSERT_EQ(result.status, StatisticsStatus::SUCCESS);
    EXPECT_DOUBLE_EQ(*result.rows[0].value, 10000000000000002.0);
}
