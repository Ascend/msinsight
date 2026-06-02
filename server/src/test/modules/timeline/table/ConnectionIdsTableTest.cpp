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
#include "ConnectionIdsTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class ConnectionIdsTableTest : public ::testing::Test {};

TEST_F(ConnectionIdsTableTest, testConnectionIdsTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE  CONNECTION_IDS ("
                      "  id INTEGER,"
                      "  connectionId INTEGER"
                      ");";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO CONNECTION_IDS (id, connectionId) VALUES (1, 67786), (2, 7487378);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<ConnectionIdsPO> connectionIdsPOs;
    Dic::Protocol::ConnectionIdsTable connectionIdsTable;
    const uint64_t expectSize = 2;
    const uint64_t expectId = 2;
    const uint64_t expectConnectionId = 7487378;
    const uint64_t index = 1;
    connectionIdsTable.Select(ConnectionIdsColumn::ID, ConnectionIdsColumn::CONNECTIONID)
        .ExcuteQuery(db, connectionIdsPOs);
    EXPECT_EQ(connectionIdsPOs.size(), expectSize);
    EXPECT_EQ(connectionIdsPOs[index].connectionId, expectConnectionId);
    EXPECT_EQ(connectionIdsPOs[index].id, expectId);
}
