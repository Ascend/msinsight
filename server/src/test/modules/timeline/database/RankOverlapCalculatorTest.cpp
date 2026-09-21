/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#include "RankOverlapCalculator.h"

using Dic::Module::Timeline::RankInterval;
using Dic::Module::Timeline::RankOverlapCalculator;

namespace {
std::vector<Dic::Module::FullDb::OVERLAP_INFO> OfType(
    const std::vector<Dic::Module::FullDb::OVERLAP_INFO> &items, int64_t type) {
    std::vector<Dic::Module::FullDb::OVERLAP_INFO> result;
    for (const auto &item : items) {
        if (item.type == type) {
            result.emplace_back(item);
        }
    }
    return result;
}
}

TEST(RankOverlapCalculatorTest, MergesSourcesAndDiscardsInvalidIntervals) {
    auto result = RankOverlapCalculator::Calculate({{10, 20}, {20, 30}, {15, 25}, {40, 40}}, {}, std::nullopt);
    auto computing = OfType(result, 0);
    ASSERT_EQ(computing.size(), 1U);
    EXPECT_EQ(computing[0].startNs, 10);
    EXPECT_EQ(computing[0].endNs, 30);
}

TEST(RankOverlapCalculatorTest, SubtractsComputeFromCommunication) {
    auto result = RankOverlapCalculator::Calculate({{20, 30}, {40, 50}}, {{10, 60}}, std::nullopt);
    auto communicationOnly = OfType(result, 2);
    ASSERT_EQ(communicationOnly.size(), 3U);
    EXPECT_EQ(communicationOnly[0].startNs, 10);
    EXPECT_EQ(communicationOnly[0].endNs, 20);
    EXPECT_EQ(communicationOnly[1].startNs, 30);
    EXPECT_EQ(communicationOnly[1].endNs, 40);
    EXPECT_EQ(communicationOnly[2].startNs, 50);
    EXPECT_EQ(communicationOnly[2].endNs, 60);
}

TEST(RankOverlapCalculatorTest, CalculatesFreeAcrossGlobalTaskSpan) {
    auto result = RankOverlapCalculator::Calculate({{20, 30}}, {{40, 50}}, RankInterval{10, 60});
    auto free = OfType(result, 3);
    ASSERT_EQ(free.size(), 3U);
    EXPECT_EQ(free[0].startNs, 10);
    EXPECT_EQ(free[0].endNs, 20);
    EXPECT_EQ(free[1].startNs, 30);
    EXPECT_EQ(free[1].endNs, 40);
    EXPECT_EQ(free[2].startNs, 50);
    EXPECT_EQ(free[2].endNs, 60);
}

TEST(RankOverlapCalculatorTest, SourceOrderDoesNotAffectOutput) {
    auto first = RankOverlapCalculator::Calculate({{30, 40}, {10, 20}}, {{15, 35}}, RankInterval{0, 50});
    auto second = RankOverlapCalculator::Calculate({{10, 20}, {30, 40}}, {{15, 35}}, RankInterval{0, 50});
    ASSERT_EQ(first.size(), second.size());
    for (size_t index = 0; index < first.size(); ++index) {
        EXPECT_EQ(first[index].startNs, second[index].startNs);
        EXPECT_EQ(first[index].endNs, second[index].endNs);
        EXPECT_EQ(first[index].type, second[index].type);
    }
}
