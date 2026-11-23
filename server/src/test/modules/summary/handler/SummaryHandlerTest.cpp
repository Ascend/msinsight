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
#include "SummaryProtocolRequest.h"
#include "../../timeline/handler/HandlerTest.cpp"
#include "QueryFwdBwdTimelineHandler.h"
#include "QueryCommunicationDetailHandler.h"
#include "QueryComputeDetailInfoHandler.h"
#include "QueryParallelStrategyConfigHandler.h"
#include "SetParallelStrategyConfigHandler.h"
#include "QueryParallelismArrangementHandler.h"
#include "QueryParallelismPerformanceHandler.h"
#include "SummaryTopRankHandler.h"
#include "ImportExpertDataHandler.h"
#include "QueryExpertHotspotHandler.h"
#include "QueryModelInfoHandler.h"
#include "SummaryModule.h"
#include "SummaryProtocol.h"
#include "SummarySlowRankAdvisorHandler.h"
#include "utils/Utf8TestUtil.h"

using namespace Dic::Module;
using namespace Dic::Module::Summary;

const int NUMBER_FIVE_HUNDRED = 500;
const int NUMBER_TEN = 10;
const int NUMBER_ONE = 1;

class SummaryClusterStatisticsHandlerTest : public HandlerTest {
  protected:
    class TestModule : public SummaryModule {
      public:
        void Handle(std::unique_ptr<Dic::Protocol::Request> request) {
            const auto handler = requestHandlerMap.find(request->command);
            ASSERT_NE(handler, requestHandlerMap.end());
            handler->second->HandleRequest(std::move(request));
        }
    };

    void SetUp() override {
        module.RegisterRequestHandlers();
        protocol.Register();
    }

    void TearDown() override { Timeline::DataBaseManager::Instance().EraseClusterDb(databasePath); }

    std::shared_ptr<VirtualClusterDatabase> AddDatabase(Timeline::DataType type) {
        auto &manager = Timeline::DataBaseManager::Instance();
        manager.CreateClusterConnectionPool(projectPath, databasePath, type);
        return manager.GetClusterDatabase(projectPath);
    }

    std::optional<Dic::document_t> Query(const std::string &command, const std::string &params) {
        ResponseCapture capture;
        std::string requestJson = R"({"id":73,"moduleName":"summary","type":"request","command":")" + command +
            R"(","params":)" + params + "}";
        Dic::document_t json;
        json.Parse(requestJson.c_str());
        std::string error;
        auto request = protocol.FromJson(json, error);
        EXPECT_NE(request, nullptr) << error;
        if (!request) {
            return std::nullopt;
        }
        module.Handle(std::move(request));
        EXPECT_EQ(capture.session->responses.size(), 1u);
        if (capture.session->responses.size() != 1) {
            return std::nullopt;
        }
        return protocol.ToJson(*capture.session->responses.front(), error);
    }

    TestModule module;
    Dic::Protocol::SummaryProtocol protocol;
    Dic::Test::Utf8TempDirectory directory;
    const std::string projectPath = "summary-cluster-statistics-test";
    const std::string databasePath = directory.Path("cluster_analysis.db");
    const std::string params = R"({"clusterPath":"summary-cluster-statistics-test","step":"all"})";
};

TEST_F(SummaryClusterStatisticsHandlerTest, RoutesAndSerializesAllFourCommands) {
    auto database = AddDatabase(Timeline::DataType::DB);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql(R"(
        CREATE TABLE ClusterStepTraceTime(step TEXT, type TEXT, "index" TEXT, computing REAL,
            communication_not_overlapped REAL, overlapped REAL, communication REAL, free REAL, preparing REAL);
        INSERT INTO ClusterStepTraceTime VALUES ('2', 'rank', '9', 600, 200, 200, 400, 100, 100);
        CREATE TABLE CommTimeStatistic(step TEXT, rank_id INTEGER, comm_transit_time REAL, comm_wait_time REAL,
            comm_sync_time REAL, comm_free_time REAL);
        INSERT INTO CommTimeStatistic VALUES ('step2', 9, 1.25, NULL, 0.5, 1);
        CREATE TABLE TransitStatistic(step TEXT, band_type TEXT, transit_size REAL, transit_time REAL);
        INSERT INTO TransitStatistic VALUES ('step2', 'RDMA', 100, 20);
    )"));
    database.reset();

    auto cluster = Query(REQ_RES_SUMMARY_CLUSTER_TIME, params);
    ASSERT_TRUE(cluster.has_value());
    ASSERT_TRUE((*cluster)["result"].GetBool());
    EXPECT_EQ((*cluster)["requestId"].GetUint(), 73u);
    EXPECT_STREQ((*cluster)["body"]["units"]["time"].GetString(), "ms");
    EXPECT_DOUBLE_EQ((*cluster)["body"]["time"]["computingNotOverlapped"].GetDouble(), 0.4);

    auto rank = Query(REQ_RES_SUMMARY_RANK_TIME, params);
    ASSERT_TRUE(rank.has_value());
    ASSERT_TRUE((*rank)["result"].GetBool());
    EXPECT_STREQ((*rank)["body"]["units"]["ratio"].GetString(), "percent");
    const auto rankRows = (*rank)["body"]["ranks"].GetArray();
    ASSERT_EQ(rankRows.Size(), 1u);
    EXPECT_STREQ(rankRows[0]["rankId"].GetString(), "9");
    EXPECT_DOUBLE_EQ(rankRows[0]["time"]["total"].GetDouble(), 1.0);
    EXPECT_DOUBLE_EQ(rankRows[0]["ratio"]["compute"].GetDouble(), 60.0);

    auto comm = Query(REQ_RES_SUMMARY_COMMUNICATION_TIME, params);
    ASSERT_TRUE(comm.has_value());
    ASSERT_TRUE((*comm)["result"].GetBool());
    EXPECT_STREQ((*comm)["body"]["units"]["time"].GetString(), "ms");
    const auto commRows = (*comm)["body"]["ranks"].GetArray();
    ASSERT_EQ(commRows.Size(), 1u);
    EXPECT_DOUBLE_EQ(commRows[0]["time"]["transit"].GetDouble(), 1.25);
    EXPECT_DOUBLE_EQ(commRows[0]["time"]["wait"].GetDouble(), 0.0);

    auto bandwidth = Query(REQ_RES_SUMMARY_CLUSTER_BANDWIDTH, params);
    ASSERT_TRUE(bandwidth.has_value());
    ASSERT_TRUE((*bandwidth)["result"].GetBool());
    EXPECT_STREQ((*bandwidth)["body"]["units"]["bandwidth"].GetString(), "GB/s");
    const auto transportTypes = (*bandwidth)["body"]["transportTypes"].GetArray();
    ASSERT_EQ(transportTypes.Size(), 1u);
    EXPECT_STREQ(transportTypes[0]["transportType"].GetString(), "RDMA");
    EXPECT_DOUBLE_EQ(transportTypes[0]["bandwidth"].GetDouble(), 5.0);
}

TEST_F(SummaryClusterStatisticsHandlerTest, EmptyOrDefaultStepAggregatesEveryStep) {
    auto database = AddDatabase(Timeline::DataType::DB);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql(R"(
        CREATE TABLE ClusterStepTraceTime(step TEXT, type TEXT, "index" TEXT, computing REAL,
            communication_not_overlapped REAL, overlapped REAL, communication REAL, free REAL, preparing REAL);
        INSERT INTO ClusterStepTraceTime VALUES
            ('1', 'rank', '9', 400, 0, 0, 0, 0, 0),
            ('2', 'rank', '9', 600, 0, 0, 0, 0, 0);
    )"));
    database.reset();

    for (const auto *requestParams : {R"({"clusterPath":"summary-cluster-statistics-test","step":""})",
             R"({"clusterPath":"summary-cluster-statistics-test"})",
             R"({"clusterPath":"summary-cluster-statistics-test","step":null})",
             R"({"clusterPath":"summary-cluster-statistics-test","step":2})"}) {
        SCOPED_TRACE(requestParams);
        auto response = Query(REQ_RES_SUMMARY_CLUSTER_TIME, requestParams);
        ASSERT_TRUE(response.has_value());
        ASSERT_TRUE((*response)["result"].GetBool());
        EXPECT_DOUBLE_EQ((*response)["body"]["time"]["computingNotOverlapped"].GetDouble(), 1.0);
    }
}

TEST_F(SummaryClusterStatisticsHandlerTest, ReportsInvalidParametersAndUnsupportedDataSource) {
    for (const auto *invalidParams :
        {R"({"clusterPath":"","step":""})", R"({"clusterPath":"anywhere","step":"2|3"})"}) {
        SCOPED_TRACE(invalidParams);
        auto invalid = Query(REQ_RES_SUMMARY_CLUSTER_TIME, invalidParams);
        ASSERT_TRUE(invalid.has_value());
        EXPECT_FALSE((*invalid)["result"].GetBool());
        EXPECT_EQ((*invalid)["error"]["code"].GetInt(), static_cast<int>(Summary::ErrorCode::PARAMS_ERROR));
    }

    auto unknown = Query(REQ_RES_SUMMARY_RANK_TIME, params);
    ASSERT_TRUE(unknown.has_value());
    EXPECT_FALSE((*unknown)["result"].GetBool());
    EXPECT_EQ((*unknown)["error"]["code"].GetInt(), static_cast<int>(Summary::ErrorCode::CONNECT_DATABASE_FAILED));

    ASSERT_NE(AddDatabase(Timeline::DataType::TEXT), nullptr);
    auto text = Query(REQ_RES_SUMMARY_COMMUNICATION_TIME, params);
    ASSERT_TRUE(text.has_value());
    EXPECT_FALSE((*text)["result"].GetBool());
    EXPECT_EQ((*text)["error"]["code"].GetInt(), static_cast<int>(Summary::ErrorCode::CONNECT_DATABASE_FAILED));
}

TEST_F(SummaryClusterStatisticsHandlerTest, ReportsQueryFailureForMalformedStatisticsTable) {
    auto database = AddDatabase(Timeline::DataType::DB);
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->ExecSql("CREATE TABLE TransitStatistic(step TEXT, band_type TEXT)"));
    database.reset();

    auto bandwidth = Query(REQ_RES_SUMMARY_CLUSTER_BANDWIDTH, params);
    ASSERT_TRUE(bandwidth.has_value());
    EXPECT_FALSE((*bandwidth)["result"].GetBool());
    EXPECT_EQ(
        (*bandwidth)["error"]["code"].GetInt(), static_cast<int>(Summary::ErrorCode::QUERY_CLUSTER_STATISTICS_FAILED));
}

TEST_F(HandlerTest, QueryFwdBwdTimelineHandlerHandleRequestReturnFalseWhenWrongParameter) {
    auto request = std::make_unique<PipelineFwdBwdTimelineRequest>();
    request->params.stageId = "";
    request->params.stepId = "";
    QueryFwdBwdTimelineHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryFwdBwdTimelineHandlerHandleRequestReturnTrueWhenNormalParameter) {
    auto request = std::make_unique<PipelineFwdBwdTimelineRequest>();
    request->params.stageId = "(0)";
    request->params.stepId = "1";
    request->params.clusterPath = "test";
    QueryFwdBwdTimelineHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, QueryFwdBwdTimelineHandlerHandleRequestStageIdEmty) {
    auto request = std::make_unique<PipelineFwdBwdTimelineRequest>();
    request->params.stageId = "()";
    request->params.stepId = "1";
    request->params.clusterPath = "test";
    QueryFwdBwdTimelineHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryCommunicationDetailHandlerWithExecuteSqlFail) {
    auto request = std::make_unique<CommunicationDetailRequest>();
    request->params.rankId = "100";
    request->params.currentPage = NUMBER_ONE;
    request->params.pageSize = NUMBER_TEN;
    request->params.orderBy = "orderBy";
    request->params.order = "order";
    QueryCommunicationDetailHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryComputeDetailInfoHandlerWithExecuteSqlFail) {
    auto request = std::make_unique<ComputeDetailRequest>();
    request->params.rankId = "100";
    request->params.currentPage = NUMBER_ONE;
    request->params.pageSize = NUMBER_TEN;
    request->params.orderBy = "orderBy";
    request->params.order = "order";
    request->params.timeFlag = "flag";
    QueryComputeDetailInfoHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryParallelStrategyConfigHandlerWithExecuteSqlFail) {
    auto request = std::make_unique<QueryParallelStrategyRequest>();
    request->params.clusterPath = "test";
    QueryParallelStrategyConfigHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, SetParallelStrategyConfigHandlerWithParamError) {
    auto request = std::make_unique<SetParallelStrategyRequest>();
    request->params.config.tpSize = NUMBER_FIVE_HUNDRED;
    request->params.config.dpSize = NUMBER_TEN;
    request->params.config.cpSize = NUMBER_TEN;
    request->params.config.ppSize = NUMBER_TEN;
    request->params.config.epSize = NUMBER_TEN;
    request->params.clusterPath = "test";
    SetParallelStrategyConfigHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, SetParallelStrategyConfigHandlerWithExecuteSqlError) {
    auto request = std::make_unique<SetParallelStrategyRequest>();
    request->params.config.tpSize = NUMBER_TEN;
    request->params.config.dpSize = NUMBER_TEN;
    request->params.config.cpSize = NUMBER_TEN;
    request->params.config.ppSize = NUMBER_TEN;
    request->params.config.epSize = NUMBER_TEN;
    request->params.clusterPath = "test";
    SetParallelStrategyConfigHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryParallelismArrangementHandlerShouldReturnFalseWithParamError) {
    auto request = std::make_unique<QueryParallelismArrangementRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 0; // 0
    request->params.clusterPath = "test";
    QueryParallelismArrangementHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryParallelismArrangementHandlerShouldReturnFalseWithDataBaseError) {
    auto request = std::make_unique<QueryParallelismArrangementRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 1; // 1
    request->params.dimension = "ep-dp";
    request->params.clusterPath = "test";
    QueryParallelismArrangementHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryParallelismPerformanceHandlerShouldReturnFalseWithParamError) {
    auto request = std::make_unique<QueryParallelismPerformanceRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 0; // 0
    request->params.clusterPath = "test";
    QueryParallelismPerformanceHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryParallelismPerformanceHandlerShouldReturnTrue) {
    auto request = std::make_unique<QueryParallelismPerformanceRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 1; // 1
    request->params.dimension = "ep-dp";
    request->params.clusterPath = "test";
    QueryParallelismPerformanceHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, SummaryTopRankHandlerHandleRequestReturnTrue) {
    auto request = std::make_unique<SummaryTopRankRequest>();
    SummaryTopRankHandler handler;
    request->params.clusterPath = "test";
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, ImportExpertDataHandlerRequestReturnFalse) {
    auto request = std::make_unique<ImportExpertDataRequest>();
    request->params.version = "1";
    request->params.filePath = "filePath";
    request->params.clusterPath = "test";
    ImportExpertDataHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, ImportExpertDataHandlerRequestErrorParams) {
    auto request = std::make_unique<ImportExpertDataRequest>();
    request->params.version = "1";
    request->params.filePath = "filePath";
    request->params.clusterPath = "test";
    ImportExpertDataHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryExpertHotspotHandlerRequestLayerAbnormal) {
    auto request = std::make_unique<QueryExpertHotspotRequest>();
    request->params.modelStage = "prefill";
    request->params.version = "1";
    request->params.denseLayerList = {0, 1};
    const int layerNum = -1;
    const int expertNum = 256;
    request->params.layerNum = layerNum;
    request->params.expertNum = expertNum;
    request->params.clusterPath = "test";
    QueryExpertHotspotHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryExpertHotspotHandlerRequestDenseAbnormal) {
    auto request = std::make_unique<QueryExpertHotspotRequest>();
    request->params.modelStage = "prefill";
    request->params.version = "1";
    request->params.denseLayerList = {0, 61};
    const int layerNum = 60;
    const int expertNum = 256;
    request->params.layerNum = layerNum;
    request->params.expertNum = expertNum;
    request->params.clusterPath = "test";
    QueryExpertHotspotHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryExpertHotspotHandlerRequestLayerNormal) {
    auto request = std::make_unique<QueryExpertHotspotRequest>();
    request->params.modelStage = "prefill";
    request->params.version = "1";
    request->params.denseLayerList = {0, 1};
    const int layerNum = 60;
    const int expertNum = 256;
    request->params.layerNum = layerNum;
    request->params.expertNum = expertNum;
    request->params.clusterPath = "test";
    QueryExpertHotspotHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryModelInfoHandlerNormal) {
    auto request = std::make_unique<QueryModelInfoRequest>();
    request->params.clusterPath = "test";
    QueryModelInfoHandler handler;
    bool res = handler.HandleRequest(std::move(request));
    EXPECT_EQ(res, true);
}

TEST_F(HandlerTest, SummarySlowRankAdvisorHandlerShouldReturnFalseWithParamError) {
    auto request = std::make_unique<QueryParallelismArrangementRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 0; // 0
    SummarySlowRankAdvisorHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, SummarySlowRankAdvisorHandlerShouldReturnFalseWithDataBaseError) {
    auto request = std::make_unique<QueryParallelismArrangementRequest>();
    request->params.config.tpSize = 2; // 2
    request->params.config.dpSize = 4; // 4
    request->params.config.cpSize = 2; // 2
    request->params.config.ppSize = 2; // 2
    request->params.config.epSize = 1; // 1
    SummarySlowRankAdvisorHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}
