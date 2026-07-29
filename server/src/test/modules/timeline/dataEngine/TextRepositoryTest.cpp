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

#include "DataBaseManager.h"
#include "FileUtil.h"
#include "TestSuit.h"
#include "TextRepository.h"

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
        ASSERT_TRUE(testDatabase.ExecSql("DELETE FROM slice; DELETE FROM thread; DELETE FROM kernel_detail;"));
        ASSERT_TRUE(
            testDatabase.ExecSql("INSERT INTO thread(track_id, tid, pid, thread_name, thread_sort_index) VALUES "
                                 "(100, '0', '900', 'Group group_a Communication', 0), "
                                 "(101, '1', '900', 'Plane 0', 1);"));
        ASSERT_TRUE(testDatabase.ExecSql(
            "INSERT INTO slice(id, timestamp, duration, name, depth, track_id, cat, args, cname, end_time, "
            "flag_id, group_id) VALUES "
            "(1, 100, 200, 'hcom_allReduce__0_0_1', 0, 100, '', '{}', '', 300, '', ''), "
            "(2, 110, 50, 'Memcpy', 0, 101, '', '{}', '', 160, '', '');"));
    }

    static bool CreateSchema() {
        return testDatabase.ExecSql(
                   "CREATE TABLE slice (id INTEGER PRIMARY KEY, timestamp INTEGER, duration INTEGER, name TEXT, "
                   "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, "
                   "flag_id TEXT, group_id TEXT);") &&
            testDatabase.ExecSql(
                "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, thread_name TEXT, "
                "thread_sort_index INTEGER);") &&
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
} // namespace
