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
#include <future>

#include <gtest/gtest.h>

#include "ConstantDefs.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "TextTraceDatabase.h"
#include "TraceFileSimulationParser.h"

using namespace Dic::Module;
using namespace Dic::Module::Timeline;

namespace Dic::Module::Timeline {
class TraceFileSimulationParserTestAccessor {
  public:
    static void TestEndParseTask(const std::string &rankId, const std::string &fileId) {
        TraceFileSimulationParser::EndParseTask(rankId, {}, std::make_shared<std::vector<std::future<void>>>(),
            std::chrono::high_resolution_clock::now(), fileId);
    }

    static bool TestPrepareDatabaseForParse(const std::shared_ptr<TextTraceDatabase> &database) {
        return TraceFileSimulationParser::PrepareDatabaseForParse(database);
    }
};
}

namespace {

class TraceFileSimulationParserTest : public ::testing::Test {
  protected:
    const std::string rankId = "simulation_parser_depth_test";
    std::string dbPath;
    std::shared_ptr<TextTraceDatabase> database;

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        dbPath = (std::filesystem::temp_directory_path() /
            ("msinsight-simulation-parser-depth-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".db"))
                     .string();
        std::filesystem::remove(dbPath);
        DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath);
        ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(rankId, dbPath));
        DataBaseManager::Instance().SetDbPathMapping(rankId, dbPath, "");
        database =
            std::dynamic_pointer_cast<TextTraceDatabase>(DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId));
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->CreateTable());
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::RUNNING);
    }

    void TearDown() override {
        database.reset();
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        std::filesystem::remove(dbPath);
    }
};

TEST_F(TraceFileSimulationParserTest, DepthCompletesBeforeSuccessCallback) {
    bool callbackCalled = false;
    bool callbackResult = false;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackCalled, &callbackResult](const std::string, const std::string, bool result, const std::string) {
            callbackCalled = true;
            callbackResult = result;
        };
    TraceFileSimulationParser::Instance().SetParseEndCallBack(callback);

    TraceFileSimulationParserTestAccessor::TestEndParseTask(rankId, dbPath);

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackResult);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH);
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST_F(TraceFileSimulationParserTest, DepthFailureBlocksSuccessCallback) {
    ASSERT_TRUE(database->ExecSql(
        "DROP TABLE slice; CREATE TABLE slice(id INTEGER PRIMARY KEY, track_id INTEGER, cat TEXT, group_id TEXT);"));
    bool callbackCalled = false;
    bool callbackResult = true;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&callbackCalled, &callbackResult](const std::string, const std::string, bool result, const std::string) {
            callbackCalled = true;
            callbackResult = result;
        };
    TraceFileSimulationParser::Instance().SetParseEndCallBack(callback);

    TraceFileSimulationParserTestAccessor::TestEndParseTask(rankId, dbPath);

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(callbackResult);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::TERMINATE);
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST_F(TraceFileSimulationParserTest, RebuildingTablesInvalidatesFinishedOperatorDepth) {
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
    ASSERT_TRUE(database->ExecSql(
        "INSERT INTO slice(id, timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id, group_id) "
        "VALUES (1, 10, 10, 'old', 1, '', '{}', '', 20, '', '');"));

    ASSERT_TRUE(TraceFileSimulationParserTestAccessor::TestPrepareDatabaseForParse(database));

    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
    auto stmt = database->CreatPreparedStatement("SELECT COUNT(*) AS count FROM slice");
    ASSERT_NE(stmt, nullptr);
    auto resultSet = stmt->ExecuteQuery();
    ASSERT_NE(resultSet, nullptr);
    ASSERT_TRUE(resultSet->Next());
    EXPECT_EQ(resultSet->GetUint64("count"), 0);
}
}
