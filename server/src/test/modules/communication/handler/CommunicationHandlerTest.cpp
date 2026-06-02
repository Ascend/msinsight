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
#include "CommunicationProtocolRequest.h"
#include "../../timeline/handler/HandlerTest.cpp"
#include "CommunicationOperatorListsHandler.h"
#include "BandwidthHandler.h"
#include "CommunicationOperatorDetailsHandler.h"
#include "DistributionHandler.h"
#include "GroupHandler.h"
#include "IterationsHandler.h"
#include "MatrixListHandler.h"
#include "MatrixSortOpNamesHandler.h"
#include "OperatorNamesHandler.h"
#include "DurationListHandler.h"
#include "CommunicationAdvisorHandler.h"
#include "CommunicationSlowRankAnalysisHandler.h"

using namespace Dic::Module;
using namespace Dic::Module::Communication;

TEST_F(HandlerTest, QueryCommunicationOperatorListsWithParamError) {
    auto request = std::make_unique<DurationListRequest>();
    request->params.iterationId = ";";
    request->params.operatorName = "opName";
    request->params.stage = "1";
    CommunicationOperatorListsHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryCommunicationOperatorListsWithExeSqlFail) {
    auto request = std::make_unique<DurationListRequest>();
    request->params.iterationId = "1";
    request->params.operatorName = "opName";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    CommunicationOperatorListsHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, QueryBandwidthHandlerParamError) {
    auto request = std::make_unique<BandwidthDataRequest>();
    request->params.iterationId = ";";
    request->params.operatorName = "opName";
    request->params.stage = "1";
    BandwidthHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, QueryBandwidthHandlerWithExeSqlFail) {
    auto request = std::make_unique<BandwidthDataRequest>();
    request->params.iterationId = "1";
    request->params.operatorName = "opName";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    request->params.rankId = "0";
    BandwidthHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, CommunicationOperatorDetailsParamError) {
    auto request = std::make_unique<OperatorDetailsRequest>();
    request->params.iterationId = ";";
    request->params.orderBy = "orderBy";
    request->params.order = "order";
    request->params.stage = "1";
    CommunicationOperatorDetailsHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, CommunicationOperatorDetailsWithExeSqlFail) {
    auto request = std::make_unique<OperatorDetailsRequest>();
    request->params.iterationId = "1";
    request->params.orderBy = "orderBy";
    request->params.order = "order";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    request->params.pageSize = 1; // pageSize set to 1
    request->params.currentPage = 1; // curPage set to 1
    CommunicationOperatorDetailsHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, DistributionHandlerParamError) {
    auto request = std::make_unique<DistributionDataRequest>();
    request->params.iterationId = ";";
    request->params.operatorName = "name";
    request->params.transportType = "type";
    request->params.stage = "1";
    DistributionHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, DistributionHandlerWithExeSqlFail) {
    auto request = std::make_unique<DistributionDataRequest>();
    request->params.iterationId = "1";
    request->params.operatorName = "name";
    request->params.transportType = "type";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    request->params.rankId = "0";
    DistributionHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, GroupHandlerParamError) {
    auto request = std::make_unique<MatrixGroupRequest>();
    request->params.iterationId = ";";
    GroupHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, GroupHandlerWithExeSqlFail) {
    auto request = std::make_unique<MatrixGroupRequest>();
    request->params.iterationId = "1";
    request->params.clusterPath = "test";
    GroupHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, IterationsHandlerWithExeSqlFail) {
    auto request = std::make_unique<IterationsRequest>();
    request->params.clusterPath = "test";
    IterationsHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, MatrixListHandlerParamError) {
    auto request = std::make_unique<MatrixBandwidthRequest>();
    request->params.iterationId = ";";
    request->params.operatorName = "name";
    request->params.stage = "1";
    MatrixListHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, MatrixListHandlerWithExeSqlFail) {
    auto request = std::make_unique<MatrixBandwidthRequest>();
    request->params.iterationId = "1";
    request->params.operatorName = "name";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    MatrixListHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, MatrixSortOpNamesHandlerWithExeSqlFail) {
    auto request = std::make_unique<MatrixSortOpNamesRequest>();
    request->params.clusterPath = "test";
    request->params.stage = "0";
    request->params.iterationId = "0";
    MatrixSortOpNamesHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, OperatorNamesHandlerParamError) {
    auto request = std::make_unique<OperatorNamesRequest>();
    request->params.iterationId = ";";
    request->params.stage = "1";
    OperatorNamesHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, OperatorNamesHandlerWithExeSqlFail) {
    auto request = std::make_unique<OperatorNamesRequest>();
    request->params.iterationId = "1";
    request->params.stage = "1";
    request->params.clusterPath = "test";
    OperatorNamesHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}

TEST_F(HandlerTest, AdvisorHandlerNormalTest) {
    std::unique_ptr<CommunicationAdvisorRequest> request = std::make_unique<CommunicationAdvisorRequest>();
    request->params.clusterPath = "COMPARE";
    CommunicationAdvisorHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_TRUE(result);
}

TEST_F(HandlerTest, DurationListHandlerExecSqlFailed) {
    auto request = std::make_unique<DurationListRequest>();
    request->params.clusterPath = "test";
    request->params.iterationId = "0";
    request->params.operatorName = "op detail";
    request->params.stage = "0";
    DurationListHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_TRUE(result);
}

TEST_F(HandlerTest, CommunicationSlowRankAnalysisHandlerExecSqlFailed) {
    auto request = std::make_unique<DurationListRequest>();
    request->params.clusterPath = "test";
    request->params.iterationId = "0";
    request->params.operatorName = "xxx";
    request->params.stage = "0";
    CommunicationSlowRankAnalysisHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, false);
}
