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
#include "EnumApiTypeTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class EnumApiTypeTableTest : public ::testing::Test {};

TEST_F(EnumApiTypeTableTest, testEnumApiTypeColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE ENUM_API_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"ENUM_API_TYPE\" (\"id\", \"name\") VALUES (10000, 'node');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<EnumApiTypePO> EnumApiTypePOs;
    Dic::Protocol::EnumApiTypeTable table;
    const uint64_t expectSize = 1;
    const uint64_t expectId = 10000;
    const uint64_t index = 0;
    table.Select(EnumApiTypeColumn::ID, EnumApiTypeColumn::NAME).ExcuteQuery(db, EnumApiTypePOs);
    EXPECT_EQ(EnumApiTypePOs.size(), expectSize);
    EXPECT_EQ(EnumApiTypePOs[index].id, expectId);
    EXPECT_EQ(EnumApiTypePOs[index].name, "node");
}
