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
#include "ComputeTaskInfoTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class ComputeTaskInfoTableTest : public ::testing::Test {};

TEST_F(ComputeTaskInfoTableTest, TestComputeTaskInfoTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql =
        "CREATE TABLE COMPUTE_TASK_INFO (name INTEGER,globalTaskId INTEGER PRIMARY KEY,blockNum INTEGER,mixBlockDim "
        "INTEGER,taskType INTEGER,opType INTEGER,inputFormats INTEGER,inputDataTypes INTEGER,inputShapes "
        "INTEGER,outputFormats INTEGER,outputDataTypes INTEGER,outputShapes INTEGER,attrInfo INTEGER, waitNs INTEGER);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert = "INSERT INTO \"main\".\"COMPUTE_TASK_INFO\" (\"name\", \"globalTaskId\", \"blockNum\", "
                            "\"mixBlockDim\", \"taskType\", \"opType\", \"inputFormats\", \"inputDataTypes\", "
                            "\"inputShapes\", \"outputFormats\", \"outputDataTypes\", \"outputShapes\", \"attrInfo\", "
                            "\"waitNs\") VALUES (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<ComputeTaskInfoPO> computeTaskInfoPOs;
    Dic::Module::Timeline::ComputeTaskInfoTable computeTaskInfoTable;
    const uint64_t expectSize = 1;
    uint64_t initInt = 1;
    const uint64_t index = 0;
    computeTaskInfoTable.Select(ComputeTaskInfoColumn::NAME, ComputeTaskInfoColumn::GLOBAL_TASK_ID)
        .Select(ComputeTaskInfoColumn::BLOCK_NUM, ComputeTaskInfoColumn::MIX_BLOCK_DIM)
        .Select(ComputeTaskInfoColumn::TASK_TYPE, ComputeTaskInfoColumn::OP_TYPE)
        .Select(ComputeTaskInfoColumn::INPUT_FORMATS, ComputeTaskInfoColumn::INPUT_DATA_TYPES)
        .Select(ComputeTaskInfoColumn::INPUT_SHAPES, ComputeTaskInfoColumn::OUTOUT_FORMATS)
        .Select(ComputeTaskInfoColumn::OUTPUT_DATA_TYPES, ComputeTaskInfoColumn::OUTPUT_SHAPES)
        .Select(ComputeTaskInfoColumn::ATTRINFO, ComputeTaskInfoColumn::WAIT_NS)
        .ExcuteQuery(db, computeTaskInfoPOs);
    EXPECT_EQ(computeTaskInfoPOs.size(), expectSize);
    EXPECT_EQ(computeTaskInfoPOs[index].name, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].globalTaskId, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].blockNum, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].mixBlockDim, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].taskType, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].opType, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].inputFormats, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].inputDataTypes, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].inputShapes, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].outputFormats, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].outputDataTypes, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].outputShapes, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].attrInfo, initInt++);
    EXPECT_EQ(computeTaskInfoPOs[index].waitNs, initInt++);
}
