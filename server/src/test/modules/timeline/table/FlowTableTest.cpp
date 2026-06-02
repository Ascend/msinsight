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
#include "FlowTable.h"
#include "TestCaseDatabaseUtil.h"
using namespace Dic::Protocol;
using namespace Dic::TimeLine::TestCaseUtil;
class FlowTableTest : public ::testing::Test {};

TEST_F(FlowTableTest, TestFlowTableColumnMaping) {
    sqlite3 *db = nullptr;
    std::string sql = "CREATE TABLE flow (id INTEGER PRIMARY KEY AUTOINCREMENT, flow_id TEXT, name TEXT, cat TEXT, "
                      "track_id INTEGER, timestamp INTEGER, type TEXT);";
    TestCaseDatabaseUtil::CreateDatabse(db, sql);
    std::string sqlInsert =
        "INSERT INTO flow (flow_id, name, cat, track_id, timestamp, type) VALUES ('1', '2','3',5,6,'7');";
    TestCaseDatabaseUtil::InsertData(db, sqlInsert);
    std::vector<FlowPO> flowPOs;
    Dic::Protocol::FlowTable flowTable;
    const uint64_t expectSize = 1;
    const uint64_t expectId = 1;
    const uint64_t index = 0;
    const std::string expectFlowId = "1";
    const std::string expectName = "2";
    const std::string expectCat = "3";
    const std::string expectType = "7";
    const uint64_t expectTrackId = 5;
    const uint64_t expectTime = 6;
    flowTable.Select(FlowColumn::ID, FlowColumn::FLOW_ID)
        .Select(FlowColumn::NAME, FlowColumn::CAT)
        .Select(FlowColumn::TRACK_ID, FlowColumn::TIMESTAMP, FlowColumn::TYPE)
        .ExcuteQuery(db, flowPOs);
    EXPECT_EQ(flowPOs.size(), expectSize);
    EXPECT_EQ(flowPOs[index].id, expectId);
    EXPECT_EQ(flowPOs[index].flowId, expectFlowId);
    EXPECT_EQ(flowPOs[index].name, expectName);
    EXPECT_EQ(flowPOs[index].cat, expectCat);
    EXPECT_EQ(flowPOs[index].timestamp, expectTime);
    EXPECT_EQ(flowPOs[index].trackId, expectTrackId);
    EXPECT_EQ(flowPOs[index].type, expectType);
}
