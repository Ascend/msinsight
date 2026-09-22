/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
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
#include "SliceAnalyzer.h"
#include "CacheManager.h"

using namespace Dic::Module::Timeline;

class SliceAnalyzerTest : public ::testing::Test {
  protected:
    void SetUp() override { CacheManager::Instance().ClearAll(); }
    void TearDown() override { CacheManager::Instance().ClearAll(); }
};

TEST_F(SliceAnalyzerTest, ComputeScreenSliceIdsUsesPersistedOrdinaryDepthsAndMaxDepth) {
    class RepositoryMock : public TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &sliceVec) override {
            sliceVec = {
                SliceDomain{1, 0, 100, 4, "groupA"},
                SliceDomain{2, 10, 20, 1, ""},
                SliceDomain{3, 30, 40, 9, "pythonGroup"},
            };
        }

        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &) override { return 1; }

        void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override {
            EXPECT_EQ(sliceQuery.cat, "python_function");
            sliceIds = {3};
        }
    };

    SliceAnalyzer analyzer;
    analyzer.SetRepository(std::make_shared<RepositoryMock>());
    SliceQuery query;
    query.trackId = 1001;
    query.rankId = "persisted_ordinary";
    query.startTime = 0;
    query.endTime = 100;
    std::set<uint64_t> ids;
    uint64_t maxDepth = 0;
    std::map<uint64_t, uint32_t> depthMap;

    analyzer.ComputeScreenSliceIds(query, ids, maxDepth, depthMap);

    EXPECT_EQ(ids, std::set<uint64_t>({1, 2}));
    EXPECT_EQ(maxDepth, 5);
    EXPECT_EQ(depthMap[1], 4);
    EXPECT_EQ(depthMap[2], 1);
    EXPECT_EQ(depthMap.count(3), 0);
}

TEST_F(SliceAnalyzerTest, ComputePythonFunctionSliceIdsUsesIndependentPersistedDepths) {
    class RepositoryMock : public TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &sliceVec) override {
            sliceVec = {SliceDomain{1, 0, 100, 8, ""}};
        }

        bool QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            EXPECT_EQ(sliceQuery.cat, "python_function");
            sliceVec = {
                SliceDomain{2, 10, 50, 6, "pythonA"},
                SliceDomain{3, 20, 40, 2, "pythonB"},
            };
            return true;
        }

        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &) override { return 2; }

        void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override {
            EXPECT_EQ(sliceQuery.cat, "python_function");
            sliceIds = {2, 3};
        }
    };

    SliceAnalyzer analyzer;
    analyzer.SetRepository(std::make_shared<RepositoryMock>());
    SliceQuery query;
    query.trackId = 1002;
    query.rankId = "persisted_python_stack";
    query.startTime = 0;
    query.endTime = 100;
    std::set<uint64_t> ids;
    uint64_t maxDepth = 0;
    std::map<uint64_t, uint32_t> depthMap;

    analyzer.ComputePythonFunctionSliceIds(query, ids, maxDepth, depthMap);

    EXPECT_EQ(ids, std::set<uint64_t>({2, 3}));
    EXPECT_EQ(maxDepth, 7);
    EXPECT_EQ(depthMap[2], 6);
    EXPECT_EQ(depthMap[3], 2);
    EXPECT_EQ(depthMap.count(1), 0);
    const std::string statusKey = SliceCacheManager::BuildPythonFunctionCacheKey(query.rankId, query.trackId);
    EXPECT_EQ(SliceCacheManager::Instance().GetPythonFunctionStatus(statusKey), PYTHON_FUNCTION_STATUS::EXIST);
}

TEST_F(SliceAnalyzerTest, OrdinaryCacheDoesNotMaskPythonStackLane) {
    class RepositoryMock : public TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &sliceVec) override {
            ++ordinaryQueryCount;
            sliceVec = {SliceDomain{1, 0, 100, 4, ""}};
        }

        bool QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            ++pythonStackQueryCount;
            EXPECT_EQ(sliceQuery.cat, "python_function");
            sliceVec = {SliceDomain{2, 10, 90, 6, ""}};
            return true;
        }

        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &) override { return 1; }

        void QuerySliceIdsByCat(const SliceQuery &, std::vector<uint64_t> &sliceIds) override { sliceIds = {2}; }

        int ordinaryQueryCount = 0;
        int pythonStackQueryCount = 0;
    };

    auto repository = std::make_shared<RepositoryMock>();
    SliceAnalyzer analyzer;
    analyzer.SetRepository(repository);
    SliceQuery query;
    query.trackId = 1008;
    query.rankId = "independent_pytorch_lanes";
    query.startTime = 0;
    query.endTime = 100;

    std::set<uint64_t> ordinaryIds;
    uint64_t ordinaryMaxDepth = 0;
    std::map<uint64_t, uint32_t> ordinaryDepthMap;
    analyzer.ComputeScreenSliceIds(query, ordinaryIds, ordinaryMaxDepth, ordinaryDepthMap);

    std::set<uint64_t> pythonStackIds;
    uint64_t pythonStackMaxDepth = 0;
    std::map<uint64_t, uint32_t> pythonStackDepthMap;
    analyzer.ComputePythonFunctionSliceIds(query, pythonStackIds, pythonStackMaxDepth, pythonStackDepthMap);

    EXPECT_EQ(ordinaryIds, std::set<uint64_t>({1}));
    EXPECT_EQ(ordinaryMaxDepth, 5);
    EXPECT_EQ(ordinaryDepthMap[1], 4);
    EXPECT_EQ(pythonStackIds, std::set<uint64_t>({2}));
    EXPECT_EQ(pythonStackMaxDepth, 7);
    EXPECT_EQ(pythonStackDepthMap[2], 6);
    EXPECT_EQ(repository->ordinaryQueryCount, 1);
    EXPECT_EQ(repository->pythonStackQueryCount, 1);
}

TEST_F(SliceAnalyzerTest, ComputeScreenSliceIdsCacheHitKeepsPersistedDepth) {
    class RepositoryMock : public TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &sliceVec) override {
            ++queryCount;
            sliceVec = {SliceDomain{1, 0, 10, 3, ""}, SliceDomain{2, 20, 30, 5, ""}};
        }

        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &) override { return 0; }

        int queryCount = 0;
    };

    auto repository = std::make_shared<RepositoryMock>();
    SliceAnalyzer analyzer;
    analyzer.SetRepository(repository);
    SliceQuery query;
    query.trackId = 1003;
    query.rankId = "persisted_cache_hit";
    query.startTime = 0;
    query.endTime = 30;
    std::set<uint64_t> firstIds;
    std::set<uint64_t> secondIds;
    uint64_t firstMaxDepth = 0;
    uint64_t secondMaxDepth = 0;
    std::map<uint64_t, uint32_t> firstDepthMap;
    std::map<uint64_t, uint32_t> secondDepthMap;

    analyzer.ComputeScreenSliceIds(query, firstIds, firstMaxDepth, firstDepthMap);
    analyzer.ComputeScreenSliceIds(query, secondIds, secondMaxDepth, secondDepthMap);

    EXPECT_EQ(repository->queryCount, 1);
    EXPECT_EQ(secondIds, firstIds);
    EXPECT_EQ(secondDepthMap, firstDepthMap);
    EXPECT_EQ(secondMaxDepth, firstMaxDepth);
    EXPECT_EQ(secondDepthMap[1], 3);
    EXPECT_EQ(secondDepthMap[2], 5);
    EXPECT_EQ(secondMaxDepth, 6);
}

TEST_F(SliceAnalyzerTest, ComputeSmallScreenIdsPreservesPersistedGroupDepths) {
    std::vector<SliceDomain> slices = {
        SliceDomain{1, 0, 10, 3, "groupA"},
        SliceDomain{2, 12, 20, 3, "groupA"},
        SliceDomain{3, 5, 15, 5, "groupB"},
        SliceDomain{4, 8, 9, 7, "pythonGroup"},
    };
    std::vector<DepthHelper> endList;

    auto ids = SliceAnalyzer::ComputeSmallScreenIds(0, 50, slices, endList, {4});

    EXPECT_EQ(ids, (std::set<std::pair<uint64_t, uint32_t>>{{1, 3}, {2, 3}, {3, 5}}));
    EXPECT_EQ(endList.size(), 6);
    EXPECT_EQ(slices[0].depth, 3);
    EXPECT_EQ(slices[1].depth, 3);
    EXPECT_EQ(slices[2].depth, 5);
    EXPECT_EQ(slices[3].depth, 7);
}

TEST_F(SliceAnalyzerTest, ComputeSelfTimeUsesPersistedOrdinaryDepthWithoutSimpleSliceQuery) {
    class RepositoryMock : public TextRepository {
      public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &, std::vector<CompeteSliceDomain> &sliceVec) override {
            sliceVec = {
                CompeteSliceDomain{1, 0, 100, 100, 4, "parent"},
                CompeteSliceDomain{2, 10, 30, 40, 5, "child"},
                CompeteSliceDomain{3, 20, 10, 30, 8, "python"},
            };
        }

        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &) override {
            ADD_FAILURE() << "self-time must not rebuild depth from simple slices";
        }

        void QuerySliceIdsByCat(const SliceQuery &, std::vector<uint64_t> &sliceIds) override { sliceIds = {3}; }
    };

    SliceAnalyzer analyzer;
    analyzer.SetRepository(std::make_shared<RepositoryMock>());
    SliceQuery query;
    query.trackId = 1004;
    query.rankId = "persisted_self_time";
    query.startTime = 0;
    query.endTime = 100;
    std::vector<CompeteSliceDomain> slices;
    std::map<std::string, uint64_t> selfTime;

    analyzer.ComputeSliceDomainVecAndSelfTimeByTimeRange(query, slices, selfTime);

    ASSERT_EQ(slices.size(), 2);
    EXPECT_EQ(slices[0].depth, 4);
    EXPECT_EQ(slices[1].depth, 5);
    EXPECT_EQ(selfTime["parent"], 70);
    EXPECT_EQ(selfTime["child"], 30);
}

TEST_F(SliceAnalyzerTest, ComputePythonStackSelfTimeUsesIndependentPersistedDepth) {
    class RepositoryMock : public TextRepository {
      public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &, std::vector<CompeteSliceDomain> &sliceVec) override {
            sliceVec = {
                CompeteSliceDomain{1, 0, 100, 100, 1, "ordinary"},
                CompeteSliceDomain{2, 0, 100, 100, 7, "python_parent"},
                CompeteSliceDomain{3, 10, 30, 40, 8, "python_child"},
            };
        }

        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &) override {
            ADD_FAILURE() << "Python Stack self-time must not rebuild depth";
        }

        void QuerySliceIdsByCat(const SliceQuery &, std::vector<uint64_t> &sliceIds) override { sliceIds = {2, 3}; }
    };

    SliceAnalyzer analyzer;
    analyzer.SetRepository(std::make_shared<RepositoryMock>());
    SliceQuery query;
    query.trackId = 1005;
    query.rankId = "persisted_python_self_time";
    query.startTime = 0;
    query.endTime = 100;
    std::vector<CompeteSliceDomain> slices;
    std::map<std::string, uint64_t> selfTime;

    analyzer.ComputeSliceDomainVecAndSelfTimeByTimeRange(query, slices, selfTime, true);

    ASSERT_EQ(slices.size(), 2);
    EXPECT_EQ(slices[0].depth, 7);
    EXPECT_EQ(slices[1].depth, 8);
    EXPECT_EQ(selfTime["python_parent"], 70);
    EXPECT_EQ(selfTime["python_child"], 30);
}

TEST_F(SliceAnalyzerTest, ComputePythonFunctionSliceVecByTimeRangeKeepsPersistedDepth) {
    class RepositoryMock : public TextRepository {
      public:
        bool QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            EXPECT_EQ(sliceQuery.cat, "python_function");
            sliceVec = {
                SliceDomain{2, 20, 30, 7, ""},
                SliceDomain{1, 0, 100, 5, ""},
            };
            return true;
        }

        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &, std::vector<SliceDomain> &) override {
            ADD_FAILURE() << "range query should avoid the simple-slice fallback";
        }
    };

    SliceAnalyzer analyzer;
    analyzer.SetRepository(std::make_shared<RepositoryMock>());
    SliceQuery query;
    query.startTime = 0;
    query.endTime = 100;
    std::vector<SliceDomain> slices;

    analyzer.ComputePythonFunctionSliceVecByTimeRange(query, slices);

    ASSERT_EQ(slices.size(), 2);
    EXPECT_EQ(slices[0].id, 1);
    EXPECT_EQ(slices[0].depth, 5);
    EXPECT_EQ(slices[1].id, 2);
    EXPECT_EQ(slices[1].depth, 7);
}

TEST_F(SliceAnalyzerTest, PythonFunctionCacheSeparatesRanksWithSameTrackId) {
    auto &cache = SliceCacheManager::Instance();
    const uint64_t trackId = 1007;
    SliceQuery rankAQuery;
    rankAQuery.rankId = "rank_a";
    rankAQuery.trackId = trackId;
    rankAQuery.startTime = 0;
    rankAQuery.endTime = 100;
    SliceQuery rankBQuery = rankAQuery;
    rankBQuery.rankId = "rank_b";
    const std::string rankAKey = SliceCacheManager::BuildPythonFunctionCacheKey(rankAQuery.rankId, trackId);
    const std::string rankBKey = SliceCacheManager::BuildPythonFunctionCacheKey(rankBQuery.rankId, trackId);

    cache.PutPythonFunctionIdVec(rankAKey, {1}, rankAQuery);
    cache.PutPythonFunctionIdVec(rankBKey, {2}, rankBQuery);
    cache.SetPythonFunctionStatus(rankAKey, PYTHON_FUNCTION_STATUS::EXIST);
    cache.SetPythonFunctionStatus(rankBKey, PYTHON_FUNCTION_STATUS::NOT_EXIST);

    EXPECT_EQ(cache.GetPythonFunctionIdVec(rankAKey, rankAQuery), std::vector<uint64_t>({1}));
    EXPECT_EQ(cache.GetPythonFunctionIdVec(rankBKey, rankBQuery), std::vector<uint64_t>({2}));
    EXPECT_EQ(cache.GetPythonFunctionStatus(rankAKey), PYTHON_FUNCTION_STATUS::EXIST);
    EXPECT_EQ(cache.GetPythonFunctionStatus(rankBKey), PYTHON_FUNCTION_STATUS::NOT_EXIST);
}
