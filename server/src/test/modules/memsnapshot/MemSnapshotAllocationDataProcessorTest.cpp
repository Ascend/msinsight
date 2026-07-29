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
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <algorithm>
#include "MemSnapshotAllocationDataProcessor.h"
#include "MemSnapshotProtocolResponse.h"

using namespace Dic::Module::MemSnapshot;

TEST(MemSnapshotAllocationDataProcessorTest, ExtractAllocationTurningPointsHandlesEmptyAndSingleRecord) {
    const auto emptyResult = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints({});
    EXPECT_TRUE(emptyResult.empty());

    const std::vector<Dic::Protocol::AllocationRecord> singleRecord = {{7, 100, 100}};
    const auto singleResult = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(singleRecord);

    ASSERT_EQ(singleResult.size(), size_t{1});
    EXPECT_EQ(singleResult.front().timestamp, 7);
    EXPECT_EQ(singleResult.front().totalSize, uint64_t{100});
}

TEST(MemSnapshotAllocationDataProcessorTest, ExtractAllocationTurningPointsKeepsPeaksValleysAndEndpoints) {
    std::vector<Dic::Protocol::AllocationRecord> records;
    records.reserve(3001);
    for (int64_t timestamp = 0; timestamp <= 3000; ++timestamp) {
        records.emplace_back(timestamp, static_cast<uint64_t>(timestamp), static_cast<uint64_t>(timestamp));
    }
    records[1000].allocated = 5000;
    records[2000].allocated = 0;
    records[2500].allocated = 4000;
    records[2501].allocated = 4000;

    const auto turningPoints = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(records);

    const std::vector<int64_t> expectedTimestamps = {0, 1000, 2000, 2500, 2501, 3000};
    for (int64_t timestamp : expectedTimestamps) {
        EXPECT_NE(std::find_if(turningPoints.begin(), turningPoints.end(),
                      [timestamp](const auto &record) { return record.timestamp == timestamp; }),
            turningPoints.end());
    }
}

TEST(MemSnapshotAllocationDataProcessorTest, ExtractAllocationTurningPointsKeepsAllAvailableSparseRecords) {
    const std::vector<Dic::Protocol::AllocationRecord> records = {{0, 10, 10}, {100, 20, 20}, {200, 30, 30}};

    const auto turningPoints = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(records);

    ASSERT_EQ(turningPoints.size(), records.size());
    for (size_t index = 0; index < records.size(); ++index) {
        EXPECT_EQ(turningPoints[index].timestamp, records[index].timestamp);
        EXPECT_EQ(turningPoints[index].totalSize, records[index].allocated);
    }
}

TEST(MemSnapshotAllocationDataProcessorTest, ExtractAllocationTurningPointsLimitsTimestampGap) {
    std::vector<Dic::Protocol::AllocationRecord> records;
    records.reserve(3001);
    for (int64_t timestamp = 0; timestamp <= 3000; ++timestamp) {
        records.emplace_back(timestamp, static_cast<uint64_t>(timestamp), static_cast<uint64_t>(timestamp));
    }

    const auto turningPoints = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(records);

    ASSERT_LT(turningPoints.size(), records.size());
    EXPECT_EQ(turningPoints.front().timestamp, 0);
    EXPECT_EQ(turningPoints.back().timestamp, 3000);
    for (size_t index = 1; index < turningPoints.size(); ++index) {
        EXPECT_LE(turningPoints[index].timestamp - turningPoints[index - 1].timestamp, 3);
    }
}

TEST(MemSnapshotAllocationDataProcessorTest, CompressReservedLineDropsOnlyMiddleDuplicatePoints) {
    const std::vector<Dic::Protocol::AllocationRecord> records = {
        {0, 5, 10}, {1, 6, 10}, {2, 7, 10}, {3, 20, 15}, {4, 19, 20}, {5, 30, 25}};

    const auto compressed = MemSnapshotAllocationDataProcessor::CompressReservedLine(records);

    ASSERT_EQ(compressed.size(), 5);
    EXPECT_EQ(compressed[0].timestamp, 0);
    EXPECT_EQ(compressed[0].reservedSize, 10);
    EXPECT_EQ(compressed[1].timestamp, 2);
    EXPECT_EQ(compressed[1].reservedSize, 10);
    EXPECT_EQ(compressed[2].timestamp, 3);
    EXPECT_EQ(compressed[2].reservedSize, 20);
    EXPECT_EQ(compressed[3].timestamp, 4);
    EXPECT_EQ(compressed[3].reservedSize, 20);
    EXPECT_EQ(compressed[4].timestamp, 5);
    EXPECT_EQ(compressed[4].reservedSize, 30);
}

TEST(MemSnapshotAllocationDataProcessorTest, ResponseSeparatesAllocatedAndReservedLines) {
    Dic::Protocol::MemSnapshotAllocationsResponse response;
    response.allocations.emplace_back(1, 100);
    response.reservedLine.emplace_back(1, 200);

    const auto json = response.ToJson();

    ASSERT_TRUE(json.has_value());
    ASSERT_TRUE((*json)["body"]["allocations"].IsArray());
    ASSERT_TRUE((*json)["body"]["reservedLine"].IsArray());
    const auto &allocation = (*json)["body"]["allocations"][0];
    EXPECT_TRUE(allocation.HasMember("timestamp"));
    EXPECT_TRUE(allocation.HasMember("totalSize"));
    EXPECT_FALSE(allocation.HasMember("reservedSize"));
    const auto &reserved = (*json)["body"]["reservedLine"][0];
    EXPECT_TRUE(reserved.HasMember("timestamp"));
    EXPECT_TRUE(reserved.HasMember("reservedSize"));
    EXPECT_FALSE(reserved.HasMember("totalSize"));
}
