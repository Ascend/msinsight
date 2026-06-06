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
#include "Database.h"
#include "../../../DatabaseTestCaseMockUtil.h"
using namespace Dic::Global::PROFILER::MockUtil;
class DataBaseTest : public ::testing::Test {
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
};

/**
 * 根据表名查询表结构，正常情况
 */
TEST_F(DataBaseTest, TestQueryTableInfoByNameNormal) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    auto res = database.QueryTableInfoByName("decode_gen_speed");
    const uint64_t expectSize = 5;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front().key, "timestamp");
    EXPECT_EQ(res.front().type, "TEXT");
    EXPECT_EQ(res.back().key, "p50");
    EXPECT_EQ(res.back().type, "REAL");
}

/**
 * 根据表名查询表结构，sql注入情况
 */
TEST_F(DataBaseTest, TestQueryTableInfoByNameWhenSqlInjectReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    auto res = database.QueryTableInfoByName("decode_gen_speed()");
    const uint64_t expectSize = 0;
    EXPECT_EQ(res.size(), expectSize);
}

/**
 * 根据表名查询表结构，表名不正确
 */
TEST_F(DataBaseTest, TestQueryTableInfoByNameWhenNameErrReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    auto res = database.QueryTableInfoByName("SELECT");
    const uint64_t expectSize = 0;
    EXPECT_EQ(res.size(), expectSize);
}

/**
 * 根据表名查询表结构，db未打开
 */
TEST_F(DataBaseTest, TestQueryTableInfoByNameWhenDbNotOpenReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    database.SetDbPtr(dbPtr);
    auto res = database.QueryTableInfoByName("SELECT");
    const uint64_t expectSize = 0;
    EXPECT_EQ(res.size(), expectSize);
}

/**
 * 根据表名查询表数据量，正常情况
 */
TEST_F(DataBaseTest, TestQueryCountByTableNameNormal) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:735760', 6.0869, 6.1759, 6.1512, 6.041);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:44:344296', 2.1341, 2.1451, 2.142, 2.1285);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736910', 6.0657, 6.1541, 6.1295, 6.0201);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736935', 6.0652, 6.1537, 6.129, 6.0196);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736967', 6.0646, 6.1531, 6.1285, 6.0191);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    Dic::Module::PageQuery query;
    const std::vector<Dic::Module::ColumnAtt> columns = database.QueryTableInfoByName("decode_gen_speed");
    query.viewName = "decode_gen_speed";
    auto res = database.QueryCountByTableName(query, columns);
    const uint64_t expectSize = 5;
    EXPECT_EQ(res, expectSize);
}

/**
 * 根据表名查询表数据量，搜索情况
 */
TEST_F(DataBaseTest, TestQueryCountByTableNameSearch) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:735760', 6.0869, 6.1759, 6.1512, 6.041);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:44:344296', 2.1341, 2.1451, 2.142, 2.1285);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736910', 6.0657, 6.1541, 6.1295, 6.0201);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736935', 6.0652, 6.1537, 6.129, 6.0196);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736967', 6.0646, 6.1531, 6.1285, 6.0191);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    Dic::Module::PageQuery query;
    const std::vector<Dic::Module::ColumnAtt> columns = database.QueryTableInfoByName("decode_gen_speed");
    query.viewName = "decode_gen_speed";
    query.pageFilters.push_back({"avg", "6.065"});
    auto res = database.QueryCountByTableName(query, columns);
    const uint64_t expectSize = 2;
    EXPECT_EQ(res, expectSize);
}

/**
 * 根据表名查询表数据量，sql注入情况
 */
TEST_F(DataBaseTest, TestQueryCountByTableNameWhenSqlInjectReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    Dic::Module::PageQuery query;
    const std::vector<Dic::Module::ColumnAtt> columns = database.QueryTableInfoByName("decode_gen_speed()");
    query.viewName = "decode_gen_speed()";
    auto res = database.QueryCountByTableName(query, columns);
    const uint64_t expectSize = 0;
    EXPECT_EQ(res, expectSize);
}

/**
 * 根据表名查询表数据量，表名不正确
 */
TEST_F(DataBaseTest, TestQueryCountByTableNameWhenNameErrReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    Dic::Module::PageQuery query;
    const std::vector<Dic::Module::ColumnAtt> columns = database.QueryTableInfoByName("SELECT");
    query.viewName = "SELECT";
    auto res = database.QueryCountByTableName(query, columns);
    const uint64_t expectSize = 0;
    EXPECT_EQ(res, expectSize);
}

/**
 * 根据表名查询表数据量，db未打开
 */
TEST_F(DataBaseTest, TestQueryCountByTableNameWhenDbNotOpenReturnEmpty) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    database.SetDbPtr(dbPtr);
    Dic::Module::PageQuery query;
    const std::vector<Dic::Module::ColumnAtt> columns = database.QueryTableInfoByName("SELECT");
    query.viewName = "SELECT";
    auto res = database.QueryCountByTableName(query, columns);
    const uint64_t expectSize = 0;
    EXPECT_EQ(res, expectSize);
}

/**
 * 分页查询表数据，正常情况
 */
TEST_F(DataBaseTest, TestQueryDataByPageNormal) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:646147', NULL, NULL, NULL, NULL);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:646138', NULL, NULL, NULL, NULL);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:735760', 6.0869, 6.1759, 6.1512, 6.041);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:44:344296', 2.1341, 2.1451, 2.142, 2.1285);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    auto cols = database.QueryTableInfoByName("decode_gen_speed");
    Dic::Module::PageQuery pageQuery;
    pageQuery.curPage = 1; // 1
    pageQuery.size = 10; // 10
    pageQuery.viewName = "decode_gen_speed";
    auto res = database.QueryDataByPage(pageQuery, cols);
    const uint64_t expectSize = 4;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front()[cols.front().key], "2025-03-28 22:57:43:646147");
    EXPECT_EQ(res.front()[cols.back().key], "0");
    EXPECT_EQ(res.back()[cols.front().key], "2025-03-28 22:57:44:344296");
    EXPECT_EQ(res.back()[cols.back().key], "2.1285");
}

/**
 * 分页查询表数据，搜索情况
 */
TEST_F(DataBaseTest, TestQueryDataByPageSearch) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:646147', NULL, NULL, NULL, NULL);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:646138', NULL, NULL, NULL, NULL);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:735760', 6.0869, 6.1759, 6.1512, 6.041);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:44:344296', 2.1341, 2.1451, 2.142, 2.1285);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    auto cols = database.QueryTableInfoByName("decode_gen_speed");
    Dic::Module::PageQuery pageQuery;
    pageQuery.curPage = 1; // 1
    pageQuery.size = 10; // 10
    pageQuery.viewName = "decode_gen_speed";
    pageQuery.pageFilters.push_back({"timestamp", "646"});
    auto res = database.QueryDataByPage(pageQuery, cols);
    const uint64_t expectSize = 2;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front()[cols.front().key], "2025-03-28 22:57:43:646147");
    EXPECT_EQ(res.front()[cols.back().key], "0");
    EXPECT_EQ(res.back()[cols.front().key], "2025-03-28 22:57:43:646138");
    EXPECT_EQ(res.back()[cols.back().key], "0");
}

/**
 * 分页查询表数据，有排序条件
 */
TEST_F(DataBaseTest, TestQueryDataByPageWhenOrderAndLimit) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"decode_gen_speed\" (\n"
                      "\"timestamp\" TEXT,\n"
                      "  \"avg\" REAL,\n"
                      "  \"p99\" REAL,\n"
                      "  \"p90\" REAL,\n"
                      "  \"p50\" REAL\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:735760', 6.0869, 6.1759, 6.1512, 6.041);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:44:344296', 2.1341, 2.1451, 2.142, 2.1285);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736910', 6.0657, 6.1541, 6.1295, 6.0201);\n"
                          "INSERT INTO \"main\".\"decode_gen_speed\" (\"timestamp\", \"avg\", \"p99\", \"p90\", "
                          "\"p50\") VALUES ('2025-03-28 22:57:43:736935', 6.0652, 6.1537, 6.129, 6.0196);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    auto cols = database.QueryTableInfoByName("decode_gen_speed");
    Dic::Module::PageQuery pageQuery;
    pageQuery.curPage = 1; // 1
    pageQuery.size = 2; // 2
    pageQuery.viewName = "decode_gen_speed";
    pageQuery.orderBy = "timestamp";
    pageQuery.order = "descend";
    auto res = database.QueryDataByPage(pageQuery, cols);
    const uint64_t expectSize = 2;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front()[cols.front().key], "2025-03-28 22:57:44:344296");
    EXPECT_EQ(res.front()[cols.back().key], "2.1285");
    EXPECT_EQ(res.back()[cols.front().key], "2025-03-28 22:57:43:736935");
    EXPECT_EQ(res.back()[cols.back().key], "6.0196");
}

/**
 * 查询翻译正常
 */
TEST_F(DataBaseTest, TestQueryTranslateNormal) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"translate\" (\n"
                      "  \"key\" TEXT NOT NULL,\n"
                      "  \"value_en\" TEXT,\n"
                      "  \"value_zh\" TEXT,\n"
                      "  PRIMARY KEY (\"key\")\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    std::string dataSql = "INSERT INTO \"main\".\"translate\" (\"key\", \"value_en\", \"value_zh\") VALUES "
                          "('request_data', 'bbbb', 'nnnn');\n"
                          "INSERT INTO \"main\".\"translate\" (\"key\", \"value_en\", \"value_zh\") VALUES "
                          "('batch_info', 'ffff', 'mmm');\n"
                          "INSERT INTO \"main\".\"translate\" (\"key\", \"value_en\", \"value_zh\") VALUES "
                          "('kvcache_usage', 'sssss', 'kkk');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, dataSql);
    auto cols = database.QueryTranslate(false);
    const uint64_t expectSize = 3;
    EXPECT_EQ(cols.size(), expectSize);
    EXPECT_EQ(cols["request_data"], "bbbb");
    cols = database.QueryTranslate(true);
    EXPECT_EQ(cols.size(), expectSize);
    EXPECT_EQ(cols["request_data"], "nnnn");
}

/**
 * 查询翻译不正常
 */
TEST_F(DataBaseTest, TestQueryTranslateAbnormal) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    std::string sql = "CREATE TABLE \"translates\" (\n"
                      "  \"key\" TEXT NOT NULL,\n"
                      "  \"value_en\" TEXT,\n"
                      "  \"value_zh\" TEXT,\n"
                      "  PRIMARY KEY (\"key\")\n"
                      ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sql);
    auto cols = database.QueryTranslate(false);
    const uint64_t expectSize = 0;
    EXPECT_EQ(cols.size(), expectSize);
}

/**
 * 设置属性，正常情况
 * 验证：SetAttr后通过GetAttr能获取到设置的值
 */
TEST_F(DataBaseTest, TestSetAttrNormalExpectGetValue) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 设置属性
    database.SetAttr("db.type", "sqlite");
    database.SetAttr("db.subtype", "cluster");

    // 验证属性值
    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");
    EXPECT_EQ(database.GetAttr("db.subtype"), "cluster");
}

/**
 * 获取存在的属性，验证返回正确值
 */
TEST_F(DataBaseTest, TestGetAttrWhenExistExpectReturnValue) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 预先设置属性
    database.SetAttr("db.version", "1.0.0");
    database.SetAttr("db.source", "file");

    // 获取存在的属性
    EXPECT_EQ(database.GetAttr("db.version"), "1.0.0");
    EXPECT_EQ(database.GetAttr("db.source"), "file");
}

/**
 * 获取不存在的属性，验证返回默认值
 */
TEST_F(DataBaseTest, TestGetAttrWhenNotExistExpectReturnDefault) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 未设置任何属性
    // 获取不存在的属性，验证返回空字符串（默认值）
    EXPECT_EQ(database.GetAttr("nonexistent_key"), "");

    // 获取不存在的属性，验证返回指定的默认值
    EXPECT_EQ(database.GetAttr("nonexistent_key", "default_value"), "default_value");
}

/**
 * 检查属性存在，验证返回true
 */
TEST_F(DataBaseTest, TestHasAttrWhenExistExpectReturnTrue) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 设置属性
    database.SetAttr("db.type", "text");

    // 检查存在的属性
    EXPECT_TRUE(database.HasAttr("db.type"));
}

/**
 * 检查属性不存在，验证返回false
 */
TEST_F(DataBaseTest, TestHasAttrWhenNotExistExpectReturnFalse) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 未设置任何属性
    // 检查不存在的属性
    EXPECT_FALSE(database.HasAttr("nonexistent_key"));

    // 设置一个属性，检查另一个不存在的属性
    database.SetAttr("db.type", "sqlite");
    EXPECT_FALSE(database.HasAttr("db.subtype"));
}

/**
 * 移除存在的属性，验证移除成功
 */
TEST_F(DataBaseTest, TestRemoveAttrWhenExistExpectRemoveSuccess) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 设置多个属性
    database.SetAttr("db.type", "sqlite");
    database.SetAttr("db.subtype", "trace");
    database.SetAttr("db.version", "2.0.0");

    // 验证属性存在
    EXPECT_TRUE(database.HasAttr("db.type"));
    EXPECT_TRUE(database.HasAttr("db.subtype"));

    // 移除一个属性
    database.RemoveAttr("db.subtype");

    // 验证移除成功
    EXPECT_TRUE(database.HasAttr("db.type"));
    EXPECT_FALSE(database.HasAttr("db.subtype"));
    EXPECT_EQ(database.GetAttr("db.subtype"), "");

    // 验证其他属性不受影响
    EXPECT_TRUE(database.HasAttr("db.version"));
    EXPECT_EQ(database.GetAttr("db.version"), "2.0.0");
}

/**
 * 移除不存在的属性，验证不报错
 */
TEST_F(DataBaseTest, TestRemoveAttrWhenNotExistExpectNoError) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 未设置任何属性
    // 移除不存在的属性，应不报错
    EXPECT_NO_THROW(database.RemoveAttr("nonexistent_key"));

    // 设置一个属性后，移除另一个不存在的属性
    database.SetAttr("db.type", "sqlite");
    EXPECT_NO_THROW(database.RemoveAttr("nonexistent_key"));
}

/**
 * 获取所有属性，验证返回完整的属性集合
 */
TEST_F(DataBaseTest, TestGetAllAttrsExpectReturnAllAttributes) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 初始状态：无属性
    auto attrs = database.GetAllAttrs();
    EXPECT_EQ(attrs.size(), 0);

    // 设置属性
    database.SetAttr("db.type", "virtual");
    database.SetAttr("db.subtype", "memory");
    database.SetAttr("db.rank_count", "8");

    // 验证所有属性都被正确存储
    attrs = database.GetAllAttrs();
    EXPECT_EQ(attrs.size(), 3);
    EXPECT_EQ(attrs.at("db.type"), "virtual");
    EXPECT_EQ(attrs.at("db.subtype"), "memory");
    EXPECT_EQ(attrs.at("db.rank_count"), "8");
}

/**
 * 覆盖已存在的属性，验证新值生效
 */
TEST_F(DataBaseTest, TestSetAttrOverwriteExpectNewValue) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 设置属性
    database.SetAttr("db.type", "text");
    EXPECT_EQ(database.GetAttr("db.type"), "text");

    // 覆盖属性
    database.SetAttr("db.type", "sqlite");
    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");

    // 再次覆盖
    database.SetAttr("db.type", "virtual");
    EXPECT_EQ(database.GetAttr("db.type"), "virtual");
}

/**
 * 空字符串键值处理，验证能正常存储
 */
TEST_F(DataBaseTest, TestSetAttrWithEmptyStringExpectStoreSuccess) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    // 设置空键（边界情况）
    database.SetAttr("", "value");
    EXPECT_FALSE(database.HasAttr(""));
    EXPECT_EQ(database.GetAttr(""), "");

    // 设置空值
    database.SetAttr("db.empty", "");
    EXPECT_TRUE(database.HasAttr("db.empty"));
    EXPECT_EQ(database.GetAttr("db.empty"), "");

    // 设置空键空值
    database.SetAttr("", "");
    EXPECT_FALSE(database.HasAttr(""));
    EXPECT_EQ(database.GetAttr(""), "");
}

/**
 * 批量设置属性（使用map），验证所有属性都能正确设置
 */
TEST_F(DataBaseTest, TestSetAttrsWithMapExpectAllSetSuccess) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    std::map<std::string, std::string> attrsMap = {
        {"db.type", "sqlite"}, {"db.subtype", "cluster"}, {"db.source", "db"}, {"db.version", "1.0.0"}};

    database.SetAttrs(attrsMap);

    EXPECT_TRUE(database.HasAttr("db.type"));
    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");
    EXPECT_TRUE(database.HasAttr("db.subtype"));
    EXPECT_EQ(database.GetAttr("db.subtype"), "cluster");
    EXPECT_TRUE(database.HasAttr("db.source"));
    EXPECT_EQ(database.GetAttr("db.source"), "db");
    EXPECT_TRUE(database.HasAttr("db.version"));
    EXPECT_EQ(database.GetAttr("db.version"), "1.0.0");
}

/**
 * 批量设置属性覆盖已有属性，验证新值生效
 */
TEST_F(DataBaseTest, TestSetAttrsOverwriteExistingExpectNewValues) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    database.SetAttr("db.type", "sqlite");
    database.SetAttr("db.version", "1.0.0");
    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");
    EXPECT_EQ(database.GetAttr("db.version"), "1.0.0");

    std::map<std::string, std::string> newAttrs = {
        {"db.type", "virtual"}, {"db.version", "2.0.0"}, {"db.source", "api"}};

    database.SetAttrs(newAttrs);

    EXPECT_EQ(database.GetAttr("db.type"), "virtual");
    EXPECT_EQ(database.GetAttr("db.version"), "2.0.0");
    EXPECT_EQ(database.GetAttr("db.source"), "api");
}

/**
 * 批量设置空map，验证不报错且无副作用
 */
TEST_F(DataBaseTest, TestSetAttrsWithEmptyMapExpectNoError) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    database.SetAttr("db.type", "sqlite");
    EXPECT_TRUE(database.HasAttr("db.type"));

    std::map<std::string, std::string> emptyMap;
    EXPECT_NO_THROW(database.SetAttrs(emptyMap));

    EXPECT_TRUE(database.HasAttr("db.type"));
    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");
}

/**
 * 批量设置属性包含空字符串键值，验证能正常处理
 */
TEST_F(DataBaseTest, TestSetAttrsWithEmptyStringsExpectStoreSuccess) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    std::map<std::string, std::string> attrsMap = {{"", "empty_key_value"}, {"db.empty_value", ""}, {"", ""}};

    database.SetAttrs(attrsMap);

    EXPECT_FALSE(database.HasAttr(""));
    EXPECT_EQ(database.GetAttr(""), "");
    EXPECT_TRUE(database.HasAttr("db.empty_value"));
    EXPECT_EQ(database.GetAttr("db.empty_value"), "");
}

/**
 * 批量设置属性与单个设置混合使用，验证行为正确
 */
TEST_F(DataBaseTest, TestSetAttrsMixedWithSingleSetExpectCorrect) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);

    database.SetAttr("db.type", "sqlite");

    std::map<std::string, std::string> attrsMap = {{"db.subtype", "cluster"}, {"db.version", "1.0.0"}};
    database.SetAttrs(attrsMap);

    database.SetAttr("db.source", "db");

    EXPECT_EQ(database.GetAttr("db.type"), "sqlite");
    EXPECT_EQ(database.GetAttr("db.subtype"), "cluster");
    EXPECT_EQ(database.GetAttr("db.version"), "1.0.0");
    EXPECT_EQ(database.GetAttr("db.source"), "db");
}
