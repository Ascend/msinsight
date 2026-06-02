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
#include "ClusterDef.h"
class ClusterDefTest : public ::testing::Test {};

/**
 * 测试ParallelStrategyConfig
 */
using namespace Dic::Module;
TEST_F(ClusterDefTest, TestParallelStrategyConfigForBaseCheck) {
    const int64_t maxSize = Dic::Module::MAX_PARALLEL_SIZE + 1;
    const int64_t minSize = 1;
    ParallelStrategyConfig config1 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, maxSize, minSize, minSize, minSize, minSize, minSize};
    ParallelStrategyConfig config2 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, maxSize, minSize, minSize, minSize, minSize};
    ParallelStrategyConfig config3 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, maxSize, minSize, minSize, minSize};
    ParallelStrategyConfig config4 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, maxSize, minSize, minSize};
    ParallelStrategyConfig config5 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, minSize, maxSize, minSize};
    ParallelStrategyConfig config11 = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, minSize, minSize, maxSize};
    ParallelStrategyConfig config6 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 0, minSize, minSize, minSize, minSize, minSize};
    ParallelStrategyConfig config7 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, 0, minSize, minSize, minSize, minSize};
    ParallelStrategyConfig config8 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, 0, minSize, minSize, minSize};
    ParallelStrategyConfig config9 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, 0, minSize, minSize};
    ParallelStrategyConfig config10 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, minSize, 0, minSize};
    ParallelStrategyConfig config12 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, minSize, minSize, minSize, minSize, minSize, 0};
    std::string error;
    EXPECT_EQ(config1.CheckParams(error), false);
    EXPECT_EQ(error, "PP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config2.CheckParams(error), false);
    EXPECT_EQ(error, "TP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config3.CheckParams(error), false);
    EXPECT_EQ(error, "DP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config4.CheckParams(error), false);
    EXPECT_EQ(error, "CP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config5.CheckParams(error), false);
    EXPECT_EQ(error, "EP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config11.CheckParams(error), false);
    EXPECT_EQ(error, "MOE_TP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config6.CheckParams(error), false);
    EXPECT_EQ(error, "PP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config7.CheckParams(error), false);
    EXPECT_EQ(error, "TP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config8.CheckParams(error), false);
    EXPECT_EQ(error, "DP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config9.CheckParams(error), false);
    EXPECT_EQ(error, "CP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config10.CheckParams(error), false);
    EXPECT_EQ(error, "EP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
    EXPECT_EQ(config12.CheckParams(error), false);
    EXPECT_EQ(error, "MOE_TP size must be between 1 and " + std::to_string(MAX_PARALLEL_SIZE));
}

// Test for DP size evenly divided by EP size
TEST_F(ClusterDefTest, TestDPSizeEvenlyDividedByEPSize) {
    ParallelStrategyConfig config = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 5, 5, 100, 100, 254};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error, "DP size must be evenly divided by EP Size for the Megatron.");
}

// Test for product of PP, TP, DP, CP sizes being less than MAX_WORLD_SIZE
TEST_F(ClusterDefTest, TestProductOfPPAndTPAndDPAndCPSizesLessThanUINT32Max) {
    ParallelStrategyConfig config = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 10000, 10000, 10000, 10000, 100};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error,
        "The product of PP size, TP size, DP size, and CP size must be less than " + std::to_string(MAX_WORLD_SIZE));
}

// Test for DP and CP sizes being evenly divided by EP size
TEST_F(ClusterDefTest, TestDPAndCPSizeEvenlyDividedByEPSize) {
    ParallelStrategyConfig config = {MINDSPEED_TP_CP_EP_DP_PP_ALG, 5, 5, 10, 10, 254};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error,
        "The product of DP size and CP size must be evenly divided by EP Size for the "
        "MindSpeed.");
}

// Test for MOE algorithm validation (MOE_TP and EP sizes match TP and DP sizes)
TEST_F(ClusterDefTest, TestMOEAlgorithmValidation) {
    ParallelStrategyConfig config = {MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG, 5, 5, 5, 1, 5, 1};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error,
        "The product of MOE_TP size and EP size should match the product of TP size and "
        "DP size for the MindIE-LLM.");
}

// Test for unsupported CP parallelism in MOE algorithm
TEST_F(ClusterDefTest, TestUnsupportedCPParallelismForMOEAlgorithm) {
    ParallelStrategyConfig config = {MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG, 5, 5, 5, 5, 5, 5};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error, "The CP Parallelism is not supported by the MOE algorithm.");
}

// Test for DP and TP sizes being evenly divided by EP size
TEST_F(ClusterDefTest, TestDPAndTPSizeEvenlyDividedByEPSize) {
    ParallelStrategyConfig config = {VLLM_TP_PP_DP_EP_ALG, 5, 5, 5, 1, 2, 1};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error, "The product of DP size and TP size must be evenly divided by EP Size for the vLLM.");
}

// Test for EP size being evenly divided by TP size
TEST_F(ClusterDefTest, TestEPSizeEvenlyDividedByTPSize) {
    ParallelStrategyConfig config = {VLLM_TP_PP_DP_EP_ALG, 2, 8, 2, 1, 2, 1};
    std::string error;
    EXPECT_EQ(config.CheckParams(error), false);
    EXPECT_EQ(error, "EP size must be evenly divided by TP Size for the vLLM.");
}

// Test for valid configuration (all parameters are valid)
TEST_F(ClusterDefTest, TestValidConfiguration) {
    std::string error;
    ParallelStrategyConfig config = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 2, 2, 4, 2, 2};
    EXPECT_EQ(config.CheckParams(error), true);
    EXPECT_EQ(error, "");
}

TEST_F(ClusterDefTest, TestCheckParamForMindSpeedForParallelStrategyConfigReturnFalseWithInvalidParameter) {
    std::vector<ParallelStrategyConfig> configList = {{MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 5, 1, "AA", {}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 6, 1, "AA", {false, 1, 1, "XXX", 2, 1}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 6, 1, "AA", {false, 1, 1, MINDSPEED_HYBIRD_CP_ALG, 0, 1}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 6, 1, "AA", {false, 1, 1, MINDSPEED_HYBIRD_CP_ALG, 2, 1}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 6, 1, "AA", {false, 1, 1, MINDSPEED_HYBIRD_CP_ALG, 3, 0}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 6, 6, 1, "AA", {false, 1, 1, MINDSPEED_HYBIRD_CP_ALG, 3, 3}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 6, 6, 1, "AA", {false, 1, 1, MINDSPEED_MEGATRON_CP_ALG, 3, 0}},
        {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 6, 6, 1, "AA", {false, 1, 1, MINDSPEED_MEGATRON_CP_ALG, 3, 4}}};
    std::vector<std::string> errList = {
        "The product of DP size and CP size must be evenly divided by EP Size for the MindSpeed.",
        "Mindspeed CP algorithm is not allowed.", "Ulysses degree must be greater than 0.",
        "CP size must be evenly divided by ulysses degree for hybird cp.", "CP Window size must be greater than 0.",
        "CP size must be evenly divided by the product of ulysses degree and cp window size.",
        "CP Window size must be greater than 0.", "CP size must be evenly divided by cp window size."};
    std::string error;
    ParallelStrategyConfig config;
    for (size_t i = 0; i < configList.size() && i < errList.size(); i++) {
        config = configList[i];
        EXPECT_EQ(config.CheckParamForMindSpeed(error), false);
        EXPECT_EQ(error, errList[i]);
    }
}

TEST_F(ClusterDefTest, TestCheckParamForMindSpeedForParallelStrategyConfigReturnTrueWithValidParameter) {
    ParallelStrategyConfig config = {
        MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 3, 6, 1, "AA", {false, 1, 1, MINDSPEED_HYBIRD_CP_ALG, 3, 1}};
    std::string error;
    EXPECT_EQ(config.CheckParamForMindSpeed(error), true);
    EXPECT_EQ(error, "");
}
TEST_F(ClusterDefTest, TestCheckTp2DSizeForMindSpeedForParallelStrategyConfigReturnFalseWithInvalidParameter) {
    ParallelStrategyConfig config1 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 1, 1, 1, "AA", {true, 0, 0, "", 0, 0}};
    std::string error;
    EXPECT_EQ(config1.CheckTp2DSizeForMindSpeed(error), false);
    EXPECT_EQ(error, "Nd1dim1 or nd2dim1 must be greater than 0.");
    ParallelStrategyConfig config2 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 2, 4, 1, 1, 1, "AA", {true, 3, 1, "", 0, 0}};
    EXPECT_EQ(config2.CheckTp2DSizeForMindSpeed(error), false);
    EXPECT_EQ(error, "TP size must be evenly divided by nd1dim1 and nd2dim1 for tp2d.");
}

TEST_F(ClusterDefTest, TestCheckTp2DSizeForMindSpeedForParallelStrategyConfigReturnTrueWithValidParameter) {
    ParallelStrategyConfig config1 = {MEGATRON_LM_TP_CP_EP_DP_PP_ALG, 1, 4, 2, 1, 1, 1, "AA", {true, 2, 2, "", 0, 0}};
    std::string error;
    EXPECT_EQ(config1.CheckTp2DSizeForMindSpeed(error), true);
    EXPECT_EQ(error, "");
}
