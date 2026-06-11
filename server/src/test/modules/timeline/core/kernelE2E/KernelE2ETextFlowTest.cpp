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
#include "../../../../DatabaseTestCaseMockUtil.h"
#include "KernelE2EAnalyzer.h"
#include "TextKernelE2ERepo.h"
#include "TextTraceDatabase.h"

using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::Timeline;

namespace {
KernelE2EEvent MakeEvent(uint64_t id, const std::string &name, const std::string &eventType, uint64_t startNs,
    uint64_t endNs, uint64_t globalTid = 1) {
    KernelE2EEvent event;
    event.id = id;
    event.name = name;
    event.eventType = eventType;
    event.startNs = startNs;
    event.endNs = endNs;
    event.globalTid = globalTid;
    return event;
}

class FakeTextKernelE2ERepo : public KernelE2ERepoInterface {
  public:
    std::vector<KernelE2EEvent> QueryPythonApiEvents(const KernelE2EQuery &query) override {
        return {
            MakeEvent(1, "<built-in method add>", "PYTHON_CALL", 100, 260),
            MakeEvent(2, "Enqueue", "ENQUEUE", 140, 160),
            MakeEvent(3, "Dequeue", "DEQUEUE", 220, 360),
        };
    }

    std::vector<KernelE2EEvent> QueryCannApiEvents(const KernelE2EQuery &query) override {
        KernelE2EEvent aclop = MakeEvent(4, "AscendCL@aclopCompileAndExecute", "CANN_API", 240, 350);
        aclop.pathType = "ACLOP";
        return {
            aclop,
            MakeEvent(5, "Node@launch", "LAUNCH", 300, 340),
        };
    }

    std::vector<KernelE2EEvent> QueryHardwareTaskEvents(const KernelE2EQuery &query,
        const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) override {
        (void)pythonEvents;
        (void)cannEvents;
        return {
            MakeEvent(6, "AddKernel", "HARDWARE", 350, 400),
        };
    }

    std::vector<KernelE2EFlow> QueryFlows(const KernelE2EQuery &query, const std::vector<KernelE2EEvent> &pythonEvents,
        const std::vector<KernelE2EEvent> &cannEvents, const std::vector<KernelE2EEvent> &hardwareTasks) override {
        (void)query;
        auto enqueue = pythonEvents[1];
        auto dequeue = pythonEvents[2];
        auto launch = cannEvents[1];
        auto task = hardwareTasks[0];
        KernelE2EFlow queueFlow;
        queueFlow.cat = KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE;
        queueFlow.flowId = "flow-queue";
        queueFlow.from = enqueue;
        queueFlow.to = dequeue;
        KernelE2EFlow hostToDeviceFlow;
        hostToDeviceFlow.cat = KERNEL_E2E_FLOW_HOST_TO_DEVICE;
        hostToDeviceFlow.flowId = "flow-host-device";
        hostToDeviceFlow.from = launch;
        hostToDeviceFlow.to = task;
        return {queueFlow, hostToDeviceFlow};
    }
};

class MockTextKernelE2EDatabase : public TextTraceDatabase {
  public:
    explicit MockTextKernelE2EDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}

    void SetDbPtr(sqlite3 *dbPtr) {
        isOpen = true;
        db = dbPtr;
        path = ":memory:";
    }
};
}

TEST(KernelE2ETextFlowTest, AnalyzeAclopWithTextFlows) {
    KernelE2EAnalyzer analyzer(std::make_unique<FakeTextKernelE2ERepo>());
    KernelE2EQuery query;
    query.rankId = "text";
    query.startNs = 0;
    query.endNs = 500;

    auto records = analyzer.Analyze(query);

    ASSERT_EQ(1, records.size());
    const auto &record = records[0];
    EXPECT_EQ("normal", record.status);
    EXPECT_EQ("ACLOP", record.pathType);
    EXPECT_EQ("<built-in method add>", record.opName);
    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(40, record.prepareTime.value());
    ASSERT_TRUE(record.enqueueTime.has_value());
    EXPECT_EQ(20, record.enqueueTime.value());
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(60, record.queueTime.value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(120, record.pipeline2Time.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(240, record.endToEndTime.value());
}

TEST(KernelE2ETextFlowTest, QueryPythonApiEventsUsesPythonStackTidForTextPythonFunction) {
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr,
        "CREATE TABLE process (pid TEXT PRIMARY KEY, process_name TEXT, label TEXT, process_sort_index INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(dbPtr,
        "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, "
        "thread_name TEXT, thread_sort_index INTEGER);");
    DatabaseTestCaseMockUtil::CreateTable(dbPtr,
        "CREATE TABLE slice (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER, name TEXT, "
        "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT, "
        "group_id TEXT);");
    DatabaseTestCaseMockUtil::InsertData(dbPtr,
        "INSERT INTO process (pid, process_name, label, process_sort_index) VALUES ('100', 'Python', 'CPU', 0);");
    DatabaseTestCaseMockUtil::InsertData(dbPtr,
        "INSERT INTO thread (track_id, tid, pid, thread_name, thread_sort_index) VALUES "
        "(10, '100', '100', 'Thread 100', 0), (20, '200', '100', 'Thread 200', 1);");
    DatabaseTestCaseMockUtil::InsertData(dbPtr,
        "INSERT INTO slice (id, timestamp, duration, name, depth, track_id, cat, args, cname, end_time, flag_id) "
        "VALUES (1, 100, 20, '<built-in method add>', 0, 10, 'python_function', '', '', 120, ''), "
        "(2, 130, 20, 'aten::add', 0, 20, '', '', '', 150, '');");
    std::recursive_mutex sqlMutex;
    auto database = std::make_shared<MockTextKernelE2EDatabase>(sqlMutex);
    database->SetDbPtr(dbPtr);
    TextKernelE2ERepo repo(database, "text");
    KernelE2EQuery query;
    query.rankId = "text";

    const auto events = repo.QueryPythonApiEvents(query);

    ASSERT_EQ(2, events.size());
    EXPECT_EQ("python_stack:text:100", events[0].tid);
    EXPECT_EQ("PYTHON_CALL", events[0].eventType);
    EXPECT_EQ("200", events[1].tid);
    EXPECT_EQ("PYTHON_OP", events[1].eventType);
}
