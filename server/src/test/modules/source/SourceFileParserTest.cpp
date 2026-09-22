/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

#include <gtest/gtest.h>
#include "ConstantDefs.h"
#include "ParserStatusManager.h"
#include "SourceFileParser.h"
#include "SourceProtocolRequest.h"
#include "TextTraceDatabase.h"
#include "ProjectParserFactory.h"
#include "mockUtils/BinFileGenerator.h"
#include "mockUtils/DataBlock.h"
#include "../../TestSuit.h"

using namespace std;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

namespace Dic::Module::Source {
class SourceFileParserTestAccessor {
  public:
    static bool TestPrepareDatabaseForParse(const std::shared_ptr<TextTraceDatabase> &database) {
        return SourceFileParser::PrepareDatabaseForParse(database);
    }
};
}

class SourceFileParserTest : public ::testing::Test {
  public:
    static std::string dataPath;
    static std::string dbPath;

    static void SetUpTestCase() {
        dataPath = TestSuit::GetTestDataFile("data.bin");
        dbPath = TestSuit::GetTestDataFile("compute_mindstudio_insight_data.db");
        DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool(dataPath, dbPath);
    }

    static void TearDownTestCase() {
        SourceFileParser::Instance().Reset();
        DataBaseManager::Instance().Clear();
        DataBaseManager::Instance().ReleaseDatabaseByRankId(dbPath);
        if (std::remove(dbPath.c_str()) == 0) {
            ServerLog::Info("Remove database file success.");
        } else {
            ServerLog::Info("Remove database file failed: ", dbPath);
        }
    }
};

std::string SourceFileParserTest::dataPath;
std::string SourceFileParserTest::dbPath;

static void WaitParseEnd(std::vector<std::string> &&statusList) {
    if (statusList.empty()) {
        return;
    }
    while (true) {
        size_t i = 0;
        for (const auto &tmp : statusList) {
            if (ParserStatusManager::Instance().GetParserStatus(tmp) != ParserStatus::FINISH) {
                break;
            } else {
                i++;
            }
        }
        if (i < statusList.size()) {
            continue;
        } else {
            Dic::Server::ServerLog::Info("parse end");
            return;
        }
    }
}

TEST_F(SourceFileParserTest, Parse) {
    auto &parser = SourceFileParser::Instance();
    parser.SetFilePath(dataPath);
    parser.Parse(std::vector<std::string>(), dataPath, dataPath, dbPath);
    // 等待解析任务完成
    WaitParseEnd({dataPath});
    auto list = parser.GetSourceList();
    int sourceListSize = 6;
    EXPECT_EQ(list.size(), sourceListSize);
    parser.Reset();
}

// 生成包含 0x0f Top Warp Stall Reason 数据段的测试 bin 文件
static std::string GenerateStallReasonBinFile() {
    std::string testBinPath = TestSuit::GetSrcTestPath() + "/test_stall_reason.bin";
    std::string stallJson = R"({
        "top_stall_reason_table": {
            "IBuf_Empty": 100,
            "Nop_Cycles": 200,
            "Scoreboard_Not_Ready": 300,
            "Register_bank_conflict": 400,
            "Resource_conflict": 500,
            "Warp_Level_Sync": 600,
            "Divergence_Stack_Spill": 700,
            "Others": 400
        }
    })";

    BinFileGenerator generator;
    generator.AddDataBlock(std::make_unique<NormalDataBlock>(DataTypeEnum::TOP_WARP_STALL_REASON, stallJson));
    generator.Generate(testBinPath);
    return testBinPath;
}

TEST_F(SourceFileParserTest, GetTopWarpStallReason_DataExists) {
    std::string testBinPath = GenerateStallReasonBinFile();
    std::string testDbPath = TestSuit::GetSrcTestPath() + "/test_stall_reason.db";
    DataBaseManager::Instance().SetDataType(DataType::TEXT, testDbPath);
    DataBaseManager::Instance().CreateTraceConnectionPool(testBinPath, testDbPath);
    auto &parser = SourceFileParser::Instance();
    parser.SetFilePath(testBinPath);

    // 直接调用 Parse 来解析 bin 文件并填充 dataBlockMap
    parser.Parse(std::vector<std::string>(), testBinPath, testBinPath, testDbPath);
    WaitParseEnd({testBinPath});

    std::vector<Protocol::StallReasonItem> data;
    bool result = parser.GetTopWarpStallReason(data, false);
    EXPECT_TRUE(result);
    EXPECT_EQ(data.size(), 8);

    // 验证特定条目
    auto it = std::find_if(
        data.begin(), data.end(), [](const Protocol::StallReasonItem &item) { return item.name == "IBuf_Empty"; });
    ASSERT_NE(it, data.end());
    EXPECT_EQ(it->value, 100);

    it = std::find_if(data.begin(), data.end(),
        [](const Protocol::StallReasonItem &item) { return item.name == "Divergence_Stack_Spill"; });
    ASSERT_NE(it, data.end());
    EXPECT_EQ(it->value, 700);
    parser.Reset();
    BinFileGenerator::RemoveFile(testBinPath);
    BinFileGenerator::RemoveFile(testDbPath);
}

TEST_F(SourceFileParserTest, GetTopWarpStallReason_NoDataBlock) {
    std::string testBinPath = GenerateStallReasonBinFile();
    auto &parser = SourceFileParser::Instance();
    parser.SetFilePath(testBinPath);

    // 不调用 Parse，直接获取数据 — 应该返回 false 因为 dataBlockMap 为空
    std::vector<Protocol::StallReasonItem> data;
    bool result = parser.GetTopWarpStallReason(data, false);
    EXPECT_FALSE(result);
    EXPECT_TRUE(data.empty());

    parser.Reset();
    BinFileGenerator::RemoveFile(testBinPath);
}

class SourceFileParserDepthTest : public ::testing::Test {
  protected:
    const std::string rankId = "source_bin_depth_test";
    std::string testDbPath;
    std::shared_ptr<TextTraceDatabase> database;

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        testDbPath = (std::filesystem::temp_directory_path() /
            ("msinsight-source-bin-depth-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db"))
                         .string();
        std::filesystem::remove(testDbPath);
        DataBaseManager::Instance().SetDataType(DataType::TEXT, testDbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(rankId, testDbPath));
        DataBaseManager::Instance().SetDbPathMapping(rankId, testDbPath, "");
        database =
            std::dynamic_pointer_cast<TextTraceDatabase>(DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId));
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->CreateTable());
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::RUNNING);
    }

    void TearDown() override {
        std::function<void(const std::string, const std::string, bool, const std::string)> emptyCallback;
        SourceFileParser::Instance().SetParseEndCallBack(emptyCallback);
        database.reset();
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        std::filesystem::remove(testDbPath);
    }
};

TEST_F(SourceFileParserDepthTest, DepthCompletesBeforeSuccessCallback) {
    ASSERT_TRUE(database->ExecSql(
        "INSERT INTO slice(id, timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id, group_id) "
        "VALUES (1, 10, 20, 'ordinary-1', 1, '', '{}', '', 30, '', ''), "
        "(2, 20, 20, 'ordinary-2', 1, '', '{}', '', 40, '', ''), "
        "(3, 10, 20, 'python-1', 1, 'python_function', '{}', '', 30, '', ''), "
        "(4, 20, 20, 'python-2', 1, 'python_function', '{}', '', 40, '', '');"));
    bool callbackCalled = false;
    bool callbackResult = false;
    bool depthFinishedAtCallback = false;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&](const std::string, const std::string, bool result, const std::string) {
            callbackCalled = true;
            callbackResult = result;
            depthFinishedAtCallback = database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS);
        };
    SourceFileParser::Instance().SetParseEndCallBack(callback);

    SourceFileParser::EndParseTask(rankId, std::make_shared<std::vector<std::future<void>>>(), testDbPath);

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackResult);
    EXPECT_TRUE(depthFinishedAtCallback);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH);
    auto stmt = database->CreatPreparedStatement("SELECT id, depth FROM slice ORDER BY id");
    ASSERT_NE(stmt, nullptr);
    auto resultSet = stmt->ExecuteQuery();
    ASSERT_NE(resultSet, nullptr);
    const std::vector<uint64_t> expectedDepths = {0, 1, 0, 1};
    for (uint64_t expectedDepth : expectedDepths) {
        ASSERT_TRUE(resultSet->Next());
        EXPECT_EQ(resultSet->GetUint64("depth"), expectedDepth);
    }
}

TEST_F(SourceFileParserDepthTest, DepthFailureBlocksSuccessCallback) {
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(CONNECTION_UNIT, FINISH_STATUS));
    ASSERT_TRUE(SourceFileParserTestAccessor::TestPrepareDatabaseForParse(database));
    ASSERT_TRUE(database->ExecSql(
        "DROP TABLE slice; CREATE TABLE slice(id INTEGER PRIMARY KEY, track_id INTEGER, cat TEXT, group_id TEXT);"));
    bool callbackCalled = false;
    bool callbackResult = true;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&](const std::string, const std::string, bool result, const std::string) {
            callbackCalled = true;
            callbackResult = result;
        };
    SourceFileParser::Instance().SetParseEndCallBack(callback);

    SourceFileParser::EndParseTask(rankId, std::make_shared<std::vector<std::future<void>>>(), testDbPath);

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(callbackResult);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::TERMINATE);
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(CONNECTION_UNIT, NOT_FINISH_STATUS));
}

TEST_F(SourceFileParserDepthTest, RebuildingTablesInvalidatesFinishedOperatorDepth) {
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(CONNECTION_UNIT, FINISH_STATUS));
    ASSERT_TRUE(database->ExecSql(
        "INSERT INTO slice(id, timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id, group_id) "
        "VALUES (1, 10, 10, 'old', 1, '', '{}', '', 20, '', '');"));

    ASSERT_TRUE(SourceFileParserTestAccessor::TestPrepareDatabaseForParse(database));

    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(CONNECTION_UNIT, NOT_FINISH_STATUS));
    auto stmt = database->CreatPreparedStatement("SELECT COUNT(*) AS count FROM slice");
    ASSERT_NE(stmt, nullptr);
    auto resultSet = stmt->ExecuteQuery();
    ASSERT_NE(resultSet, nullptr);
    ASSERT_TRUE(resultSet->Next());
    EXPECT_EQ(resultSet->GetUint64("count"), 0);
}
