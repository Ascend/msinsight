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
#include <algorithm>
#include "TraceTime.h"

using namespace Dic::Module::Timeline;
class TraceTimeTest : public ::testing::Test {
    void SetUp() override { Dic::Module::Timeline::TraceTime::Instance().Reset(); }
    void TearDown() override { TraceTime::Instance().Reset(); }
};

/**
 * 测试单卡情况
 */
TEST_F(TraceTimeTest, TestSingleRank) {
    const uint64_t min = 20;
    const uint64_t max = 40;
    TraceTime::Instance().UpdateTime(min, max);
    TraceTime::Instance().UpdateCardTimeDuration("kkk", min, max);
    uint64_t offset = TraceTime::Instance().GetOffsetByFileId("kkk");
    EXPECT_EQ(offset, 0);
    uint64_t duration = TraceTime::Instance().GetDuration();
    const uint64_t expectDur = 20;
    const uint64_t expectStar = 20;
    EXPECT_EQ(duration, expectDur);
    std::vector<std::pair<std::string, uint64_t>> res = TraceTime::Instance().ComputeCardMinTimeInfo();
    EXPECT_EQ(res.size(), 1);
    EXPECT_EQ(res.begin()->first, "kkk");
    EXPECT_EQ(res.begin()->second, 0);
    uint64_t startTime = TraceTime::Instance().GetStartTime();
    EXPECT_EQ(startTime, expectStar);
}

/**
 * 测试单机八卡的情况
 */
TEST_F(TraceTimeTest, TestSingleHostAndManyRanks) {
    std::vector<std::pair<uint64_t, uint64_t>> data = {
        {10, 50}, {12, 53}, {17, 59}, {12, 52}, {18, 58}, {19, 53}, {9, 56}, {11, 49}};
    uint64_t count = 0;
    const uint64_t targetFileId = 4;
    for (const auto &item : data) {
        const std::string fileId = std::to_string(count);
        TraceTime::Instance().UpdateTime(item.first, item.second);
        TraceTime::Instance().UpdateCardTimeDuration(fileId, item.first, item.second);
        count++;
        if (count == targetFileId) {
            uint64_t offset = TraceTime::Instance().GetOffsetByFileId(fileId);
            EXPECT_EQ(offset, 0);
        }
    }
    uint64_t duration = TraceTime::Instance().GetDuration();
    const uint64_t expectDur = 50;
    EXPECT_EQ(duration, expectDur);
    std::vector<std::pair<std::string, uint64_t>> res = TraceTime::Instance().ComputeCardMinTimeInfo();
    std::sort(res.begin(), res.end());
    const uint64_t expectSize = 8;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front().first, "0");
    EXPECT_EQ(res.front().second, 0);
    EXPECT_EQ(res.back().first, "7");
    EXPECT_EQ(res.back().second, 0);
    uint64_t startTime = TraceTime::Instance().GetStartTime();
    const uint64_t expectTime = 9;
    EXPECT_EQ(startTime, expectTime);
}

/**
 * 测试双机16卡的情况，采集的数据在同一时间段
 */
TEST_F(TraceTimeTest, TestTwoHostAndManyRanks) {
    std::vector<std::pair<uint64_t, uint64_t>> data = {{10, 50}, {12, 53}, {17, 59}, {12, 52}, {18, 58}, {19, 53},
        {9, 56}, {11, 49}, {73, 101}, {75, 94}, {61, 96}, {63, 99}, {52, 112}, {59, 114}, {70, 105}, {55, 111}};
    uint64_t count = 0;
    const uint64_t firstTargetFileId = 4;
    const uint64_t secondTargetFileId = 14;
    for (const auto &item : data) {
        const std::string fileId = std::to_string(count);
        TraceTime::Instance().UpdateTime(item.first, item.second);
        TraceTime::Instance().UpdateCardTimeDuration(fileId, item.first, item.second);
        count++;
        if (count == firstTargetFileId) {
            uint64_t offset = TraceTime::Instance().GetOffsetByFileId(fileId);
            EXPECT_EQ(offset, 0);
        }
        if (count == secondTargetFileId) {
            uint64_t offset = TraceTime::Instance().GetOffsetByFileId(fileId);
            EXPECT_EQ(offset, 0);
        }
    }
    uint64_t duration = TraceTime::Instance().GetDuration();
    const uint64_t expectDur = 105;
    EXPECT_EQ(duration, expectDur);
    std::vector<std::pair<std::string, uint64_t>> res = TraceTime::Instance().ComputeCardMinTimeInfo();
    std::sort(res.begin(), res.end());
    const uint64_t expecSize = 16;
    EXPECT_EQ(res.size(), expecSize);
    EXPECT_EQ(res.front().first, "0");
    EXPECT_EQ(res.front().second, 0);
    EXPECT_EQ(res.back().first, "9");
    EXPECT_EQ(res.back().second, 0);
    uint64_t startTime = TraceTime::Instance().GetStartTime();
    const uint64_t expectTime = 9;
    EXPECT_EQ(startTime, expectTime);
}

/**
 * 测试双机16卡的情况，采集的数据不在同一时间段
 */
TEST_F(TraceTimeTest, TestTwoHostAndManyRanksWithTimeDurationNotSame) {
    std::vector<std::pair<uint64_t, uint64_t>> data = {{10, 50}, {12, 53}, {17, 59}, {12, 52}, {18, 58}, {19, 53},
        {9, 56}, {11, 49}, {73, 101}, {75, 94}, {61, 96}, {63, 99}, {62, 112}, {69, 114}, {70, 105}, {65, 111}};
    uint64_t count = 0;
    const uint64_t firstTargetFileId = 4;
    const uint64_t secondTargetFileId = 14;
    const uint64_t secondExpectOffset = 52;
    for (const auto &item : data) {
        const std::string fileId = std::to_string(count);
        TraceTime::Instance().UpdateTime(item.first, item.second);
        TraceTime::Instance().UpdateCardTimeDuration(fileId, item.first, item.second);
        count++;
        if (count == firstTargetFileId) {
            uint64_t offset = TraceTime::Instance().GetOffsetByFileId(fileId);
            EXPECT_EQ(offset, 0);
        }
        if (count == secondTargetFileId) {
            uint64_t offset = TraceTime::Instance().GetOffsetByFileId(fileId);
            EXPECT_EQ(offset, secondExpectOffset);
        }
    }
    uint64_t duration = TraceTime::Instance().GetDuration();
    const uint64_t expectDur = 53;
    EXPECT_EQ(duration, expectDur);
    std::vector<std::pair<std::string, uint64_t>> res = TraceTime::Instance().ComputeCardMinTimeInfo();
    std::sort(res.begin(), res.end());
    const uint64_t expectSize = 16;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front().first, "0");
    EXPECT_EQ(res.front().second, 0);
    EXPECT_EQ(res.back().first, "9");
    EXPECT_EQ(res.back().second, secondExpectOffset);
    uint64_t startTime = TraceTime::Instance().GetStartTime();
    const uint64_t expectTime = 9;
    EXPECT_EQ(startTime, expectTime);
}
