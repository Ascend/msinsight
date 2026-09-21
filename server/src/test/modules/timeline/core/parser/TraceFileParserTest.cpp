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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include "TraceFileParser.h"
#include "ConstantDefs.h"
#include "TextTraceDatabase.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "ThreadPool.h"
#include "../../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Module::Timeline;

/**
 * @brief TraceFileParser test helper class
 * Used to access protected methods
 */
class TraceFileParserTestHelper : public TraceFileParser {
  public:
    explicit TraceFileParserTestHelper(std::shared_ptr<ThreadPool> threadPool) : TraceFileParser(threadPool) {}

    bool persistDepthResult = true;
    bool postParseResult = true;
    bool replaceSlicesInPostParse = false;
    std::vector<std::string> stageOrder;

    // Expose protected method for testing
    static void TestUpdateRankIdDeviceIdMapByProcessData(
        std::shared_ptr<TextTraceDatabase> db, const std::string &rankId) {
        UpdateRankIdDeviceIdMapByProcessData(db, rankId);
    }

    void TestEndParseTask(const std::string &rankId, const std::vector<std::string> &filePathArr) {
        EndParseTask(rankId, filePathArr, std::make_shared<std::vector<std::future<void>>>(),
            std::chrono::high_resolution_clock::now());
    }

    bool TestInitParser(
        const std::vector<std::string> &filePathArr, const std::string &rankId, const std::string &fileId) {
        return InitParser(filePathArr, rankId, fileId);
    }

  protected:
    bool PersistOperatorDepth(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId) override {
        stageOrder.emplace_back("depth");
        return persistDepthResult && TraceFileParser::PersistOperatorDepth(std::move(db), rankId);
    }

    bool PostParse(std::shared_ptr<TextTraceDatabase> database, const std::string &) override {
        stageOrder.emplace_back("post");
        if (replaceSlicesInPostParse &&
            !database->ExecSql(
                "DELETE FROM slice;"
                "INSERT INTO slice(id, timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id, "
                "group_id) VALUES (1, 10, 90, 'parent', 7, '', '{}', '', 100, '', ''), "
                "(2, 20, 20, 'child', 7, '', '{}', '', 40, '', '');")) {
            return false;
        }
        return postParseResult;
    }

    void NotifyParseCompletionUnits(
        std::shared_ptr<TextTraceDatabase>, const std::string &, const std::string &) override {}
};

/**
 * @brief TraceFileParser test class
 * Tests UpdateRankIdDeviceIdMapByProcessData function with various scenarios
 */
class TraceFileParserTest : public ::testing::Test {
  protected:
    std::recursive_mutex sqlMutex;

    void SetUp() override { DataBaseManager::Instance().Clear(); }

    void TearDown() override { DataBaseManager::Instance().Clear(); }

    sqlite3 *CreateTestDatabase() {
        sqlite3 *db = nullptr;
        Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(db);

        std::string createProcessTableSql = "CREATE TABLE process ("
                                            "pid TEXT PRIMARY KEY, "
                                            "process_name TEXT, "
                                            "label TEXT, "
                                            "process_sort_index INTEGER, "
                                            "parentPid TEXT);";

        Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::CreateTable(db, createProcessTableSql);
        return db;
    }

    void InsertProcessData(sqlite3 *db, const std::string &pid, const std::string &processName,
        const std::string &label, int sortIndex, const std::string &parentPid = "0") {
        std::string sql = "INSERT INTO process (pid, process_name, label, process_sort_index, parentPid) VALUES ('" +
            pid + "', '" + processName + "', '" + label + "', " + std::to_string(sortIndex) + ", '" + parentPid + "');";
        Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(db, sql);
    }
};

class MockTextDatabase : public TextTraceDatabase {
  public:
    explicit MockTextDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
    void SetDbPtr(sqlite3 *dbPtr) {
        isOpen = true;
        db = dbPtr;
        path = ":memory:";
    }
};

class TraceFileParserDepthTest : public ::testing::Test {
  protected:
    const std::string rankId = "trace_parser_depth_test";
    std::string dbPath;
    std::shared_ptr<TextTraceDatabase> database;

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        ParserStatusManager::Instance().ClearAllParserStatus();
        dbPath = (std::filesystem::temp_directory_path() /
            ("msinsight-trace-parser-depth-" +
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

TEST_F(TraceFileParserDepthTest, PostParseCompletesBeforeDepthAndSuccessCallback) {
    auto parser = TraceFileParserTestHelper(std::make_shared<ThreadPool>(1));
    bool callbackResult = false;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&parser, &callbackResult](const std::string, const std::string, bool result, const std::string) {
            parser.stageOrder.emplace_back("callback");
            callbackResult = result;
        };
    parser.SetParseEndCallBack(callback);

    parser.TestEndParseTask(rankId, {});

    EXPECT_TRUE(callbackResult);
    EXPECT_EQ(parser.stageOrder, (std::vector<std::string>{"post", "depth", "callback"}));
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH);
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST_F(TraceFileParserDepthTest, DepthFailureAfterPostParseBlocksSuccessCallback) {
    auto parser = TraceFileParserTestHelper(std::make_shared<ThreadPool>(1));
    parser.persistDepthResult = false;
    bool callbackCalled = false;
    bool callbackResult = true;
    std::string callbackMessage;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&parser, &callbackCalled, &callbackResult, &callbackMessage](
            const std::string, const std::string, bool result, const std::string &message) {
            parser.stageOrder.emplace_back("callback");
            callbackCalled = true;
            callbackResult = result;
            callbackMessage = message;
        };
    parser.SetParseEndCallBack(callback);

    parser.TestEndParseTask(rankId, {});

    EXPECT_TRUE(callbackCalled);
    EXPECT_FALSE(callbackResult);
    EXPECT_NE(callbackMessage.find("persist operator depth"), std::string::npos);
    EXPECT_EQ(parser.stageOrder, (std::vector<std::string>{"post", "depth", "callback"}));
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::TERMINATE);
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST_F(TraceFileParserDepthTest, RecalculatesDepthAfterPostParseReplacesSlices) {
    auto parser = TraceFileParserTestHelper(std::make_shared<ThreadPool>(1));
    parser.replaceSlicesInPostParse = true;

    parser.TestEndParseTask(rankId, {});

    auto stmt = database->CreatPreparedStatement("SELECT depth FROM slice ORDER BY id");
    ASSERT_NE(stmt, nullptr);
    auto resultSet = stmt->ExecuteQuery();
    ASSERT_NE(resultSet, nullptr);
    ASSERT_TRUE(resultSet->Next());
    EXPECT_EQ(resultSet->GetUint32("depth"), 0);
    ASSERT_TRUE(resultSet->Next());
    EXPECT_EQ(resultSet->GetUint32("depth"), 1);
    EXPECT_FALSE(resultSet->Next());
    EXPECT_EQ(parser.stageOrder, (std::vector<std::string>{"post", "depth"}));
}

TEST_F(TraceFileParserDepthTest, ExistingTextDatabaseBackfillsDepthBeforeSuccessCallback) {
    ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::INIT);
    auto parser = TraceFileParserTestHelper(std::make_shared<ThreadPool>(1));
    bool callbackCalled = false;
    bool callbackResult = false;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&parser, &callbackCalled, &callbackResult](
            const std::string, const std::string, bool result, const std::string) {
            parser.stageOrder.emplace_back("callback");
            callbackCalled = true;
            callbackResult = result;
        };
    parser.SetParseEndCallBack(callback);

    EXPECT_TRUE(parser.TestInitParser({"/tmp/profiler.db"}, rankId, dbPath));

    EXPECT_TRUE(callbackCalled);
    EXPECT_TRUE(callbackResult);
    EXPECT_EQ(parser.stageOrder, (std::vector<std::string>{"depth", "callback"}));
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(rankId), ParserStatus::FINISH);
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataEmptyProcess) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataNoNpuLabel) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "CPU 0", 0);
    InsertProcessData(dbPtr, "1001", "Process2", "GPU 1", 1);
    InsertProcessData(dbPtr, "1002", "Process3", "SomeLabel", 2);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataSingleNpuLabel) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 5", 0);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "5");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataMultipleSameNpuLabel) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 3", 0);
    InsertProcessData(dbPtr, "1001", "Process2", "NPU 3", 1);
    InsertProcessData(dbPtr, "1002", "Process3", "NPU 3", 2);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "3");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataMultipleDifferentNpuLabel) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 1", 0);
    InsertProcessData(dbPtr, "1001", "Process2", "NPU 2", 1);
    InsertProcessData(dbPtr, "1002", "Process3", "NPU 3", 2);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataNpuLabelWithNonDigit) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU abc", 0);
    InsertProcessData(dbPtr, "1001", "Process2", "NPU 1a", 1);
    InsertProcessData(dbPtr, "1002", "Process3", "NPU ", 2);
    InsertProcessData(dbPtr, "1003", "Process4", "NPU 1 2", 3);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataMixedLabels) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 7", 0);
    InsertProcessData(dbPtr, "1001", "Process2", "CPU 0", 1);
    InsertProcessData(dbPtr, "1002", "Process3", "NPU 7", 2);
    InsertProcessData(dbPtr, "1003", "Process4", "SomeLabel", 3);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "7");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataLargeDeviceId) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 999", 0);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "999");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataDeviceIdZero) {
    sqlite3 *dbPtr = CreateTestDatabase();
    ASSERT_NE(dbPtr, nullptr);

    auto mockDb = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb->SetDbPtr(dbPtr);

    InsertProcessData(dbPtr, "1000", "Process1", "NPU 0", 0);

    const std::string rankId = "0";
    const std::string fileId = "test_file_0";
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId, fileId);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb, rankId);

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    EXPECT_EQ(deviceId, "0");
}

TEST_F(TraceFileParserTest, UpdateRankIdDeviceIdMapByProcessDataMultipleRankIds) {
    sqlite3 *dbPtr1 = CreateTestDatabase();
    sqlite3 *dbPtr2 = CreateTestDatabase();
    ASSERT_NE(dbPtr1, nullptr);
    ASSERT_NE(dbPtr2, nullptr);

    auto mockDb1 = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb1->SetDbPtr(dbPtr1);
    auto mockDb2 = std::make_shared<MockTextDatabase>(sqlMutex);
    mockDb2->SetDbPtr(dbPtr2);

    InsertProcessData(dbPtr1, "1000", "Process1", "NPU 1", 0);
    InsertProcessData(dbPtr2, "2000", "Process2", "NPU 2", 0);

    const std::string rankId1 = "rank_1";
    const std::string fileId1 = "test_file_1";
    const std::string rankId2 = "rank_2";
    const std::string fileId2 = "test_file_2";

    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId1, fileId1);
    DataBaseManager::Instance().SetRankIdFileIdMapping(rankId2, fileId2);

    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb1, rankId1);
    TraceFileParserTestHelper::TestUpdateRankIdDeviceIdMapByProcessData(mockDb2, rankId2);

    std::string deviceId1 = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId1);
    std::string deviceId2 = DataBaseManager::Instance().GetDeviceIdFromRankId(rankId2);
    EXPECT_EQ(deviceId1, "1");
    EXPECT_EQ(deviceId2, "2");
}
