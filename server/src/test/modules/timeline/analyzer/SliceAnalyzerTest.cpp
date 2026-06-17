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
#include "SliceAnalyzer_mock_data.h"
#include "SliceAnalyzer.h"
#include "CacheManager.h"
using namespace Dic::TimeLine::SliceAnalyzer::Mock;
class SliceAnalyzerTest : public ::testing::Test {};
/**
 * 测试过滤pf的框选功能
 */
TEST_F(SliceAnalyzerTest, test_ComputeSliceDomainVecAndSelfTimeByTimeRange_filter_python_function)
{
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
    public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(const SliceQuery &sliceQuery,
            std::vector<CompeteSliceDomain> &sliceVec) override
        {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = { 3, 0, 23, 2 };
    SliceCacheFliterPythonMock();
    SliceCacheManager::Instance().PutPythonFunctionIdVec(std::to_string(sliceQuery.trackId), { 1 }, sliceQuery);
    SliceCacheManager::Instance().UpdatePythonFilterSet(std::to_string(sliceQuery.trackId), true);
    std::vector<CompeteSliceDomain> sliceDomainVec;
    std::map<std::string, uint64_t> selfTimeKeyValue;
    sliceAnalyzer.ComputeSliceDomainVecAndSelfTimeByTimeRange(sliceQuery, sliceDomainVec, selfTimeKeyValue);
    const uint64_t expectSize = 9;
    const uint64_t expectSlice2SelfTime = 6;
    const uint64_t expectSlice3SelfTime = 4;
    EXPECT_EQ(sliceDomainVec.size(), expectSize);
    EXPECT_EQ(sliceDomainVec.begin()->name, "slice2");
    EXPECT_EQ(selfTimeKeyValue["slice2"], expectSlice2SelfTime);
    EXPECT_EQ(selfTimeKeyValue["slice3"], expectSlice3SelfTime);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试不过滤过滤pf的框选功能
 */
TEST_F(SliceAnalyzerTest, test_ComputeSliceDomainVecAndSelfTimeByTimeRange_not_filter_python_function)
{
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
    public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(const SliceQuery &sliceQuery,
            std::vector<CompeteSliceDomain> &sliceVec) override
        {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2 };
    SliceCacheNotFliterPythonMock();
    std::vector<CompeteSliceDomain> sliceDomainVec;
    std::map<std::string, uint64_t> selfTimeKeyValue;
    sliceAnalyzer.ComputeSliceDomainVecAndSelfTimeByTimeRange(sliceQuery, sliceDomainVec, selfTimeKeyValue);
    const uint64_t expectSize = 10;
    const uint64_t expectSlice1SelfTime = 6;
    const uint64_t expectSlice2SelfTime = 6;
    const uint64_t expectSlice3SelfTime = 4;
    EXPECT_EQ(sliceDomainVec.size(), expectSize);
    EXPECT_EQ(sliceDomainVec.begin()->name, "slice1");
    EXPECT_EQ(selfTimeKeyValue["slice1"], expectSlice1SelfTime);
    EXPECT_EQ(selfTimeKeyValue["slice2"], expectSlice2SelfTime);
    EXPECT_EQ(selfTimeKeyValue["slice3"], expectSlice3SelfTime);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试简单算子过期后不过滤过滤pf的框选功能
 */
TEST_F(SliceAnalyzerTest, test_ComputeSelfTimeByTimeRange_cache_isExpire_not_filter_python_function)
{
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
    public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(const SliceQuery &sliceQuery,
            std::vector<CompeteSliceDomain> &sliceVec) override
        {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
        void QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &sliceQuery,
            std::vector<SliceDomain> &sliceVec) override
        {
            QuerySimpleSliceWithOutNameByTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2 };
    std::vector<CompeteSliceDomain> sliceDomainVec;
    std::map<std::string, uint64_t> selfTimeKeyValue;
    sliceAnalyzer.ComputeSliceDomainVecAndSelfTimeByTimeRange(sliceQuery, sliceDomainVec, selfTimeKeyValue);
    const uint64_t expectSize = 10;
    const uint64_t expectSlice1SelfTime = 6;
    const uint64_t expectSlice2SelfTime = 6;
    const uint64_t expectSlice3SelfTime = 4;
    EXPECT_EQ(sliceDomainVec.size(), expectSize);
    EXPECT_EQ(sliceDomainVec.begin()->name, "slice1");
    EXPECT_EQ(selfTimeKeyValue["slice1"], expectSlice1SelfTime);
    EXPECT_EQ(selfTimeKeyValue["slice2"], expectSlice2SelfTime);
    EXPECT_EQ(selfTimeKeyValue["slice3"], expectSlice3SelfTime);
    CacheManager::Instance().ClearAll();
}

TEST_F(SliceAnalyzerTest, test_ComputeDepthInfoByTrackId_group_id_depth) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, "groupA"},
                SliceDomain{2, 12, 20, 0, "groupA"},
                SliceDomain{3, 5, 15, 0, "groupB"},
                SliceDomain{4, 8, 9, 0, ""},
                SliceDomain{5, 20, 25, 0, ""},
                SliceDomain{6, 30, 40, 0, "dirtyGroup"},
                SliceDomain{7, 35, 45, 0, "dirtyGroup"},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1001;
    sliceQuery.rankId = "group_depth";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 0);
    EXPECT_EQ(depthInfo[3], 1);
    EXPECT_EQ(depthInfo[4], 2);
    EXPECT_EQ(depthInfo[5], 0);
    EXPECT_EQ(depthInfo[6], 0);
    EXPECT_EQ(depthInfo[7], 0);
    CacheManager::Instance().ClearAll();
}

TEST_F(SliceAnalyzerTest, test_ComputeDepthInfoByTrackId_group_id_filter_python_function) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 100, 0, "pythonGroup"},
                SliceDomain{2, 10, 20, 0, ""},
            };
        }

        void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override {
            (void)sliceQuery;
            sliceIds = {1};
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1002;
    sliceQuery.rankId = "group_depth_filter";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 100;
    sliceQuery.isFilterPythonFunction = true;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo.count(1), 0);
    EXPECT_EQ(depthInfo[2], 0);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试无 group_id 时保持原有 first-fit 深度分配行为
 */
TEST_F(SliceAnalyzerTest, test_NoGroupIdKeepOriginalDepth) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, ""},
                SliceDomain{2, 5, 15, 0, ""},
                SliceDomain{3, 12, 20, 0, ""},
                SliceDomain{4, 18, 25, 0, ""},
                SliceDomain{5, 30, 40, 0, ""},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1003;
    sliceQuery.rankId = "no_group_depth";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 1);
    EXPECT_EQ(depthInfo[3], 0);
    EXPECT_EQ(depthInfo[4], 1);
    EXPECT_EQ(depthInfo[5], 0);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试不同 group_id 且时间重叠时分配到不同 depth
 */
TEST_F(SliceAnalyzerTest, test_DifferentGroupIdOverlapUseDifferentDepth) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 20, 0, "groupA"},
                SliceDomain{2, 5, 25, 0, "groupB"},
                SliceDomain{3, 30, 40, 0, ""},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1004;
    sliceQuery.rankId = "different_group_overlap";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 1);
    EXPECT_EQ(depthInfo[3], 0);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试同一 group_id 内存在时间重叠脏数据时仍强制同 depth
 */
TEST_F(SliceAnalyzerTest, test_DirtySameGroupOverlapStillSameDepth) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 20, 0, "dirtyGroup"},
                SliceDomain{2, 10, 30, 0, "dirtyGroup"},
                SliceDomain{3, 5, 15, 0, "other"},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1005;
    sliceQuery.rankId = "dirty_group_overlap";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 0);
    EXPECT_EQ(depthInfo[3], 1);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试同一 group_id 在排序后形成连续 block，一次性分配同 depth
 */
TEST_F(SliceAnalyzerTest, test_ContiguousGroupBlockUseSameDepth) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, "blockGroup"},
                SliceDomain{2, 12, 20, 0, "blockGroup"},
                SliceDomain{3, 22, 30, 0, "blockGroup"},
                SliceDomain{4, 5, 25, 0, "other"},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1006;
    sliceQuery.rankId = "contiguous_block";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 0);
    EXPECT_EQ(depthInfo[3], 0);
    EXPECT_EQ(depthInfo[4], 1);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试空 group_id 算子与已分配 group 无时间冲突时可复用同一 depth
 */
TEST_F(SliceAnalyzerTest, test_UngroupedCanReuseGroupedDepthWhenNoOverlap) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, "reuseGroup"},
                SliceDomain{2, 20, 30, 0, "reuseGroup"},
                SliceDomain{3, 35, 45, 0, ""},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1007;
    sliceQuery.rankId = "ungrouped_reuse";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 0);
    EXPECT_EQ(depthInfo[3], 0);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试 group block 的包络区间阻止后续算子占用间隙深度
 * groupA 包含 [0,10) 和 [20,30)，合并后为 [0,30)。
 * 未分组算子 [12,18) 落在间隙中，应分配到不同 depth。
 */
TEST_F(SliceAnalyzerTest, test_GroupBlockEnvelopPreventsGapOccupancy) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, "gapGroup"},
                SliceDomain{2, 20, 30, 0, "gapGroup"},
                SliceDomain{3, 12, 18, 0, ""},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery;
    sliceQuery.trackId = 1010;
    sliceQuery.rankId = "gap_prevent";
    sliceQuery.startTime = 0;
    sliceQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthInfo);

    EXPECT_EQ(depthInfo[1], 0);
    EXPECT_EQ(depthInfo[2], 0);
    // [12,18) falls inside merged group envelope [0,30), must use different depth
    EXPECT_EQ(depthInfo[3], 1);
    CacheManager::Instance().ClearAll();
}

/**
 * 测试同一组数据分别走小屏深度计算和深度缓存，验证 id->depth 一致
 */
TEST_F(SliceAnalyzerTest, test_ResultIdsSmallScreenAndDepthCacheConsistent) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            sliceVec = {
                SliceDomain{1, 0, 10, 0, "groupA"},
                SliceDomain{2, 12, 20, 0, "groupA"},
                SliceDomain{3, 5, 15, 0, "groupB"},
                SliceDomain{4, 8, 9, 0, ""},
                SliceDomain{5, 20, 25, 0, ""},
            };
        }
    };
    CacheManager::Instance().ClearAll();
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);

    std::vector<SliceDomain> sliceDomain;
    ptr->QuerySimpleSliceWithOutNameByTrackId(SliceQuery{}, sliceDomain);
    SliceAnalyzer::SortByTimestampASC(sliceDomain);

    std::vector<uint64_t> pythonFunctionIds;
    std::vector<DepthHelper> endList;
    auto smallScreenIds = SliceAnalyzer::ComputeSmallScreenIds(0, 50, sliceDomain, endList, pythonFunctionIds);
    std::map<uint64_t, uint32_t> smallScreenDepthMap;
    for (const auto &item : smallScreenIds) {
        smallScreenDepthMap[item.first] = item.second;
    }

    SliceQuery depthQuery;
    depthQuery.trackId = 1008;
    depthQuery.rankId = "consistency_check";
    depthQuery.startTime = 0;
    depthQuery.endTime = 50;
    std::unordered_map<uint64_t, uint32_t> depthCacheInfo;
    sliceAnalyzer.ComputeDepthInfoByTrackId(depthQuery, depthCacheInfo);

    EXPECT_EQ(smallScreenDepthMap.size(), depthCacheInfo.size());
    for (const auto &pair : smallScreenDepthMap) {
        EXPECT_EQ(depthCacheInfo[pair.first], pair.second);
    }

    EXPECT_EQ(smallScreenDepthMap[1], 0);
    EXPECT_EQ(smallScreenDepthMap[2], 0);
    EXPECT_EQ(smallScreenDepthMap[3], 1);
    EXPECT_EQ(smallScreenDepthMap[4], 2);
    EXPECT_EQ(smallScreenDepthMap[5], 0);

    CacheManager::Instance().ClearAll();
}
