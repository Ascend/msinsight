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
#include "DpuRepo.h"
#include "FileUtil.h"
#include "TestSuit.h"
#include "TrackInfoManager.h"

using namespace Dic::Module::Timeline;

namespace {
constexpr const char *TEST_RANK_ID = "dpu_repo_test_rank";
// DPU globalTid uses the same packed layout as Host APIs. These fixtures use PID 100 and TID 1000+.

class DpuRepoTest : public ::testing::Test {
  protected:
    static std::string testDbPath;
    static std::recursive_mutex testMutex;
    static Module::Database testDatabase;

    static void SetUpTestSuite() {
        testDbPath = TestSuit::GetTestDataFile("test_dpu_repository.db");
        if (FileUtil::CheckFilePathExist(testDbPath)) {
            FileUtil::RemoveFile(testDbPath);
        }
        ASSERT_TRUE(testDatabase.OpenDb(testDbPath, false));
        ASSERT_TRUE(testDatabase.ExecSql(
            "CREATE TABLE DPU_TASK(dpuDeviceId INTEGER, globalTid INTEGER, startNs INTEGER, endNs INTEGER, "
            "globalTaskId INTEGER, streamId INTEGER, taskId INTEGER, opName INTEGER, args INTEGER);"
            "CREATE TABLE STRING_IDS(id INTEGER PRIMARY KEY, value TEXT);"));
        DataBaseManager::Instance().SetDataType(DataType::DB, testDbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(TEST_RANK_ID, testDbPath));
        DataBaseManager::Instance().SetDbPathMapping(TEST_RANK_ID, testDbPath, "");
    }

    static void TearDownTestSuite() {
        DataBaseManager::Instance().ReleaseDatabaseByRankId(TEST_RANK_ID);
        testDatabase.CloseDb();
        if (FileUtil::CheckFilePathExist(testDbPath)) {
            FileUtil::RemoveFile(testDbPath);
        }
    }

    void SetUp() override {
        ASSERT_TRUE(testDatabase.ExecSql("DELETE FROM DPU_TASK; DELETE FROM STRING_IDS;"));
        TrackInfoManager::Instance().Reset();
        DataBaseManager::Instance().SetDbPathMapping(TEST_RANK_ID, testDbPath, "");
    }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    static CompeteSliceDomain QueryDetail(const std::string &rowId) {
        DpuRepo repo;
        SliceQuery query;
        query.rankId = TEST_RANK_ID;
        query.sliceId = rowId;
        CompeteSliceDomain detail;
        EXPECT_TRUE(repo.QuerySliceDetailInfo(query, detail));
        return detail;
    }
};

std::string DpuRepoTest::testDbPath;
std::recursive_mutex DpuRepoTest::testMutex;
Module::Database DpuRepoTest::testDatabase(DpuRepoTest::testMutex);

TEST_F(DpuRepoTest, QuerySimpleSliceIsolatesDpuGlobalTidDeviceStreamAndUsesIntersectingTimeRange) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "INSERT INTO DPU_TASK(ROWID, dpuDeviceId, globalTid, startNs, endNs, globalTaskId, streamId, taskId, "
        "opName, args) VALUES "
        "(101, 0, 429496730600, 100, 130, 1, 7, 11, 1, 0),"
        "(102, 1, 429496730601, 110, 140, 2, 7, 12, 1, 0),"
        "(103, 0, 429496730601, 115, 145, 3, 7, 13, 1, 0),"
        "(104, 0, 429496730600, 120, 150, 4, 8, 14, 1, 0),"
        "(105, 0, 429496730600, 151, 180, 5, 7, 15, 1, 0);"));

    DpuRepo repo;
    SliceQuery query;
    query.rankId = TEST_RANK_ID;
    query.startTime = 125;
    query.endTime = 150;
    query.trackId = TrackInfoManager::Instance().GetTrackId(TEST_RANK_ID, "DPU_429496730600_0", "7");
    std::vector<SliceDomain> slices;

    repo.QuerySimpleSliceWithOutNameByTrackId(query, slices);

    ASSERT_EQ(slices.size(), 1);
    EXPECT_EQ(slices[0].id, 101);
    EXPECT_EQ(slices[0].timestamp, 100);
    EXPECT_EQ(slices[0].endTime, 130);

    query.trackId = TrackInfoManager::Instance().GetTrackId(TEST_RANK_ID, "DPU_429496730601_1", "7");
    slices.clear();
    repo.QuerySimpleSliceWithOutNameByTrackId(query, slices);
    ASSERT_EQ(slices.size(), 1);
    EXPECT_EQ(slices[0].id, 102);
}

TEST_F(DpuRepoTest, QueryCompeteSliceResolvesOperatorNameFromStringIds) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "INSERT INTO STRING_IDS(id, value) VALUES (1, 'dpu_kernel');"
        "INSERT INTO DPU_TASK(ROWID, dpuDeviceId, globalTid, startNs, endNs, globalTaskId, streamId, taskId, "
        "opName, args) VALUES (201, 0, 429496730600, 100, 120, 684, 0, 791, 1, 0);"));

    DpuRepo repo;
    SliceQuery query;
    query.rankId = TEST_RANK_ID;
    std::vector<CompeteSliceDomain> slices;
    repo.QueryCompeteSliceByIds(query, {201}, slices);

    ASSERT_EQ(slices.size(), 1);
    EXPECT_EQ(slices[0].id, 201);
    EXPECT_EQ(slices[0].name, "dpu_kernel");
    EXPECT_EQ(slices[0].timestamp, 100);
    EXPECT_EQ(slices[0].endTime, 120);
}

TEST_F(DpuRepoTest, QuerySliceDetailMergesObjectArgsAndPreservesBaseFields) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "INSERT INTO STRING_IDS(id, value) VALUES "
        "(1, 'dpu_kernel'), (11, '{\"bytes\":4096,\"name\":\"must-not-overwrite\",\"nested\":{\"ok\":true}}');"
        "INSERT INTO DPU_TASK(ROWID, dpuDeviceId, globalTid, startNs, endNs, globalTaskId, streamId, taskId, "
        "opName, args) VALUES "
        "(301, 0, 429496730600, 1787904232206642435, 1787904232206660192, 684, 0, 791, 1, 11);"));

    const auto detail = QueryDetail("301");

    EXPECT_EQ(detail.id, 301);
    EXPECT_EQ(detail.name, "dpu_kernel");
    EXPECT_EQ(detail.timestamp, 1787904232206642435ULL);
    EXPECT_EQ(detail.endTime, 1787904232206660192ULL);
    EXPECT_NE(detail.args.find("\"dpuDeviceId\":\"0\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"globalTid\":\"429496730600\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"globalTaskId\":\"684\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"streamId\":\"0\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"taskId\":\"791\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"startNs\":\"1787904232206642435\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"endNs\":\"1787904232206660192\""), std::string::npos);
    EXPECT_NE(detail.args.find("\"bytes\":4096"), std::string::npos);
    EXPECT_NE(detail.args.find("\"nested\":{\"ok\":true}"), std::string::npos);
    EXPECT_NE(detail.args.find("\"name\":\"dpu_kernel\""), std::string::npos);
    EXPECT_EQ(detail.args.find("must-not-overwrite"), std::string::npos);
}

TEST_F(DpuRepoTest, QuerySliceDetailKeepsInvalidArgsAsRawValue) {
    ASSERT_TRUE(testDatabase.ExecSql(
        "INSERT INTO STRING_IDS(id, value) VALUES (1, 'dpu_kernel'), (13, 'not-json');"
        "INSERT INTO DPU_TASK(ROWID, dpuDeviceId, globalTid, startNs, endNs, globalTaskId, streamId, taskId, "
        "opName, args) VALUES (303, 1, 429496730602, 300, 320, 686, 3, 793, 1, 13);"));

    const auto detail = QueryDetail("303");

    EXPECT_NE(detail.args.find("\"args\":\"not-json\""), std::string::npos);
}
} // namespace
