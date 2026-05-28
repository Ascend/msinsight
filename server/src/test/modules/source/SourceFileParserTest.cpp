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
#include "../../TestSuit.h"

using namespace std;
using namespace Dic::Module::Source;

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
