/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the License at:
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
#include "HostFlowRepo.h"
#include "TestSuit.h"
#include "TrackInfoManager.h"

using namespace Dic::Module::Timeline;

namespace {
constexpr const char *TEST_RANK_ID = "host_flow_repo_depth_test";

class HostFlowRepoTest : public ::testing::Test {
  protected:
    std::string dbPath;
    std::recursive_mutex mutex;
    Dic::Module::Database database{mutex};

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        TrackInfoManager::Instance().Reset();
        dbPath = TestSuit::GetTestDataFile("host_flow_repo_depth_test.db");
        if (FileUtil::CheckFilePathExist(dbPath)) {
            FileUtil::RemoveFile(dbPath);
        }
        ASSERT_TRUE(database.OpenDb(dbPath, false));
        ASSERT_TRUE(database.ExecSql(
            "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY, value TEXT);"
            "CREATE TABLE CONNECTION_IDS (id INTEGER PRIMARY KEY, connectionId INTEGER);"
            "CREATE TABLE PYTORCH_API (startNs INTEGER, endNs INTEGER, globalTid INTEGER, connectionId INTEGER, "
            "name INTEGER, type INTEGER, depth INTEGER);"
            "CREATE TABLE CANN_API (startNs INTEGER, endNs INTEGER, type INTEGER, globalTid INTEGER, "
            "connectionId INTEGER PRIMARY KEY, name INTEGER, depth INTEGER);"
            "CREATE TABLE MSTX_EVENTS (startNs INTEGER, endNs INTEGER, eventType INTEGER, rangeId INTEGER, "
            "category INTEGER, message INTEGER, globalTid INTEGER, endGlobalTid INTEGER, domainId INTEGER, "
            "connectionId INTEGER, depth INTEGER);"));
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(TEST_RANK_ID, dbPath));
        DataBaseManager::Instance().SetDbPathMapping(TEST_RANK_ID, dbPath, "");
    }

    void TearDown() override {
        DataBaseManager::Instance().ReleaseDatabaseByRankId(TEST_RANK_ID);
        database.CloseDb();
        if (FileUtil::CheckFilePathExist(dbPath)) {
            FileUtil::RemoveFile(dbPath);
        }
        TrackInfoManager::Instance().Reset();
    }
};

TEST_F(HostFlowRepoTest, QueryFwdbwdUsesPersistedDepthAndExcludesPythonStack) {
    ASSERT_TRUE(
        database.ExecSql("INSERT INTO STRING_IDS(id, value) VALUES (1, 'Enqueue'), (2, 'operator');"
                         "INSERT INTO CONNECTION_IDS(id, connectionId) VALUES "
                         "(10, 900), (11, 900), (12, 901), (13, 901);"
                         "INSERT INTO PYTORCH_API(startNs, endNs, globalTid, connectionId, name, type, depth) VALUES "
                         "(100, 110, 101, 10, 2, NULL, 4), (120, 130, 101, 11, 2, 50002, 5), "
                         "(105, 115, 101, 12, 2, 50003, 14), (125, 135, 101, 13, 2, 50003, 15);"));
    HostFlowRepo repository;
    FlowQuery query;
    query.fileId = TEST_RANK_ID;
    std::vector<FlowPoint> flowPoints;

    repository.QueryFwdbwd(query, flowPoints);

    ASSERT_EQ(flowPoints.size(), 2);
    EXPECT_EQ(flowPoints[0].depth, 4);
    EXPECT_EQ(flowPoints[1].depth, 5);
}

TEST_F(HostFlowRepoTest, AsyncNpuUsesPersistedDepthAndExcludesPythonStack) {
    ASSERT_TRUE(
        database.ExecSql("INSERT INTO STRING_IDS(id, value) VALUES (1, 'Enqueue'), (2, 'operator');"
                         "INSERT INTO CONNECTION_IDS(id, connectionId) VALUES (20, 920), (21, 921);"
                         "INSERT INTO PYTORCH_API(startNs, endNs, globalTid, connectionId, name, type, depth) VALUES "
                         "(100, 110, 101, 20, 2, NULL, 6), (105, 115, 101, 21, 2, 50003, 16);"));
    HostFlowRepo repository;
    FlowQuery query;
    query.fileId = TEST_RANK_ID;
    std::vector<FlowPoint> flowPoints;

    repository.AddAsyncNpuFlowPoint(query, flowPoints);

    ASSERT_EQ(flowPoints.size(), 1);
    EXPECT_EQ(flowPoints[0].depth, 6);
}

TEST_F(HostFlowRepoTest, CannAndMstxUsePersistedDepth) {
    ASSERT_TRUE(database.ExecSql(
        "INSERT INTO CANN_API(startNs, endNs, type, globalTid, connectionId, name, depth) "
        "VALUES (100, 110, 10000, 101, 1000, 1, 7);"
        "INSERT INTO MSTX_EVENTS(startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES (120, 130, 1, 1, 1, 1, 101, 101, 42, 2000, 8);"));
    HostFlowRepo repository;
    FlowQuery query;
    query.fileId = TEST_RANK_ID;
    std::vector<FlowPoint> cannFlowPoints;
    std::vector<FlowPoint> mstxFlowPoints;

    repository.AddCANNFlowPoint(query, cannFlowPoints);
    repository.AddMstxFlowPoint(query, mstxFlowPoints);

    ASSERT_EQ(cannFlowPoints.size(), 1);
    EXPECT_EQ(cannFlowPoints[0].depth, 7);
    ASSERT_EQ(mstxFlowPoints.size(), 1);
    EXPECT_EQ(mstxFlowPoints[0].depth, 8);
}
} // namespace
