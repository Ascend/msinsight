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
#include "ProcessTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Module::Timeline;
using namespace Dic::TimeLine::TestCaseUtil;
class ProcessTableTest : public ::testing::Test {};

TEST_F(ProcessTableTest, TestProcessColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql =
        "CREATE TABLE process (pid TEXT PRIMARY KEY, process_name TEXT, label TEXT, process_sort_index INTEGER);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
                            "\"process_sort_index\") VALUES ('207552928', 'Ascend Hardware', 'NPU', 13);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<ProcessPO> processPOS;
    ProcessTable processTable;
    const uint64_t expectSize = 1;
    const std::string expectPid = "207552928";
    const uint64_t index = 0;
    const std::string expectProcessName = "Ascend Hardware";
    const std::string expectLabel = "NPU";
    const uint64_t expectSort = 13;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::PROCESS_SORT_INDEX, ProcessColumn::LABEL)
        .ExcuteQuery(db, processPOS);
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ(processPOS[index].pid, expectPid);
    EXPECT_EQ(processPOS[index].processName, expectProcessName);
    EXPECT_EQ(processPOS[index].label, expectLabel);
    EXPECT_EQ(processPOS[index].processSortIndex, expectSort);
}
