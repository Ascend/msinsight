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
#include "HostInfoTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class HostInfoTableTest : public ::testing::Test {};

TEST_F(HostInfoTableTest, TestHostInfoTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE  HOST_INFO ("
                      "  hostUid TEXT,"
                      "  hostName TEXT"
                      ");";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO HOST_INFO (hostUid, hostName) VALUES ('yyyy', 'djsjjksd'), ('zzzz', 'wwww');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<HostInfoPO> hostInfoPOs;
    HostInfoTable hostInfoTable;
    const uint64_t expectSize = 2;
    const std::string expectHostUid = "zzzz";
    const std::string expectHostName = "wwww";
    const uint64_t index = 1;
    hostInfoTable.Select(HostInfoColumn::HOST_UID, HostInfoColumn::HOST_NAME).ExcuteQuery(db, hostInfoPOs);
    EXPECT_EQ(hostInfoPOs.size(), expectSize);
    EXPECT_EQ(hostInfoPOs[index].hostUid, expectHostUid);
    EXPECT_EQ(hostInfoPOs[index].hostName, expectHostName);
}
