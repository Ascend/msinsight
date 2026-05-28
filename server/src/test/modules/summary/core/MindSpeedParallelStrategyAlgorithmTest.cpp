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
#include "MindSpeedParallelStrategyAlgorithm.h"
#include "ParallelStrategyAlgorithmManager.h"
using namespace Dic::Module;
using namespace Dic::Protocol;
using namespace Dic::Module::Summary;
class MindSpeedParallelStrategyAlgorithmTest : public ::testing::Test {};

TEST_F(MindSpeedParallelStrategyAlgorithmTest, UpdateParallelDimension_ShouldReturnTrue_WhenUpdateSuccess) {
    MindSpeedParallelStrategyAlgorithm algorithm;
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 4; // 4
    config.cpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    std::string err;
    bool res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_TRUE(res);
}
TEST_F(MindSpeedParallelStrategyAlgorithmTest, UpdateParallelDimension_ShouldReturnFalse_WhenWrongInput) {
    MindSpeedParallelStrategyAlgorithm algorithm;
    std::string dimension = "yyyyy";
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 4; // 4
    config.cpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = "xxxx";
    std::string err;
    bool res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_FALSE(res);
    EXPECT_EQ(err, "Failed to update parallel view. Unexpected algorithm for MindSpeed.");
    MindSpeedParallelStrategyAlgorithm algorithm2;
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    res = algorithm.UpdateParallelDimension(dimension, config, err);
    EXPECT_FALSE(res);
    EXPECT_EQ(err, "Failed to update show map for parallel view. Unexpected dimension.");
}

TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetArrangementByDimension_ShouldGetArrangement_TestWithTpDimension) {
    MindSpeedParallelStrategyAlgorithm algorithm;
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.cpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    const std::vector<std::string> EXPECTED_NAME = {"dp0-pp0-cp0-tp0", "dp0-pp0-cp0-tp1", "dp0-pp0-cp1-tp0",
        "dp0-pp0-cp1-tp1", "dp1-pp0-cp0-tp0", "dp1-pp0-cp0-tp1", "dp1-pp0-cp1-tp0", "dp1-pp0-cp1-tp1",
        "dp0-pp1-cp0-tp0", "dp0-pp1-cp0-tp1", "dp0-pp1-cp1-tp0", "dp0-pp1-cp1-tp1", "dp1-pp1-cp0-tp0",
        "dp1-pp1-cp0-tp1", "dp1-pp1-cp1-tp0", "dp1-pp1-cp1-tp1"};
    const std::vector<Position> EXPECTED_POSITION = {
        {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0}, // y = 0
        {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, // y = 1
    };
    const std::vector<std::vector<uint32_t>> EXPECT_RANKS = {
        {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11}, {12}, {13}, {14}, {15}};
    std::string err;
    algorithm.UpdateParallelDimension(dimension, config, err);
    algorithm.GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm.GetArrangementData();
    ASSERT_EQ(data.size, EXPECTED_NAME.size());
    for (const auto &item : data.arrangements) {
        EXPECT_EQ(item.name, EXPECTED_NAME[item.index]);
        EXPECT_EQ(item.position, EXPECTED_POSITION[item.index]);
        EXPECT_EQ(item.ranks.size(), 1);
        EXPECT_EQ(item.ranks.at(0), EXPECT_RANKS[item.index].at(0));
    }
}

TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetArrangementByDimension_ShouldGetArrangement_TestWithCpDimension) {
    MindSpeedParallelStrategyAlgorithm algorithm;
    std::string dimension = DIMENSIONS_CP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.cpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    const std::vector<std::string> EXPECTED_NAME = {"dp0-pp0-cp0", "dp0-pp0-cp1", "dp1-pp0-cp0", "dp1-pp0-cp1",
        "dp0-pp1-cp0", "dp0-pp1-cp1", "dp1-pp1-cp0", "dp1-pp1-cp1"};
    const std::vector<Position> EXPECTED_POSITION = {
        {0, 0}, {1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}, {3, 1}}; // position(x, y)
    const std::vector<std::vector<uint32_t>> EXPECT_RANKS = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {12, 13}, {14, 15}};
    std::string err;
    algorithm.UpdateParallelDimension(dimension, config, err);
    algorithm.GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm.GetArrangementData();
    ASSERT_EQ(data.arrangements.size(), EXPECTED_NAME.size());
    for (const auto &item : data.arrangements) {
        EXPECT_EQ(item.name, EXPECTED_NAME[item.index]);
        EXPECT_EQ(item.position, EXPECTED_POSITION[item.index]);
        EXPECT_EQ(item.ranks.size(), config.tpSize);
        for (int64_t i = 0; i < config.tpSize; ++i) {
            EXPECT_EQ(item.ranks.at(i), EXPECT_RANKS[item.index].at(i));
        }
    }
}

TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetArrangementByDimension_ShouldGetArrangement_TestWithPpDimension) {
    MindSpeedParallelStrategyAlgorithm algorithm;
    std::string dimension = DIMENSIONS_PP;
    ParallelStrategyConfig config;
    config.ppSize = 2; // 2
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.cpSize = 2; // 2
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    const std::vector<std::string> EXPECTED_NAME = {"dp0-pp0", "dp1-pp0", "dp0-pp1", "dp1-pp1"};
    const std::vector<std::vector<uint32_t>> EXPECT_RANKS = {
        {0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, 10, 11}, {12, 13, 14, 15}};
    const std::vector<Position> EXPECTED_POSITION = {{0, 0}, {1, 0}, {0, 1}, {1, 1}}; // position(x, y)
    std::string err;
    algorithm.UpdateParallelDimension(dimension, config, err);
    algorithm.GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm.GetArrangementData();
    ASSERT_EQ(data.arrangements.size(), EXPECTED_NAME.size());
    for (const auto &item : data.arrangements) {
        EXPECT_EQ(item.name, EXPECTED_NAME[item.index]);
        EXPECT_EQ(item.position, EXPECTED_POSITION[item.index]);
        EXPECT_EQ(item.ranks.size(), config.tpSize * config.ppSize);
        for (size_t i = 0; i < item.ranks.size(); ++i) {
            EXPECT_EQ(item.ranks.at(i), EXPECT_RANKS[item.index].at(i));
        }
    }
}

/**
 * 返回93个connections: 'tp-cp': 2, 'dp-cp': 2, 'tp-dp-cp': 1, 'tp-dp': 6, 'tp': 12, 'cp': 4, 'dp': 12,
 * 'cp_ulysses': 12, 'cp_ring': 8, 'cp_ring_intra': 8, 'dp_modulo_exp_cp': 4, 'tp_exp': 6, 'dp_modulo_exp': 4,
 * 'exp': 12
 */
TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetConnectionsByTokenlist_ShouleGetConnections_TestWithHybridCp) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    std::string projectName = "testProject";
    config.ppSize = 1; // 1
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.cpSize = 6; // 6
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    config.configForMindSpeed.cpAlgo = MINDSPEED_HYBIRD_CP_ALG;
    config.configForMindSpeed.ulyssesDegree = 2; // 2
    config.configForMindSpeed.winSize = 3; // 3
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(projectName);
    algorithm->UpdateParallelDimension(dimension, config, err);
    algorithm->GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm->GetArrangementData();
    EXPECT_EQ(data.arrangements.size(), 24); // 24 = 2*2*6
    EXPECT_EQ(data.connections.size(), 93); // 93
    ParallelStrategyAlgorithmManager::Instance().DeleteAlgorithm(projectName);
}

/**
 * 返回73个connections: 'tp-cp': 2, 'dp-cp': 2, 'tp-dp-cp': 1, 'tp-dp': 6, 'tp': 12, 'cp': 4, 'dp': 12,
 * 'cp_ring_intra': 8, 'dp_modulo_exp_cp': 4, 'tp_exp': 6, 'dp_modulo_exp': 4,
 * 'exp': 12
 */
TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetConnectionsByTokenlist_ShouleGetConnections_TestWithMegatronCp) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    std::string projectName = "testProject";
    config.ppSize = 1; // 1
    config.tpSize = 2; // 2
    config.dpSize = 2; // 2
    config.cpSize = 6; // 6
    config.epSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    config.configForMindSpeed.cpAlgo = MINDSPEED_MEGATRON_CP_ALG;
    config.configForMindSpeed.winSize = 3; // 3
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    algorithm->GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm->GetArrangementData();
    EXPECT_EQ(data.arrangements.size(), 24); // 24 = 2*2*6
    EXPECT_EQ(data.connections.size(), 73); // 73
    ParallelStrategyAlgorithmManager::Instance().DeleteAlgorithm(projectName);
}

/**
 * 返回79个connections: 'tp-cp': 2, 'dp-cp': 6, 'tp-dp-cp': 1, 'tp-dp': 2, 'tp': 4, 'cp': 12, 'dp': 12,
 * 'nd1_dim1': 12, 'nd1_dim2': 8, nd1_dim1': 8, 'nd1_dim2': 12,
 */
TEST_F(MindSpeedParallelStrategyAlgorithmTest, GetConnectionsByTokenlist_ShouleGetConnections_TestWithTp2d) {
    std::string dimension = DIMENSIONS_TP;
    ParallelStrategyConfig config;
    std::string projectName = "testProject";
    config.ppSize = 1; // 1
    config.tpSize = 6; // 6
    config.dpSize = 2; // 2
    config.cpSize = 2; // 2
    config.algorithm = MINDSPEED_TP_CP_EP_DP_PP_ALG;
    config.configForMindSpeed.useTp2D = true;
    config.configForMindSpeed.nd1dim1 = 2; // 2
    config.configForMindSpeed.nd2dim1 = 3; // 3
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(projectName, config, err);
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName("testProject");
    algorithm->UpdateParallelDimension(dimension, config, err);
    algorithm->GenerateArrangementByDimension(err);
    ArrangementAndConnectionData data = algorithm->GetArrangementData();
    EXPECT_EQ(data.arrangements.size(), 24); // 24 = 2*2*6
    EXPECT_EQ(data.connections.size(), 79); // 79
    ParallelStrategyAlgorithmManager::Instance().DeleteAlgorithm(projectName);
}
