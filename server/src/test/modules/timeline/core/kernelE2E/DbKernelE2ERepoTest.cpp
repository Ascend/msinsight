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
#include "../../dataEngine/TimelineMemoryDatabaseFixture.h"
#include "DbKernelE2ERepo.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::Timeline::Test;
namespace {
class DbKernelE2ERepoTest : public TimelineMemoryDatabaseTestBase {
  protected:
    DbKernelE2ERepo repo;
    KernelE2EQuery Query(uint64_t start = 0, uint64_t end = 0) {
        KernelE2EQuery query;
        query.rankId = rank;
        query.startNs = start;
        query.endNs = end;
        return query;
    }
    static KernelE2EEvent Event(const std::string &type, int64_t connection, uint64_t start, uint64_t id) {
        KernelE2EEvent event;
        event.eventType = type;
        event.connectionId = connection;
        event.startNs = start;
        event.id = id;
        return event;
    }
};

TEST_F(DbKernelE2ERepoTest, PythonEventsClassifyNamesMapConnectionAndKeepStackIdentity) {
    ASSERT_TRUE(
        database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'Enqueue'),(2,'Dequeue'),(3,'<built-in method add>'),"
                          "(4,'aten::add'),(5,'ignored'),(6,'npu::linear'),(7,'vllm::attention');"
                          "INSERT INTO CONNECTION_IDS VALUES(8,80);"
                          "INSERT INTO PYTORCH_API VALUES(100,110,2,50003,8,3,0),(110,120,1,1,9,1,0),"
                          "(120,130,1,1,9,2,0),(130,140,1,1,9,4,0),(140,150,1,1,9,5,0),"
                          "(150,160,1,1,9,6,0),(160,170,1,1,9,7,0);"));
    const auto events = repo.QueryPythonApiEvents(Query());
    ASSERT_EQ(events.size(), 6U);
    EXPECT_EQ(events[0].eventType, "ENQUEUE");
    EXPECT_EQ(events[0].connectionId, 9);
    EXPECT_EQ(events[1].eventType, "DEQUEUE");
    EXPECT_EQ(events[2].eventType, "PYTHON_OP");
    EXPECT_EQ(events[3].eventType, "PYTHON_OP");
    EXPECT_EQ(events[4].eventType, "PYTHON_OP");
    EXPECT_EQ(events[5].eventType, "PYTHON_CALL");
    EXPECT_EQ(events[5].connectionId, 80);
    EXPECT_EQ(events[5].pid, "2");
    EXPECT_EQ(events[5].tid, "python_stack:2");
    EXPECT_EQ(events[5].rankId, rank);
    EXPECT_EQ(events[5].startNs, 100U);
    const auto boundary = repo.QueryPythonApiEvents(Query(120, 130));
    ASSERT_EQ(boundary.size(), 3U);
    EXPECT_EQ(boundary[0].name, "Enqueue");
    EXPECT_TRUE(repo.QueryPythonApiEvents(Query(1000, 2000)).empty());
}

TEST_F(DbKernelE2ERepoTest, CannEventsClassifyLaunchAclopAclnnAndFilterTime) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'launch'),(2,'Node@launch'),(3,'aclnnAdd'),"
                                  "(4,'AscendCL@aclopCompileAndExecute'),(5,'ignored');"
                                  "INSERT INTO CANN_API VALUES(10,20,1,2,101,1,0),(20,30,1,2,102,2,0),"
                                  "(30,40,1,2,103,3,0),(40,50,1,2,104,4,0),(50,60,1,2,105,5,0);"));
    const auto events = repo.QueryCannApiEvents(Query());
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events[0].eventType, "LAUNCH");
    EXPECT_EQ(events[1].eventType, "LAUNCH");
    EXPECT_EQ(events[2].pathType, "ACLNN");
    EXPECT_EQ(events[3].pathType, "ACLOP");
    EXPECT_EQ(events[0].connectionId, 101);
    EXPECT_EQ(events[0].pid, "1");
    EXPECT_EQ(events[0].tid, "2");
    EXPECT_EQ(repo.QueryCannApiEvents(Query(30, 40)).size(), 3U);
    EXPECT_TRUE(repo.QueryCannApiEvents(Query(100, 200)).empty());
}

TEST_F(DbKernelE2ERepoTest, HardwareCandidatesDeduplicateSkipInvalidAndResolveNames) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'TaskType'),(2,'aclnnKernel');"
                                  "INSERT INTO COMPUTE_TASK_INFO VALUES(20,2);"
                                  "INSERT INTO TASK VALUES(100,120,7,3,0,20,1,0),(130,150,8,4,1,21,1,0),"
                                  "(160,180,9,5,0,22,1,0);"));
    std::vector<KernelE2EEvent> python = {Event("PYTHON_OP", 7, 0, 1), Event("ENQUEUE", 7, 0, 2),
        Event("DEQUEUE", -1, 0, 3), Event("PYTHON_CALL", 9, 0, 4)};
    const auto tasks = repo.QueryHardwareTaskEvents(Query(), python, {Event("LAUNCH", 8, 0, 5)});
    ASSERT_EQ(tasks.size(), 2U);
    EXPECT_EQ(tasks[0].name, "aclnnKernel");
    EXPECT_EQ(tasks[0].pathType, "ACLNN");
    EXPECT_EQ(tasks[0].tid, "3");
    EXPECT_EQ(tasks[0].deviceId, 0U);
    EXPECT_EQ(tasks[0].eventType, "HARDWARE");
    EXPECT_EQ(tasks[1].name, "TaskType");
    EXPECT_EQ(tasks[1].deviceId, 1U);
    EXPECT_EQ(repo.QueryHardwareTaskEvents(Query(120, 125), python, {}).size(), 1U);
    EXPECT_TRUE(repo.QueryHardwareTaskEvents(Query(), {}, {}).empty());
}

TEST_F(DbKernelE2ERepoTest, HardwareQueriesMoreThanOneConnectionBatchWithoutDroppingMatches) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO TASK VALUES(1,2,1,1,0,1,0,0),(3,4,30001,2,0,2,0,0);"));
    std::vector<KernelE2EEvent> events;
    for (int64_t id = 1; id <= 30001; ++id) {
        events.push_back(Event("LAUNCH", id, 0, id));
    }
    const auto tasks = repo.QueryHardwareTaskEvents(Query(), {}, events);
    ASSERT_EQ(tasks.size(), 2U);
    EXPECT_EQ(tasks[0].connectionId, 1);
    EXPECT_EQ(tasks[1].connectionId, 30001);
}

TEST_F(DbKernelE2ERepoTest, MissingDatabaseAndMissingTablesReturnEmpty) {
    auto missing = Query();
    missing.rankId += "_missing";
    EXPECT_TRUE(repo.QueryPythonApiEvents(missing).empty());
    EXPECT_TRUE(repo.QueryCannApiEvents(missing).empty());
    EXPECT_TRUE(repo.QueryHardwareTaskEvents(missing, {}, {}).empty());
    ASSERT_TRUE(database->ExecSql("DROP TABLE PYTORCH_API; DROP TABLE CANN_API; DROP TABLE TASK;"));
    EXPECT_TRUE(repo.QueryPythonApiEvents(Query()).empty());
    EXPECT_TRUE(repo.QueryCannApiEvents(Query()).empty());
    EXPECT_TRUE(repo.QueryHardwareTaskEvents(Query(), {}, {Event("LAUNCH", 1, 0, 1)}).empty());
}

TEST_F(DbKernelE2ERepoTest, FlowsSelectLatestPrecedingEnqueueAndSeparateRelationshipTypes) {
    std::vector<KernelE2EEvent> python = {Event("ENQUEUE", 7, 30, 1), Event("ENQUEUE", 7, 10, 2),
        Event("DEQUEUE", 7, 20, 3), Event("DEQUEUE", 7, 30, 4), Event("DEQUEUE", 7, 5, 5), Event("DEQUEUE", 99, 50, 6),
        Event("PYTHON_OP", 8, 10, 7)};
    const auto flows = repo.QueryFlows(
        Query(), python, {Event("LAUNCH", 8, 15, 8)}, {Event("HARDWARE", 8, 30, 9), Event("HARDWARE", 8, 40, 10)});
    ASSERT_EQ(flows.size(), 6U);
    EXPECT_EQ(flows[0].cat, KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE);
    EXPECT_EQ(flows[0].from.id, 2U);
    EXPECT_EQ(flows[0].to.id, 3U);
    EXPECT_EQ(flows[1].from.id, 1U);
    EXPECT_EQ(flows[1].to.id, 4U);
    EXPECT_EQ(flows[2].cat, KERNEL_E2E_FLOW_HOST_TO_DEVICE);
    EXPECT_EQ(flows[3].to.id, 10U);
    EXPECT_EQ(flows[4].cat, KERNEL_E2E_FLOW_ASYNC_NPU);
    EXPECT_EQ(flows[4].flowId, "8");
    EXPECT_TRUE(repo.QueryFlows(Query(), {}, {}, {}).empty());
}
}
