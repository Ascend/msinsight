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
TEST_F(SliceAnalyzerTest, test_ComputeSliceDomainVecAndSelfTimeByTimeRange_filter_python_function) {
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2};
    SliceCacheFliterPythonMock();
    SliceCacheManager::Instance().PutPythonFunctionIdVec(std::to_string(sliceQuery.trackId), {1}, sliceQuery);
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
TEST_F(SliceAnalyzerTest, test_ComputeSliceDomainVecAndSelfTimeByTimeRange_not_filter_python_function) {
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2};
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
TEST_F(SliceAnalyzerTest, test_ComputeSelfTimeByTimeRange_cache_isExpire_not_filter_python_function) {
    // 对Repository进行mock数据
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override {
            QueryCompeteSliceVecByTimeRangeAndTrackId_mock(sliceQuery, sliceVec);
        }
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            QuerySimpleSliceWithOutNameByTrackId_mock(sliceQuery, sliceVec);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2};
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

TEST_F(SliceAnalyzerTest, test_ComputePythonFunctionSliceIdsWithoutFilterFlag) {
    class RepositoryMock : public Dic::Module::Timeline::TextRepository {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            QuerySimpleSliceWithOutNameByTrackId_mock(sliceQuery, sliceVec);
        }

        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) override { return 1; }

        void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override {
            sliceIds.emplace_back(2);
        }
    };
    std::shared_ptr<Dic::Module::Timeline::TextRepository> ptr = std::make_shared<RepositoryMock>();
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(ptr);
    SliceQuery sliceQuery = {3, 0, 23, 2};
    sliceQuery.isFilterPythonFunction = false;
    std::set<uint64_t> ids;
    uint64_t maxDepth = 0;
    std::map<uint64_t, uint32_t> depthMap;

    sliceAnalyzer.ComputePythonFunctionSliceIds(sliceQuery, ids, maxDepth, depthMap);

    EXPECT_EQ(ids, std::set<uint64_t>({2}));
    EXPECT_EQ(maxDepth, 1);
    CacheManager::Instance().ClearAll();
}
