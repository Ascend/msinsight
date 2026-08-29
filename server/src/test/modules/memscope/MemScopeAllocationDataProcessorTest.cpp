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
#include "MemScopeAllocationDataProcessor.h"
#include "MemScopeDefs.h"

using namespace Dic::Module::MemScope;

namespace {
MemoryAllocation MakeAlloc(uint64_t timestamp, uint64_t reservedSize, uint64_t processUsed, uint64_t deviceUsed) {
    return {timestamp, 0, "1", "PTA", false, reservedSize, processUsed, deviceUsed};
}
} // namespace

TEST(MemScopeAllocationDataProcessorTest, CompressUsageLineDropsOnlyMiddleDuplicatePoints) {
    const std::vector<MemoryAllocation> allocations = {
        MakeAlloc(0, 10, 100, 1000),
        MakeAlloc(1, 10, 100, 1000),
        MakeAlloc(2, 10, 100, 1000),
        MakeAlloc(3, 20, 200, 2000),
        MakeAlloc(4, 20, 200, 2000),
        MakeAlloc(5, 30, 300, 3000),
    };

    const auto reserved = MemScopeAllocationDataProcessor::CompressReservedLine(allocations);
    ASSERT_EQ(reserved.size(), 5);
    EXPECT_EQ(reserved[0].timestamp, 0);
    EXPECT_EQ(reserved[0].value, 10);
    EXPECT_EQ(reserved[1].timestamp, 2);
    EXPECT_EQ(reserved[1].value, 10);
    EXPECT_EQ(reserved[2].timestamp, 3);
    EXPECT_EQ(reserved[2].value, 20);
    EXPECT_EQ(reserved[3].timestamp, 4);
    EXPECT_EQ(reserved[3].value, 20);
    EXPECT_EQ(reserved[4].timestamp, 5);
    EXPECT_EQ(reserved[4].value, 30);

    const auto processUsed = MemScopeAllocationDataProcessor::CompressProcessUsedLine(allocations);
    ASSERT_EQ(processUsed.size(), 5);
    EXPECT_EQ(processUsed[2].value, 200);
    EXPECT_EQ(processUsed[4].value, 300);

    const auto deviceUsed = MemScopeAllocationDataProcessor::CompressDeviceUsedLine(allocations);
    ASSERT_EQ(deviceUsed.size(), 5);
    EXPECT_EQ(deviceUsed[0].value, 1000);
    EXPECT_EQ(deviceUsed[4].value, 3000);
}

TEST(MemScopeAllocationDataProcessorTest, CompressUsageLineHandlesEmptyAndSinglePoint) {
    EXPECT_TRUE(MemScopeAllocationDataProcessor::CompressReservedLine({}).empty());
    EXPECT_TRUE(MemScopeAllocationDataProcessor::CompressReservedLine({MakeAlloc(7, 0, 0, 0)}).empty());

    const auto single = MemScopeAllocationDataProcessor::CompressReservedLine({MakeAlloc(7, 42, 0, 0)});
    ASSERT_EQ(single.size(), 1);
    EXPECT_EQ(single.front().timestamp, 7);
    EXPECT_EQ(single.front().value, 42);
}
