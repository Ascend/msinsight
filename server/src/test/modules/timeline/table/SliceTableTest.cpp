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
#include "SliceTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class SliceTableTest : public ::testing::Test {};

TEST_F(SliceTableTest, TestSliceTableColumnMaping)
{
    sqlite3 *db = nullptr;
    std::string sql =
        "CREATE TABLE slice (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER, name TEXT, "
        "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT, "
        "group_id TEXT);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO slice (timestamp, duration, name,track_id, cat, "
                            "args,cname,end_time,flag_id,group_id) VALUES (1, 2,'3',5,'6','7','8',9,'10','11');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<SlicePO> slicePos;
    Dic::Protocol::SliceTable sliceTable;
    const uint64_t expectSize = 1;
    const uint64_t expectId = 1;
    const uint64_t index = 0;
    const std::string expect1 = "3";
    const std::string expect2 = "6";
    const std::string expect3 = "7";
    const std::string expect4 = "8";
    const std::string expect5 = "10";
    const std::string expect6 = "11";
    const uint64_t expect11 = 1;
    const uint64_t expect12 = 2;
    const uint64_t expect14 = 5;
    const uint64_t expect15 = 9;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::NAME)
        .Select(SliceColumn::TRACKID, SliceColumn::CAT, SliceColumn::ARGS)
        .Select(SliceColumn::CNAME, SliceColumn::ENDTIME, SliceColumn::FLAGID)
        .Select(SliceColumn::GROUPID)
        .ExcuteQuery(db, slicePos);
    EXPECT_EQ(slicePos.size(), expectSize);
    EXPECT_EQ(slicePos[index].id, expectId);
    EXPECT_EQ(slicePos[index].timestamp, expect11);
    EXPECT_EQ(slicePos[index].duration, expect12);
    EXPECT_EQ(slicePos[index].name, expect1);
    EXPECT_EQ(slicePos[index].trackId, expect14);
    EXPECT_EQ(slicePos[index].cat, expect2);
    EXPECT_EQ(slicePos[index].args, expect3);
    EXPECT_EQ(slicePos[index].cname, expect4);
    EXPECT_EQ(slicePos[index].endTime, expect15);
    EXPECT_EQ(slicePos[index].flagId, expect5);
    EXPECT_EQ(slicePos[index].groupId, expect6);
}
