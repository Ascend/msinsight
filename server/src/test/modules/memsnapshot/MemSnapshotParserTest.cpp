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
#include <gtest/gtest.h>
#include "FileUtil.h"
#include "MemSnapshotParser.h"
#include "MemSnapshotSliceService.h"

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
        testOutputDbPath = Dic::Module::MemSnapshot::MemSnapshotSliceService::GetArtifactDirectory(testPicklePath);

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

TEST_F(MemSnapshotParserTest, ReadsScriptCacheIdentityBeforePublishingSlices) {
    const std::string hash(64, 'a');
    for (const auto &action : {"validated", "rebuild"}) {
        parser->GetParseContext().SetFileHash("");
        {
            std::ofstream output(testLogPath, std::ios::trunc);
            output << "Snapshot cache " << action << ": " << hash << "\n";
            output << "1% of entries have been processed\n";
        }
        std::ifstream log(testLogPath);
        std::string error;
        EXPECT_EQ(ReadProgressInLogFile(log, error), 1);
        EXPECT_EQ(parser->GetParseContext().GetFileHash(), hash);
    }
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

TEST_F(MemSnapshotParserTest, FileHashIsPublishedByParserScript) {
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath);
    EXPECT_TRUE(parser->GetParseContext().GetFileHash().empty());
    parser->GetParseContext().SetFileHash("script-generated-hash");
    EXPECT_EQ(parser->GetParseContext().GetFileHash(), "script-generated-hash");
    parser->GetParseContext().Reset();
    EXPECT_TRUE(parser->GetParseContext().GetFileHash().empty());
}
