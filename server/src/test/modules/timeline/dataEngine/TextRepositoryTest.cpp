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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <limits>

#include "DataBaseManager.h"
#include "FileUtil.h"
#include "TestSuit.h"
#include "TextRepository.h"
#include "TrackInfoManager.h"

using namespace Dic::Module::Timeline;

namespace {
constexpr const char *TEST_RANK_ID = "text_repository_group_marker_test";
constexpr uint64_t GROUP_TRACK_ID = 100;
constexpr uint64_t PLANE_TRACK_ID = 101;

class TextRepositoryTest : public ::testing::Test {
  protected:
    static std::string testDbPath;
    static std::recursive_mutex testMutex;
    static Module::Database testDatabase;

    static void SetUpTestSuite() {
        testDbPath = TestSuit::GetTestDataFile("text_repository_group_marker_test.db");
        if (FileUtil::CheckFilePathExist(testDbPath)) {
            FileUtil::RemoveFile(testDbPath);
        }
        ASSERT_TRUE(testDatabase.OpenDb(testDbPath, false));
        DataBaseManager::Instance().SetDataType(DataType::TEXT, testDbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(TEST_RANK_ID, testDbPath));
        DataBaseManager::Instance().SetDbPathMapping(TEST_RANK_ID, testDbPath, "");
        ASSERT_TRUE(CreateSchema());
        ASSERT_TRUE(testDatabase.SetDataBaseVersion());
    }

    static void TearDownTestSuite() {
        DataBaseManager::Instance().ReleaseDatabaseByRankId(TEST_RANK_ID);
        testDatabase.CloseDb();
        if (FileUtil::CheckFilePathExist(testDbPath)) {
            FileUtil::RemoveFile(testDbPath);
        }
    }

    void SetUp() override {
        TrackInfoManager::Instance().Reset();
        TrackInfoManager::Instance().UpdateTrackIdMap(
            TEST_RANK_ID, {{GROUP_TRACK_ID, {"0", "900"}}, {PLANE_TRACK_ID, {"1", "900"}}});
        ASSERT_TRUE(testDatabase.ExecSql("DELETE FROM slice; DELETE FROM thread; DELETE FROM flow; "
                                         "DELETE FROM kernel_detail;"));
        ASSERT_TRUE(
            testDatabase.ExecSql("INSERT INTO thread(track_id, tid, pid, thread_name, thread_sort_index) VALUES "
                                 "(100, '0', '900', 'Group group_a Communication', 0), "
                                 "(101, '1', '900', 'Plane 0', 1);"));
        ASSERT_TRUE(testDatabase.ExecSql(
            "INSERT INTO slice(id, timestamp, duration, name, depth, track_id, cat, args, cname, end_time, "
            "flag_id, group_id) VALUES "
            "(1, 100, 200, 'hcom_allReduce__0_0_1', 7, 100, '', '{}', '', 300, '', ''), "
            "(2, 110, 50, 'Memcpy', 9, 101, '', '{}', '', 160, '', ''), "
            "(3, 120, 20, 'python_call', 13, 101, 'python_function', '{}', '', 140, '', '');"));
        ASSERT_TRUE(testDatabase.ExecSql("INSERT INTO flow(id, flow_id, name, cat, track_id, timestamp, type) VALUES "
                                         "(1, 'flow_1', 'flow', 'test_flow', 101, 120, 's');"));
    }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    static bool CreateSchema() {
        return testDatabase.ExecSql(
                   "CREATE TABLE slice (id INTEGER PRIMARY KEY, timestamp INTEGER, duration INTEGER, name TEXT, "
                   "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, "
                   "flag_id TEXT, group_id TEXT);") &&
            testDatabase.ExecSql(
                "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, thread_name TEXT, "
                "thread_sort_index INTEGER);") &&
            testDatabase.ExecSql(
                "CREATE TABLE flow (id INTEGER PRIMARY KEY, flow_id TEXT, name TEXT, cat TEXT, track_id INTEGER, "
                "timestamp INTEGER, type TEXT);") &&
            testDatabase.ExecSql(
                "CREATE TABLE kernel_detail (output_formats TEXT, input_shapes TEXT, input_data_types TEXT, "
                "input_formats TEXT, output_shapes TEXT, output_data_types TEXT, start_time INTEGER, name TEXT, "
                "accelerator_core TEXT);");
    }

    static CompeteSliceDomain QueryDetail(uint64_t trackId, const std::string &sliceId) {
        TextRepository repository;
        SliceQuery query;
        query.rankId = TEST_RANK_ID;
        query.trackId = trackId;
        query.sliceId = sliceId;
        CompeteSliceDomain detail;
        EXPECT_TRUE(repository.QuerySliceDetailInfo(query, detail));
        return detail;
    }
};

std::string TextRepositoryTest::testDbPath;
std::recursive_mutex TextRepositoryTest::testMutex;
Module::Database TextRepositoryTest::testDatabase(TextRepositoryTest::testMutex);

TEST_F(TextRepositoryTest, MarksOnlyGroupCommunicationTrackForCommunicationAnalysisEnhancement) {
    const CompeteSliceDomain groupDetail = QueryDetail(GROUP_TRACK_ID, "1");
    const CompeteSliceDomain planeDetail = QueryDetail(PLANE_TRACK_ID, "2");

    EXPECT_TRUE(groupDetail.isCommunicationGroup);
    EXPECT_FALSE(planeDetail.isCommunicationGroup);
}

TEST_F(TextRepositoryTest, ReturnsPersistedDepthForSimplePythonRangeIdsAndDetailQueries) {
    TextRepository repository;
    SliceQuery query;
    query.rankId = TEST_RANK_ID;
    query.trackId = GROUP_TRACK_ID;
    query.endTime = 1000;

    std::vector<SliceDomain> simpleSlices;
    repository.QuerySimpleSliceWithOutNameByTrackId(query, simpleSlices);
    ASSERT_EQ(simpleSlices.size(), 1);
    EXPECT_EQ(simpleSlices[0].depth, 7);

    query.trackId = PLANE_TRACK_ID;
    simpleSlices.clear();
    repository.QuerySimpleSliceWithOutNameByTrackId(query, simpleSlices);
    ASSERT_EQ(simpleSlices.size(), 1);
    EXPECT_EQ(simpleSlices[0].id, 2);
    EXPECT_EQ(simpleSlices[0].depth, 9);

    query.cat = "python_function";
    std::vector<SliceDomain> pythonSlices;
    ASSERT_TRUE(repository.QuerySliceByCatAndTimeRange(query, pythonSlices));
    ASSERT_EQ(pythonSlices.size(), 1);
    EXPECT_EQ(pythonSlices[0].depth, 13);

    std::vector<CompeteSliceDomain> rangedSlices;
    repository.QueryCompeteSliceVecByTimeRangeAndTrackId(query, rangedSlices);
    ASSERT_EQ(rangedSlices.size(), 2);
    EXPECT_EQ(rangedSlices[0].depth, 9);
    EXPECT_EQ(rangedSlices[1].depth, 13);

    std::vector<CompeteSliceDomain> slicesById;
    repository.QueryCompeteSliceByIds(query, {2}, slicesById);
    ASSERT_EQ(slicesById.size(), 1);
    EXPECT_EQ(slicesById[0].depth, 9);

    const CompeteSliceDomain detail = QueryDetail(PLANE_TRACK_ID, "3");
    EXPECT_EQ(detail.depth, 13);
}

TEST_F(TextRepositoryTest, FullRangeQueryWithTimestampOffsetDoesNotOverflow) {
    TextRepository repository;
    SliceQuery query;
    query.rankId = TEST_RANK_ID;
    query.trackId = PLANE_TRACK_ID;
    query.cat = "python_function";
    query.startTime = 0;
    query.endTime = std::numeric_limits<uint64_t>::max();
    query.minTimestamp = 100;

    std::vector<SliceDomain> pythonSlices;
    ASSERT_TRUE(repository.QuerySliceByCatAndTimeRange(query, pythonSlices));
    ASSERT_EQ(pythonSlices.size(), 1);
    EXPECT_EQ(pythonSlices[0].id, 3);

    FlowQuery flowQuery;
    flowQuery.fileId = TEST_RANK_ID;
    flowQuery.trackId = PLANE_TRACK_ID;
    flowQuery.startTime = 0;
    flowQuery.endTime = std::numeric_limits<uint64_t>::max();
    flowQuery.minTimestamp = 100;
    std::vector<FlowPoint> flowPoints;
    repository.QueryFlowPointByTimeRange(flowQuery, flowPoints);
    ASSERT_EQ(flowPoints.size(), 1);
    EXPECT_EQ(flowPoints[0].flowId, "flow_1");
}

TEST_F(TextRepositoryTest, QueryFlowPointByCategoryDefersPersistedDepthResolution) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "INSERT INTO slice(id, timestamp, duration, name, depth, track_id, cat, args, cname, end_time, flag_id, "
        "group_id) VALUES (4, 170, 20, 'next_slice', 11, 101, '', '{}', '', 190, '', '');"
        "INSERT INTO flow(id, flow_id, name, cat, track_id, timestamp, type) VALUES "
        "(2, 'flow_1', 'flow', 'test_flow', 101, 165, 'f');"));
    TextRepository repository;
    FlowQuery flowQuery;
    flowQuery.fileId = TEST_RANK_ID;
    flowQuery.cat = "test_flow";
    flowQuery.minTimestamp = 100;
    std::vector<FlowPoint> flowPoints;

    repository.QueryFlowPointByCategory(flowQuery, flowPoints);

    ASSERT_EQ(flowPoints.size(), 2);
    EXPECT_EQ(flowPoints[0].type, Protocol::LINE_START);
    EXPECT_EQ(flowPoints[0].depth, 0);
    EXPECT_TRUE(flowPoints[0].resolveDepthAfterSampling);
    EXPECT_EQ(flowPoints[1].type, Protocol::LINE_END);
    EXPECT_EQ(flowPoints[1].depth, 0);
    EXPECT_TRUE(flowPoints[1].resolveDepthAfterSampling);
}

TEST_F(TextRepositoryTest, QueryFlowPointByCategoryMarksLegacyStartPointForDeferredResolution) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "DELETE FROM flow;"
        "INSERT INTO slice(id, timestamp, duration, name, depth, track_id, cat, args, cname, end_time, flag_id, "
        "group_id) VALUES (4, 170, 20, 'next_slice', 11, 101, '', '{}', '', 190, '', '');"
        "INSERT INTO flow(id, flow_id, name, cat, track_id, timestamp, type) VALUES "
        "(2, 'flow_gap', 'flow', 'test_flow', 101, 165, 's');"));
    TextRepository repository;
    FlowQuery flowQuery;
    flowQuery.fileId = TEST_RANK_ID;
    flowQuery.cat = "test_flow";
    flowQuery.minTimestamp = 100;
    std::vector<FlowPoint> flowPoints;

    repository.QueryFlowPointByCategory(flowQuery, flowPoints);

    ASSERT_EQ(flowPoints.size(), 1);
    EXPECT_EQ(flowPoints[0].depth, 0);
    EXPECT_TRUE(flowPoints[0].resolveDepthAfterSampling);
}
} // namespace
