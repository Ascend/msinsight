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
#include "DbTraceDataBase.h"
#include "ConstantDefs.h"
#include "TableDefs.h"
#include "../../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;

class UpstreamOverlapAnalysisTest : public ::testing::Test {
  protected:
    class MockDatabase : public Dic::Module::FullDb::DbTraceDataBase {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : DbTraceDataBase(sqlMutex) {}
        ~MockDatabase() override {
            if (isOpen && db != nullptr) {
                sqlite3_close(db);
                isOpen = false;
            }
        }
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
            InitStringsCache();
        }
        using Database::GetValueFromStatusInfoTable;
    };

    void SetUp() override {
        DatabaseTestCaseMockUtil::OpenDB(db_);
        database_ = std::make_unique<MockDatabase>(testMutex_);
        database_->SetDbPtr(db_);
    }

    void TearDown() override { database_.reset(); }

    void CreateTaskAndComputeTables() {
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_TASK_SQL);
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_COMPUTE_TASK_INFO_SQL);
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_RANK_DEVICE_MAP_SQL);
    }

    void CreateCommOpTable() {
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_COMMUNICATION_OP_SQL);
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_RANK_DEVICE_MAP_SQL);
    }

    void CreateOverlapAnalysisTable() {
        DatabaseTestCaseMockUtil::CreateTable(db_, CREATE_TABLE_DB_OVERLAP_ANALYSIS_SQL);
    }

    void InsertOverlapAnalysisData() {
        DatabaseTestCaseMockUtil::InsertData(
            db_, "INSERT INTO OVERLAP_ANALYSIS (deviceId, startNs, endNs, type) VALUES (0, 100, 200, 0);");
        DatabaseTestCaseMockUtil::InsertData(
            db_, "INSERT INTO OVERLAP_ANALYSIS (deviceId, startNs, endNs, type) VALUES (0, 150, 250, 1);");
        DatabaseTestCaseMockUtil::InsertData(
            db_, "INSERT INTO OVERLAP_ANALYSIS (deviceId, startNs, endNs, type) VALUES (0, 150, 200, 2);");
    }

    void SetStatusInfoValue(const std::string &key, const std::string &value) {
        database_->UpdateValueIntoStatusInfoTable(key, value);
    }

    std::string GetStatusInfoValue(const std::string &key) { return database_->GetValueFromStatusInfoTable(key); }

    int CountOverlapAnalysisRows() {
        sqlite3_stmt *stmt = nullptr;
        int count = 0;
        std::string sql = "SELECT COUNT(*) FROM OVERLAP_ANALYSIS";
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        return count;
    }

    sqlite3 *db_ = nullptr;
    std::recursive_mutex testMutex_;
    std::unique_ptr<MockDatabase> database_;
};

TEST_F(UpstreamOverlapAnalysisTest, TestUpstreamFirstImport_TableExistsNoSourceNoUnit) {
    CreateOverlapAnalysisTable();
    InsertOverlapAnalysisData();
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_UPSTREAM);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), FINISH_STATUS);
    EXPECT_EQ(CountOverlapAnalysisRows(), 3);
}

TEST_F(UpstreamOverlapAnalysisTest, TestUpstreamVersionChange_SourceUpstream) {
    CreateOverlapAnalysisTable();
    InsertOverlapAnalysisData();
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    SetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE, OVERLAP_ANALYSIS_SOURCE_UPSTREAM);
    SetStatusInfoValue(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS);

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_UPSTREAM);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), FINISH_STATUS);
    EXPECT_EQ(CountOverlapAnalysisRows(), 3);
}

TEST_F(UpstreamOverlapAnalysisTest, TestOldFormatFirstImport_NoOverlapTable) {
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_LOCAL);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), NOT_FINISH_STATUS);
    EXPECT_TRUE(database_->CheckTableExist(TABLE_OVERLAP_ANALYSIS));
}

TEST_F(UpstreamOverlapAnalysisTest, TestOldFormatVersionChange_SourceLocal) {
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    CreateOverlapAnalysisTable();
    SetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE, OVERLAP_ANALYSIS_SOURCE_LOCAL);
    SetStatusInfoValue(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS);

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_LOCAL);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), NOT_FINISH_STATUS);
}

TEST_F(UpstreamOverlapAnalysisTest, TestOldVersionUpgrade_SourceEmptyUnitFinish) {
    CreateOverlapAnalysisTable();
    InsertOverlapAnalysisData();
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    SetStatusInfoValue(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS);

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_LOCAL);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), NOT_FINISH_STATUS);
}

TEST_F(UpstreamOverlapAnalysisTest, TestPureHostData_NoTaskNoCommOp) {
    database_->SetConfigForTesting();

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_TRUE(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE).empty());
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), NOT_FINISH_STATUS);
    EXPECT_FALSE(database_->CheckTableExist(TABLE_OVERLAP_ANALYSIS));
}

TEST_F(UpstreamOverlapAnalysisTest, TestUpstreamEmptyTable) {
    CreateOverlapAnalysisTable();
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_UPSTREAM);
    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_UNIT), FINISH_STATUS);
    EXPECT_EQ(CountOverlapAnalysisRows(), 0);
}

TEST_F(UpstreamOverlapAnalysisTest, TestSourceLocalNotOverwrittenByDbStatusList) {
    CreateTaskAndComputeTables();
    database_->SetConfigForTesting();

    CreateOverlapAnalysisTable();
    SetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE, OVERLAP_ANALYSIS_SOURCE_LOCAL);
    SetStatusInfoValue(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS);

    database_->AddHelperColumnsAndSetStatus();

    EXPECT_EQ(GetStatusInfoValue(OVERLAP_ANALYSIS_SOURCE), OVERLAP_ANALYSIS_SOURCE_LOCAL);
}

TEST_F(UpstreamOverlapAnalysisTest, TestQueryUnitsMetadata_UpstreamTableOnly) {
    CreateOverlapAnalysisTable();
    database_->SetConfigForTesting();

    std::string fileId = "1";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    database_->QueryUnitsMetadata(fileId, metaData);

    bool found = false;
    for (const auto &track : metaData) {
        if (track->metaData.processId == "OVERLAP_ANALYSIS") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(UpstreamOverlapAnalysisTest, TestQueryUnitsMetadata_NoOverlapTable) {
    database_->SetConfigForTesting();

    std::string fileId = "1";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    database_->QueryUnitsMetadata(fileId, metaData);

    bool found = false;
    for (const auto &track : metaData) {
        if (track->metaData.processId == "OVERLAP_ANALYSIS") {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}
