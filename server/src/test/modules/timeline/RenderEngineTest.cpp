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
#include "DataEngine.h"
#include "TrackInfoManager.h"
#include "CacheManager.h"
#include "RenderEngine.h"
#include "DominQuery.h"

using namespace Dic::Module::Timeline;
class RenderEngineTest : public ::testing::Test {
    void SetUp() override {
        TrackInfoManager::Instance().Reset();
        CacheManager::Instance().ClearAll();
    }

    void TearDown() override {
        TrackInfoManager::Instance().Reset();
        CacheManager::Instance().ClearAll();
    }
};

/**
 * 根据时间点查询算子，名字存在，但没有算子信息
 */
TEST_F(RenderEngineTest, TestFindSliceByTimePointNormal) {
    const uint64_t expectTrackId = 8;
    const uint64_t expectId = 70;
    const uint32_t expectDepth = 2;
    class DataEngineMock : public DataEngine {
      public:
        bool QuerySliceByTimepointAndName(
            const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override {
            competeSliceDomain.trackId = expectTrackId;
            competeSliceDomain.id = expectId;
            return true;
        }
    };
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    std::vector<SliceDomain> sliceVec;
    SliceDomain sliceDomain1;
    sliceDomain1.id = expectId;
    sliceDomain1.depth = expectDepth;
    sliceVec.emplace_back(sliceDomain1);
    SliceQuery sliceQuery;
    sliceQuery.endTime = 3 * MINUTE_NS;
    sliceCacheManager.UpdateSliceCache("8", sliceVec, sliceQuery);
    RenderEngine renderEngine;
    std::shared_ptr<DataEngineMock> dataEngineMock = std::make_unique<DataEngineMock>();
    renderEngine.SetDataEngineInterface(dataEngineMock);
    CompeteSliceDomain slice = renderEngine.FindSliceByTimePoint("", "", 0, "TEXT");
    EXPECT_EQ(slice.depth, expectDepth);
}

/**
 * 根据时间点查询算子，查询返回 false，打印日志中的特殊字符转义
 */
TEST_F(RenderEngineTest, TestFindSliceByTimePointTypeWrong) {
    class DataEngineMock : public DataEngine {
      public:
        bool QuerySliceByTimepointAndName(
            const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override {
            return false;
        }
    };
    RenderEngine renderEngine;
    std::shared_ptr<DataEngineMock> dataEngineMock = std::make_unique<DataEngineMock>();
    renderEngine.SetDataEngineInterface(dataEngineMock);
    CompeteSliceDomain slice = renderEngine.FindSliceByTimePoint("", "AAA\n%\t\\", 0, "TEXT");
}

TEST_F(RenderEngineTest, QueryThreadDetailUsesDepthIndexForSelfTime) {
    class DataEngineMock : public DataEngine {
      public:
        bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override {
            competeSliceDomain.id = 1;
            competeSliceDomain.timestamp = 0;
            competeSliceDomain.endTime = 100;
            competeSliceDomain.name = "parent";
            return true;
        }

        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            (void)sliceQuery;
            (void)sliceVec;
            ADD_FAILURE() << "depth index hit should avoid fallback slice scan";
        }
    };

    SliceQuery cacheQuery;
    cacheQuery.rankId = "0";
    cacheQuery.startTime = 0;
    cacheQuery.endTime = 100;
    std::vector<SliceDomain> sliceVec = {
        SliceDomain{1, 0, 100, 0, ""},
        SliceDomain{2, 10, 120, 1, ""},
        SliceDomain{3, 50, 70, 1, ""},
        SliceDomain{4, 60, 65, 2, ""},
    };
    SliceCacheManager::Instance().UpdateSliceCache("8", sliceVec, cacheQuery);

    RenderEngine renderEngine;
    std::shared_ptr<DataEngineMock> dataEngineMock = std::make_unique<DataEngineMock>();
    renderEngine.SetDataEngineInterface(dataEngineMock);
    ThreadDetailParams request;
    request.id = "1";
    request.metaType = "TEXT";
    request.rankId = "0";
    UnitThreadDetailBody response;

    renderEngine.QueryThreadDetail(request, response, 8);

    EXPECT_EQ(response.data.duration, 100);
    EXPECT_EQ(response.data.selfTime, 10);
}
