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
#include "CANNApiTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class CANNApiTableTest : public ::testing::Test {};

TEST_F(CANNApiTableTest, TestCANNApiTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE  CANN_API ("
                      "  startNs INTEGER,"
                      "  endNs INTEGER,"
                      "  type INTEGER,"
                      "  globalTid INTEGER,"
                      "  connectionId INTEGER,"
                      "  name INTEGER,"
                      "  depth INTEGER,"
                      " PRIMARY KEY (connectionId)"
                      ");";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO CANN_API (startNs, endNs, type, globalTid, connectionId, name, depth) VALUES (1, 2,3,4,5,6,7);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<CANNApiPO> cannApiPOs;
    Dic::Module::Timeline::CANNApiTable cannApiTable;
    const uint64_t expectSize = 1;
    const uint64_t index = 0;
    uint64_t initInt = 1;
    cannApiTable.Select(CANNApiColumn::ID, CANNApiColumn::TIMESTAMP)
        .Select(CANNApiColumn::ENDTIME, CANNApiColumn::TYPE)
        .Select(CANNApiColumn::GLOBAL_TID, CANNApiColumn::NAME, CANNApiColumn::DEPTH)
        .ExcuteQuery(db, cannApiPOs);
    EXPECT_EQ(cannApiPOs.size(), expectSize);
    EXPECT_EQ(cannApiPOs[index].timestamp, initInt++);
    EXPECT_EQ(cannApiPOs[index].endTime, initInt++);
    EXPECT_EQ(cannApiPOs[index].type, initInt++);
    EXPECT_EQ(cannApiPOs[index].globalTid, initInt++);
    EXPECT_EQ(cannApiPOs[index].id, initInt++);
    EXPECT_EQ(cannApiPOs[index].name, initInt++);
    EXPECT_EQ(cannApiPOs[index].depth, initInt++);
}
