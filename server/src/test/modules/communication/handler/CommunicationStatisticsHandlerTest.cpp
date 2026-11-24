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
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include "../../timeline/handler/HandlerTest.cpp"
#include "CommunicationGroupRangeRatioHandler.h"
#include "CommunicationGroupBandwidthHandler.h"
#include "CommunicationOperatorFluctuationHandler.h"
#include "CommunicationRankStatisticsHandler.h"
#include "CommunicationStatisticsProtocol.h"
#include "CommunicationProtocol.h"
#include "CommunicationModule.h"
#include "ClusterService.h"
#include "DataBaseManager.h"

using namespace Dic;
using namespace Dic::Protocol;
using namespace Dic::Module::Communication;

class CommunicationStatisticsHandlerTest : public HandlerTest {
  protected:
    class StatisticsModule : public Module::CommunicationModule {
      public:
        Module::ModuleRequestHandler &GetHandler(const std::string &command) { return *requestHandlerMap.at(command); }
    };

    void SetUp() override {
        capture = std::make_unique<ResponseCapture>();
        protocol.Register();
        module.RegisterRequestHandlers();
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for (int index = 0; index < 2; ++index) {
            paths.push_back((std::filesystem::temp_directory_path() /
                ("communication_statistics_" + unique + "_" + std::to_string(index) + ".db"))
                                .string());
            projects.push_back(paths.back() + ".project");
            sqlite3 *database = nullptr;
            ASSERT_EQ(sqlite3_open(paths.back().c_str(), &database), SQLITE_OK);
            const std::string sql =
                "CREATE TABLE CommTimeStatistic(step TEXT,rank_id INTEGER,group_name TEXT,comm_total_time REAL);"
                "CREATE TABLE TransitStatistic(step TEXT,rank_id INTEGER,group_name TEXT,band_type TEXT,"
                "transit_size REAL,transit_time REAL,bandwidth REAL);"
                "CREATE TABLE CommOpStatistic(step TEXT,group_name TEXT,op_name TEXT,min_time REAL,max_time REAL);"
                "CREATE TABLE CommunicationGroupMapping(group_name TEXT,group_id TEXT);"
                "INSERT INTO CommunicationGroupMapping VALUES ('3985310405473756128','group_name_19');"
                "INSERT INTO CommTimeStatistic VALUES ('step13',0,'3985310405473756128'," +
                std::to_string(index == 0 ? 4 : 10) +
                "),('step13',1,'3985310405473756128',2);"
                "INSERT INTO TransitStatistic VALUES ('step13',0,'3985310405473756128','HCCS',10,1,10),"
                "('step13',1,'3985310405473756128','HCCS',90,3,30);"
                "INSERT INTO CommOpStatistic VALUES ('step13','3985310405473756128','op',1,5);";
            const int status = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr);
            sqlite3_close(database);
            ASSERT_EQ(status, SQLITE_OK);
            Module::Timeline::DataBaseManager::Instance().CreateClusterConnectionPool(
                projects.back(), paths.back(), Module::Timeline::DataType::DB);
        }
    }

    void TearDown() override {
        for (const auto &path : paths) {
            Module::Timeline::DataBaseManager::Instance().EraseClusterDb(path);
            for (const auto &suffix : {"", "-wal", "-shm"}) {
                std::error_code error;
                std::filesystem::remove(path + suffix, error);
            }
        }
        protocol.UnRegister();
        capture.reset();
    }

    bool Run(const std::string &command, const std::string &project, const std::string &iteration = "13",
        std::optional<int64_t> currentPage = std::nullopt, std::optional<int64_t> pageSize = std::nullopt,
        const std::string &orderBy = "", const std::string &order = "", const std::string &bandType = "") {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        json.AddMember("id", 42, allocator);
        json.AddMember("moduleName", "communication", allocator);
        json.AddMember("type", "request", allocator);
        json.AddMember("resultCallbackId", 7, allocator);
        json.AddMember("command", json_t(command.c_str(), allocator), allocator);
        json_t params(kObjectType);
        params.AddMember("clusterPath", json_t(project.c_str(), allocator), allocator);
        params.AddMember("iterationId", json_t(iteration.c_str(), allocator), allocator);
        params.AddMember("groupName", "3985310405473756128", allocator);
        params.AddMember("transportType", "HCCS", allocator);
        if (currentPage) {
            params.AddMember("currentPage", *currentPage, allocator);
        }
        if (pageSize) {
            params.AddMember("pageSize", *pageSize, allocator);
        }
        if (!orderBy.empty()) {
            params.AddMember("orderBy", json_t(orderBy.c_str(), allocator), allocator);
        }
        if (!order.empty()) {
            params.AddMember("order", json_t(order.c_str(), allocator), allocator);
        }
        if (!bandType.empty()) {
            params.AddMember("bandType", json_t(bandType.c_str(), allocator), allocator);
        }
        json.AddMember("params", params, allocator);
        std::string error;
        auto request = protocol.FromJson(json, error);
        if (request == nullptr) {
            ADD_FAILURE() << error;
            return false;
        }
        return module.GetHandler(command).HandleRequestEntrance(std::move(request));
    }

    const Response &LastResponse() const { return *capture->session->responses.back(); }

    std::unique_ptr<ResponseCapture> capture;
    std::vector<std::string> paths;
    std::vector<std::string> projects;
    CommunicationProtocol protocol;
    StatisticsModule module;
};

TEST_F(CommunicationStatisticsHandlerTest, FourCommandsPreserveEnvelopeThroughHandlerAndSerialization) {
    for (const auto &command : {REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, REQ_RES_COMMUNICATION_GROUP_BANDWIDTH,
             REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION, REQ_RES_COMMUNICATION_RANK_STATISTICS}) {
        ASSERT_TRUE(Run(command, projects[0]));
        const auto &response = LastResponse();
        EXPECT_TRUE(response.result);
        EXPECT_EQ(response.requestId, 42);
        EXPECT_EQ(response.command, command);
        EXPECT_EQ(response.moduleName, "communication");
        ASSERT_TRUE(response.resultCallbackId.has_value());
        EXPECT_EQ(*response.resultCallbackId, 7);
        std::string error;
        auto json = protocol.ToJson(response, error);
        ASSERT_TRUE(json.has_value()) << error;
        EXPECT_FALSE((*json)["body"]["items"].Empty());
    }
}

TEST_F(CommunicationStatisticsHandlerTest, WrongRequestTypeSendsOneFailureWithOriginalEnvelope) {
    for (const auto &command : {REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, REQ_RES_COMMUNICATION_GROUP_BANDWIDTH,
             REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION, REQ_RES_COMMUNICATION_RANK_STATISTICS}) {
        for (bool hasCallback : {false, true}) {
            auto request = std::make_unique<Request>(command);
            request->id = 42;
            request->moduleName = "communication";
            if (hasCallback) {
                request->resultCallbackId = 7;
            }
            const auto responseCount = capture->session->responses.size();
            ASSERT_FALSE(module.GetHandler(command).HandleRequestEntrance(std::move(request)));
            ASSERT_EQ(capture->session->responses.size(), responseCount + 1);
            const auto &response = LastResponse();
            EXPECT_FALSE(response.result);
            EXPECT_EQ(response.requestId, 42);
            EXPECT_EQ(response.command, command);
            EXPECT_EQ(response.moduleName, "communication");
            EXPECT_EQ(response.resultCallbackId, hasCallback ? std::optional<int>{7} : std::nullopt);
            ASSERT_TRUE(response.error.has_value());
            EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Communication::ErrorCode::PARAMS_ERROR));
            std::string error;
            auto json = protocol.ToJson(response, error);
            ASSERT_TRUE(json.has_value()) << error;
            EXPECT_FALSE((*json)["result"].GetBool());
            EXPECT_TRUE((*json)["body"]["items"].Empty());
        }
    }
}

TEST_F(CommunicationStatisticsHandlerTest, NullRequestFailsWithoutSendingAnUncorrelatedResponse) {
    for (const auto &command : {REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, REQ_RES_COMMUNICATION_GROUP_BANDWIDTH,
             REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION, REQ_RES_COMMUNICATION_RANK_STATISTICS}) {
        EXPECT_FALSE(module.GetHandler(command).HandleRequest(nullptr));
    }
    EXPECT_TRUE(capture->session->responses.empty());
}

TEST_F(CommunicationStatisticsHandlerTest, ExplicitClusterPathsNeverMixConnections) {
    for (size_t index = 0; index < projects.size(); ++index) {
        ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, projects[index]));
        const auto &response = dynamic_cast<const CommunicationGroupRangeRatioResponse &>(LastResponse());
        const auto &items = response.items;
        ASSERT_EQ(items.size(), 1);
        EXPECT_DOUBLE_EQ(*items[0].value, index == 0 ? 2 : 5);
    }
}

TEST_F(CommunicationStatisticsHandlerTest, UnregisteredPathFailsWithoutOpeningAnArbitraryDatabase) {
    ASSERT_FALSE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, paths[0]));
    const auto &response = LastResponse();
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Communication::ErrorCode::CONNECT_DATABASE_FAILED));
    const std::string nonexistent = paths[0] + ".unregistered";
    ASSERT_FALSE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, nonexistent));
    EXPECT_FALSE(std::filesystem::exists(nonexistent));
}

TEST_F(CommunicationStatisticsHandlerTest, EmptyResultsAreSuccessfulArraysForEveryCommand) {
    for (const auto &command : {REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, REQ_RES_COMMUNICATION_GROUP_BANDWIDTH,
             REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION, REQ_RES_COMMUNICATION_RANK_STATISTICS}) {
        ASSERT_TRUE(Run(command, projects[0], "999"));
        std::string error;
        auto json = protocol.ToJson(LastResponse(), error);
        ASSERT_TRUE(json.has_value());
        EXPECT_TRUE((*json)["body"]["items"].Empty());
    }
}

TEST_F(CommunicationStatisticsHandlerTest, MissingTableHasDifferentErrorFromInvalidMapping) {
    auto &manager = Module::Timeline::DataBaseManager::Instance();
    auto database = manager.GetClusterDatabase(projects[0]);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql("DELETE FROM CommunicationGroupMapping;"));
    database.reset();
    ASSERT_FALSE(Run(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, projects[0]));
    ASSERT_TRUE(LastResponse().error.has_value());
    EXPECT_EQ(LastResponse().error->code,
        static_cast<int>(Dic::Module::Communication::ErrorCode::COMMUNICATION_STATISTICS_INVALID_DATA));
    database = manager.GetClusterDatabase(projects[0]);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql("DROP TABLE CommTimeStatistic;"));
    database.reset();
    ASSERT_FALSE(Run(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, projects[0]));
    EXPECT_EQ(LastResponse().error->code,
        static_cast<int>(Dic::Module::Communication::ErrorCode::COMMUNICATION_STATISTICS_UNSUPPORTED));
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0]));
}

TEST_F(CommunicationStatisticsHandlerTest, DirectHandlerCallsAlsoValidateParams) {
    auto request = std::make_unique<CommunicationRankStatisticsRequest>();
    request->params.iterationId = "13";
    request->params.groupName = "3985310405473756128";
    EXPECT_FALSE(module.GetHandler(REQ_RES_COMMUNICATION_RANK_STATISTICS).HandleRequest(std::move(request)));
    ASSERT_FALSE(capture->session->responses.empty());
    ASSERT_TRUE(LastResponse().error.has_value());
    EXPECT_EQ(LastResponse().error->code, static_cast<int>(Dic::Module::Communication::ErrorCode::PARAMS_ERROR));
}

TEST_F(CommunicationStatisticsHandlerTest, ModuleRegistersEveryNewCommand) {
    EXPECT_NE(dynamic_cast<CommunicationGroupRangeRatioHandler *>(
                  &module.GetHandler(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO)),
        nullptr);
    EXPECT_NE(
        dynamic_cast<CommunicationGroupBandwidthHandler *>(&module.GetHandler(REQ_RES_COMMUNICATION_GROUP_BANDWIDTH)),
        nullptr);
    EXPECT_NE(dynamic_cast<CommunicationOperatorFluctuationHandler *>(
                  &module.GetHandler(REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION)),
        nullptr);
    EXPECT_NE(
        dynamic_cast<CommunicationRankStatisticsHandler *>(&module.GetHandler(REQ_RES_COMMUNICATION_RANK_STATISTICS)),
        nullptr);
}

TEST_F(CommunicationStatisticsHandlerTest, InvalidBusinessParametersProduceAnErrorResponse) {
    ASSERT_FALSE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "-1"));
    const auto &response = LastResponse();
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, 1101);
}

TEST_F(CommunicationStatisticsHandlerTest, RankPaginationPreservesTotalAndPageMetadataThroughSerialization) {
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "13", 2, 1));
    const auto &response = dynamic_cast<const CommunicationRankStatisticsResponse &>(LastResponse());
    ASSERT_EQ(response.total, 2);
    ASSERT_EQ(response.items.size(), 1);
    EXPECT_EQ(response.items[0].rankId, 1);
    auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ((*json)["body"]["total"].GetUint64(), 2);
    EXPECT_EQ((*json)["body"]["currentPage"].GetInt64(), 2);
    EXPECT_EQ((*json)["body"]["pageSize"].GetInt64(), 1);
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "13", 3, 1));
    auto beyond = dynamic_cast<const CommunicationRankStatisticsResponse &>(LastResponse()).ToJson();
    ASSERT_TRUE(beyond.has_value());
    EXPECT_TRUE((*beyond)["body"]["items"].Empty());
    EXPECT_EQ((*beyond)["body"]["total"].GetUint64(), 2);
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "999", 1, 1));
    EXPECT_EQ(dynamic_cast<const CommunicationRankStatisticsResponse &>(LastResponse()).total, 0);
}

TEST_F(CommunicationStatisticsHandlerTest, InvalidRankPaginationProducesParameterErrorInsteadOfOpeningAConnection) {
    for (const auto &pagination : {std::pair<int64_t, int64_t>{0, 1}, {-1, 1}, {1, 0}, {1, 1001}, {10000000001LL, 1}}) {
        ASSERT_FALSE(
            Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, "unregistered", "13", pagination.first, pagination.second));
        ASSERT_TRUE(LastResponse().error.has_value());
        EXPECT_EQ(LastResponse().error->code, 1101);
    }
}

TEST_F(CommunicationStatisticsHandlerTest, GroupQueriesFilterSelectedStepThroughTheHandler) {
    auto database = Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(projects[0]);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(
        database->ExecSql("INSERT INTO CommTimeStatistic VALUES ('step14',0,'3985310405473756128',10);"
                          "INSERT INTO TransitStatistic VALUES ('step14',0,'3985310405473756128','HCCS',300,3,100);"));
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO, projects[0], "14"));
    auto range = dynamic_cast<const CommunicationGroupRangeRatioResponse &>(LastResponse()).ToJson();
    ASSERT_TRUE(range.has_value());
    EXPECT_DOUBLE_EQ((*range)["body"]["items"][0]["rangeRatio"].GetDouble(), 1);
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_GROUP_BANDWIDTH, projects[0], "14"));
    auto bandwidth = dynamic_cast<const CommunicationGroupBandwidthResponse &>(LastResponse()).ToJson();
    ASSERT_TRUE(bandwidth.has_value());
    EXPECT_DOUBLE_EQ((*bandwidth)["body"]["items"][0]["bandwidth"].GetDouble(), 100);
}

TEST_F(CommunicationStatisticsHandlerTest, MatrixGroupServiceCarriesMappedDisplayIdToResponse) {
    auto database = Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(projects[0]);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(
        database->ExecSql("ALTER TABLE CommunicationGroupMapping ADD COLUMN rank_set TEXT DEFAULT '(0,1)';"
                          "ALTER TABLE CommunicationGroupMapping ADD COLUMN pg_name TEXT DEFAULT 'data_parallel';"));
    MatrixGroupRequest request;
    request.params.clusterPath = projects[0];
    request.params.iterationId = "13";
    MatrixGroupResponse response;
    Module::Communication::ClusterService::QueryGroupInfo(request, response);
    ASSERT_EQ(response.body.groupList.size(), 1);
    EXPECT_EQ(response.body.groupList[0].groupId.compare, "group_name_19");
    EXPECT_TRUE(response.body.groupList[0].groupId.baseline.empty());
    EXPECT_EQ(response.body.groupList[0].groupIdHash.compare, "3985310405473756128");
}

TEST_F(CommunicationStatisticsHandlerTest, RankSortingFlowsThroughProtocolAndHandlerBeforePagination) {
    auto database = Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(projects[0]);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql("INSERT INTO TransitStatistic VALUES "
                                  "('step13',0,'3985310405473756128','SDMA',1,1,1);"));
    database.reset();
    for (const auto &orderBy : {"rankId", "bandwidth"}) {
        ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "13", 1, 1, orderBy, "descend", "HCCS"));
        const auto &response = dynamic_cast<const CommunicationRankStatisticsResponse &>(LastResponse());
        EXPECT_EQ(response.total, 2);
        auto json = response.ToJson();
        ASSERT_TRUE(json.has_value());
        const auto &body = (*json)["body"];
        ASSERT_EQ(body["items"].Size(), 1);
        EXPECT_EQ(body["items"][0]["rankId"].GetInt64(), 1);
        EXPECT_EQ(body["total"].GetUint64(), 2);
        EXPECT_EQ(body["currentPage"].GetInt64(), 1);
        EXPECT_EQ(body["pageSize"].GetInt64(), 1);
        EXPECT_FALSE(body.HasMember("orderBy"));
    }
    ASSERT_TRUE(Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, projects[0], "13", 1, 1, "bandwidth", "ascend", "HCCS"));
    const auto &response = dynamic_cast<const CommunicationRankStatisticsResponse &>(LastResponse());
    const auto &items = response.items;
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].rankId, 0);
    EXPECT_EQ(items[0].bandwidths.size(), 2);
}

TEST_F(CommunicationStatisticsHandlerTest, InvalidRankSortingReturns1101BeforeConnectionLookup) {
    for (const auto &sorting : {std::vector<std::string>{"rank_id", "ascend", ""}, {"rankId", "ASC", ""},
             {"bandwidth", "descend", ""}, {"rankId;DROP TABLE", "ascend", ""}}) {
        ASSERT_FALSE(
            Run(REQ_RES_COMMUNICATION_RANK_STATISTICS, "unregistered", "13", 1, 1, sorting[0], sorting[1], sorting[2]));
        ASSERT_TRUE(LastResponse().error.has_value());
        EXPECT_EQ(LastResponse().error->code, 1101);
    }
}
