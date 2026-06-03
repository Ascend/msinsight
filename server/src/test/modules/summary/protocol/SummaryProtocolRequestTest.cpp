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

TEST_F(SummaryProtocolRequestTest, SummaryTopRankParamsTestClusterPathInvalid) {
    Dic::Protocol::SummaryTopRankParams params;
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, SummaryTopRankParamsTestNormal) {
    Dic::Protocol::SummaryTopRankParams params;
    params.isCompare = true;
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), true);
}

TEST_F(SummaryProtocolRequestTest, PipelineStageTimeParamTestStepIdInvalid) {
    Dic::Protocol::PipelineStageTimeParam params;
    params.stepId = ";";
    params.stageId = "0";
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, PipelineStageTimeParamTestClusterPathInvalid) {
    Dic::Protocol::PipelineStageTimeParam params;
    params.stepId = "0";
    params.stageId = "0";
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, PipelineFwdBwdTimelineParamTestStepIdInvalid) {
    Dic::Protocol::PipelineFwdBwdTimelineParam params;
    params.stepId = ";";
    params.stageId = "0";
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, PipelineFwdBwdTimelineParamTestStageIdInvalid) {
    Dic::Protocol::PipelineFwdBwdTimelineParam params;
    params.stepId = "0";
    params.stageId = ";";
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, PipelineFwdBwdTimelineParamTestClusterPathInvalid) {
    Dic::Protocol::PipelineFwdBwdTimelineParam params;
    params.stepId = "0";
    params.stageId = "0";
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismArrangementParamTestDimensionInvalid) {
    Dic::Protocol::ParallelismArrangement params;
    params.config.algorithm = "megatron-lm(tp-cp-ep-dp-pp)";
    params.config.tpSize = 2;
    params.config.ppSize = 2;
    params.config.dpSize = 2;
    params.config.cpSize = 1;
    params.config.epSize = 1;
    params.dimension = "invalid";
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ParallelismArrangementParamTestConfigInvalid) {
    Dic::Protocol::ParallelismArrangement params;
    params.dimension = "ep-dp";
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ComputeDetailParamsTestPageInvalid) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = "0";
    params.timeFlag = "time";
    params.pageSize = 0;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ComputeDetailParamsTestRankIdInvalid) {
    Dic::Protocol::ComputeDetailParams params;
    params.rankId = ";";
    params.timeFlag = "time";
    params.pageSize = 10;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestRankIdInvalid) {
    Dic::Protocol::CommunicationDetailParams params;
    params.rankId = ";";
    params.timeFlag = "HCCL";
    params.pageSize = 10;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestTimeFlagInvalid) {
    Dic::Protocol::CommunicationDetailParams params;
    params.rankId = "0";
    params.timeFlag = ";";
    params.pageSize = 10;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestOrderByInvalid) {
    Dic::Protocol::CommunicationDetailParams params;
    params.rankId = "0";
    params.timeFlag = "HCCL";
    params.orderBy = ";";
    params.pageSize = 10;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestOrderInvalid) {
    Dic::Protocol::CommunicationDetailParams params;
    params.rankId = "0";
    params.timeFlag = "HCCL";
    params.orderBy = "time";
    params.order = ";";
    params.pageSize = 10;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, CommunicationDetailParamsTestPageInvalid) {
    Dic::Protocol::CommunicationDetailParams params;
    params.rankId = "0";
    params.timeFlag = "HCCL";
    params.pageSize = 0;
    params.currentPage = 1;
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ImportExpertDataParamsTestClusterPathInvalid) {
    Dic::Protocol::ImportExpertDataParams params;
    params.filePath = "/data/test.json";
    params.version = "1.0";
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ModelInfoParamTestClusterPathInvalid) {
    Dic::Protocol::ModelInfoParam params;
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, ModelInfoParamTestNormal) {
    Dic::Protocol::ModelInfoParam params;
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), true);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestLayerNumInvalid) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = "prefill";
    params.version = "1.0";
    params.layerNum = 0;
    params.expertNum = 8;
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestExpertNumInvalid) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = "prefill";
    params.version = "1.0";
    params.layerNum = 60;
    params.expertNum = 0;
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestDenseLayerListInvalid) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = "prefill";
    params.version = "1.0";
    params.layerNum = 10;
    params.expertNum = 8;
    params.denseLayerList = {15};
    params.clusterPath = "/data/test";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}

TEST_F(SummaryProtocolRequestTest, QueryExpertHotspotParamsTestClusterPathInvalid) {
    Dic::Protocol::QueryExpertHotspotParams params;
    params.modelStage = "prefill";
    params.version = "1.0";
    params.layerNum = 60;
    params.expertNum = 8;
    params.clusterPath = ";";
    std::string msg;
    EXPECT_EQ(params.CheckParams(msg), false);
}
