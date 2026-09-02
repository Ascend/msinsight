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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "DataBaseManager.h"
#include "FileUtil.h"

using namespace Dic::Module::Timeline;

class DataBaseManagerTest : public ::testing::Test {
  protected:
    void SetUp() override { DataBaseManager::Instance().Clear(); }

    void TearDown() override {
        DataBaseManager::Instance().Clear();
        for (const auto &path : tempDbPaths) {
            for (const auto &suffix : {"", "-shm", "-wal"}) {
                std::remove((path + suffix).c_str());
            }
        }
    }

    std::string MakeTempDbPath(const std::string &prefix) {
        const auto uniqueId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto path = Dic::FileUtil::SplicePath(::testing::TempDir(), prefix + uniqueId + ".db");
        tempDbPaths.emplace_back(path);
        return path;
    }

    void OpenCommunicationDetailDatabase(const std::string &fileId, const std::string &dbPath) {
        auto &databaseManager = DataBaseManager::Instance();
        ASSERT_TRUE(databaseManager.CreateCommunicationDetailConnectionPool(fileId, dbPath));
        auto databaseHandle = databaseManager.GetCommunicationDetailDatabaseHandleByFileId(fileId);
        ASSERT_TRUE(databaseHandle.has_value());
        auto connection = databaseHandle->GetConnection();
        ASSERT_NE(connection, nullptr);
        ASSERT_TRUE(connection->ExecSql("CREATE TABLE IF NOT EXISTS LifecycleTest(id INTEGER);"));
    }

    std::vector<std::string> tempDbPaths;
};

TEST_F(DataBaseManagerTest, ClearReleasesCommunicationDetailPoolAndFileHandle) {
    const std::string fileId = "clear_all_trace.db";
    const std::string dbPath = MakeTempDbPath("DataBaseManagerTest_clear_all_");
    OpenCommunicationDetailDatabase(fileId, dbPath);

    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.Clear();

    EXPECT_FALSE(databaseManager.GetCommunicationDetailDatabaseHandleByFileId(fileId).has_value());
    // On Windows, removing this file fails if an idle SQLite connection is still holding its handle.
    EXPECT_EQ(std::remove(dbPath.c_str()), 0);
}

TEST_F(DataBaseManagerTest, ClearTraceReleasesCommunicationDetailPoolAndFileHandle) {
    const std::string fileId = "clear_trace.db";
    const std::string dbPath = MakeTempDbPath("DataBaseManagerTest_clear_trace_");
    OpenCommunicationDetailDatabase(fileId, dbPath);

    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.Clear(DatabaseType::TRACE);

    EXPECT_FALSE(databaseManager.GetCommunicationDetailDatabaseHandleByFileId(fileId).has_value());
    EXPECT_EQ(std::remove(dbPath.c_str()), 0);
}

TEST_F(DataBaseManagerTest, ReleaseDatabaseByFileIdReleasesOnlyTargetCommunicationDetailPool) {
    const std::string targetFileId = "release_target_trace.db";
    const std::string survivorFileId = "release_survivor_trace.db";
    const std::string targetDbPath = MakeTempDbPath("DataBaseManagerTest_release_target_");
    const std::string survivorDbPath = MakeTempDbPath("DataBaseManagerTest_release_survivor_");
    OpenCommunicationDetailDatabase(targetFileId, targetDbPath);
    OpenCommunicationDetailDatabase(survivorFileId, survivorDbPath);

    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.ReleaseDatabaseByFileId(targetFileId);

    EXPECT_FALSE(databaseManager.GetCommunicationDetailDatabaseHandleByFileId(targetFileId).has_value());
    auto survivorHandle = databaseManager.GetCommunicationDetailDatabaseHandleByFileId(survivorFileId);
    ASSERT_TRUE(survivorHandle.has_value());
    EXPECT_NE(survivorHandle->GetConnection(), nullptr);
    EXPECT_EQ(std::remove(targetDbPath.c_str()), 0);
}

TEST_F(DataBaseManagerTest, TracksOrderedSourcesSeparatelyFromRepresentative) {
    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.RegisterRankSource("rank0", "z.db");
    databaseManager.RegisterRankSource("rank0", "a.db");
    databaseManager.RegisterRankSource("rank0", "a.db");

    auto sources = databaseManager.GetSourceFileIdsByRankId("rank0");
    ASSERT_EQ(sources.size(), 2U);
    EXPECT_LT(sources[0], sources[1]);

    databaseManager.SetRepresentativeSource("rank0", "z.db");
    EXPECT_EQ(databaseManager.GetFileIdByRankId("rank0"), "z.db");
    EXPECT_EQ(databaseManager.GetSourceFileIdsByRankId("rank0"), sources);
}

TEST_F(DataBaseManagerTest, GetTraceDatabaseByRankIdAcceptsPhysicalFileId) {
    const std::string fileId = "thread-2.db";
    const std::string dbPath = MakeTempDbPath("DataBaseManagerTest_physical_file_id_");
    auto &databaseManager = DataBaseManager::Instance();
    ASSERT_TRUE(databaseManager.CreateTraceConnectionPool(fileId, dbPath));

    auto database = databaseManager.GetTraceDatabaseByRankId(fileId);

    ASSERT_NE(database, nullptr);
    EXPECT_EQ(database->GetDbPath(), dbPath);
}

TEST_F(DataBaseManagerTest, ClearRemovesRankSourceAndRepresentativeMappings) {
    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.RegisterRankSource("rank0", "a.db");
    databaseManager.SetRepresentativeSource("rank0", "a.db");

    databaseManager.Clear();

    EXPECT_TRUE(databaseManager.GetSourceFileIdsByRankId("rank0").empty());
    EXPECT_TRUE(databaseManager.GetFileIdByRankId("rank0").empty());
}

TEST_F(DataBaseManagerTest, ReleaseSourceSelectsRemainingRepresentativeAndClearsRemovedMapping) {
    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.RegisterRankSource("rank0", "a.db");
    databaseManager.RegisterRankSource("rank0", "b.db");
    databaseManager.SetRepresentativeSource("rank0", "a.db");

    databaseManager.ReleaseDatabaseByFileId("a.db");

    auto sources = databaseManager.GetSourceFileIdsByRankId("rank0");
    ASSERT_EQ(sources.size(), 1U);
    EXPECT_EQ(databaseManager.GetFileIdByRankId("rank0"), sources.front());
    EXPECT_TRUE(databaseManager.GetRankIdByFileId("a.db").empty());
}

TEST_F(DataBaseManagerTest, SourceMappingsPreserveOriginalFileIdsWhileOrderingByNormalizedPath) {
    auto &databaseManager = DataBaseManager::Instance();
    const std::string windowsStyle = R"(D:\data\profile\z.db)";
    const std::string slashStyle = "D:/data/profile/a.db";

    databaseManager.RegisterRankSource("rank0", windowsStyle);
    databaseManager.RegisterRankSource("rank0", slashStyle);

    auto sources = databaseManager.GetSourceFileIdsByRankId("rank0");
    ASSERT_EQ(sources.size(), 2U);
    EXPECT_EQ(sources[0], slashStyle);
    EXPECT_EQ(sources[1], windowsStyle);
}
