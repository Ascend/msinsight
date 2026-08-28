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
#include <functional>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "DataBaseManager.h"
#include "FullDbParser.h"
#include "ParserStatusManager.h"

using namespace Dic::Module;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::Timeline;

class FullDbParserTest : public ::testing::Test {
  protected:
    void SetUp() override { ResetParserState(); }
    void TearDown() override { ResetParserState(); }

    void ResetParserState() {
        FullDbParser::Instance().FileParser::Reset();
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
    }
};

TEST_F(FullDbParserTest, PlatformOpenFailureCompletesEveryRankWithFailure) {
    const std::string invalidDbPath = ::testing::TempDir();
    const std::vector<std::string> rankIds = {"platform-rank-0", "platform-rank-1"};
    auto &databaseManager = DataBaseManager::Instance();
    databaseManager.SetFileType(FileType::PLATFORM, invalidDbPath);
    for (const auto &rankId : rankIds) {
        databaseManager.SetRankIdFileIdMapping(rankId, invalidDbPath);
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::INIT);
    }

    struct CallbackResult {
        std::string rankId;
        std::string fileId;
        bool result;
    };
    std::vector<CallbackResult> callbackResults;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackResults](const std::string rankId, const std::string fileId, bool result, const std::string) {
            callbackResults.push_back({rankId, fileId, result});
        };
    FullDbParser::Instance().SetParseEndCallBack(callback);

    FullDbParser::InitOpenDb(invalidDbPath, rankIds);

    ASSERT_EQ(callbackResults.size(), rankIds.size());
    for (size_t index = 0; index < rankIds.size(); ++index) {
        EXPECT_EQ(callbackResults[index].rankId, rankIds[index]);
        EXPECT_EQ(callbackResults[index].fileId, invalidDbPath);
        EXPECT_FALSE(callbackResults[index].result);
        EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankIds[index]), ParserStatus::FINISH_ALL);
    }
}
