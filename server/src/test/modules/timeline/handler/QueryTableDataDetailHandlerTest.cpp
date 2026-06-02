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
#include "QueryTableDataDetailHandler.h"
#include "QueryTableDataNameListHandler.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "HandlerTest.cpp"

TEST_F(HandlerTest, QueryTableDataDetailHandlerTestNormal) {
    Dic::Module::Timeline::QueryTableDataDetailHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::TableDataDetailRequest>();
    auto res = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(res, false);
}

TEST_F(HandlerTest, QueryTableDataNameListHandlerTestNormal) {
    Dic::Module::Timeline::QueryTableDataNameListHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::TableDataNameListRequest>();
    auto res = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(res, false);
}

TEST_F(HandlerTest, ComputeTableDetailTestNormal) {
    Dic::Module::Timeline::QueryTableDataDetailHandler handler;
    TableDataDetailRequest request;
    TableDataDetailResponse response;
    std::recursive_mutex sqlMutex;
    std::shared_ptr<MockDatabase> vDb = std::make_shared<MockDatabase>(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    std::string sql1 = "CREATE TABLE \"data_table\" (\n"
                       "\"id\" INTEGER NOT NULL,\n"
                       "\"name\" TEXT,\n"
                       "\"view_name\" TEXT,\n"
                       "PRIMARY KEY (\"id\")\n"
                       ");";
    std::string sql2 =
        "INSERT INTO \"main\".\"data_table\" (\"id\", \"name\", \"view_name\") VALUES (2, 'kvcache', 'kvcache');";
    std::string sql3 = "CREATE TABLE \"kvcache\" (\n"
                       "\"rid\" TEXT,\n"
                       "  \"name\" TEXT,\n"
                       "  \"real_start_time\" TEXT,\n"
                       "  \"device_kvcache_left\" REAL,\n"
                       "  \"kvcache_usage_rate\" REAL\n"
                       ");";
    std::string sql4 =
        "INSERT INTO \"main\".\"kvcache\" (\"rid\", \"name\", \"real_start_time\", \"device_kvcache_left\", "
        "\"kvcache_usage_rate\") VALUES ('0', 'Allocate', '2025-03-28 22:57:43:410383', 1217.0, "
        "0.000821018062397373);\n"
        "INSERT INTO \"main\".\"kvcache\" (\"rid\", \"name\", \"real_start_time\", \"device_kvcache_left\", "
        "\"kvcache_usage_rate\") VALUES ('1', 'Allocate', '2025-03-28 22:57:43:410414', 1217.0, 0.000821018062397373);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql1);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql2);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql3);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql4);
    vDb->SetDbPtr(dbPtr);
    request.params.tableIndex = 0; // 0
    request.params.currentPage = 1; // 1
    request.params.pageSize = 10; // 10
    handler.ComputeTableDetail(request, response, vDb);
    EXPECT_EQ(response.body.columnAttr.size(), 5); // 5
    EXPECT_EQ(response.body.columnData.size(), 2); // 2
    EXPECT_EQ(response.body.totalNum, 2); // 2
}

TEST_F(HandlerTest, ComputeTableDetailTestWhenFilterBrackets) {
    Dic::Module::Timeline::QueryTableDataDetailHandler handler;
    TableDataDetailRequest request;
    TableDataDetailResponse response;
    std::recursive_mutex sqlMutex;
    std::shared_ptr<MockDatabase> vDb = std::make_shared<MockDatabase>(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    std::string sql1 = "CREATE TABLE \"data_table\" (\"id\" INTEGER NOT NULL,\n"
                       "\"name\" TEXT,\"view_name\" TEXT,PRIMARY KEY (\"id\"));";
    std::string sql2 =
        "INSERT INTO \"main\".\"data_table\" (\"id\", \"name\", \"view_name\") VALUES (1, 'batch_info', 'batch_info');";
    std::string sql3 = "CREATE TABLE \"batch\" (\n"
                       "\"name\" TEXT,\n"
                       "  \"res_list\" TEXT,\n"
                       "  \"start_time\" REAL,\n"
                       "  \"end_time\" REAL,\n"
                       "  \"batch_size\" REAL,\n"
                       "  \"batch_type\" TEXT,\n"
                       "  \"during_time\" REAL\n"
                       ");";
    std::string sql4 =
        "INSERT INTO \"main\".\"batch\" (\"name\", \"res_list\", \"start_time\", \"end_time\", \"batch_size\", "
        "\"batch_type\", \"during_time\") VALUES ('batchFrameworkProcessing', '[{''rid'': ''0'', ''iter'': 0}]', "
        "1754039537969.0, 1754039537969.0, 1.0, 'Prefill', 0.12975);\n"
        "INSERT INTO \"main\".\"batch\" (\"name\", \"res_list\", \"start_time\", \"end_time\", \"batch_size\", "
        "\"batch_type\", \"during_time\") VALUES ('modelExec', '[{''rid'': 0, ''iter'': 0}]', 1754039537969.0, "
        "1754039537986.0, 1.0, 'Prefill', 17.04825);";
    std::string sql5 = "CREATE VIEW batch_info AS\n"
                       "    SELECT name, res_list, \"start_time\" AS \"start_time(ms)\", \"end_time\" AS "
                       "\"end_time(ms)\", batch_size, batch_type, \"during_time\" AS \"during_time(ms)\"\n"
                       "    FROM batch";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql1);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql2);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql3);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql4);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql5);
    vDb->SetDbPtr(dbPtr);
    request.params.tableIndex = 0; // 0
    request.params.currentPage = 1; // 1
    request.params.pageSize = 10; // 10
    request.params.filterconditions.push_back({"end_time(ms)", "1754039537986"});
    handler.ComputeTableDetail(request, response, vDb);
    EXPECT_EQ(response.body.columnAttr.size(), 7); // 7
    EXPECT_EQ(response.body.columnData.size(), 1); // 1
    EXPECT_EQ(response.body.totalNum, 1); // 1
}

TEST_F(HandlerTest, ComputeLinkPageDetailNormal) {
    Dic::Module::Timeline::QueryTableDataDetailHandler handler;
    TableDataDetailRequest request;
    TableDataDetailResponse response;
    std::recursive_mutex sqlMutex;
    std::shared_ptr<MockDatabase> vDb = std::make_shared<MockDatabase>(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    std::string sql1 = "CREATE TABLE \"data_link\" (\n"
                       "\"source_name\" TEXT NOT NULL,\n"
                       "\"target_table\" TEXT NOT NULL,\n"
                       "\"target_name\" TEXT NOT NULL,\n"
                       "PRIMARY KEY (\"source_name\")\n"
                       ");";
    std::string sql2 = "INSERT INTO \"main\".\"data_link\" (\"source_name\", \"target_table\", \"target_name\") VALUES "
                       "('rid', 'request', 'http_rid');";
    std::string sql3 = "CREATE TABLE \"request\" (\n"
                       "\"http_rid\" TEXT,\n"
                       "  \"start_time\" REAL,\n"
                       "  \"recv_token_size\" REAL,\n"
                       "  \"reply_token_size\" REAL,\n"
                       "  \"execution_time\" REAL,\n"
                       "  \"queue_wait_time\" REAL,\n"
                       "  \"first_token_latency\" REAL\n"
                       ");";
    std::string sql4 = "INSERT INTO \"main\".\"request\" (\"http_rid\", \"start_time\", \"recv_token_size\", "
                       "\"reply_token_size\", \"execution_time\", \"queue_wait_time\", \"first_token_latency\") VALUES "
                       "('1', 1754039538152.0, 587.0, 800.0, 99.0, 5.30875, 11.814);\n"
                       "INSERT INTO \"main\".\"request\" (\"http_rid\", \"start_time\", \"recv_token_size\", "
                       "\"reply_token_size\", \"execution_time\", \"queue_wait_time\", \"first_token_latency\") VALUES "
                       "('10', 1754039538933.0, 141.0, 800.0, 54.0, 5.20775, 8.60875);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql1);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql2);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql3);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(dbPtr, sql4);
    vDb->SetDbPtr(dbPtr);
    request.params.currentPage = 1; // 1
    request.params.pageSize = 10; // 10
    request.params.equalConditions.push_back({"rid", "1"});
    request.params.type = "1";
    handler.ComputeLinkPageDetail(request, response, vDb);
    EXPECT_EQ(response.body.columnAttr.size(), 7); // 7
    EXPECT_EQ(response.body.columnData.size(), 1); // 1
    EXPECT_EQ(response.body.totalNum, 1); // 1
}
