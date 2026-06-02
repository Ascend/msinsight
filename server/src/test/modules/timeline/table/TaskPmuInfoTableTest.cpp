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
#include "TaskPmuInfoTable.h"
#include "TestCaseDatabaseUtil.h"

using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class TaskPmuInfoTableTest : public ::testing::Test {};

TEST_F(TaskPmuInfoTableTest, GetPmuInfo) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE TASK_PMU_INFO (globalTaskId INTEGER, name INTEGER, value NUMERIC)";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO \"main\".\"TASK_PMU_INFO\" (\"globalTaskId\", \"name\", \"value\") VALUES (1, 2, 3);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<TaskPmuInfoPO> pmuInfos;
    TaskPmuInfoTable pmuInfoTable;
    const uint64_t expectSize = 1;
    pmuInfoTable.Select(TaskPmuInfoColumn::GLOBAL_TASK_ID, TaskPmuInfoColumn::NAME_ID, TaskPmuInfoColumn::VALUE_ID)
        .ExcuteQuery(db, pmuInfos);
    EXPECT_EQ(pmuInfos.size(), expectSize);
    EXPECT_EQ(pmuInfos.at(0).globalTaskId, 1); // 1
    EXPECT_EQ(pmuInfos.at(0).name, 2); // 2
    EXPECT_EQ(pmuInfos.at(0).value, 3); // 3
}
