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
#include "PytorchCallchainsTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class PytorchCallchainsTableTest : public ::testing::Test {};

TEST_F(PytorchCallchainsTableTest, testPytorchCallchainsColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE PYTORCH_CALLCHAINS (id INTEGER, stack INTEGER, stackDepth INTEGER);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO \"main\".\"PYTORCH_CALLCHAINS\" (\"id\", \"stack\", \"stackDepth\") VALUES (1, 2, 3);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<PytorchCallchainsPO> pytorchApiPOs;
    Dic::Protocol::PytorchCallchainsTable pytorchApiTable;
    const uint64_t expectSize = 1;
    uint64_t initInt = 1;
    const uint64_t index = 0;
    pytorchApiTable.Select(PytorchCallchainsColumn::ID, PytorchCallchainsColumn::STACK)
        .Select(PytorchCallchainsColumn::STACK_DEPTH)
        .ExcuteQuery(db, pytorchApiPOs);
    EXPECT_EQ(pytorchApiPOs.size(), expectSize);
    EXPECT_EQ(pytorchApiPOs[index].id, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].stack, initInt++);
    EXPECT_EQ(pytorchApiPOs[index].stackDepth, initInt++);
}
