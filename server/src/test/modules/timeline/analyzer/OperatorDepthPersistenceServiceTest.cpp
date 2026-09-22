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
#include <optional>

#include <gtest/gtest.h>

#include "ConstantDefs.h"
#include "DbTraceDataBase.h"
#include "OperatorDepthPersistenceService.h"
#include "TextTraceDatabase.h"

using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Timeline;

namespace {
class TestTextDatabase : public TextTraceDatabase {
  public:
    explicit TestTextDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}

    void SetDbPtr(sqlite3 *dbPtr) {
        isOpen = true;
        db = dbPtr;
        path = ":memory:";
    }

    bool StartImmediateTransaction() override {
        startedImmediateTransaction = true;
        return TextTraceDatabase::StartImmediateTransaction();
    }

    bool HasStartedImmediateTransaction() const { return startedImmediateTransaction; }

  private:
    bool startedImmediateTransaction = false;
};

class TestDbTraceDatabase : public FullDb::DbTraceDataBase {
  public:
    explicit TestDbTraceDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}

    void SetDbPtr(sqlite3 *dbPtr) {
        isOpen = true;
        db = dbPtr;
        path = ":memory:";
    }
};

class OperatorDepthPersistenceServiceAccessor : public OperatorDepthPersistenceService {
  public:
    static bool Run(Database &database, const std::function<bool()> &work) {
        return RunWithStatusAndTransaction(database, work);
    }
};

sqlite3 *OpenInMemoryDatabase() {
    sqlite3 *db = nullptr;
    EXPECT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    return db;
}

uint64_t QueryRowCount(sqlite3 *db, const std::string &tableName) {
    sqlite3_stmt *stmt = nullptr;
    const std::string sql = "SELECT COUNT(*) FROM " + tableName;
    EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    uint64_t count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    sqlite3_finalize(stmt);
    return count;
}

void CreateTextSliceTable(TestTextDatabase &database) {
    ASSERT_TRUE(database.ExecSql(
        "CREATE TABLE slice(id INTEGER PRIMARY KEY, timestamp INTEGER, end_time INTEGER, depth INTEGER, "
        "track_id INTEGER, cat TEXT, group_id TEXT);"));
}

void InsertTextSlice(TestTextDatabase &database, uint64_t id, uint64_t timestamp, uint64_t endTime, uint64_t trackId,
    const std::string &cat = "", const std::string &groupId = "") {
    auto stmt = database.CreatPreparedStatement(
        "INSERT INTO slice(id, timestamp, end_time, depth, track_id, cat, group_id) VALUES (?, ?, ?, 99, ?, ?, ?);");
    ASSERT_NE(stmt, nullptr);
    ASSERT_TRUE(stmt->Execute(id, timestamp, endTime, trackId,
        cat.empty() ? std::optional<std::string>{} : std::optional<std::string>{cat}, groupId));
}

uint32_t QueryTextDepth(sqlite3 *db, uint64_t id) {
    sqlite3_stmt *stmt = nullptr;
    EXPECT_EQ(sqlite3_prepare_v2(db, "SELECT depth FROM slice WHERE id = ?", -1, &stmt, nullptr), SQLITE_OK);
    EXPECT_EQ(sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(id)), SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    uint32_t depth = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0));
    sqlite3_finalize(stmt);
    return depth;
}

void CreateDbDepthTables(TestDbTraceDatabase &database) {
    ASSERT_TRUE(database.ExecSql(
        "CREATE TABLE TASK(startNs INTEGER, endNs INTEGER, deviceId INTEGER, streamId INTEGER, "
        "connectionId INTEGER, depth INTEGER);"
        "CREATE TABLE CANN_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER, type INTEGER, depth INTEGER);"
        "CREATE TABLE PYTORCH_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER, type INTEGER, depth INTEGER);"
        "CREATE TABLE MSTX_EVENTS(startNs INTEGER, endNs INTEGER, globalTid INTEGER, domainId INTEGER, "
        "connectionId INTEGER, depth INTEGER);"
        "CREATE TABLE DPU_TASK(startNs INTEGER, endNs INTEGER, globalTid INTEGER, dpuDeviceId INTEGER, "
        "streamId INTEGER, depth INTEGER);"));
}

uint32_t QueryDbDepth(sqlite3 *db, const std::string &tableName, uint64_t rowId) {
    sqlite3_stmt *stmt = nullptr;
    const std::string sql = "SELECT depth FROM " + tableName + " WHERE ROWID = ?";
    EXPECT_EQ(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr), SQLITE_OK);
    EXPECT_EQ(sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(rowId)), SQLITE_OK);
    EXPECT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    uint32_t depth = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0));
    sqlite3_finalize(stmt);
    return depth;
}
}

TEST(OperatorDepthPersistenceServiceTest, FinishStatusSkipsRecalculation) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    database.SetDbPtr(OpenInMemoryDatabase());
    ASSERT_TRUE(database.UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
    bool called = false;

    EXPECT_TRUE(OperatorDepthPersistenceServiceAccessor::Run(database, [&called]() {
        called = true;
        return true;
    }));
    EXPECT_FALSE(called);
}

TEST(OperatorDepthPersistenceServiceTest, MissingStatusRunsCalculation) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    database.SetDbPtr(OpenInMemoryDatabase());
    bool called = false;

    EXPECT_TRUE(OperatorDepthPersistenceServiceAccessor::Run(database, [&database, &called]() {
        called = true;
        EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
        return true;
    }));
    EXPECT_TRUE(called);
    EXPECT_TRUE(database.HasStartedImmediateTransaction());
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, FailureRollsBackDepthAndKeepsNotFinish) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    ASSERT_TRUE(database.ExecSql("CREATE TABLE depth_updates(id INTEGER PRIMARY KEY, depth INTEGER);"));

    EXPECT_FALSE(OperatorDepthPersistenceServiceAccessor::Run(database, [&database]() {
        EXPECT_TRUE(database.ExecSql("INSERT INTO depth_updates VALUES (1, 3);"));
        return false;
    }));
    EXPECT_EQ(QueryRowCount(db, "depth_updates"), 0);
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, NotFinishStatusRetriesAllWorkAfterRollback) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    ASSERT_TRUE(database.ExecSql("CREATE TABLE depth_updates(id INTEGER PRIMARY KEY, depth INTEGER);"));
    uint32_t attempts = 0;

    EXPECT_FALSE(OperatorDepthPersistenceServiceAccessor::Run(database, [&database, &attempts]() {
        ++attempts;
        EXPECT_TRUE(database.ExecSql("INSERT INTO depth_updates VALUES (1, 3);"));
        return false;
    }));
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
    EXPECT_TRUE(OperatorDepthPersistenceServiceAccessor::Run(database, [&database, &attempts]() {
        ++attempts;
        return database.ExecSql("INSERT INTO depth_updates VALUES (1, 7);");
    }));

    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(QueryRowCount(db, "depth_updates"), 1);
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, SuccessfulTransactionMarksFinishAfterCommit) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    ASSERT_TRUE(database.ExecSql("CREATE TABLE depth_updates(id INTEGER PRIMARY KEY, depth INTEGER);"));

    EXPECT_TRUE(OperatorDepthPersistenceServiceAccessor::Run(database, [&database]() {
        EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
        return database.ExecSql("INSERT INTO depth_updates VALUES (1, 3);");
    }));
    EXPECT_EQ(QueryRowCount(db, "depth_updates"), 1);
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, TextEmptyOrMissingOptionalTablesComplete) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    database.SetDbPtr(OpenInMemoryDatabase());

    EXPECT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, DbEmptyOrMissingOptionalTablesComplete) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    database.SetDbPtr(OpenInMemoryDatabase());

    EXPECT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, TextOrdinaryAndPythonStackUseIndependentDepthSpaces) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateTextSliceTable(database);
    InsertTextSlice(database, 1, 0, 10, 7);
    InsertTextSlice(database, 2, 1, 9, 7);
    InsertTextSlice(database, 3, 2, 8, 7, "python_function");
    InsertTextSlice(database, 4, 3, 7, 7, "python_function");

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));

    EXPECT_EQ(QueryTextDepth(db, 1), 0);
    EXPECT_EQ(QueryTextDepth(db, 2), 1);
    EXPECT_EQ(QueryTextDepth(db, 3), 0);
    EXPECT_EQ(QueryTextDepth(db, 4), 1);
}

TEST(OperatorDepthPersistenceServiceTest, TextGroupRowsShareDepth) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateTextSliceTable(database);
    InsertTextSlice(database, 1, 0, 4, 1, "", "group-a");
    InsertTextSlice(database, 2, 8, 12, 1, "", "group-a");
    InsertTextSlice(database, 3, 5, 7, 1);

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));

    EXPECT_EQ(QueryTextDepth(db, 1), 0);
    EXPECT_EQ(QueryTextDepth(db, 2), 0);
    EXPECT_EQ(QueryTextDepth(db, 3), 1);
}

TEST(OperatorDepthPersistenceServiceTest, TextTracksDoNotShareOccupiedDepths) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateTextSliceTable(database);
    InsertTextSlice(database, 1, 0, 10, 1);
    InsertTextSlice(database, 2, 1, 9, 1);
    InsertTextSlice(database, 3, 2, 8, 2);

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));

    EXPECT_EQ(QueryTextDepth(db, 1), 0);
    EXPECT_EQ(QueryTextDepth(db, 2), 1);
    EXPECT_EQ(QueryTextDepth(db, 3), 0);
}

TEST(OperatorDepthPersistenceServiceTest, TextSecondRunSkipsFinishedDepth) {
    std::recursive_mutex sqlMutex;
    TestTextDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateTextSliceTable(database);
    InsertTextSlice(database, 1, 0, 10, 1);
    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));
    ASSERT_TRUE(database.ExecSql("UPDATE slice SET depth = 42 WHERE id = 1;"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistTextDepth(database, "0"));

    EXPECT_EQ(QueryTextDepth(db, 1), 42);
}

TEST(OperatorDepthPersistenceServiceTest, DbTaskOrdinaryStreamsUseIndependentDepthSpaces) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO TASK VALUES (0, 10, 0, 7, 1, 99);"
                                 "INSERT INTO TASK VALUES (1, 9, 0, 7, 2, 99);"
                                 "INSERT INTO TASK VALUES (2, 8, 1, 7, 3, 99);"
                                 "INSERT INTO TASK VALUES (3, 7, 0, 8, 4, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "TASK", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 3), 0);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 4), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbTaskMstxPartitionsByDeviceStreamAndDomain) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO TASK VALUES (0, 10, 0, 7, 100, 99);"
                                 "INSERT INTO TASK VALUES (1, 9, 0, 7, 101, 99);"
                                 "INSERT INTO TASK VALUES (2, 8, 0, 7, 102, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (0, 1, 11, 3, 100, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (0, 1, 11, 3, 101, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (0, 1, 11, 4, 102, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "TASK", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 3), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbTaskLinkedToMultipleMstxDomainsRollsBackAllDepths) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO CANN_API VALUES (0, 10, 1, 2, 99);"
                                 "INSERT INTO TASK VALUES (0, 10, 0, 7, 100, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (0, 1, 11, 3, 100, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (0, 1, 11, 4, 100, 99);"));

    EXPECT_FALSE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 1), 99);
    EXPECT_EQ(QueryDbDepth(db, "TASK", 1), 99);
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
}

TEST(OperatorDepthPersistenceServiceTest, DbCannPartitionsByGlobalTidAndType) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO CANN_API VALUES (0, 10, 1, 2, 99);"
                                 "INSERT INTO CANN_API VALUES (1, 9, 1, 2, 99);"
                                 "INSERT INTO CANN_API VALUES (2, 8, 1, 3, 99);"
                                 "INSERT INTO CANN_API VALUES (3, 7, 2, 2, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 3), 0);
    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 4), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbPytorchOrdinaryAndPythonStackUseIndependentDepthSpaces) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO PYTORCH_API VALUES (0, 10, 1, 1, 99);"
                                 "INSERT INTO PYTORCH_API VALUES (1, 9, 1, 2, 99);"
                                 "INSERT INTO PYTORCH_API VALUES (2, 8, 1, 50003, 99);"
                                 "INSERT INTO PYTORCH_API VALUES (3, 7, 1, 50003, 99);"
                                 "INSERT INTO PYTORCH_API VALUES (4, 6, 2, 1, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "PYTORCH_API", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "PYTORCH_API", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "PYTORCH_API", 3), 0);
    EXPECT_EQ(QueryDbDepth(db, "PYTORCH_API", 4), 1);
    EXPECT_EQ(QueryDbDepth(db, "PYTORCH_API", 5), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbMstxPartitionsByGlobalTidAndDomain) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO MSTX_EVENTS VALUES (0, 10, 1, 3, 100, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (1, 9, 1, 3, 101, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (2, 8, 1, 4, 102, 99);"
                                 "INSERT INTO MSTX_EVENTS VALUES (3, 7, 2, 3, 103, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "MSTX_EVENTS", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "MSTX_EVENTS", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "MSTX_EVENTS", 3), 0);
    EXPECT_EQ(QueryDbDepth(db, "MSTX_EVENTS", 4), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbDpuPartitionsByGlobalTidDeviceAndStream) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO DPU_TASK VALUES (0, 10, 100, 0, 7, 99);"
                                 "INSERT INTO DPU_TASK VALUES (1, 9, 100, 0, 7, 99);"
                                 "INSERT INTO DPU_TASK VALUES (2, 8, 100, 1, 7, 99);"
                                 "INSERT INTO DPU_TASK VALUES (3, 7, 100, 0, 8, 99);"
                                 "INSERT INTO DPU_TASK VALUES (4, 6, 101, 0, 7, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "DPU_TASK", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "DPU_TASK", 2), 1);
    EXPECT_EQ(QueryDbDepth(db, "DPU_TASK", 3), 0);
    EXPECT_EQ(QueryDbDepth(db, "DPU_TASK", 4), 0);
    EXPECT_EQ(QueryDbDepth(db, "DPU_TASK", 5), 0);
}

TEST(OperatorDepthPersistenceServiceTest, DbMissingOptionalTableDoesNotBlockOtherTables) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    ASSERT_TRUE(database.ExecSql(
        "CREATE TABLE CANN_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER, type INTEGER, depth INTEGER);"
        "INSERT INTO CANN_API VALUES (0, 10, 1, 2, 99);"
        "INSERT INTO CANN_API VALUES (1, 9, 1, 2, 99);"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 1), 0);
    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 2), 1);
}

TEST(OperatorDepthPersistenceServiceTest, DbSecondRunSkipsFinishedDepth) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    sqlite3 *db = OpenInMemoryDatabase();
    database.SetDbPtr(db);
    CreateDbDepthTables(database);
    ASSERT_TRUE(database.ExecSql("INSERT INTO CANN_API VALUES (0, 10, 1, 2, 99);"));
    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));
    ASSERT_TRUE(database.ExecSql("UPDATE CANN_API SET depth = 42 WHERE ROWID = 1;"));

    ASSERT_TRUE(OperatorDepthPersistenceService::CalculateAndPersistDbDepth(database, "db"));

    EXPECT_EQ(QueryDbDepth(db, "CANN_API", 1), 42);
}

TEST(OperatorDepthPersistenceServiceTest, DbHelperColumnsResetOperatorDepthStatusOnVersionChange) {
    std::recursive_mutex sqlMutex;
    TestDbTraceDatabase database(sqlMutex);
    database.SetDbPtr(OpenInMemoryDatabase());
    ASSERT_TRUE(database.ExecSql(
        "CREATE TABLE TASK(startNs INTEGER, endNs INTEGER, deviceId INTEGER, streamId INTEGER, connectionId INTEGER);"
        "CREATE TABLE CANN_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER, type INTEGER);"
        "CREATE TABLE PYTORCH_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER);"
        "CREATE TABLE MSTX_EVENTS(startNs INTEGER, endNs INTEGER, globalTid INTEGER, domainId INTEGER, "
        "connectionId INTEGER);"
        "CREATE TABLE DPU_TASK(startNs INTEGER, endNs INTEGER, globalTid INTEGER, dpuDeviceId INTEGER, "
        "streamId INTEGER);"));
    ASSERT_TRUE(database.SetConfigForTesting());
    ASSERT_TRUE(database.UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS));

    database.AddHelperColumnsAndSetStatus();

    EXPECT_TRUE(database.CheckColumnExist("TASK", "depth"));
    EXPECT_TRUE(database.CheckColumnExist("CANN_API", "depth"));
    EXPECT_TRUE(database.CheckColumnExist("PYTORCH_API", "depth"));
    EXPECT_TRUE(database.CheckColumnExist("PYTORCH_API", "type"));
    EXPECT_TRUE(database.CheckColumnExist("MSTX_EVENTS", "depth"));
    EXPECT_TRUE(database.CheckColumnExist("DPU_TASK", "depth"));
    EXPECT_TRUE(database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS));
    EXPECT_TRUE(database.InitStmt());
}
