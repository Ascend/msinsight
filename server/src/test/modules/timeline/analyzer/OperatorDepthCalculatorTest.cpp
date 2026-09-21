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

#include <algorithm>

#include <gtest/gtest.h>

#include "OperatorDepthCalculator.h"

using namespace Dic::Module::Timeline;

TEST(OperatorDepthCalculatorTest, EmptyInputReturnsZeroDepth) {
    std::vector<SliceDomain> slices;

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 0);
    EXPECT_TRUE(slices.empty());
}

TEST(OperatorDepthCalculatorTest, TouchingIntervalsReuseDepth) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 10, 7, ""},
        SliceDomain{2, 10, 20, 7, ""},
        SliceDomain{3, 20, 30, 7, ""},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 1);
    for (const auto &slice : slices) {
        EXPECT_EQ(slice.depth, 0);
    }
}

TEST(OperatorDepthCalculatorTest, SameTimestampUsesIdOrderAfterCanonicalSort) {
    std::vector<SliceDomain> slices = {
        SliceDomain{2, 0, 5, 0, ""},
        SliceDomain{1, 0, 10, 0, ""},
        SliceDomain{3, 10, 15, 0, ""},
    };
    std::sort(slices.begin(), slices.end(), SliceDomain::CompareTimestampASC);

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 2);
    EXPECT_EQ(slices[0].id, 1);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].id, 2);
    EXPECT_EQ(slices[1].depth, 1);
    EXPECT_EQ(slices[2].id, 3);
    EXPECT_EQ(slices[2].depth, 0);
}

TEST(OperatorDepthCalculatorTest, AssignsGroupedAndUngroupedSlices) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 10, 0, "groupA"},
        SliceDomain{2, 12, 20, 0, "groupA"},
        SliceDomain{3, 5, 15, 0, "groupB"},
        SliceDomain{4, 8, 9, 0, ""},
        SliceDomain{5, 20, 25, 0, ""},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 3);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].depth, 0);
    EXPECT_EQ(slices[2].depth, 1);
    EXPECT_EQ(slices[3].depth, 2);
    EXPECT_EQ(slices[4].depth, 0);
}

TEST(OperatorDepthCalculatorTest, ReusesDepthForNonOverlappingSlices) {
    std::vector<SliceDomain> slices;
    for (uint64_t i = 0; i < 200; ++i) {
        slices.emplace_back(SliceDomain{i + 1, i * 10, i * 10 + 5, 7, ""});
    }

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 1);
    for (const auto &slice : slices) {
        EXPECT_EQ(slice.depth, 0);
    }
}

TEST(OperatorDepthCalculatorTest, KeepsOriginalOrderSemanticsForUnsortedInput) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 10, 20, 0, ""},
        SliceDomain{2, 0, 30, 0, ""},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 2);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].depth, 1);
}

TEST(OperatorDepthCalculatorTest, MergesDirtyGroupIntoOneDepthUnit) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 20, 0, "dirtyGroup"},
        SliceDomain{2, 10, 30, 0, "dirtyGroup"},
        SliceDomain{3, 5, 15, 0, "other"},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 2);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].depth, 0);
    EXPECT_EQ(slices[2].depth, 1);
}

TEST(OperatorDepthCalculatorTest, GroupSpanningOtherSlicesOccupiesSingleDepthUnit) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 5, 0, "groupA"},
        SliceDomain{2, 5, 20, 0, ""},
        SliceDomain{3, 20, 25, 0, "groupA"},
        SliceDomain{4, 25, 30, 0, ""},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 2);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].depth, 1);
    EXPECT_EQ(slices[2].depth, 0);
    EXPECT_EQ(slices[3].depth, 0);
}

TEST(OperatorDepthCalculatorTest, ZeroDurationAndInvalidEndTimeUseNormalizedIntervals) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 10, 0, ""},
        SliceDomain{2, 5, 5, 0, ""},
        SliceDomain{3, 10, 10, 0, ""},
        SliceDomain{4, 12, 8, 0, ""},
    };

    EXPECT_EQ(OperatorDepthCalculator::AssignDepths(slices), 2);
    EXPECT_EQ(slices[0].depth, 0);
    EXPECT_EQ(slices[1].depth, 1);
    EXPECT_EQ(slices[2].depth, 0);
    EXPECT_EQ(slices[3].depth, 0);
}
