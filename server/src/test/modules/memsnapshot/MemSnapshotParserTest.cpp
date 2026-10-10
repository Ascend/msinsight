/*
 * ------------------------------------------------------------------------- * This file is part of the MindStudio project.
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
 * ------------------------------------------------------------------------- */

#include <fstream>
#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include "FileUtil.h"
#include "MemSnapshotParser.h"
#include "MemSnapshotDatabase.h"
#include "MemSnapshotSliceService.h"
#include "sqlite3.h"

using namespace Dic::Module;
using namespace Dic;

namespace Dic::Module {
int ReadProgressInLogFile(std::ifstream &file, std::string &err);
bool DoubleCheckSuccessInLogFile(std::ifstream &file);
}

class MemSnapshotParserTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        testPicklePath = FileUtil::SplicePath(::testing::TempDir(), "mem_snapshot_parser_test.pkl");
        testLogPath = FileUtil::SplicePath(::testing::TempDir(), "mem_snapshot_parser_test.log");
        testOutputDbPath = MemSnapshotSliceService::GetArtifactDirectory(testPicklePath);

        std::ofstream pickleFile(testPicklePath);
        pickleFile.close();

        parser = &MemSnapshotParser::Instance();
        ASSERT_TRUE(parser != nullptr);
    }

    static void TearDownTestSuite() {
        parser->Reset();
        FileUtil::RemoveFile(testPicklePath);
        fs::remove_all(testOutputDbPath);
    }

  protected:
    static std::string testPicklePath;
    static std::string testLogPath;
    static std::string testOutputDbPath;
    static MemSnapshotParser *parser;
};

std::string MemSnapshotParserTest::testPicklePath;
std::string MemSnapshotParserTest::testLogPath;
std::string MemSnapshotParserTest::testOutputDbPath;
MemSnapshotParser *MemSnapshotParserTest::parser = nullptr;

TEST_F(MemSnapshotParserTest, ReadsLatestProgressAndFailureFromLog) {
    {
        std::ofstream log(testLogPath, std::ios::trunc);
        log << "10% of entries have been processed\n";
        log << "47% of entries have been processed\n";
    }
    std::ifstream progress(testLogPath);
    std::string error;
    EXPECT_EQ(ReadProgressInLogFile(progress, error), 47);
    EXPECT_TRUE(error.empty());
    progress.close();

    {
        std::ofstream log(testLogPath, std::ios::trunc);
        log << "Failed to dump the snapshot to database. invalid input\n";
    }
    progress.open(testLogPath);
    EXPECT_EQ(ReadProgressInLogFile(progress, error), -1);
    EXPECT_FALSE(error.empty());
    progress.close();
    FileUtil::RemoveFile(testLogPath);
}

TEST_F(MemSnapshotParserTest, ConfirmsSuccessKeywordInLog) {
    {
        std::ofstream log(testLogPath, std::ios::trunc);
        log << "progress only\n";
    }
    std::ifstream log(testLogPath);
    EXPECT_FALSE(DoubleCheckSuccessInLogFile(log));
    log.close();
    {
        std::ofstream output(testLogPath, std::ios::trunc);
        output << "Successfully dump the snapshot to database for devices 0\n";
    }
    log.open(testLogPath);
    EXPECT_TRUE(DoubleCheckSuccessInLogFile(log));
    log.close();
    FileUtil::RemoveFile(testLogPath);
}

// 测试解析器重置功能
TEST_F(MemSnapshotParserTest, Reset) {
    // 先设置一些状态
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath);
    parser->GetParseContext().SetState(ParserState::Processing);
    parser->GetParseContext().SetProgress(50);

    // 调用重置方法
    parser->Reset();

    // 验证状态已重置
    EXPECT_EQ(parser->GetParseContext().GetState(), ParserState::INIT);
    EXPECT_EQ(parser->GetParseContext().GetProgress(), 0);
    EXPECT_TRUE(parser->GetParseContext().GetPicklePath().empty());
    EXPECT_TRUE(parser->GetParseContext().GetLogPath().empty());
    EXPECT_TRUE(parser->GetParseContext().GetOutputDbPath().empty());
}

// 测试解析上下文管理
TEST_F(MemSnapshotParserTest, ParseContextManagement) {
    // 测试重置上下文
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath);

    // 验证上下文设置正确
    EXPECT_EQ(parser->GetParseContext().GetPicklePath(), testPicklePath);
    EXPECT_EQ(parser->GetParseContext().GetLogPath(), testLogPath);
    EXPECT_EQ(parser->GetParseContext().GetOutputDbPath(), testOutputDbPath);
    EXPECT_EQ(parser->GetParseContext().GetState(), ParserState::INIT);
    EXPECT_EQ(parser->GetParseContext().GetProgress(), 0);
    EXPECT_FALSE(parser->GetParseContext().GetWorkDir().empty());
    EXPECT_TRUE(parser->GetParseContext().IsReadyToParse());
    EXPECT_FALSE(parser->GetParseContext().IsInitialSuccessSent());

    // 测试状态更新
    parser->GetParseContext().SetState(ParserState::Processing);
    EXPECT_EQ(parser->GetParseContext().GetState(), ParserState::Processing);
    EXPECT_FALSE(parser->GetParseContext().IsReadyToParse());
    parser->GetParseContext().MarkInitialSuccessSent(false);
    EXPECT_TRUE(parser->GetParseContext().IsInitialSuccessSent());
    EXPECT_TRUE(parser->GetParseContext().WasInitialSuccessSentWhileBuilding());

    // 测试进度更新
    parser->GetParseContext().SetProgress(75);
    EXPECT_EQ(parser->GetParseContext().GetProgress(), 75);

    // 测试完成状态检查
    EXPECT_FALSE(parser->GetParseContext().IsFinished());

    parser->GetParseContext().SetState(ParserState::FINISH_SUCCESS);
    EXPECT_TRUE(parser->GetParseContext().IsFinished());

    parser->GetParseContext().SetState(ParserState::FINISH_FAILURE);
    EXPECT_TRUE(parser->GetParseContext().IsFinished());
    EXPECT_TRUE(parser->GetParseContext().IsReadyToParse());
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath, "hash");
    EXPECT_EQ(parser->GetParseContext().GetFileHash(), "hash");
    EXPECT_FALSE(parser->GetParseContext().IsInitialSuccessSent());
    parser->GetParseContext().MarkInitialSuccessSent(true);
    EXPECT_FALSE(parser->GetParseContext().WasInitialSuccessSentWhileBuilding());
    parser->GetParseContext().SetState(ParserState::UP_TO_DATE);
    EXPECT_TRUE(parser->GetParseContext().IsFinished());
}

// 测试是否需要解析的检查逻辑
TEST_F(MemSnapshotParserTest, CheckIfParsingNeed) {
    // 重置解析器
    parser->Reset();

    // 设置测试上下文
    const std::string fileHash = parser->CalculateFileHash(testPicklePath);
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath, fileHash);

    // 由于输出数据库文件不存在，应该需要解析
    bool needParse = parser->CheckIfParsingNeed(parser->GetParseContext());
    EXPECT_TRUE(needParse);

    const auto deviceDir = fs::path(testOutputDbPath) / "device_0";
    fs::create_directories(deviceDir);
    const auto sliceDbPath = deviceDir / "slice_00000.db";
    sqlite3 *sliceDb = nullptr;
    ASSERT_EQ(sqlite3_open(sliceDbPath.string().c_str(), &sliceDb), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sliceDb,
                  "CREATE TABLE dictionary (`table` TEXT, `column` TEXT, `key` TEXT, `value` TEXT);"
                  "CREATE TABLE block_0 (`id` INTEGER PRIMARY KEY, `address` INTEGER, `size` INTEGER, "
                  "`requestedSize` INTEGER, `state` INTEGER DEFAULT 99, `allocEventId` INTEGER, `freeEventId` INTEGER);"
                  "CREATE TABLE trace_entry_0 (`id` INTEGER PRIMARY KEY, `action` INTEGER, `address` INTEGER, `size` "
                  "INTEGER, `stream` INTEGER, `allocated` INTEGER, `active` INTEGER, `reserved` INTEGER, `callstack` "
                  "TEXT);"
                  "INSERT INTO trace_entry_0 VALUES (0, 0, 0, 0, 0, 10, 10, 20, '');",
                  nullptr, nullptr, nullptr),
        SQLITE_OK);
    sqlite3_close(sliceDb);
    ASSERT_TRUE(Dic::Module::FullDb::MemSnapshotDatabase::BuildMemoryAllocationCache(sliceDbPath.string(), "0"));
    std::ofstream manifest(MemSnapshotSliceService::GetManifestPath(testPicklePath), std::ios::trunc);
    manifest << R"({"schemaVersion":1,"status":"complete","sourceFile":"snapshot_invalid.pkl","cacheHash":")"
             << fileHash
             << R"(","eventsPerSlice":100,"devices":{"0":{"eventCount":1,"sliceCount":1,)"
                R"("readySlices":[0],"slices":[{"index":0,"startEventId":0,"endEventId":0,)"
                R"("file":"device_0/slice_00000.db","ready":true}]}}})";
    manifest.close();

    EXPECT_FALSE(parser->CheckIfParsingNeed(parser->GetParseContext()));

    // Exercise the asynchronous cache-hit path without launching a pickle subprocess.
    parser->AsyncParseMemSnapshotPickle(testPicklePath);
    for (int attempt = 0; attempt < 100 && parser->GetParseContext().GetState() != ParserState::UP_TO_DATE; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(parser->GetParseContext().GetState(), ParserState::UP_TO_DATE);
    EXPECT_EQ(parser->GetParseContext().GetProgress(), 100);
    parser->Reset();
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath, fileHash);

    ASSERT_EQ(sqlite3_open(sliceDbPath.string().c_str(), &sliceDb), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sliceDb, "DROP TABLE memory_allocation_cache_v1_0", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(sliceDb);
    // 缺 cache 时可从源表现场重建，无需重新解析 pickle
    EXPECT_FALSE(parser->CheckIfParsingNeed(parser->GetParseContext()));
    ASSERT_TRUE(Dic::Module::FullDb::MemSnapshotDatabase::HasMemoryAllocationCache(sliceDbPath.string(), "0"));

    ASSERT_EQ(sqlite3_open(sliceDbPath.string().c_str(), &sliceDb), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sliceDb, "DROP TABLE block_0", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(sliceDb);
    EXPECT_TRUE(parser->CheckIfParsingNeed(parser->GetParseContext()));
    ASSERT_EQ(sqlite3_open(sliceDbPath.string().c_str(), &sliceDb), SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(sliceDb, "CREATE TABLE block_0 (id INTEGER PRIMARY KEY)", nullptr, nullptr, nullptr), SQLITE_OK);
    sqlite3_close(sliceDb);

    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath, "changed-hash");
    EXPECT_TRUE(parser->CheckIfParsingNeed(parser->GetParseContext()));

    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath, fileHash);
    fs::remove(sliceDbPath);
    EXPECT_TRUE(parser->CheckIfParsingNeed(parser->GetParseContext()));

    ASSERT_EQ(sqlite3_open(sliceDbPath.string().c_str(), &sliceDb), SQLITE_OK);
    sqlite3_close(sliceDb);
    std::ofstream invalidManifest(MemSnapshotSliceService::GetManifestPath(testPicklePath), std::ios::trunc);
    invalidManifest << R"({"schemaVersion":1,"status":"complete"})";
    invalidManifest.close();
    EXPECT_TRUE(parser->CheckIfParsingNeed(parser->GetParseContext()));

    fs::remove_all(testOutputDbPath);
}

TEST_F(MemSnapshotParserTest, CalculateFileHashTracksFileContent) {
    {
        std::ofstream pickleFile(testPicklePath, std::ios::binary | std::ios::trunc);
        pickleFile << "snapshot-content";
    }
    const std::string firstHash = parser->CalculateFileHash(testPicklePath);
    const std::string secondHash = parser->CalculateFileHash(testPicklePath);
    EXPECT_FALSE(firstHash.empty());
    EXPECT_EQ(firstHash, secondHash);

    {
        std::ofstream pickleFile(testPicklePath, std::ios::binary | std::ios::app);
        pickleFile << "-changed";
    }
    EXPECT_NE(firstHash, parser->CalculateFileHash(testPicklePath));
}

TEST_F(MemSnapshotParserTest, CalculateFileHashIncludesParserSalt) {
    {
        std::ofstream pickleFile(testPicklePath, std::ios::binary | std::ios::trunc);
        pickleFile << "snapshot-content";
    }
    // This value changes when MEM_SNAPSHOT_PARSER_HASH_SALT changes.
    EXPECT_EQ(
        "adaad4452d193309891d52c7803603f11ef9a73aae9d8fce35df9b22891cb1be", parser->CalculateFileHash(testPicklePath));
}
