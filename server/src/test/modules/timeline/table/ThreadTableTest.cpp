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
#include "ThreadTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class ThreadTableTest : public ::testing::Test {};

TEST_F(ThreadTableTest, TestThreadTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, thread_name TEXT, "
                      "thread_sort_index INTEGER);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO thread (track_id, tid, pid,thread_name, thread_sort_index) VALUES (1, '2','3','4',5);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<ThreadPO> threadPOs;
    Dic::Protocol::ThreadTable threadTable;
    const uint64_t expectSize = 1;
    const uint64_t index = 0;
    const std::string expect1 = "2";
    const std::string expect2 = "3";
    const std::string expect3 = "4";
    const uint64_t expect11 = 1;
    const uint64_t expect12 = 5;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::PID, ThreadColumn::THREAD_NAME)
        .Select(ThreadColumn::THREAD_SORT_INDEX)
        .ExcuteQuery(db, threadPOs);
    EXPECT_EQ(threadPOs.size(), expectSize);
    EXPECT_EQ(threadPOs[index].trackId, expect11);
    EXPECT_EQ(threadPOs[index].tid, expect1);
    EXPECT_EQ(threadPOs[index].pid, expect2);
    EXPECT_EQ(threadPOs[index].threadName, expect3);
    EXPECT_EQ(threadPOs[index].threadSortIndex, expect12);
}
