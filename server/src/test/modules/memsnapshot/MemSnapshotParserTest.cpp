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

#include <gtest/gtest.h>
#include <filesystem>
#include "StringUtil.h"
#include "../TestSuit.h"
#include "MemSnapshotParser.h"

using namespace Dic::Module;
using namespace Dic;

class MemSnapshotParserTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        // 创建临时测试目录
        testDir = TestSuit::GetTestDataFile("snapshot");

        // 创建测试pickle文件路径
        testPicklePath = testDir + "snapshot_invalid.pkl";
        testLogPath = testDir + "snapshot_invalid.log";
        testOutputDbPath = testDir + "snapshot_invalid.pkl.db";

        // 创建空的测试pickle文件
        std::ofstream pickleFile(testPicklePath);
        pickleFile.close();

        // 获取解析器实例
        parser = &MemSnapshotParser::Instance();
        ASSERT_TRUE(parser != nullptr);
    }

    static void TearDownTestSuite() {
        // 重置解析器
        parser->Reset();

        // 删除临时文件
        FileUtil::RemoveFile(testPicklePath);
        FileUtil::RemoveFile(testOutputDbPath);
    }

  protected:
    static std::string testDir;
    static std::string testPicklePath;
    static std::string testLogPath;
    static std::string testOutputDbPath;
    static MemSnapshotParser *parser;
};

std::string MemSnapshotParserTest::testDir;
std::string MemSnapshotParserTest::testPicklePath;
std::string MemSnapshotParserTest::testLogPath;
std::string MemSnapshotParserTest::testOutputDbPath;
MemSnapshotParser *MemSnapshotParserTest::parser = nullptr;

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

    // 测试状态更新
    parser->GetParseContext().SetState(ParserState::Processing);
    EXPECT_EQ(parser->GetParseContext().GetState(), ParserState::Processing);

    // 测试进度更新
    parser->GetParseContext().SetProgress(75);
    EXPECT_EQ(parser->GetParseContext().GetProgress(), 75);

    // 测试完成状态检查
    EXPECT_FALSE(parser->GetParseContext().IsFinished());

    parser->GetParseContext().SetState(ParserState::FINISH_SUCCESS);
    EXPECT_TRUE(parser->GetParseContext().IsFinished());

    parser->GetParseContext().SetState(ParserState::FINISH_FAILURE);
    EXPECT_TRUE(parser->GetParseContext().IsFinished());
}

// 测试是否需要解析的检查逻辑
TEST_F(MemSnapshotParserTest, CheckIfParsingNeed) {
    // 重置解析器
    parser->Reset();

    // 设置测试上下文
    parser->GetParseContext().Reset(testPicklePath, testLogPath, testOutputDbPath);

    // 由于输出数据库文件不存在，应该需要解析
    bool needParse = parser->CheckIfParsingNeed(parser->GetParseContext());
    EXPECT_TRUE(needParse);

    // 创建一个空的输出数据库文件
    std::ofstream dbFile(testOutputDbPath);
    dbFile.close();

    // 再次检查，由于数据库文件存在但未打开，仍需要解析
    needParse = parser->CheckIfParsingNeed(parser->GetParseContext());
    EXPECT_TRUE(needParse);

    // 清理测试文件
    FileUtil::RemoveFile(testOutputDbPath);
}
