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
#include "CommunicationProtocolResponse.h"

using namespace Dic::Protocol;
class CommunicationProtocolResponseTest : public ::testing::Test {};

TEST_F(CommunicationProtocolResponseTest, PaginateDurationListKeepsAdviceAndRankOrder) {
    DurationListsResponseBody body;
    for (int rank = 0; rank < 3; ++rank) {
        Duration duration;
        duration.rankId = std::to_string(rank);
        duration.dbPath = "db" + std::to_string(rank);
        body.durationList.push_back(duration);
    }
    body.bwStatistics.push_back({"SDMA", 1, 2, 0.5, 1.5, 3});

    body.Paginate(2, 2);

    EXPECT_TRUE(body.paginated);
    EXPECT_EQ(body.total, 3);
    ASSERT_EQ(body.durationList.size(), 1);
    EXPECT_EQ(body.durationList[0].rankId, "2");
    ASSERT_EQ(body.bwStatistics.size(), 1);
    EXPECT_EQ(body.bwStatistics[0].type, "SDMA");
}

TEST_F(CommunicationProtocolResponseTest, PaginateDurationListBeyondTotalReturnsEmptyPage) {
    DurationListsResponseBody body;
    body.durationList.emplace_back();
    body.bwStatistics.push_back({"RDMA", 1, 2, 0.5, 1.5, 3});

    body.Paginate(2, 1);

    EXPECT_TRUE(body.durationList.empty());
    EXPECT_EQ(body.total, 1);
    EXPECT_EQ(body.bwStatistics.size(), 1);
}

TEST_F(CommunicationProtocolResponseTest, UnpagedDurationListKeepsLegacyState) {
    DurationListsResponseBody body;
    body.durationList.emplace_back();

    body.Paginate(0, 0);

    EXPECT_FALSE(body.paginated);
    EXPECT_EQ(body.total, 0);
    EXPECT_EQ(body.durationList.size(), 1);
}

TEST_F(CommunicationProtocolResponseTest, SetDurationPageFromKeepsFullSourceUnchanged) {
    DurationListsResponseBody source;
    for (int rank = 0; rank < 3; ++rank) {
        Duration duration;
        duration.rankId = std::to_string(rank);
        source.durationList.push_back(duration);
    }
    source.bwStatistics.push_back({"SDMA", 1, 2, 0.5, 1.5, 3});

    DurationListsResponseBody page;
    page.SetPageFrom(source, 2, 2);

    EXPECT_EQ(source.durationList.size(), 3);
    EXPECT_FALSE(source.paginated);
    EXPECT_TRUE(page.paginated);
    EXPECT_EQ(page.total, 3);
    ASSERT_EQ(page.durationList.size(), 1);
    EXPECT_EQ(page.durationList[0].rankId, "2");
    ASSERT_EQ(page.bwStatistics.size(), source.bwStatistics.size());
    EXPECT_EQ(page.bwStatistics[0].type, source.bwStatistics[0].type);
    EXPECT_EQ(page.bwStatistics[0].avgBw, source.bwStatistics[0].avgBw);
}

TEST_F(CommunicationProtocolResponseTest, SetDurationPageFromBeyondTotalKeepsMetadata) {
    DurationListsResponseBody source;
    source.durationList.emplace_back();
    source.bwStatistics.push_back({"RDMA", 1, 2, 0.5, 1.5, 3});

    DurationListsResponseBody page;
    page.SetPageFrom(source, 2, 1);

    EXPECT_TRUE(page.durationList.empty());
    EXPECT_EQ(page.total, 1);
    ASSERT_EQ(page.bwStatistics.size(), source.bwStatistics.size());
    EXPECT_EQ(page.bwStatistics[0].type, source.bwStatistics[0].type);
    EXPECT_EQ(page.bwStatistics[0].allTime, source.bwStatistics[0].allTime);
}

/**
 * 正常采集八卡的情况
 */
TEST_F(CommunicationProtocolResponseTest, TestAdjustTimeWhen8RankNormal) {
    OperatorListsResponseBody body;
    const int rankSize = 8;
    const uint64_t min = 1;
    const uint64_t max = 11;
    uint64_t init = min;
    for (int i = 0; i < rankSize; ++i) {
        CompareData<std::vector<OperatorTimeItem>> data1;
        OperatorTimeItem operatorTime1 = {"", init++, max};
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.compare.emplace_back(operatorTime1);
        body.opLists.emplace_back(data1);
    }
    body.AdjustTime("");
    const uint64_t expectMax = 19;
    EXPECT_EQ(body.maxTime, expectMax);
    EXPECT_EQ(body.minTime, min);
    EXPECT_EQ(body.opLists.size(), rankSize);
    EXPECT_EQ(body.opLists.front().compare.front().startTime, min);
    EXPECT_EQ(body.opLists.back().compare.back().elapseTime, max);
}

TEST_F(CommunicationProtocolResponseTest, PaginateKeepsAlignedMetadataAndRankOrder) {
    OperatorListsResponseBody body;
    body.minTime = 10;
    body.maxTime = 100;
    for (int rank = 0; rank < 3; ++rank) {
        body.rankLists.push_back(std::to_string(rank));
        body.dbPathList.push_back("db" + std::to_string(rank));
        CompareData<std::vector<OperatorTimeItem>> data;
        data.compare.push_back({"op", static_cast<uint64_t>(rank + 10), 5});
        body.opLists.push_back(data);
    }

    body.Paginate(2, 2);

    EXPECT_TRUE(body.paginated);
    EXPECT_EQ(body.total, 3);
    ASSERT_EQ(body.rankLists.size(), 1);
    EXPECT_EQ(body.rankLists[0], "2");
    EXPECT_EQ(body.dbPathList[0], "db2");
    EXPECT_EQ(body.opLists[0].compare[0].startTime, 12);
    EXPECT_EQ(body.minTime, 10);
    EXPECT_EQ(body.maxTime, 100);
}

TEST_F(CommunicationProtocolResponseTest, PaginateBeyondTotalReturnsEmptyPage) {
    OperatorListsResponseBody body;
    body.minTime = 10;
    body.maxTime = 100;
    body.rankLists.push_back("0");
    body.dbPathList.push_back("db0");
    body.opLists.emplace_back();

    body.Paginate(2, 1);

    EXPECT_EQ(body.total, 1);
    EXPECT_TRUE(body.rankLists.empty());
    EXPECT_TRUE(body.dbPathList.empty());
    EXPECT_TRUE(body.opLists.empty());
    EXPECT_EQ(body.minTime, 10);
    EXPECT_EQ(body.maxTime, 100);
}

TEST_F(CommunicationProtocolResponseTest, SetOperatorPageFromKeepsFullSourceUnchanged) {
    OperatorListsResponseBody source;
    source.minTime = 10;
    source.maxTime = 100;
    for (int rank = 0; rank < 3; ++rank) {
        source.rankLists.push_back(std::to_string(rank));
        source.dbPathList.push_back("db" + std::to_string(rank));
        CompareData<std::vector<OperatorTimeItem>> data;
        data.compare.push_back({"op", static_cast<uint64_t>(rank + 10), 5});
        source.opLists.push_back(data);
    }

    OperatorListsResponseBody page;
    page.SetPageFrom(source, 2, 2);

    EXPECT_EQ(source.rankLists.size(), 3);
    EXPECT_FALSE(source.paginated);
    EXPECT_TRUE(page.paginated);
    EXPECT_EQ(page.total, 3);
    EXPECT_EQ(page.minTime, source.minTime);
    EXPECT_EQ(page.maxTime, source.maxTime);
    ASSERT_EQ(page.rankLists.size(), 1);
    EXPECT_EQ(page.rankLists[0], "2");
    EXPECT_EQ(page.dbPathList[0], "db2");
    EXPECT_EQ(page.opLists[0].compare[0].startTime, 12);
}

TEST_F(CommunicationProtocolResponseTest, SetOperatorPageFromBeyondTotalKeepsMetadata) {
    OperatorListsResponseBody source;
    source.minTime = 10;
    source.maxTime = 100;
    source.rankLists.push_back("0");
    source.dbPathList.push_back("db0");
    source.opLists.emplace_back();

    OperatorListsResponseBody page;
    page.SetPageFrom(source, 2, 1);

    EXPECT_EQ(page.total, 1);
    EXPECT_TRUE(page.rankLists.empty());
    EXPECT_TRUE(page.dbPathList.empty());
    EXPECT_TRUE(page.opLists.empty());
    EXPECT_EQ(page.minTime, source.minTime);
    EXPECT_EQ(page.maxTime, source.maxTime);
}

/**
 * 两次采集的数据，每次都是8卡
 */
TEST_F(CommunicationProtocolResponseTest, TestAdjustTimeWhen16Rank2Group) {
    OperatorListsResponseBody body;
    const int rankSize = 8;
    const uint64_t min = 1;
    const uint64_t max = 11;
    uint64_t init = min;
    for (int i = 0; i < rankSize; ++i) {
        CompareData<std::vector<OperatorTimeItem>> data1;
        OperatorTimeItem operatorTime1 = {"", init++, max};
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.compare.emplace_back(operatorTime1);
        body.opLists.emplace_back(data1);
    }

    const int secondMin = 100;
    init = secondMin;
    for (int i = 0; i < rankSize; ++i) {
        CompareData<std::vector<OperatorTimeItem>> data1;
        OperatorTimeItem operatorTime1 = {"", init++, max};
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.compare.emplace_back(operatorTime1);
        body.opLists.emplace_back(data1);
    }
    body.AdjustTime("");
    const uint64_t expectMax = 118;
    EXPECT_EQ(body.maxTime, expectMax);
    const uint64_t expectMin = 100;
    EXPECT_EQ(body.minTime, expectMin);
    EXPECT_EQ(body.opLists.size(), rankSize + rankSize);
    EXPECT_EQ(body.opLists.front().compare.front().startTime, expectMin);
    EXPECT_EQ(body.opLists.back().compare.back().elapseTime, max);
}

/**
 * 采集八卡,对比情况,base和baseline在同一时间范围内
 */
TEST_F(CommunicationProtocolResponseTest, TestAdjustTimeWhen8RankAndHaveBaseWithOneGroup) {
    OperatorListsResponseBody body;
    const int rankSize = 8;
    const uint64_t min = 1;
    const uint64_t max = 11;
    uint64_t init = min;
    for (int i = 0; i < rankSize; ++i) {
        CompareData<std::vector<OperatorTimeItem>> data1;
        OperatorTimeItem operatorTime1 = {"", init++, max};
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.compare.emplace_back(operatorTime1);
        data1.baseline.emplace_back(operatorTime1);
        body.opLists.emplace_back(data1);
    }
    body.AdjustTime("");
    const uint64_t expectMax = 19;
    EXPECT_EQ(body.maxTime, expectMax);
    EXPECT_EQ(body.minTime, min);
    EXPECT_EQ(body.opLists.size(), rankSize);
    EXPECT_EQ(body.opLists.front().compare.front().startTime, min);
    EXPECT_EQ(body.opLists.back().compare.back().elapseTime, max);
    EXPECT_EQ(body.opLists.front().baseline.front().startTime, min);
    EXPECT_EQ(body.opLists.back().baseline.back().elapseTime, max);
}

/**
 * 采集八卡,对比情况,base和baseline不在同一时间范围内
 */
TEST_F(CommunicationProtocolResponseTest, TestAdjustTimeWhen8RankAndHaveBaseWithTwoGroup) {
    OperatorListsResponseBody body;
    const int rankSize = 8;
    const uint64_t min = 1;
    const uint64_t max = 11;
    const uint64_t gap = 10000000000000;
    uint64_t init = min;
    for (int i = 0; i < rankSize; ++i) {
        CompareData<std::vector<OperatorTimeItem>> data1;
        OperatorTimeItem operatorTime1 = {"", init++, max};
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.compare.emplace_back(operatorTime1);
        operatorTime1.startTime += gap;
        body.maxTime = std::max(body.maxTime, operatorTime1.startTime + operatorTime1.elapseTime);
        body.minTime = std::min(body.minTime, operatorTime1.startTime);
        data1.baseline.emplace_back(operatorTime1);
        body.opLists.emplace_back(data1);
    }
    body.AdjustTime("");
    const uint64_t expectMax = 10000000000019;
    EXPECT_EQ(body.maxTime, expectMax);
    const uint64_t expectMin = 10000000000001;
    EXPECT_EQ(body.minTime, expectMin);
    EXPECT_EQ(body.opLists.size(), rankSize);
    EXPECT_EQ(body.opLists.front().compare.front().startTime, expectMin);
    EXPECT_EQ(body.opLists.back().compare.back().elapseTime, max);
    EXPECT_EQ(body.opLists.front().baseline.front().startTime, expectMin);
    EXPECT_EQ(body.opLists.back().baseline.back().elapseTime, max);
}

TEST_F(CommunicationProtocolResponseTest, TestAdjustTimeMergesNestedAndTouchingIntervals) {
    OperatorListsResponseBody body;
    body.minTime = 10;
    body.maxTime = 110;

    CompareData<std::vector<OperatorTimeItem>> first;
    first.compare.push_back({"", 10, 10});
    body.opLists.push_back(first);

    CompareData<std::vector<OperatorTimeItem>> overlapping;
    overlapping.baseline.push_back({"", 15, 10});
    body.opLists.push_back(overlapping);

    CompareData<std::vector<OperatorTimeItem>> touching;
    touching.compare.push_back({"", 25, 5});
    body.opLists.push_back(touching);

    CompareData<std::vector<OperatorTimeItem>> separate;
    separate.baseline.push_back({"", 100, 10});
    body.opLists.push_back(separate);

    body.AdjustTime("");

    EXPECT_EQ(body.minTime, 90);
    EXPECT_EQ(body.maxTime, 110);
    EXPECT_EQ(body.opLists[0].compare[0].startTime, 90);
    EXPECT_EQ(body.opLists[1].baseline[0].startTime, 95);
    EXPECT_EQ(body.opLists[2].compare[0].startTime, 105);
    EXPECT_EQ(body.opLists[3].baseline[0].startTime, 100);
}
