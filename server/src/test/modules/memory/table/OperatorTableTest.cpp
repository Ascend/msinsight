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
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TextMemoryDataBase.h"
#include "MemoryTableView.h"
#include "OperatorTable.h"
using namespace Dic::Module::Memory;
class OperatorTableTest : public ::testing::Test {};

TEST_F(OperatorTableTest, TestOperatorTableColumnMaping) {
    sqlite3 *db = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(db);
    std::string sql =
        "CREATE TABLE operator (name TEXT, size INTEGER, allocationTime INTEGER, releaseTime INTEGER, "
        "activeReleaseTime INTEGER, duration INTEGER, activeDuration INTEGER, allocationTotalAllocated INTEGER, "
        "allocationTotalReserved INTEGER, allocationTotalActive INTEGER, releaseTotalAllocated INTEGER, "
        "releaseTotalReserved INTEGER, releaseTotalActive INTEGER, streamPtr TEXT, deviceId TEXT);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::CreateTable(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"operator\" (" +
        StringUtil::GenerateColumnString(OpMemoryColumn::FULL_COLUMNS_WITHOUT_ID) +
        " ) VALUES ('aten::empty_strided', 2, 3, 4, 5, 6, "
        "7, 8, 9, 10, 11, 12, 13, "
        "'187651271017536', '0');";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(db, sqlInsert);
    std::vector<OperatorPO> operatorPos;
    OperatorTable operatorTable;
    const uint64_t expectSize = 1;
    const uint64_t index = 0;
    uint64_t initInt = 1;
    operatorTable.Select(OpMemoryColumn::ID, OpMemoryColumn::NAME)
        .Select(OpMemoryColumn::SIZE, OpMemoryColumn::ALLOCATION_TIME)
        .Select(OpMemoryColumn::RELEASE_TIME, OpMemoryColumn::ACTIVE_RELEASE_TIME)
        .Select(OpMemoryColumn::DURATION, OpMemoryColumn::ACTIVE_DURATION)
        .Select(OpMemoryColumn::ALLOCATION_ALLOCATED, OpMemoryColumn::ALLOCATION_RESERVE)
        .Select(OpMemoryColumn::ALLOCATION_ACTIVE, OpMemoryColumn::RELEASE_ALLOCATED)
        .Select(OpMemoryColumn::RELEASE_RESERVE, OpMemoryColumn::RELEASE_ACTIVE, OpMemoryColumn::STREAM)
        .ExcuteQuery(db, operatorPos);
    EXPECT_EQ(operatorPos.size(), expectSize);
    EXPECT_EQ(operatorPos[index].id, initInt++);
    EXPECT_EQ(operatorPos[index].name, "aten::empty_strided");
    EXPECT_EQ(operatorPos[index].size, initInt++);
    EXPECT_EQ(operatorPos[index].allocationTime, initInt++);
    EXPECT_EQ(operatorPos[index].releaseTime, initInt++);
    EXPECT_EQ(operatorPos[index].activeReleaseTime, initInt++);
    EXPECT_EQ(operatorPos[index].duration, initInt++);
    EXPECT_EQ(operatorPos[index].activeDuration, initInt++);
    EXPECT_EQ(operatorPos[index].allocationAllocated, initInt++);
    EXPECT_EQ(operatorPos[index].allocationReserve, initInt++);
    EXPECT_EQ(operatorPos[index].allocationActive, initInt++);
    EXPECT_EQ(operatorPos[index].releaseAllocated, initInt++);
    EXPECT_EQ(operatorPos[index].releaseReserve, initInt++);
    EXPECT_EQ(operatorPos[index].releaseActive, initInt++);
    EXPECT_EQ(operatorPos[index].stream, "187651271017536");
}
