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
#include "TimelineProtocolRequest.h"
#include "SearchSliceCacheManager.h"

using namespace Dic::Module::Timeline;
namespace {
LightSliceCache ExampleCache() {
    LightSliceCache cache;
    cache.rankId = "rank0";
    cache.primarySearchContent = "op";
    cache.dictMap = {{1, "MatMul"}, {2, "add"}, {3, "matmul_grad"}};
    cache.tableTypes = {0, 1, 2};
    cache.stringIds = {1, 2, 3};
    cache.rowIds = {11, 22, 33};
    cache.timestamps = {300, 100, 200};
    cache.durations = {10, 30, 20};
    SearchSliceCacheManager::InitializeSortedIndices(cache);
    return cache;
}
}
class SearchSliceCacheManagerTest : public testing::Test {
  protected:
    void SetUp() override { SearchSliceCacheManager::Instance().clearAll(); }
    void TearDown() override { SearchSliceCacheManager::Instance().clearAll(); }
};

TEST_F(SearchSliceCacheManagerTest, CacheIdentityIncludesRankSourceAndSearch) {
    EXPECT_NE(SearchSliceCacheManager::makeKey("r", "db0", "op"), SearchSliceCacheManager::makeKey("r", "db1", "op"));
    EXPECT_NE(SearchSliceCacheManager::makeKey("r", "db", "op"), SearchSliceCacheManager::makeKey("s", "db", "op"));
    auto cache = ExampleCache();
    EXPECT_TRUE(cache.isDataValid("rank0", "op"));
    EXPECT_FALSE(cache.isDataValid("rank1", "op"));
    EXPECT_FALSE(cache.isDataValid("rank0", "other"));
    cache.clear();
    EXPECT_EQ(cache.size(), 0U);
    EXPECT_TRUE(cache.dictMap.empty());
    EXPECT_TRUE(cache.sortedIndices.empty());
    EXPECT_FALSE(cache.isDataValid("rank0", "op"));
}

TEST_F(SearchSliceCacheManagerTest, SortsAllFieldsInBothDirectionsAndReusesTheSameOrder) {
    auto cache = ExampleCache();
    SearchSliceCacheManager::SortCache(cache, "timestamp", "ascend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{1, 2, 0}));
    SearchSliceCacheManager::SortCache(cache, "timestamp", "descend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{0, 2, 1}));
    SearchSliceCacheManager::SortCache(cache, "duration", "ascend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{0, 2, 1}));
    SearchSliceCacheManager::SortCache(cache, "duration", "descend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{1, 2, 0}));
    SearchSliceCacheManager::SortCache(cache, "name", "ascend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{0, 1, 2}));
    SearchSliceCacheManager::SortCache(cache, "name", "descend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{2, 1, 0}));
    SearchSliceCacheManager::SortCache(cache, "name", "descend");
    EXPECT_TRUE(cache.isOrderValid("name", "descend"));
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{2, 1, 0}));
    SearchSliceCacheManager::SortCache(cache, "unsupported", "ascend");
    EXPECT_EQ(cache.sortedIndices, (std::vector<uint32_t>{0, 1, 2}));
}

TEST_F(SearchSliceCacheManagerTest, FiltersCaseInsensitivelyBeforePaginationAndKeepsTableIdentity) {
    auto cache = ExampleCache();
    SearchSliceCacheManager::SortCache(cache, "timestamp", "ascend");
    auto result = SearchSliceCacheManager::FilterAndPage(cache, "MATMUL", 2, 1);
    ASSERT_EQ(result.rows.size(), 1U);
    EXPECT_EQ(result.totalCount, 2U);
    EXPECT_EQ(result.rows[0].rowId, 11U);
    EXPECT_EQ(result.rows[0].tableType, SliceTableType::TASK);
    result = SearchSliceCacheManager::FilterAndPage(cache, "", 1, 1);
    ASSERT_EQ(result.rows.size(), 1U);
    EXPECT_EQ(result.rows[0].rowId, 22U);
    EXPECT_EQ(result.rows[0].tableType, SliceTableType::CANN_API);
    EXPECT_EQ(result.totalCount, 3U);
}

TEST_F(SearchSliceCacheManagerTest, EmptyMissingInvalidAndOutOfRangePagesHaveDefinedCounts) {
    auto cache = ExampleCache();
    EXPECT_TRUE(SearchSliceCacheManager::FilterAndPage(cache, "absent", 1, 10).rows.empty());
    EXPECT_EQ(SearchSliceCacheManager::FilterAndPage(cache, "absent", 1, 10).totalCount, 0U);
    EXPECT_EQ(SearchSliceCacheManager::FilterAndPage(cache, "", 0, 10).totalCount, 0U);
    EXPECT_EQ(SearchSliceCacheManager::FilterAndPage(cache, "", 1, 0).totalCount, 0U);
    auto result = SearchSliceCacheManager::FilterAndPage(cache, "", 4, 1);
    EXPECT_TRUE(result.rows.empty());
    EXPECT_EQ(result.totalCount, 3U);
    cache.clear();
    EXPECT_EQ(SearchSliceCacheManager::FilterAndPage(cache, "", 1, 10).totalCount, 0U);
}

TEST_F(SearchSliceCacheManagerTest, EvictsLeastRecentlyCreatedOrRefreshedEntryAndPreservesExistingData) {
    auto &manager = SearchSliceCacheManager::Instance();
    for (int i = 0; i < 5; ++i) {
        auto guard = manager.getOrCreate(std::to_string(i));
        guard->rankId = std::to_string(i);
    }
    EXPECT_EQ(manager.size(), 5U);
    {
        auto guard = manager.getOrCreate("0");
        EXPECT_EQ(guard->rankId, "0");
    }
    {
        auto guard = manager.getOrCreate("5");
        EXPECT_TRUE(static_cast<bool>(guard));
    }
    EXPECT_FALSE(manager.exists("1"));
    EXPECT_TRUE(manager.exists("0"));
    EXPECT_TRUE(manager.exists("5"));
    EXPECT_EQ(manager.size(), 5U);
    {
        auto missing = manager.get("missing");
        EXPECT_FALSE(static_cast<bool>(missing));
    }
    manager.clear("0");
    manager.clear("missing");
    EXPECT_FALSE(manager.exists("0"));
    EXPECT_EQ(manager.size(), 4U);
    manager.clearAll();
    EXPECT_EQ(manager.size(), 0U);
}
