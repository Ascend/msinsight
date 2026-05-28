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
#include "VLLMParallelStrategyAlgorithm.h"
#include "ParallelStrategyAlgorithmManager.h"
using namespace Dic::Module;
using namespace Dic::Protocol;
using namespace Dic::Module::Summary;
class VLLMParallelStrategyAlgorithmTest : public ::testing::Test {};

TEST_F(VLLMParallelStrategyAlgorithmTest, UpdateParallelDimension_ShouldReturnTrue_WhenUpdateSuccess) {
    VLLMParallelStrategyAlgorithm algorithm;
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 4; // 4
    config.epSize = 4; // 4
    config.algorithm = VLLM_TP_PP_DP_EP_ALG;
    std::string err;
    bool res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_TRUE(res);
}

TEST_F(VLLMParallelStrategyAlgorithmTest, UpdateParallelDimension_ShouldReturnFalse_WhenWrongInput) {
    VLLMParallelStrategyAlgorithm algorithm;
    std::string dimension = "yyyyy";
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 4; // 4
    config.epSize = 4; // 4
    config.algorithm = "xxxx";
    std::string err;
    bool res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_FALSE(res);
    EXPECT_EQ(err, "Failed to update parallel view. Unexpected algorithm for the vLLM.");
    VLLMParallelStrategyAlgorithm algorithm2;
    config.algorithm = VLLM_TP_PP_DP_EP_ALG;
    res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_FALSE(res);
    EXPECT_EQ(err, "Failed to update show map for parallel view. Unexpected dimension.");
}

TEST_F(VLLMParallelStrategyAlgorithmTest, GetArrangementByDimension_ShouldGetArrangement_TestWithPp2Tp2Dp2Ep2) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = VLLM_TP_PP_DP_EP_ALG;
    std::string err;
    std::string projectName = "testProject";
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    algorithm->GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm->GetArrangementData();
    const std::vector<std::string> EXPECTED_NAME = {"dp0-pp0-tp0", "dp0-pp0-tp1", "dp0-pp1-tp0", "dp0-pp1-tp1",
        "dp1-pp0-tp0", "dp1-pp0-tp1", "dp1-pp1-tp0", "dp1-pp1-tp1"};
    const std::vector<Position> EXPECTED_POSITION = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 0}, {3, 0}, {2, 1}, {3, 1}};
    const std::vector<std::vector<uint32_t>> EXPECT_RANKS = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
    ASSERT_EQ(data.size, EXPECTED_NAME.size());
    for (const auto &item : data.arrangements) {
        EXPECT_EQ(item.name, EXPECTED_NAME[item.index]);
        EXPECT_EQ(item.position, EXPECTED_POSITION[item.index]);
        EXPECT_EQ(item.ranks.size(), 1);
        EXPECT_EQ(item.ranks.at(0), EXPECT_RANKS[item.index].at(0));
    }
    // 16个connections: 'pp': 4, 'dp': 4, 'tp': 4, 'exp': 4
    EXPECT_EQ(data.connections.size(), 16); // 16
    const std::vector<std::vector<uint32_t>> EXPECT_INDEXES = {{0, 1}, {2, 3}, {4, 5}, {6, 7}};
    int i = 0;
    for (const auto &item : data.connections) {
        if (item.type != "exp") {
            continue;
        }
        ASSERT_LE(i, EXPECT_INDEXES.size());
        EXPECT_EQ(item.indexes, EXPECT_INDEXES[i++]);
    }
}

TEST_F(VLLMParallelStrategyAlgorithmTest, GetArrangementByDimension_ShouldGetArrangement_TestWithPp2Tp2Dp2Ep4) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.epSize = 4; // 4
    config.algorithm = VLLM_TP_PP_DP_EP_ALG;
    std::string err;
    std::string projectName = "testProject";
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    algorithm->GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm->GetArrangementData();
    const std::vector<std::string> EXPECTED_NAME = {"dp0-pp0-tp0", "dp0-pp0-tp1", "dp0-pp1-tp0", "dp0-pp1-tp1",
        "dp1-pp0-tp0", "dp1-pp0-tp1", "dp1-pp1-tp0", "dp1-pp1-tp1"};
    const std::vector<Position> EXPECTED_POSITION = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 0}, {3, 0}, {2, 1}, {3, 1}};
    const std::vector<std::vector<uint32_t>> EXPECT_RANKS = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
    ASSERT_EQ(data.size, EXPECTED_NAME.size());
    for (const auto &item : data.arrangements) {
        EXPECT_EQ(item.name, EXPECTED_NAME[item.index]);
        EXPECT_EQ(item.position, EXPECTED_POSITION[item.index]);
        EXPECT_EQ(item.ranks.size(), 1);
        EXPECT_EQ(item.ranks.at(0), EXPECT_RANKS[item.index].at(0));
    }
    // 14个connections: 'pp': 4, 'dp': 4, 'tp': 4, 'exp': 2
    EXPECT_EQ(data.connections.size(), 14); // 14
    const std::vector<std::vector<uint32_t>> EXPECT_INDEXES = {{0, 1, 4, 5}, {2, 3, 6, 7}};
    int i = 0;
    for (const auto &item : data.connections) {
        if (item.type != "exp") {
            continue;
        }
        ASSERT_LE(i, EXPECT_INDEXES.size());
        EXPECT_EQ(item.indexes, EXPECT_INDEXES[i++]);
    }
}

void PrepareParametersForVLLMGetPerformanceByDimensionTest(
    ParallelStrategyConfig &config, std::unordered_map<std::uint32_t, StepStatistic> &statistic) {
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 4; // 4
    config.epSize = 2; // 2
    config.algorithm = VLLM_TP_PP_DP_EP_ALG;
    StepStatistic statisticOne;
    statisticOne.computingTime = 90; // 90
    statisticOne.pureCommunicationTime = 60; // 60
    statisticOne.overlapCommunicationTime = 20; // 20
    statisticOne.communicationTime = 80; // 80
    statisticOne.freeTime = 50; // 50
    statisticOne.prepareTime = 10; // 10
    statisticOne.pureCommunicationExcludeReceiveTime = 40; // 40
    uint32_t wordSize = config.ppSize * config.tpSize * config.dpSize; // 16
    for (uint32_t i = 0; i < wordSize; i++) {
        if (i == 4 || i == 5) { // 4 5 空数据卡
            continue;
        }
        statistic[i] = statisticOne;
    }
}

TEST_F(VLLMParallelStrategyAlgorithmTest, GetPerformanceByDimension_ShouldReturnTrue_TestWithTpDimension) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    std::unordered_map<std::uint32_t, StepStatistic> statistic;
    PrepareParametersForVLLMGetPerformanceByDimensionTest(config, statistic);
    std::string err;
    std::string projectName = "testProject";
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    GetPerformanceIndicatorParam params;
    params.config = config;
    params.dimension = dimension;
    std::vector<IndicatorDataStruct> responseData;
    bool res = algorithm->GetPerformanceIndicatorByDimension(params, statistic, responseData, err);
    EXPECT_EQ(res, true);
    EXPECT_EQ(responseData.size(), 14); // 14 = 16 - 2(empty rank)
}

TEST_F(VLLMParallelStrategyAlgorithmTest, GetPerformanceByDimension_ShouldReturnTrue_TestWithPpDimension) {
    std::string dimension = DIMENSIONS_PP;
    ParallelStrategyConfig config;
    std::unordered_map<std::uint32_t, StepStatistic> statistic;
    PrepareParametersForVLLMGetPerformanceByDimensionTest(config, statistic);
    std::string err;
    std::string projectName = "testProject";
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    GetPerformanceIndicatorParam params;
    params.config = config;
    params.dimension = dimension;
    std::vector<IndicatorDataStruct> responseData;
    bool res = algorithm->GetPerformanceIndicatorByDimension(params, statistic, responseData, err);
    EXPECT_EQ(res, true);
    EXPECT_EQ(responseData.size(), 7); // 7 = 8 - 1 pp groups
}

TEST_F(VLLMParallelStrategyAlgorithmTest, GetPerformanceByDimension_ShouldReturnTrue_TestWithDpDimension) {
    std::string dimension = DIMENSIONS_DP;
    ParallelStrategyConfig config;
    std::unordered_map<std::uint32_t, StepStatistic> statistic;
    PrepareParametersForVLLMGetPerformanceByDimensionTest(config, statistic);
    std::string err;
    std::string projectName = "testProject";
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    GetPerformanceIndicatorParam params;
    params.config = config;
    params.dimension = dimension;
    std::vector<IndicatorDataStruct> responseData;
    bool res = algorithm->GetPerformanceIndicatorByDimension(params, statistic, responseData, err);
    EXPECT_EQ(res, true);
    EXPECT_EQ(responseData.size(), 4); // 4 dp groups
}
