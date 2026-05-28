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
#include "../../../DatabaseTestCaseMockUtil.h"
#include "CurveRepo.h"
class CurveRepoTest : public ::testing::Test {
  protected:
    class MockDatabase : public Dic::Module::Database {
      public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr) {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };

    class MockContext : public Dic::Module::IE::ServitizationContext {
      public:
        sqlite3 *dbPtr = nullptr;
        MockContext() = default;
        std::shared_ptr<Dic::Module::Database> GetDatabase(const std::string &fileId) {
            std::recursive_mutex sqlMutex;
            std::shared_ptr<MockDatabase> db = std::make_shared<MockDatabase>(sqlMutex);
            db->SetDbPtr(dbPtr);
            return db;
        }
    };

    class MockRepo : public Dic::Module::IE::CurveRepo {
      public:
        MockRepo() {}
        void SetContext(sqlite3 *dbPtr) {
            std::shared_ptr<MockContext> ct = std::make_shared<MockContext>();
            ct->dbPtr = dbPtr;
            context = ct;
        }
    };
};

TEST_F(CurveRepoTest, TestQueryAllViews) {
    MockRepo repo;
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    repo.SetContext(dbPtr);
    std::string tableSql = "CREATE TABLE \"first_token_latency\" (\n"
                           "\"timestamp\" TEXT,\n"
                           "  \"avg\" REAL,\n"
                           "  \"p99\" REAL,\n"
                           "  \"p90\" REAL,\n"
                           "  \"p50\" REAL\n"
                           ");";
    std::string viewSql = "CREATE VIEW firstTokenLatency_by_ts_curve AS\n"
                          "    WITH converted AS (\n"
                          "        SELECT\n"
                          "        substr(timestamp, 1, 10) || ' ' || substr(timestamp, 12, 8) || '.' || "
                          "substr(timestamp, 21, 6) AS datetime,\n"
                          "        avg, p99, p90, p50\n"
                          "    FROM\n"
                          "        first_token_latency\n"
                          "    )\n"
                          "    SELECT\n"
                          "        datetime as time,\n"
                          "        cast(avg as REAL) as \"avg\",\n"
                          "        cast(p99 as REAL) as \"p99\",\n"
                          "        cast(p90 as REAL) as \"p90\",\n"
                          "        cast(p50 as REAL) as \"p50\"\n"
                          "    FROM\n"
                          "        converted\n"
                          "    ORDER BY\n"
                          "        datetime ASC";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, tableSql);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, viewSql);
    auto res = repo.QueryAllViews("");
    EXPECT_EQ(res.size(), 1); // 1
    EXPECT_EQ(res.front(), "firstTokenLatency_by_ts_curve");
}

TEST_F(CurveRepoTest, TestQueryTableInfoByName) {
    MockRepo repo;
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    repo.SetContext(dbPtr);
    std::string tableSql = "CREATE TABLE \"first_token_latency\" (\n"
                           "\"timestamp\" TEXT,\n"
                           "  \"avg\" REAL,\n"
                           "  \"p99\" REAL,\n"
                           "  \"p90\" REAL,\n"
                           "  \"p50\" REAL\n"
                           ");";
    std::string viewSql = "CREATE VIEW firstTokenLatency_by_ts_curve AS\n"
                          "    WITH converted AS (\n"
                          "        SELECT\n"
                          "        substr(timestamp, 1, 10) || ' ' || substr(timestamp, 12, 8) || '.' || "
                          "substr(timestamp, 21, 6) AS datetime,\n"
                          "        avg, p99, p90, p50\n"
                          "    FROM\n"
                          "        first_token_latency\n"
                          "    )\n"
                          "    SELECT\n"
                          "        datetime as time,\n"
                          "        cast(avg as REAL) as \"avg\",\n"
                          "        cast(p99 as REAL) as \"p99\",\n"
                          "        cast(p90 as REAL) as \"p90\",\n"
                          "        cast(p50 as REAL) as \"p50\"\n"
                          "    FROM\n"
                          "        converted\n"
                          "    ORDER BY\n"
                          "        datetime ASC";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, tableSql);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, viewSql);
    auto res = repo.QueryTableInfoByName("", "firstTokenLatency_by_ts_curve");
    EXPECT_EQ(res.size(), 5); // 5
    EXPECT_EQ(res.front().key, "time");
    EXPECT_EQ(res.back().key, "p50");
}

TEST_F(CurveRepoTest, TestQueryDataByColumn) {
    MockRepo repo;
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    repo.SetContext(dbPtr);
    std::string tableSql = "CREATE TABLE \"first_token_latency\" (\n"
                           "\"timestamp\" TEXT,\n"
                           "  \"avg\" REAL,\n"
                           "  \"p99\" REAL,\n"
                           "  \"p90\" REAL,\n"
                           "  \"p50\" REAL\n"
                           ");";
    std::string dataSql = "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877116', 380.25, 380.25, 380.25, 380.25);\n"
                          "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877487', 393.5, 393.5, 393.5, 393.5);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, tableSql);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    std::vector<Dic::Module::ColumnAtt> cols;
    Dic::Module::ColumnAtt col1;
    col1.key = "timestamp";
    Dic::Module::ColumnAtt col2;
    col2.key = "avg";
    Dic::Module::ColumnAtt col3;
    col3.key = "p99";
    Dic::Module::ColumnAtt col4;
    col4.key = "p90";
    Dic::Module::ColumnAtt col5;
    col5.key = "p50";
    cols.emplace_back(col1);
    cols.emplace_back(col2);
    cols.emplace_back(col3);
    cols.emplace_back(col4);
    cols.emplace_back(col5);
    auto res = repo.QueryDataByColumn("", "first_token_latency", cols);
    EXPECT_EQ(res.size(), 2); // 2
    EXPECT_EQ(res.front()["timestamp"], "2025-05-13 12:06:41:877116");
    EXPECT_EQ(res.back()["p50"], "393.5");
}

TEST_F(CurveRepoTest, TestQueryCountByTableName) {
    MockRepo repo;
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    repo.SetContext(dbPtr);
    std::string tableSql = "CREATE TABLE \"first_token_latency\" (\n"
                           "\"timestamp\" TEXT,\n"
                           "  \"avg\" REAL,\n"
                           "  \"p99\" REAL,\n"
                           "  \"p90\" REAL,\n"
                           "  \"p50\" REAL\n"
                           ");";
    std::string dataSql = "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877116', 380.25, 380.25, 380.25, 380.25);\n"
                          "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877487', 393.5, 393.5, 393.5, 393.5);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, tableSql);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    Dic::Module::PageQuery query;
    query.viewName = "first_token_latency";
    query.curPage = 1; // 1
    query.size = 10; // 10
    query.start = "2025-05-13 12:06:41:877110";
    query.end = "2025-05-13 12:06:41:877480";
    auto res = repo.QueryCountByTableName(query, "timestamp");
    EXPECT_EQ(res, 1); // 1
}

TEST_F(CurveRepoTest, TestQueryDataByColumnPage) {
    MockRepo repo;
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    repo.SetContext(dbPtr);
    std::string tableSql = "CREATE TABLE \"first_token_latency\" (\n"
                           "\"timestamp\" TEXT,\n"
                           "  \"avg\" REAL,\n"
                           "  \"p99\" REAL,\n"
                           "  \"p90\" REAL,\n"
                           "  \"p50\" REAL\n"
                           ");";
    std::string dataSql = "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877116', 380.25, 380.25, 380.25, 380.25);\n"
                          "INSERT INTO \"main\".\"first_token_latency\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-05-13 12:06:41:877487', 393.5, 393.5, 393.5, 393.5);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, tableSql);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    Dic::Module::PageQuery query;
    query.viewName = "first_token_latency";
    query.curPage = 1; // 1
    query.size = 10; // 10
    query.start = "2025-05-13 12:06:41:877110";
    query.end = "2025-05-13 12:06:41:877489";
    query.order = "descend";
    query.orderBy = "p50";
    std::vector<Dic::Module::ColumnAtt> cols;
    Dic::Module::ColumnAtt col1;
    col1.key = "timestamp";
    Dic::Module::ColumnAtt col2;
    col2.key = "avg";
    Dic::Module::ColumnAtt col3;
    col3.key = "p99";
    Dic::Module::ColumnAtt col4;
    col4.key = "p90";
    Dic::Module::ColumnAtt col5;
    col5.key = "p50";
    cols.emplace_back(col1);
    cols.emplace_back(col2);
    cols.emplace_back(col3);
    cols.emplace_back(col4);
    cols.emplace_back(col5);
    auto res = repo.QueryDataByColumnPage(query, cols);
    EXPECT_EQ(res.size(), 2); // 2
    EXPECT_EQ(res.front()["p50"], "393.5");
}
