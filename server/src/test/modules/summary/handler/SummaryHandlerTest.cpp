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
#include "SummarySlowRankAdvisorHandler.h"

using namespace Dic::Module;
using namespace Dic::Module::Summary;

const int NUMBER_FIVE_HUNDRED = 500;
const int NUMBER_TEN = 10;
const int NUMBER_ONE = 1;

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
