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

#include <gtest/gtest.h>
#include "SourceFileParser.h"
#include "SourceProtocolRequest.h"
#include "ProjectParserFactory.h"
#include "mockUtils/BinFileGenerator.h"
#include "mockUtils/DataBlock.h"
#include "../../TestSuit.h"

using namespace std;
using namespace Dic::Module::Source;
using namespace Dic::Module::Source::Test;

class SourceFileParserTest : public ::testing::Test {
public:
    static std::string dataPath;
    static std::string dbPath;

    static void SetUpTestCase()
    {
        dataPath = TestSuit::GetSrcTestPath() + R"(test_data/data.bin)";
        dbPath = TestSuit::GetSrcTestPath() + R"(test_data/compute_mindstudio_insight_data.db)";
        DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool(dataPath, dbPath);
    }

    static void TearDownTestCase()
    {
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

static void WaitParseEnd(std::vector<std::string>&& statusList)
{
    if (statusList.empty()) {
        return;
    }
    while (true) {
        size_t i = 0;
        for (const auto& tmp : statusList) {
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

TEST_F(SourceFileParserTest, Parse)
{
    auto& parser = SourceFileParser::Instance();
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
    bool result = parser.GetTopWarpStallReason(data);
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
    bool result = parser.GetTopWarpStallReason(data);
    EXPECT_FALSE(result);
    EXPECT_TRUE(data.empty());

    parser.Reset();
    BinFileGenerator::RemoveFile(testBinPath);
}
