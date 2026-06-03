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
#include "KernelE2EAnalyzer.h"

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
