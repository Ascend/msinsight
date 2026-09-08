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

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "DataBaseManager.h"
#define private public
#include "FullDbParser.h"
#undef private
#include "ParserStatusManager.h"

namespace Dic::Module::FullDb {
namespace {
namespace fs = std::filesystem;
using namespace Timeline;

class FullDbParserTest : public ::testing::Test {
  protected:
    void SetUp() override { ResetParserState(); }

    void TearDown() override {
        ResetParserState();
        if (!temporaryDb_.empty()) {
            fs::remove(temporaryDb_);
        }
    }

    void ResetParserState() {
        FullDbParser::Instance().FileParser::Reset();
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
    }

    fs::path temporaryDb_;
};

TEST_F(FullDbParserTest, ReusesAlreadyOpenedPlatformDatabase) {
    temporaryDb_ = fs::path(::testing::TempDir()) / "full-db-parser-platform.db";
    fs::remove(temporaryDb_);
    const std::string databasePath = temporaryDb_.string();
    auto &manager = DataBaseManager::Instance();
    auto alreadyOpenedDatabase = manager.CreatePlatformDataBase("numa:" + databasePath, databasePath);
    ASSERT_NE(alreadyOpenedDatabase, nullptr);
    ASSERT_TRUE(alreadyOpenedDatabase->OpenDb(databasePath, false));

    EXPECT_TRUE(FullDbParser::InitPlatform("platform-rank", databasePath));

    auto databaseByPlatformRank = manager.GetPlatformDatabaseByRankId("platform-rank");
    ASSERT_NE(databaseByPlatformRank, nullptr);
    EXPECT_EQ(databaseByPlatformRank, alreadyOpenedDatabase);
    EXPECT_TRUE(databaseByPlatformRank->IsOpen());
}

TEST_F(FullDbParserTest, DoesNotInitializeStandalonePlatformDatabase) {
    const std::string invalidDbPath = ::testing::TempDir();
    const std::vector<std::string> rankIds = {"platform-rank-0", "platform-rank-1"};
    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.SetFileType(FileType::PLATFORM, invalidDbPath);
    for (const auto &rankId : rankIds) {
        databaseManager.SetRankIdFileIdMapping(rankId, invalidDbPath);
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::INIT);
    }

    size_t callbackCount = 0;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackCount](const std::string, const std::string, bool, const std::string) { ++callbackCount; };
    FullDbParser::Instance().SetParseEndCallBack(callback);

    FullDbParser::InitOpenDb(invalidDbPath, rankIds);

    EXPECT_EQ(callbackCount, 0U);
    for (const auto &rankId : rankIds) {
        EXPECT_EQ(databaseManager.GetPlatformDatabaseByRankId(rankId), nullptr);
        EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::INIT);
    }
}

} // namespace
} // namespace Dic::Module::FullDb
