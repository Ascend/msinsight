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
#include "EnumHcclLinkTypeTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class EnumHcclLinkTypeTableTest : public ::testing::Test {};

/**
 * 测试EnumHcclLinkTypeTable字段映射
 */
TEST_F(EnumHcclLinkTypeTableTest, TestEnumHcclLinkTypeTableTestColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE ENUM_HCCL_LINK_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"ENUM_HCCL_LINK_TYPE\" (\"id\", \"name\") VALUES (4, 'SIO');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<EnumHcclLinkTypePO> enumHcclLinkTypePOs;
    Dic::Module::Timeline::EnumHcclLinkTypeTable enumHcclDataTypeTable;
    const uint64_t expectSize = 1;
    const uint64_t expectId = 4;
    const uint64_t index = 0;
    enumHcclDataTypeTable.Select(EnumHcclLinkTypeClumn::ID, EnumHcclLinkTypeClumn::NAME)
        .ExcuteQuery(db, enumHcclLinkTypePOs);
    EXPECT_EQ(enumHcclLinkTypePOs.size(), expectSize);
    EXPECT_EQ(enumHcclLinkTypePOs[index].name, "SIO");
    EXPECT_EQ(enumHcclLinkTypePOs[index].id, expectId);
}
