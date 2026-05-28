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
using namespace Dic::Protocol;
class SummaryProtocolRequestTest : public ::testing::Test {};

TEST_F(SummaryProtocolRequestTest, SummaryStatisticParamsTestTimeFlagInvaild) {
    Dic::Protocol::SummaryStatisticParams params;
    params.rankId = "0";
    params.timeFlag = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, SummaryStatisticParamsTestStepIdInvaild) {
    Dic::Protocol::SummaryStatisticParams params;
    params.rankId = "0";
    params.timeFlag = "time";
    params.stepId = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, SummaryStatisticParamsTestRankIdInvaild) {
    Dic::Protocol::SummaryStatisticParams params;
    params.timeFlag = "time";
    params.stepId = "0";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, PipelineStageTimeParamTeststageIdInvaild) {
    Dic::Protocol::PipelineStageTimeParam params;
    params.stepId = "2";
    params.stageId = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ComputeDetailParamsTestTimeFlagInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.timeFlag = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ComputeDetailParamsTestOrderByInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.timeFlag = "time";
    params.orderBy = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ComputeDetailParamsTestOrderInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.timeFlag = "time";
    params.orderBy = "orderBy";
    params.order = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestRankIdInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestTimeFlagInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = "1";
    params.timeFlag = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestOrderByInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = "1";
    params.timeFlag = "time";
    params.orderBy = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestOrderInvaild) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = "1";
    params.timeFlag = "time";
    params.orderBy = "orderBy";
    params.order = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ImportExpertDataParamsTestVersionInvaild) {
    Dic::Protocol::ImportExpertDataParams params;
    params.filePath = "filePath";
    params.version = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ImportExpertDataParamsTestFilePathInvaild) {
    Dic::Protocol::ImportExpertDataParams params;
    params.filePath = ";";
    params.version = "1";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestModelStageInvaild) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = ";";
    params.version = "1";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestVersionInvaild) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = "prefill";
    params.version = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismArrangementParamTestClusterEmpty) {
    ParallelismArrangement params;
    params.clusterPath = "";
    params.config.ppSize = 2; // set ppSize 2
    params.config.tpSize = 2; // set tpSize 2
    params.config.dpSize = 2; // set dpSize  2
    params.dimension = "test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismPerformanceParamTestDimenInvaild) {
    ParallelismPerformance params;
    params.dimension = "test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismPerformanceParamTestOderbyInvaild) {
    ParallelismPerformance params;
    params.dimension = "ep-dp";
    params.orderBy = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismPerformanceParamTestStepInvaild) {
    ParallelismPerformance params;
    params.dimension = "ep-dp";
    params.orderBy = "test";
    params.step = "0";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismPerformanceParamTestBaselineStepInvaild) {
    ParallelismPerformance params;
    params.dimension = "ep-dp";
    params.orderBy = "test";
    params.step = "0";
    params.baselineStep = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismPerformanceParamTestClusterPathInvaild) {
    ParallelismPerformance params;
    params.dimension = "ep-dp";
    params.orderBy = "test";
    params.step = "0";
    params.baselineStep = "1";
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelStrategyParamTestClusterPathEmpty) {
    ParallelStrategyParam params;
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, SetParallelStrategyParamTestConfigErr) {
    SetParallelStrategyParam params;
    params.config.dpSize = 1000; // set dpSize to 1000
    std::string msg;
    EXPECT_FALSE(params.CheckParams(msg));
}

TEST_F(SummaryProtocolRequestTest, SetParallelStrategyParamTestClusterPathErr) {
    SetParallelStrategyParam params;
    params.clusterPath = ";";
    std::string msg;
    EXPECT_FALSE(params.CheckParams(msg));
}
