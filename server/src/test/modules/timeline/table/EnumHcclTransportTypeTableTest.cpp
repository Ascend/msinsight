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
#include "EnumHcclTransportTypeTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class EnumHcclTransportTypeTableTest : public ::testing::Test {};

/**
 * 测试EnumHcclTransportTypeTable字段映射
 */
TEST_F(EnumHcclTransportTypeTableTest, TestEnumHcclTransportTypeTableTestColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE ENUM_HCCL_TRANSPORT_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"ENUM_HCCL_TRANSPORT_TYPE\" (\"id\", \"name\") VALUES (2, 'LOCAL');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<EnumHcclTransportTypePO> enumHcclTransportTypePOs;
    Dic::Module::Timeline::EnumHcclTransportTypeTable enumHcclDataTypeTable;
    const uint64_t expectSize = 1;
    const uint64_t expectId = 2;
    const uint64_t index = 0;
    enumHcclDataTypeTable.Select(EnumHcclTransportTypeClumn::ID, EnumHcclTransportTypeClumn::NAME)
        .ExcuteQuery(db, enumHcclTransportTypePOs);
    EXPECT_EQ(enumHcclTransportTypePOs.size(), expectSize);
    EXPECT_EQ(enumHcclTransportTypePOs[index].name, "LOCAL");
    EXPECT_EQ(enumHcclTransportTypePOs[index].id, expectId);
}
