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
#include "SimulationSliceCacheManager.h"

using namespace Dic::Module::Timeline;
namespace {
Trace::Slice Flag(const std::string &type, int64_t timestamp, const std::string &name = "flag") {
    Trace::Slice result;
    result.type = type;
    result.ts = timestamp;
    result.name = name;
    return result;
}
}
class SimulationSliceCacheManagerTest : public testing::Test {
  protected:
    void SetUp() override { SimulationSliceCacheManager::Instance().ClearAll(); }
    void TearDown() override { SimulationSliceCacheManager::Instance().ClearAll(); }
};

TEST_F(SimulationSliceCacheManagerTest, PairsZeroDurationBeginThenEndAndConsumesCachedSlice) {
    auto &cache = SimulationSliceCacheManager::Instance();
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SB", 10, "begin")}}, {}, "a").empty());
    auto result = cache.GetCompeteSlice({{"1", Flag("SE", 10, "end")}}, {}, "a");
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].ts, 10);
    EXPECT_EQ(result[0].dur, 0);
    EXPECT_EQ(result[0].name, "begin");
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SE", 50)}}, {}, "a").empty());
}

TEST_F(SimulationSliceCacheManagerTest, PairsEndThenBeginAndClampsReversedTime) {
    auto &cache = SimulationSliceCacheManager::Instance();
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"1", Flag("SE", 40)}}, "a").empty());
    auto result = cache.GetCompeteSlice({}, {{"1", Flag("SB", 10)}}, "a");
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].ts, 10);
    EXPECT_EQ(result[0].dur, 30);
    EXPECT_TRUE(cache.GetCompeteSlice({{"2", Flag("SB", 80)}}, {}, "a").empty());
    result = cache.GetCompeteSlice({{"2", Flag("SE", 60)}}, {}, "a");
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].dur, 0);
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"2", Flag("SE", 60)}}, "a").empty());
    result = cache.GetCompeteSlice({}, {{"2", Flag("SB", 80)}}, "a");
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].dur, 0);
}

TEST_F(SimulationSliceCacheManagerTest, PairsEveryKeyInABatchWithPreviouslyCachedEnds) {
    auto &cache = SimulationSliceCacheManager::Instance();
    EXPECT_TRUE(cache.GetCompeteSlice({{"a", Flag("SE", 15)}}, {}, "file").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({{"b", Flag("SE", 26)}}, {}, "file").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"c", Flag("SE", 37)}}, "file").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"d", Flag("SE", 48)}}, "file").empty());
    auto result = cache.GetCompeteSlice({{"a", Flag("SB", 10, "a")}, {"b", Flag("SB", 20, "b")}},
        {{"c", Flag("SB", 30, "c")}, {"d", Flag("SB", 40, "d")}}, "file");
    ASSERT_EQ(result.size(), 4U);
    EXPECT_EQ(result[0].ts, 10);
    EXPECT_EQ(result[0].dur, 5);
    EXPECT_EQ(result[0].name, "a");
    EXPECT_EQ(result[1].ts, 20);
    EXPECT_EQ(result[1].dur, 6);
    EXPECT_EQ(result[1].name, "b");
    EXPECT_EQ(result[2].ts, 30);
    EXPECT_EQ(result[2].dur, 7);
    EXPECT_EQ(result[2].name, "c");
    EXPECT_EQ(result[3].ts, 40);
    EXPECT_EQ(result[3].dur, 8);
    EXPECT_EQ(result[3].name, "d");
}

TEST_F(SimulationSliceCacheManagerTest, IsolatesFilesAndSetWaitMapsAndClearsOnlySelectedFile) {
    auto &cache = SimulationSliceCacheManager::Instance();
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SE", 20)}}, {}, "a").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"1", Flag("SB", 10)}}, "a").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SE", 50)}}, {}, "b").empty());
    cache.ClearCacheByFileId("a");
    cache.ClearCacheByFileId("absent");
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SB", 10)}}, {}, "a").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"1", Flag("SE", 20)}}, "a").empty());
    auto result = cache.GetCompeteSlice({{"1", Flag("SB", 30)}}, {}, "b");
    ASSERT_EQ(result.size(), 1U);
    EXPECT_EQ(result[0].ts, 30);
    EXPECT_EQ(result[0].dur, 20);
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SE", 60)}}, {}, "b").empty());
    cache.ClearAll();
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SE", 20)}}, {}, "a").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({}, {{"1", Flag("SB", 10)}}, "a").empty());
    EXPECT_TRUE(cache.GetCompeteSlice({{"1", Flag("SB", 40)}}, {}, "b").empty());
}
