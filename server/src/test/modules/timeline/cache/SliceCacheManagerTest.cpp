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
#include <atomic>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include "DominQuery.h"
#include "SpinLockGuard.h"
#define private public
#include "SliceCacheManager.h"
#undef private

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

TEST_F(SliceCacheManagerTest, ReplacementCountsActualRowsAndClearResetsCapacity) {
    auto &cache = SliceCacheManager::Instance();
    SliceQuery query;
    query.rankId = "capacity";
    query.endTime = 100;
    cache.UpdateSliceCache("1", {SliceDomain{1, 0, 10, 0, ""}}, query);
    EXPECT_EQ(cache.curCapacity, 1);
    cache.UpdateSliceCache("1", {SliceDomain{2, 0, 10, 0, ""}, SliceDomain{3, 10, 20, 1, ""}}, query);
    EXPECT_EQ(cache.curCapacity, 2);
    cache.Clear();
    EXPECT_EQ(cache.curCapacity, 0);
}

TEST_F(SliceCacheManagerTest, SnapshotReadsReuseStorageAndLegacyCopiesRemainIndependent) {
    static_assert(std::is_const_v<SliceCacheManager::Snapshot::element_type>);
    SliceQuery query;
    query.rankId = "immutable";
    query.endTime = 100;
    auto &cache = SliceCacheManager::Instance();
    std::vector<SliceDomain> rows{SliceDomain{1, 0, 10, 3, "group"}};
    cache.UpdateSliceCache("1", rows, query);
    rows[0].id = 99;
    auto a = cache.GetSliceSnapshot("1", query.rankId, query);
    auto b = cache.GetSliceSnapshot("1", query.rankId, query);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a->data(), b->data());
    auto writable = cache.GetSliceDomainVec("1", query.rankId, query);
    writable[0].depth = 99;
    writable[0].groupId = "changed";
    EXPECT_EQ((*a)[0].id, 1);
    EXPECT_EQ((*a)[0].depth, 3);
    EXPECT_EQ((*a)[0].groupId, "group");
}

TEST_F(SliceCacheManagerTest, MovePublicationCompactsCapacityWithoutChangingRows) {
    SliceQuery query;
    query.rankId = "moved";
    query.endTime = 100;
    auto &cache = SliceCacheManager::Instance();
    uint64_t generation;
    EXPECT_EQ(cache.GetSliceSnapshot("1", query.rankId, query, &generation), nullptr);
    std::vector<SliceDomain> rows;
    rows.reserve(4096);
    rows.push_back(SliceDomain{1, 0, 10, 3, std::string(100, 'g')});
    rows.push_back(SliceDomain{2, 10, 30, 8, "another group"});
    auto snapshot = cache.PublishSliceCache("1", std::move(rows), query, generation);
    ASSERT_EQ(snapshot->size(), 2);
    EXPECT_EQ(snapshot->capacity(), snapshot->size());
    EXPECT_EQ((*snapshot)[0].groupId, std::string(100, 'g'));
    EXPECT_EQ((*snapshot)[1].id, 2);
    EXPECT_EQ((*snapshot)[1].depth, 8);
    EXPECT_EQ(cache.GetSliceSnapshot("1", query.rankId, query), snapshot);
}

TEST_F(SliceCacheManagerTest, PinnedSnapshotsSurviveReplacementClearAndSourceIsolation) {
    SliceQuery a;
    a.rankId = "shared-rank";
    a.dbPath = "a.db";
    a.endTime = 100;
    auto b = a;
    b.dbPath = "b.db";
    auto &cache = SliceCacheManager::Instance();
    cache.UpdateSliceCache("7", {SliceDomain{1, 0, 10, 3, "old"}}, a);
    auto old = cache.GetSliceSnapshot("7", a.GetDataSourceId(), a);
    cache.UpdateSliceCache("7", {SliceDomain{2, 0, 10, 8, "new"}}, a);
    cache.UpdateSliceCache("7", {SliceDomain{3, 0, 10, 9, "other"}}, b);
    auto replacement = cache.GetSliceSnapshot("7", a.GetDataSourceId(), a);
    auto other = cache.GetSliceSnapshot("7", b.GetDataSourceId(), b);
    ASSERT_NE(old, nullptr);
    ASSERT_NE(replacement, nullptr);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ((*old)[0].id, 1);
    EXPECT_EQ((*replacement)[0].id, 2);
    EXPECT_EQ((*other)[0].id, 3);
    cache.Clear();
    EXPECT_EQ(cache.GetSliceSnapshot("7", a.GetDataSourceId(), a), nullptr);
    EXPECT_EQ((*old)[0].groupId, "old");
    EXPECT_EQ((*replacement)[0].depth, 8);
    EXPECT_EQ((*other)[0].depth, 9);
}

TEST_F(SliceCacheManagerTest, AColdResultFromBeforeClearCannotOverwriteTheNewGeneration) {
    auto &cache = SliceCacheManager::Instance();
    SliceQuery query;
    query.rankId = "cold-generation";
    query.endTime = 100;
    uint64_t oldGeneration;
    cache.GetSliceSnapshot("1", query.rankId, query, &oldGeneration);
    cache.Clear();
    uint64_t newGeneration;
    cache.GetSliceSnapshot("1", query.rankId, query, &newGeneration);
    EXPECT_NE(oldGeneration, newGeneration);
    cache.PublishSliceCache("1", {SliceDomain{2, 0, 10, 2, "new"}}, query, newGeneration);
    auto old = cache.PublishSliceCache("1", {SliceDomain{1, 0, 10, 1, "old"}}, query, oldGeneration);
    EXPECT_EQ((*old)[0].id, 1); // The original request may still safely finish with its own data.
    EXPECT_EQ((*cache.GetSliceSnapshot("1", query.rankId, query))[0].id, 2);
    EXPECT_EQ(cache.curCapacity, 1);
}

TEST_F(SliceCacheManagerTest, SnapshotCoverageAndEmptyPublicationKeepExistingMissRules) {
    auto &cache = SliceCacheManager::Instance();
    SliceQuery page;
    page.rankId = "coverage";
    page.startTime = 100;
    page.endTime = 300;
    cache.UpdateSliceCache("1", {SliceDomain{1, 110, 120, 4, ""}}, page);
    uint64_t generation;
    auto outside = page;
    outside.startTime = 99;
    EXPECT_EQ(cache.GetSliceSnapshot("1", page.rankId, outside, &generation), nullptr);
    auto empty = cache.PublishSliceCache("1", {}, outside, generation);
    EXPECT_TRUE(empty->empty());
    EXPECT_EQ((*cache.GetSliceSnapshot("1", page.rankId, page))[0].id, 1);
    auto allTime = page;
    allTime.startTime = allTime.endTime = 0;
    EXPECT_NE(cache.GetSliceSnapshot("1", page.rankId, allTime), nullptr);
}

TEST_F(SliceCacheManagerTest, SnapshotLookupTouchesLruAndEvictionDoesNotInvalidateReaders) {
    SliceCacheManager cache(2);
    SliceQuery query;
    query.rankId = "lru";
    query.endTime = 100;
    cache.UpdateSliceCache("a", {SliceDomain{1, 0, 10, 0, ""}}, query);
    cache.UpdateSliceCache("b", {SliceDomain{2, 0, 10, 0, ""}}, query);
    auto b = cache.GetSliceSnapshot("b", query.rankId, query);
    std::weak_ptr<const std::vector<SliceDomain>> weakB = b;
    cache.GetSliceSnapshot("a", query.rankId, query);
    cache.UpdateSliceCache("c", {SliceDomain{3, 0, 10, 0, ""}}, query);
    EXPECT_EQ(cache.GetSliceSnapshot("b", query.rankId, query), nullptr);
    EXPECT_EQ(cache.curCapacity, 2);
    EXPECT_EQ((*b)[0].id, 2);
    cache.Clear();
    EXPECT_EQ(cache.curCapacity, 0);
    EXPECT_FALSE(weakB.expired());
    b.reset();
    EXPECT_TRUE(weakB.expired());
}

TEST_F(SliceCacheManagerTest, ConcurrentReadersKeepCoherentRowsDuringReplaceAndClear) {
    SliceCacheManager cache;
    SliceQuery query;
    query.rankId = "concurrent";
    query.endTime = 100;
    cache.UpdateSliceCache("1", {SliceDomain{1, 0, 10, 1, "initial"}, SliceDomain{1001, 10, 20, 2, "initial"}}, query);
    std::atomic<bool> failed{false};
    std::atomic<bool> writerDone{false};
    std::atomic<size_t> readersReady{0};
    std::atomic<size_t> observations{0};
    const auto reader = [&]() {
        auto pinned = cache.GetSliceSnapshot("1", query.rankId, query);
        ++readersReady;
        for (size_t i = 0; i < 2000 || !writerDone; ++i) {
            auto snapshot = cache.GetSliceSnapshot("1", query.rankId, query);
            if (snapshot != nullptr) {
                ++observations;
            }
            if (snapshot != nullptr &&
                (snapshot->size() != 2 || (*snapshot)[1].id != (*snapshot)[0].id + 1000 ||
                    (*snapshot)[0].groupId != (*snapshot)[1].groupId)) {
                failed = true;
            }
        }
        if (pinned == nullptr || (*pinned)[0].groupId != "initial" || (*pinned)[1].id != 1001) {
            failed = true;
        }
    };
    std::thread a(reader);
    std::thread b(reader);
    while (readersReady < 2) {
        std::this_thread::yield();
    }
    for (uint64_t i = 1; i <= 200; ++i) {
        if (i % 7 == 0) {
            cache.Clear();
        }
        cache.UpdateSliceCache("1",
            {SliceDomain{i, 0, 10, 1, std::to_string(i)}, SliceDomain{i + 1000, 10, 20, 2, std::to_string(i)}}, query);
    }
    writerDone = true;
    a.join();
    b.join();
    EXPECT_FALSE(failed);
    EXPECT_GT(observations, 0);
}
