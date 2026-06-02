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
#include <gmock/gmock.h>
#include "FlowAnalyzer.h"

using namespace Dic::Module::Timeline;
class FlowAnalyzerTest : public ::testing::Test {};

/**
 * 测试：点数组转成连线数组，基础情况 S-F
 */
TEST_F(FlowAnalyzerTest, Test_ComputeUintFlows_S_F) {
    const std::string category = "A";
    std::vector<FlowPoint> flowPoints = {
        FlowPoint{.flowId = "0", .type = Dic::Protocol::LINE_START, .tid = "0"},
        FlowPoint{.flowId = "0", .type = Dic::Protocol::LINE_END, .tid = "1"},
    };
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    const std::vector<std::tuple<std::string, std::string>> expectResultList = {
        {"0", "1"},
    };
    FlowAnalyzer::ComputeUintFlows(flowPoints, category, flowDetailList);
    ASSERT_EQ(expectResultList.size(), flowDetailList.size());

    for (size_t i = 0; i < expectResultList.size(); ++i) {
        const auto &[expectedFromId, expectedToId] = expectResultList[i];
        const auto &actual = flowDetailList[i];

        EXPECT_EQ(expectedFromId, actual->from.tid);
        EXPECT_EQ(expectedToId, actual->to.tid);
    }
}

/**
 * 测试：点数组转成连线数组，特殊结束 S-T
 */
TEST_F(FlowAnalyzerTest, Test_ComputeUintFlows_S_T) {
    const std::string category = "A";
    std::vector<FlowPoint> flowPoints = {
        FlowPoint{.flowId = "1", .type = Dic::Protocol::LINE_START, .tid = "2"},
        FlowPoint{.flowId = "1", .type = Dic::Protocol::LINE_END_OPTIONAL, .tid = "3"},
    };
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    const std::vector<std::tuple<std::string, std::string>> expectResultList = {
        {"2", "3"},
    };
    FlowAnalyzer::ComputeUintFlows(flowPoints, category, flowDetailList);
    ASSERT_EQ(expectResultList.size(), flowDetailList.size());

    for (size_t i = 0; i < expectResultList.size(); ++i) {
        const auto &[expectedFromId, expectedToId] = expectResultList[i];
        const auto &actual = flowDetailList[i];

        EXPECT_EQ(expectedFromId, actual->from.tid);
        EXPECT_EQ(expectedToId, actual->to.tid);
    }
}

/**
 * 测试：点数组转成连线数组，连续连线 S-T-T-...-T-F
 */
TEST_F(FlowAnalyzerTest, Test_ComputeUintFlows_S_ManyT_F) {
    const std::string category = "A";
    std::vector<FlowPoint> flowPoints = {
        FlowPoint{.flowId = "2", .type = Dic::Protocol::LINE_START, .tid = "4"},
        FlowPoint{.flowId = "2", .type = Dic::Protocol::LINE_END_OPTIONAL, .tid = "5"},
        FlowPoint{.flowId = "2", .type = Dic::Protocol::LINE_END_OPTIONAL, .tid = "6"},
        FlowPoint{.flowId = "2", .type = Dic::Protocol::LINE_END_OPTIONAL, .tid = "7"},
        FlowPoint{.flowId = "2", .type = Dic::Protocol::LINE_END, .tid = "8"},
    };
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    const std::vector<std::tuple<std::string, std::string>> expectResultList = {
        {"4", "5"},
        {"5", "6"},
        {"6", "7"},
        {"7", "8"},
    };
    FlowAnalyzer::ComputeUintFlows(flowPoints, category, flowDetailList);
    ASSERT_EQ(expectResultList.size(), flowDetailList.size());

    for (size_t i = 0; i < expectResultList.size(); ++i) {
        const auto &[expectedFromId, expectedToId] = expectResultList[i];
        const auto &actual = flowDetailList[i];

        EXPECT_EQ(expectedFromId, actual->from.tid);
        EXPECT_EQ(expectedToId, actual->to.tid);
    }
}

/**
 * 测试：点数组转成连线数组，连续结尾 S-F-F-...-F
 */
TEST_F(FlowAnalyzerTest, Test_ComputeUintFlows_S_ManyF) {
    const std::string category = "A";
    std::vector<FlowPoint> flowPoints = {
        FlowPoint{.flowId = "3", .type = Dic::Protocol::LINE_START, .tid = "9"},
        FlowPoint{.flowId = "3", .type = Dic::Protocol::LINE_END, .tid = "10"},
        FlowPoint{.flowId = "3", .type = Dic::Protocol::LINE_END, .tid = "11"},
        FlowPoint{.flowId = "3", .type = Dic::Protocol::LINE_END, .tid = "12"},
        FlowPoint{.flowId = "3", .type = Dic::Protocol::LINE_END, .tid = "13"},
    };
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flowDetailList;
    const std::vector<std::tuple<std::string, std::string>> expectResultList = {
        {"9", "10"},
        {"9", "11"},
        {"9", "12"},
        {"9", "13"},
    };
    FlowAnalyzer::ComputeUintFlows(flowPoints, category, flowDetailList);
    ASSERT_EQ(expectResultList.size(), flowDetailList.size());

    for (size_t i = 0; i < expectResultList.size(); ++i) {
        const auto &[expectedFromId, expectedToId] = expectResultList[i];
        const auto &actual = flowDetailList[i];

        EXPECT_EQ(expectedFromId, actual->from.tid);
        EXPECT_EQ(expectedToId, actual->to.tid);
    }
}
