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
#include "TimeRangeUtils.h"

using Dic::Module::Timeline::AddTimestampOffset;

TEST(TimeRangeUtilsTest, AddsNormalOffsetsAndPreservesZeroOffset) {
    EXPECT_EQ(AddTimestampOffset(0, 0), 0U);
    EXPECT_EQ(AddTimestampOffset(123, 0), 123U);
    EXPECT_EQ(AddTimestampOffset(0, 456), 456U);
    EXPECT_EQ(AddTimestampOffset(123, 456), 579U);
}

TEST(TimeRangeUtilsTest, SaturatesOnlyWhenTheSumExceedsUint64) {
    const auto max = std::numeric_limits<uint64_t>::max();
    EXPECT_EQ(AddTimestampOffset(max - 10, 9), max - 1);
    EXPECT_EQ(AddTimestampOffset(max - 10, 10), max);
    EXPECT_EQ(AddTimestampOffset(max - 10, 11), max);
    EXPECT_EQ(AddTimestampOffset(max, max), max);
    EXPECT_EQ(AddTimestampOffset(0, max), max);
}
