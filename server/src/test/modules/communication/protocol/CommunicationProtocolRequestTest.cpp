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

const int NUMBER_TEN = 10;
const int NUMBER_ONE = 1;
class CommunicationProtocolRequestTest : public ::testing::Test {};

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTest) {
    Dic::Protocol::OperatorDetailsParam base;
    base.iterationId = "1";
    base.orderBy = "orderBy";
    base.order = "order";
    base.stage = "stage";
    base.pageSize = NUMBER_ONE;
    base.currentPage = NUMBER_TEN;
    Dic::Protocol::OperatorDetailsParam param1(base);
    param1.iterationId = ";";
    Dic::Protocol::OperatorDetailsParam param3(base);
    param3.orderBy = ";";
    Dic::Protocol::OperatorDetailsParam param4(base);
    param4.order = ";";
    Dic::Protocol::OperatorDetailsParam param5(base);
    param5.stage = ";";
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param3.CheckParams(msg), false);
    EXPECT_EQ(param4.CheckParams(msg), false);
    EXPECT_EQ(param5.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, BandwidthDataParamTest) {
    Dic::Protocol::BandwidthDataParam param1 = {";", "1", "opName", "stage"};
    Dic::Protocol::BandwidthDataParam param2 = {"1", "1", ";", "stage"};
    Dic::Protocol::BandwidthDataParam param3 = {"1", "1", "opName", ";"};
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param2.CheckParams(msg), false);
    EXPECT_EQ(param3.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DistributionDataParamTest) {
    Dic::Protocol::DistributionDataParam param1 = {";", "1", "opName", "type", "stage", ""};
    Dic::Protocol::DistributionDataParam param2 = {"1", "1", ";", "type", "stage", ""};
    Dic::Protocol::DistributionDataParam param3 = {"1", "1", "opName", ";", "stage", ""};
    Dic::Protocol::DistributionDataParam param4 = {"1", "1", "opName", "type", ";", ""};
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param2.CheckParams(msg), false);
    EXPECT_EQ(param3.CheckParams(msg), false);
    EXPECT_EQ(param4.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorNamesParamsTest) {
    Dic::Protocol::OperatorNamesParams base;
    base.iterationId = "1";
    base.rankList.emplace_back("1");
    base.stage = "stage";
    Dic::Protocol::OperatorNamesParams param1(base);
    param1.iterationId = ";";
    Dic::Protocol::OperatorNamesParams param2(base);
    param2.stage = ";";
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param2.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTest) {
    Dic::Protocol::DurationListParams base;
    base.iterationId = "1";
    base.operatorName = "opName";
    base.stage = "stage";
    Dic::Protocol::DurationListParams param1(base);
    param1.iterationId = ";";
    Dic::Protocol::DurationListParams param2(base);
    param2.operatorName = ";";
    Dic::Protocol::DurationListParams param3(base);
    param3.stage = ";";
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param2.CheckParams(msg), false);
    EXPECT_EQ(param3.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixGroupParamTest) {
    Dic::Protocol::MatrixGroupParam param1 = {";", "1"};
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixGroupParamTestBaselineStepError) {
    Dic::Protocol::MatrixGroupParam param1 = {"1", ";"};
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixBandwidthParamTest) {
    Dic::Protocol::MatrixBandwidthParam param1 = {"stage", "opName", ";", "", "", false, "1"};
    Dic::Protocol::MatrixBandwidthParam param2 = {"stage", ";", "1", "", "", false, "1"};
    Dic::Protocol::MatrixBandwidthParam param3 = {";", "opName", "1", "", "", false, "1"};
    Dic::Protocol::MatrixBandwidthParam param4 = {"stage", "opName", "1", "", "", false, ";"};
    std::string msg;
    EXPECT_EQ(param1.CheckParams(msg), false);
    EXPECT_EQ(param2.CheckParams(msg), false);
    EXPECT_EQ(param3.CheckParams(msg), false);
    EXPECT_EQ(param4.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestPageSizeInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.orderBy = "time";
    param.order = "desc";
    param.stage = "forward";
    param.queryType = "Comparison";
    param.pgName = "test";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    param.pageSize = 0;
    param.currentPage = 1;
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestCurrentPageInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.pageSize = 10;
    param.currentPage = 0;
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestRankIdInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestQueryTypeInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.queryType = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestPgNameInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.pgName = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestClusterPathInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.clusterPath = ";";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorDetailsParamTestGroupIdHashInvalid) {
    Dic::Protocol::OperatorDetailsParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.clusterPath = "/data";
    param.groupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, BandwidthDataParamTestRankIdInvalid) {
    Dic::Protocol::BandwidthDataParam param;
    param.iterationId = "1";
    param.rankId = ";";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.pgName = "test";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, BandwidthDataParamTestPgNameInvalid) {
    Dic::Protocol::BandwidthDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.pgName = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, BandwidthDataParamTestClusterPathInvalid) {
    Dic::Protocol::BandwidthDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.clusterPath = ";";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, BandwidthDataParamTestGroupIdHashInvalid) {
    Dic::Protocol::BandwidthDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.clusterPath = "/data";
    param.groupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DistributionDataParamTestRankIdInvalid) {
    Dic::Protocol::DistributionDataParam param;
    param.iterationId = "1";
    param.rankId = ";";
    param.operatorName = "AllReduce";
    param.transportType = "SDMA";
    param.stage = "forward";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DistributionDataParamTestPgNameInvalid) {
    Dic::Protocol::DistributionDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.pgName = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DistributionDataParamTestClusterPathInvalid) {
    Dic::Protocol::DistributionDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.clusterPath = ";";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DistributionDataParamTestGroupIdHashInvalid) {
    Dic::Protocol::DistributionDataParam param;
    param.iterationId = "1";
    param.rankId = "0";
    param.clusterPath = "/data";
    param.groupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, IterationsParamsTestClusterPathInvalid) {
    Dic::Protocol::IterationsParams param;
    param.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, IterationsParamsTestNormal) {
    Dic::Protocol::IterationsParams param;
    param.isCompare = true;
    param.clusterPath = "/data";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), true);
}

TEST_F(CommunicationProtocolRequestTest, OperatorNamesParamsTestPgNameInvalid) {
    Dic::Protocol::OperatorNamesParams param;
    param.iterationId = "1";
    param.stage = "forward";
    param.pgName = ";";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorNamesParamsTestClusterPathInvalid) {
    Dic::Protocol::OperatorNamesParams param;
    param.iterationId = "1";
    param.stage = "forward";
    param.clusterPath = ";";
    param.groupIdHash = "hash123";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, OperatorNamesParamsTestGroupIdHashInvalid) {
    Dic::Protocol::OperatorNamesParams param;
    param.iterationId = "1";
    param.stage = "forward";
    param.clusterPath = "/data";
    param.groupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTestBaselineIterationIdInvalid) {
    Dic::Protocol::DurationListParams param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.baselineIterationId = ";";
    param.clusterPath = "/data";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTestPgNameInvalid) {
    Dic::Protocol::DurationListParams param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.pgName = ";";
    param.clusterPath = "/data";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTestClusterPathInvalid) {
    Dic::Protocol::DurationListParams param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTestGroupIdHashInvalid) {
    Dic::Protocol::DurationListParams param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.clusterPath = "/data";
    param.groupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, DurationListParamsTestBaselineGroupIdHashInvalid) {
    Dic::Protocol::DurationListParams param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.clusterPath = "/data";
    param.groupIdHash = "hash123";
    param.baselineGroupIdHash = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixGroupParamTestClusterPathInvalid) {
    Dic::Protocol::MatrixGroupParam param;
    param.iterationId = "1";
    param.baselineIterationId = "2";
    param.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixBandwidthParamTestPgNameInvalid) {
    Dic::Protocol::MatrixBandwidthParam param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.baselineIterationId = "2";
    param.pgName = ";";
    param.clusterPath = "/data";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, MatrixBandwidthParamTestClusterPathInvalid) {
    Dic::Protocol::MatrixBandwidthParam param;
    param.iterationId = "1";
    param.operatorName = "AllReduce";
    param.stage = "forward";
    param.baselineIterationId = "2";
    param.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, CommunicationAdvisorParamTestClusterPathInvalid) {
    Dic::Protocol::CommunicationAdvisorParam param;
    param.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), false);
}

TEST_F(CommunicationProtocolRequestTest, CommunicationAdvisorParamTestNormal) {
    Dic::Protocol::CommunicationAdvisorParam param;
    param.clusterPath = "/data";
    std::string msg;
    EXPECT_EQ(param.CheckParams(msg), true);
}
