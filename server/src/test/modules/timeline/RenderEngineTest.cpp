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
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include "DataEngine.h"
#include "TrackInfoManager.h"
#include "CacheManager.h"
#include "RenderEngine.h"
#include "DominQuery.h"

using namespace Dic::Module::Timeline;
class RenderEngineTest : public ::testing::Test {
  protected:
    void SetUp() override {
        DataBaseManager::Instance().Clear();
        TrackInfoManager::Instance().Reset();
        CacheManager::Instance().ClearAll();
    }

    void TearDown() override {
        DataBaseManager::Instance().Clear();
        for (const auto &path : tempDbPaths) {
            for (const auto &suffix : {"", "-shm", "-wal"}) {
                const std::string file = path + suffix;
                std::remove(file.c_str());
            }
        }
        TrackInfoManager::Instance().Reset();
        CacheManager::Instance().ClearAll();
    }

    std::vector<std::string> tempDbPaths;
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

TEST_F(RenderEngineTest, QueryThreadDetailUsesGlobalCommunicationRankForMultiHostMsprof) {
    constexpr uint64_t trackId = 8;
    constexpr uint64_t sliceId = 1;
    constexpr uint64_t sliceStartNs = 1000000;
    constexpr uint64_t sliceEndNs = sliceStartNs + 100;
    const std::string opName = "hcom_allReduce_multi_host";
    const std::string timelineRankId = "hostB222_0 0";
    const std::string clusterProjectPath = "multi-host-msprof";
    const std::string uniqueId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string traceDbPath =
        FileUtil::SplicePath(::testing::TempDir(), "RenderEngineTest_trace_" + uniqueId + ".db");
    const std::string clusterDbPath =
        FileUtil::SplicePath(::testing::TempDir(), "RenderEngineTest_cluster_" + uniqueId + ".db");
    tempDbPaths = {traceDbPath, clusterDbPath};

    class DataEngineMock : public DataEngine {
      public:
        bool QuerySliceDetailInfo(const SliceQuery &, CompeteSliceDomain &slice) override {
            slice.id = id;
            slice.timestamp = startTime;
            slice.endTime = endTime;
            slice.name = opName;
            slice.isCommunicationGroup = true;
            return true;
        }

        uint64_t id = 0;
        uint64_t startTime = 0;
        uint64_t endTime = 0;
        std::string opName;
    };

    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.SetDataType(DataType::DB, traceDbPath);
    databaseManager.SetFileType(FileType::MS_PROF, traceDbPath);
    ASSERT_TRUE(databaseManager.CreateTraceConnectionPool(timelineRankId, traceDbPath));
    auto traceDatabase = databaseManager.GetTraceDatabaseByRankId(timelineRankId);
    ASSERT_NE(traceDatabase, nullptr);
    ASSERT_TRUE(traceDatabase->ExecSql("CREATE TABLE HOST_INFO(hostUid INTEGER, hostName TEXT);"
                                       "INSERT INTO HOST_INFO VALUES (222, 'hostB');"));
    traceDatabase.reset();
    databaseManager.CreateClusterConnectionPool(clusterProjectPath, clusterDbPath, DataType::DB);
    auto clusterDatabase = databaseManager.GetClusterDatabase(clusterProjectPath);
    ASSERT_NE(clusterDatabase, nullptr);
    ASSERT_TRUE(clusterDatabase->ExecSql(
        "CREATE TABLE HostInfo(hostUid INTEGER, hostName TEXT);"
        "CREATE TABLE RankDeviceMap(rankId INTEGER, deviceId INTEGER, hostUid INTEGER);"
        "CREATE TABLE ClusterCommunicationTime(step TEXT, rank_id INTEGER, hccl_op_name TEXT, group_name TEXT, "
        "transit_time NUMERIC, wait_time NUMERIC);"
        "CREATE TABLE ClusterCommunicationBandwidth(step TEXT, rank_id INTEGER, hccl_op_name TEXT, "
        "group_name TEXT, band_type TEXT, transit_size NUMERIC, transit_time NUMERIC, bandwidth NUMERIC);"
        "INSERT INTO HostInfo VALUES (111, 'hostA'), (222, 'hostB');"
        "INSERT INTO RankDeviceMap VALUES (0, 0, 111), (8, 0, 222);"
        "INSERT INTO ClusterCommunicationTime VALUES "
        "('step1', 0, 'hcom_allReduce_multi_host', 'group', 1, 2), "
        "('step1', 8, 'hcom_allReduce_multi_host', 'group', 8, 9);"
        "INSERT INTO ClusterCommunicationBandwidth VALUES "
        "('step1', 0, 'hcom_allReduce_multi_host', 'group', 'HCCS', 1, 1, 1), "
        "('step1', 8, 'hcom_allReduce_multi_host', 'group', 'RDMA', 8, 8, 8);"));
    // Return the initialized connection to the pool so QueryThreadDetail reuses it.
    clusterDatabase.reset();

    TrackInfoManager::Instance().UpdateClusterDbToFileIdMap(clusterProjectPath, traceDbPath);
    TrackInfoManager::Instance().SetRankListByFileId(traceDbPath, {"cluster", "hostB222_0 ", timelineRankId, "0", "0"});

    SliceQuery cacheQuery;
    cacheQuery.rankId = timelineRankId;
    cacheQuery.startTime = sliceStartNs;
    cacheQuery.endTime = sliceEndNs;
    SliceCacheManager::Instance().UpdateSliceCache(
        std::to_string(trackId), {SliceDomain{sliceId, sliceStartNs, sliceEndNs, 0, ""}}, cacheQuery);

    RenderEngine renderEngine;
    auto dataEngine = std::make_shared<DataEngineMock>();
    dataEngine->id = sliceId;
    dataEngine->startTime = sliceStartNs;
    dataEngine->endTime = sliceEndNs;
    dataEngine->opName = opName;
    renderEngine.SetDataEngineInterface(dataEngine);
    ThreadDetailParams request;
    request.id = std::to_string(sliceId);
    request.metaType = "TEXT";
    request.rankId = timelineRankId;
    UnitThreadDetailBody response;

    renderEngine.QueryThreadDetail(request, response, trackId);

    // RankDeviceMap identifies hostB/device0 as global communication rank 8. Rank 0 belongs to hostA.
    ASSERT_TRUE(response.data.transitTime.has_value());
    ASSERT_TRUE(response.data.waitTime.has_value());
    EXPECT_DOUBLE_EQ(response.data.transitTime.value(), 8);
    EXPECT_DOUBLE_EQ(response.data.waitTime.value(), 9);
    ASSERT_EQ(response.data.communicationBandwidthInfo.size(), 1);
    EXPECT_EQ(response.data.communicationBandwidthInfo[0].transportType, "RDMA");
    EXPECT_DOUBLE_EQ(response.data.communicationBandwidthInfo[0].transitSize, 8);

    clusterDatabase = databaseManager.GetClusterDatabase(clusterProjectPath);
    ASSERT_NE(clusterDatabase, nullptr);
    ASSERT_TRUE(clusterDatabase->ExecSql("UPDATE HostInfo SET hostName = 'unknownHost' WHERE hostUid = 222;"));
    clusterDatabase.reset();

    UnitThreadDetailBody missingHostMappingResponse;
    renderEngine.QueryThreadDetail(request, missingHostMappingResponse, trackId);

    // A missing explicit host/device mapping must not fall back to local device rank 0.
    EXPECT_FALSE(missingHostMappingResponse.data.transitTime.has_value());
    EXPECT_FALSE(missingHostMappingResponse.data.waitTime.has_value());
    EXPECT_TRUE(missingHostMappingResponse.data.communicationBandwidthInfo.empty());

    clusterDatabase = databaseManager.GetClusterDatabase(clusterProjectPath);
    ASSERT_NE(clusterDatabase, nullptr);
    ASSERT_TRUE(clusterDatabase->ExecSql("UPDATE HostInfo SET hostName = 'hostB' WHERE hostUid = 222;"
                                         "INSERT INTO RankDeviceMap VALUES (9, 0, 222);"));
    clusterDatabase.reset();

    UnitThreadDetailBody ambiguousMappingResponse;
    renderEngine.QueryThreadDetail(request, ambiguousMappingResponse, trackId);

    // Never fall back to local device rank 0 when the explicit host/device mapping is not unique.
    EXPECT_FALSE(ambiguousMappingResponse.data.transitTime.has_value());
    EXPECT_FALSE(ambiguousMappingResponse.data.waitTime.has_value());
    EXPECT_TRUE(ambiguousMappingResponse.data.communicationBandwidthInfo.empty());
}
