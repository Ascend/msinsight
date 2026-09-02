#include <gtest/gtest.h>

#include "DbTraceDataBase.h"
#include "../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;
using Dic::Module::FullDb::DbTraceDataBase;
using Dic::Module::FullDb::OVERLAP_INFO;
using Dic::Module::FullDb::RANK_OVERLAP_INPUT;

class RankOverlapDatabaseTest : public testing::Test {
  protected:
    class MockDatabase : public DbTraceDataBase {
      public:
        explicit MockDatabase(std::recursive_mutex &mutex) : DbTraceDataBase(mutex) {}
        ~MockDatabase() override {
            if (isOpen && db != nullptr) {
                sqlite3_close(db);
                isOpen = false;
            }
        }
        void SetDbPtr(sqlite3 *database) {
            isOpen = true;
            db = database;
            path = ":memory:";
        }
    };

    void SetUp() override {
        DatabaseTestCaseMockUtil::OpenDB(sqlite_);
        database_ = std::make_unique<MockDatabase>(mutex_);
        database_->SetDbPtr(sqlite_);
    }

    void TearDown() override { database_.reset(); }

    int CountRows(const std::string &where) {
        sqlite3_stmt *statement = nullptr;
        int count = -1;
        const std::string sql = "SELECT COUNT(*) FROM OVERLAP_ANALYSIS WHERE " + where;
        if (sqlite3_prepare_v2(sqlite_, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_ROW) {
            count = sqlite3_column_int(statement, 0);
        }
        sqlite3_finalize(statement);
        return count;
    }

    sqlite3 *sqlite_ = nullptr;
    std::recursive_mutex mutex_;
    std::unique_ptr<MockDatabase> database_;
};

TEST_F(RankOverlapDatabaseTest, QueriesRawIntervalsAndTaskSpanForDevice) {
    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_TASK_SQL);
    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_COMPUTE_TASK_INFO_SQL);
    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_COMMUNICATION_OP_SQL);
    DatabaseTestCaseMockUtil::InsertData(sqlite_,
        "INSERT INTO TASK(startNs,endNs,deviceId,globalTaskId) VALUES"
        "(10,20,0,1),(30,50,0,2),(100,120,1,3);");
    DatabaseTestCaseMockUtil::InsertData(
        sqlite_, "INSERT INTO COMPUTE_TASK_INFO(globalTaskId,name) VALUES(1,1),(3,3);");
    DatabaseTestCaseMockUtil::InsertData(
        sqlite_, "INSERT INTO COMMUNICATION_OP(opId,startNs,endNs,deviceId) VALUES(1,15,35,0),(2,105,115,1);");

    RANK_OVERLAP_INPUT input;
    ASSERT_TRUE(database_->QueryRankOverlapInput("0", input));
    ASSERT_EQ(input.computing.size(), 1U);
    EXPECT_EQ(input.computing[0].startNs, 10);
    ASSERT_EQ(input.communication.size(), 1U);
    EXPECT_EQ(input.communication[0].endNs, 35);
    ASSERT_TRUE(input.taskSpan.has_value());
    EXPECT_EQ(input.taskSpan->first, 10);
    EXPECT_EQ(input.taskSpan->second, 50);
}

TEST_F(RankOverlapDatabaseTest, ReplacesOnlyTargetDeviceRowsTransactionally) {
    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_OVERLAP_ANALYSIS_SQL);
    DatabaseTestCaseMockUtil::InsertData(sqlite_,
        "INSERT INTO OVERLAP_ANALYSIS(deviceId,startNs,endNs,type) VALUES"
        "(0,1,2,0),(0,2,3,1),(1,5,6,0);");

    ASSERT_TRUE(database_->ReplaceOverlapAnalysisForDevice("0", {OVERLAP_INFO(10, 20, 0), OVERLAP_INFO(20, 30, 3)}));

    EXPECT_EQ(CountRows("deviceId = 0"), 2);
    EXPECT_EQ(CountRows("deviceId = 0 AND startNs = 1"), 0);
    EXPECT_EQ(CountRows("deviceId = 0 AND startNs = 10 AND type = 0"), 1);
    EXPECT_EQ(CountRows("deviceId = 1 AND startNs = 5"), 1);
}

TEST_F(RankOverlapDatabaseTest, RollsBackReplacementWhenInsertFails) {
    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_OVERLAP_ANALYSIS_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        sqlite_, "INSERT INTO OVERLAP_ANALYSIS(deviceId,startNs,endNs,type) VALUES(0,1,2,0),(1,5,6,0);");
    DatabaseTestCaseMockUtil::InsertData(sqlite_,
        "CREATE TRIGGER reject_rank_overlap BEFORE INSERT ON OVERLAP_ANALYSIS "
        "WHEN NEW.startNs = 10 BEGIN SELECT RAISE(FAIL, 'rejected'); END;");

    EXPECT_FALSE(database_->ReplaceOverlapAnalysisForDevice("0", {OVERLAP_INFO(10, 20, 0)}));

    EXPECT_EQ(CountRows("deviceId = 0 AND startNs = 1"), 1);
    EXPECT_EQ(CountRows("deviceId = 0 AND startNs = 10"), 0);
    EXPECT_EQ(CountRows("deviceId = 1 AND startNs = 5"), 1);
}

TEST_F(RankOverlapDatabaseTest, RejectsSourceWithoutTaskOrRawIntervals) {
    RANK_OVERLAP_INPUT input;
    EXPECT_FALSE(database_->QueryRankOverlapInput("0", input));

    DatabaseTestCaseMockUtil::CreateTable(sqlite_, CREATE_TABLE_DB_TASK_SQL);
    DatabaseTestCaseMockUtil::InsertData(
        sqlite_, "INSERT INTO TASK(startNs,endNs,deviceId,globalTaskId) VALUES(10,20,0,1);");
    EXPECT_FALSE(database_->QueryRankOverlapInput("0", input));
}
