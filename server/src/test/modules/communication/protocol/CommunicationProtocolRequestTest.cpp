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
