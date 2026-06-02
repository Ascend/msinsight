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
#include "PytorchApiTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class PythonApiTableTest : public ::testing::Test {};

TEST_F(PythonApiTableTest, testPytorchApiTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE PYTORCH_API ("
                      "startNs INTEGER,"
                      "endNs INTEGER,"
                      "globalTid INTEGER,"
                      "connectionId INTEGER,"
                      "name INTEGER,"
                      "sequenceNumber INTEGER,"
                      "fwdThreadId INTEGER,"
                      "inputDtypes INTEGER,"
                      "inputShapes INTEGER,"
                      "callchainId INTEGER,"
                      "type integer);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO PYTORCH_API (startNs, endNs,globalTid, connectionId,name, sequenceNumber,fwdThreadId, "
        "inputDtypes,inputShapes, callchainId,type) VALUES (1, 2,3,4,5,'6','7','8','9','10',11);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<PytorchApiPO> pytorchApiPOs;
    Dic::Protocol::PytorchApiTable pytorchApiTable;
    const uint64_t expectSize = 1;
    uint64_t expectId = 1;
    uint64_t initInt = 1;
    const uint64_t index = 0;
    pytorchApiTable.Select(PytorchApiColumn::ID, PytorchApiColumn::TIMESTAMP)
        .Select(PytorchApiColumn::ENDTIME, PytorchApiColumn::GLOBAL_TID)
        .Select(PytorchApiColumn::CONNECTIONID, PytorchApiColumn::NAME)
        .Select(PytorchApiColumn::SEQUENCE_NUMBER, PytorchApiColumn::FWD_THREAD_ID)
        .Select(PytorchApiColumn::INPUT_DTYPES, PytorchApiColumn::INPUT_SHAPES)
        .Select(PytorchApiColumn::CALL_CHAIN_ID, PytorchApiColumn::TYPE)
        .ExcuteQuery(db, pytorchApiPOs);
    const uint64_t expectType = 11;
    EXPECT_EQ(pytorchApiPOs.size(), expectSize);
    EXPECT_EQ(pytorchApiPOs[index].id, expectId);
    EXPECT_EQ(pytorchApiPOs[index].timestamp, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].endTime, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].globalTid, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].connectionId, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].name, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].sequenceNumber, "6");
    EXPECT_EQ(pytorchApiPOs[index].fwdThreadId, "7");
    EXPECT_EQ(pytorchApiPOs[index].inputDtypes, "8");
    EXPECT_EQ(pytorchApiPOs[index].inputShapes, "9");
    EXPECT_EQ(pytorchApiPOs[index].callchainId, "10");
    EXPECT_EQ(pytorchApiPOs[index].type, expectType);
}
