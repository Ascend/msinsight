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
#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <thread>
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

class FullDbParserTestAccessor {
  public:
    static void EndParseTask(const std::vector<std::string> &rankIds, const std::string &filePath,
        const std::shared_ptr<std::vector<std::future<bool>>> &futures,
        const std::string &embeddedPlatformRankId = "") {
        FullDbParser::EndParseTask(
            rankIds, filePath, futures, std::chrono::high_resolution_clock::now(), embeddedPlatformRankId);
    }
};

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

class FullDbParserDepthTest : public ::testing::Test {
  protected:
    const std::string rankId = "full_db_parser_depth_test";
    std::string dbPath;
    std::shared_ptr<DbTraceDataBase> database;

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        dbPath = (std::filesystem::temp_directory_path() /
            ("msinsight-full-db-parser-depth-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db"))
                     .string();
        std::filesystem::remove(dbPath);
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(rankId, dbPath));
        DataBaseManager::Instance().SetDbPathMapping(rankId, dbPath, "");
        database =
            std::dynamic_pointer_cast<DbTraceDataBase>(DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId));
        ASSERT_NE(database, nullptr);
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::RUNNING);
    }

    void TearDown() override {
        FullDbParser::Instance().SetParseEndCallBack(emptyCallback);
        database.reset();
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        std::filesystem::remove(dbPath);
    }

    std::function<void(const std::string, const std::string, bool, const std::string)> emptyCallback;
};

TEST_F(FullDbParserDepthTest, PendingDepthFutureBlocksSuccessUntilCompletion) {
    bool callbackCalled = false;
    bool callbackResult = false;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackCalled, &callbackResult](const std::string, const std::string, bool result, const std::string) {
            callbackCalled = true;
            callbackResult = result;
        };
    FullDbParser::Instance().SetParseEndCallBack(callback);
    std::promise<bool> depthPromise;
    auto futures = std::make_shared<std::vector<std::future<bool>>>();
    futures->emplace_back(depthPromise.get_future());

    auto endTask =
        std::async(std::launch::async, [&]() { FullDbParserTestAccessor::EndParseTask({rankId}, dbPath, futures); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_FALSE(callbackCalled);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::RUNNING);
    EXPECT_EQ(database->QueryDatabaseVersion(), "0");

    depthPromise.set_value(true);
    endTask.get();

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackResult);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH_ALL);
    EXPECT_EQ(database->QueryDatabaseVersion(), Database::GetCompileDataBaseVersion());
}

TEST_F(FullDbParserDepthTest, EmbeddedPlatformSharesSuccessfulParseResult) {
    std::vector<std::pair<std::string, bool>> callbackResults;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackResults](const std::string &callbackRankId, const std::string &, bool result, const std::string &) {
            callbackResults.emplace_back(callbackRankId, result);
        };
    FullDbParser::Instance().SetParseEndCallBack(callback);
    auto futures = std::make_shared<std::vector<std::future<bool>>>();
    futures->emplace_back(std::async(std::launch::deferred, []() { return true; }));
    const std::string embeddedRankId = rankId + "#platform";

    FullDbParserTestAccessor::EndParseTask({rankId}, dbPath, futures, embeddedRankId);

    ASSERT_EQ(callbackResults.size(), 2U);
    EXPECT_EQ(callbackResults[0], std::make_pair(rankId, true));
    EXPECT_EQ(callbackResults[1], std::make_pair(embeddedRankId, true));
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH_ALL);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(embeddedRankId), ParserStatus::FINISH_ALL);
    EXPECT_EQ(database->QueryDatabaseVersion(), Database::GetCompileDataBaseVersion());
}

TEST_F(FullDbParserDepthTest, FailedDepthFutureBlocksSuccessAndDatabaseVersionWrite) {
    bool callbackCalled = false;
    bool callbackResult = true;
    size_t callbackCount = 0;
    std::string callbackMessage;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackCalled, &callbackResult, &callbackCount, &callbackMessage](
            const std::string, const std::string, bool result, const std::string &message) {
            callbackCalled = true;
            callbackResult = result;
            ++callbackCount;
            callbackMessage = message;
        };
    FullDbParser::Instance().SetParseEndCallBack(callback);
    auto futures = std::make_shared<std::vector<std::future<bool>>>();
    futures->emplace_back(std::async(std::launch::deferred, []() { return false; }));
    const std::string embeddedRankId = rankId + "#platform";

    FullDbParserTestAccessor::EndParseTask({rankId}, dbPath, futures, embeddedRankId);

    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(callbackCount, 2U);
    EXPECT_FALSE(callbackResult);
    EXPECT_NE(callbackMessage.find("operator depth"), std::string::npos);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::TERMINATE);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(embeddedRankId), ParserStatus::TERMINATE);
    EXPECT_EQ(database->QueryDatabaseVersion(), "0");
}
} // namespace
} // namespace Dic::Module::FullDb
