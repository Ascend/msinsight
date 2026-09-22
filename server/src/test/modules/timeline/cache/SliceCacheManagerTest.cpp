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
#include "DominQuery.h"
#include "SliceCacheManager.h"

using namespace Dic::Module::Timeline;

class SliceCacheManagerTest : public ::testing::Test {
  protected:
    void SetUp() override { SliceCacheManager::Instance().Clear(); }
    void TearDown() override { SliceCacheManager::Instance().Clear(); }
};

TEST_F(SliceCacheManagerTest, SliceCacheHitPreservesPersistedDepthAndHonorsPageCoverage) {
    SliceQuery cachedPage;
    cachedPage.rankId = "cache_depth_rank";
    cachedPage.startTime = 100;
    cachedPage.endTime = 300;
    std::vector<SliceDomain> storedSlices = {
        SliceDomain{1, 110, 160, 7, ""},
        SliceDomain{2, 170, 240, 3, ""},
    };

    auto &cache = SliceCacheManager::Instance();
    cache.UpdateSliceCache("42", storedSlices, cachedPage);

    SliceQuery coveredQuery = cachedPage;
    coveredQuery.startTime = 120;
    coveredQuery.endTime = 220;
    const auto cachedSlices = cache.GetSliceDomainVec("42", cachedPage.rankId, coveredQuery);
    ASSERT_EQ(cachedSlices.size(), 2);
    EXPECT_EQ(cachedSlices[0].depth, 7);
    EXPECT_EQ(cachedSlices[1].depth, 3);

    SliceQuery uncoveredQuery = coveredQuery;
    uncoveredQuery.startTime = 99;
    EXPECT_TRUE(cache.GetSliceDomainVec("42", cachedPage.rankId, uncoveredQuery).empty());
}

TEST_F(SliceCacheManagerTest, SliceCacheReplacementAndClearKeepOrdinaryCacheBehavior) {
    SliceQuery query;
    query.rankId = "cache_replace_rank";
    query.startTime = 0;
    query.endTime = 100;
    auto &cache = SliceCacheManager::Instance();
    cache.UpdateSliceCache("8", {SliceDomain{1, 0, 50, 2, ""}}, query);
    cache.UpdateSliceCache("8", {SliceDomain{2, 10, 60, 9, ""}}, query);

    const auto replacedSlices = cache.GetSliceDomainVec("8", query.rankId, query);
    ASSERT_EQ(replacedSlices.size(), 1);
    EXPECT_EQ(replacedSlices[0].id, 2);
    EXPECT_EQ(replacedSlices[0].depth, 9);

    cache.Clear();
    EXPECT_TRUE(cache.GetSliceDomainVec("8", query.rankId, query).empty());
}
