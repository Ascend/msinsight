/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "TimelineMemoryDatabaseFixture.h"
#include "DbFlowRepo.h"
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Timeline::Test;
namespace {
class DbFlowRepoTest : public TimelineMemoryDatabaseTestBase {
  protected:
    std::vector<FlowPoint> Query(const std::string &category) {
        DbFlowRepo repo;
        FlowQuery query;
        query.fileId = rank;
        query.cat = category;
        query.minTimestamp = 100;
        std::vector<FlowPoint> result;
        repo.QueryFlowPointByCategory(query, result);
        return result;
    }
    void AssertPair(const std::vector<FlowPoint> &points) {
        ASSERT_EQ(points.size(), 2U);
        const auto start = std::find_if(points.begin(), points.end(), [](const auto &p) { return p.type == "s"; });
        const auto end = std::find_if(points.begin(), points.end(), [](const auto &p) { return p.type == "f"; });
        ASSERT_NE(start, points.end());
        ASSERT_NE(end, points.end());
        EXPECT_EQ(start->flowId, "7");
        EXPECT_EQ(end->flowId, "7");
        EXPECT_EQ(start->timestamp, 10U);
        EXPECT_EQ(end->timestamp, 30U);
        EXPECT_EQ(start->depth, 2U);
        EXPECT_EQ(end->depth, 4U);
        EXPECT_TRUE(std::is_sorted(points.begin(), points.end()));
    }
};
TEST_F(DbFlowRepoTest, HostToDeviceUsesBothRepositoriesAndSortsPoints) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO CANN_API VALUES(110,120,1,2,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("HostToDevice"));
}
TEST_F(DbFlowRepoTest, MstxPropagatesConnectionAndDomainToDevicePair) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO MSTX_EVENTS VALUES(110,120,1,9,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("MsTx"));
}
TEST_F(DbFlowRepoTest, AsyncNpuConnectsPythonOperatorAndDeviceTask) {
    ASSERT_TRUE(
        database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'aten::add'); INSERT INTO CONNECTION_IDS VALUES(1,7);"
                          "INSERT INTO PYTORCH_API VALUES(110,120,1,1,1,1,2);"
                          "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("async_npu"));
}
TEST_F(DbFlowRepoTest, QueueAndFwdbwdUseDistinctNamesAndPreserveDepth) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'Enqueue'),(2,'Dequeue'),(3,'aten::add');"
                                  "INSERT INTO CONNECTION_IDS VALUES(1,7),(2,7);"
                                  "INSERT INTO PYTORCH_API VALUES(110,120,1,1,1,1,2),(130,140,1,1,2,2,4);"));
    AssertPair(Query("async_task_queue"));
    EXPECT_TRUE(Query("fwdbwd").empty());
    ASSERT_TRUE(database->ExecSql("UPDATE PYTORCH_API SET name=3;"));
    AssertPair(Query("fwdbwd"));
    EXPECT_TRUE(Query("async_task_queue").empty());
}
TEST_F(DbFlowRepoTest, EmptyAndUnknownCategoriesDoNotInventPoints) {
    for (const auto *category : {"async_task_queue", "fwdbwd", "async_npu", "HostToDevice", "MsTx", "unknown"}) {
        EXPECT_TRUE(Query(category).empty()) << category;
    }
    DbFlowRepo repo;
    FlowQuery query;
    query.cat = "unknown";
    std::vector<FlowPoint> points(1);
    points[0].id = 99;
    repo.QueryFlowPointByCategory(query, points);
    EXPECT_EQ(points.size(), 1U);
    EXPECT_EQ(points[0].id, 99U);
}
}
