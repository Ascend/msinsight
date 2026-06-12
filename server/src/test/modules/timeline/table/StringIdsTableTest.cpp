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
#include <string>
#include <vector>
#include "StringIdsTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class StringIdsTableTest : public ::testing::Test {};

/**
 * 测试StringIdsTable字段映射
 */
TEST_F(StringIdsTableTest, testStringIdsTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE  STRING_IDS ("
                      "  id INTEGER,"
                      "  value TEXT,"
                      "  PRIMARY KEY (id)"
                      ");";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO STRING_IDS (id, value) VALUES (1, 'aaaa'), (2, 'bbb');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<StringIdsPO> stringIdsPOs;
    Dic::Protocol::StringIdsTable stringIdsTable;
    const uint64_t expectSize = 2;
    const uint64_t expectId = 2;
    const uint64_t index = 1;
    stringIdsTable.Select(StringIdsColumn::ID, StringIdsColumn::VALUE).ExcuteQuery(db, stringIdsPOs);
    EXPECT_EQ(stringIdsPOs.size(), expectSize);
    EXPECT_EQ(stringIdsPOs[index].value, "bbb");
    EXPECT_EQ(stringIdsPOs[index].id, expectId);
}

TEST_F(StringIdsTableTest, testStringIdsTableInQueryShouldHandleManyParams) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE  STRING_IDS ("
                      "  id INTEGER,"
                      "  value TEXT,"
                      "  PRIMARY KEY (id)"
                      ");";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    sqlite3_limit(db, SQLITE_LIMIT_VARIABLE_NUMBER, 1000);
    TestCaseDatabaseUtil::InsertData(db, "INSERT INTO STRING_IDS (id, value) VALUES (1, 'aaaa');");

    std::vector<uint64_t> ids;
    ids.reserve(1001);
    for (uint64_t i = 1; i <= 1001; ++i) {
        ids.emplace_back(i);
    }

    std::vector<StringIdsPO> stringIdsPOs;
    Dic::Protocol::StringIdsTable stringIdsTable;
    stringIdsTable.Select(StringIdsColumn::ID, StringIdsColumn::VALUE)
        .In(StringIdsColumn::ID, ids)
        .ExcuteQuery(db, stringIdsPOs);

    ASSERT_EQ(stringIdsPOs.size(), 1u);
    EXPECT_EQ(stringIdsPOs[0].id, 1u);
    EXPECT_EQ(stringIdsPOs[0].value, "aaaa");
}
