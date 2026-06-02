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
#include "TraceFileParser.h"
#include "TextTraceDatabase.h"
#include "DataBaseManager.h"
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

    // Expose protected method for testing
    static void TestUpdateRankIdDeviceIdMapByProcessData(
        std::shared_ptr<TextTraceDatabase> db, const std::string &rankId) {
        UpdateRankIdDeviceIdMapByProcessData(db, rankId);
    }
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
