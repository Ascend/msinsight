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
#include <limits>
#include "Paginator.h"

TEST(PaginatorTest, ReturnsCompleteAndPartialPagesWithoutChangingInput) {
    std::vector<int> input{10, 20, 30, 40, 50};
    Paginator<int> paginator(input, 2);
    input[0] = -1;
    EXPECT_EQ(paginator.GetTotal(), 5U);
    EXPECT_EQ(paginator.GetPage(1), (std::vector<int>{10, 20}));
    EXPECT_EQ(paginator.GetPage(2), (std::vector<int>{30, 40}));
    EXPECT_EQ(paginator.GetPage(3), (std::vector<int>{50}));
    EXPECT_TRUE(paginator.GetPage(4).empty());
}

TEST(PaginatorTest, EmptyInputZeroPageAndZeroPageSizeReturnEmpty) {
    EXPECT_EQ(Paginator<int>({}).GetTotal(), 0U);
    EXPECT_TRUE(Paginator<int>({}).GetPage(1).empty());
    EXPECT_TRUE(Paginator<int>({1, 2}, 1).GetPage(0).empty());
    EXPECT_TRUE(Paginator<int>({1, 2}, 0).GetPage(1).empty());
}

TEST(PaginatorTest, SupportsMaximumPageSizeAndNonOverflowingPageOffsets) {
    const auto max = std::numeric_limits<uint32_t>::max();
    Paginator<int> paginator({1, 2, 3}, max);
    EXPECT_EQ(paginator.GetPage(1), (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(paginator.GetPage(2).empty());
    Paginator<int> twoPerPage({1, 2, 3}, 2);
    EXPECT_TRUE(twoPerPage.GetPage(max / 2 + 1).empty());
    Paginator<int> onePerPage({1, 2, 3}, 1);
    EXPECT_TRUE(onePerPage.GetPage(max).empty());
}
