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
#include <memory>
#include "KernelE2EAnalyzer.h"

using namespace Dic::Module::Timeline;

class KernelE2EAnalyzerTest : public ::testing::Test {
  public:
    static KernelE2EEvent MakeEvent(const std::string &name, const std::string &eventType, uint64_t startNs,
        uint64_t endNs, int64_t connectionId = 0, uint64_t globalTid = 1) {
        KernelE2EEvent event;
        event.name = name;
        event.eventType = eventType;
        event.startNs = startNs;
        event.endNs = endNs;
        event.connectionId = connectionId;
        event.globalTid = globalTid;
        return event;
    }
};

TEST_F(KernelE2EAnalyzerTest, FindEnclosingDequeueReturnsSmallestContainingEvent) {
    auto cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 200, 230);
    std::vector<KernelE2EEvent> dequeues = {
        MakeEvent("DequeueOuter", "DEQUEUE", 100, 300),
        MakeEvent("DequeueInner", "DEQUEUE", 180, 240),
        MakeEvent("DequeueMiss", "DEQUEUE", 210, 260),
    };

    auto result = KernelE2EAnalyzer::FindEnclosingDequeue(cannApi, dequeues);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("DequeueInner", result->name);
}

TEST_F(KernelE2EAnalyzerTest, FindFlowFromAndToUseTextFlowRelationship) {
    auto enqueue = MakeEvent("Enqueue", "ENQUEUE", 100, 120, 1);
    auto dequeue = MakeEvent("Dequeue", "DEQUEUE", 200, 240, 2);
    KernelE2EFlow flow;
    flow.cat = KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE;
    flow.flowId = "text-flow-1";
    flow.from = enqueue;
    flow.to = dequeue;
    std::vector<KernelE2EFlow> flows = {flow};

    auto from = KernelE2EAnalyzer::FindFlowFrom(dequeue, flows);
    auto to = KernelE2EAnalyzer::FindFlowTo(enqueue, flows);

    ASSERT_TRUE(from.has_value());
    EXPECT_EQ("Enqueue", from->name);
    ASSERT_TRUE(to.has_value());
    EXPECT_EQ("Dequeue", to->name);
}

TEST_F(KernelE2EAnalyzerTest, FindEnclosingPythonCallRequiresSameGlobalTid) {
    auto enqueue = MakeEvent("Enqueue", "ENQUEUE", 150, 160, 1, 11);
    std::vector<KernelE2EEvent> pythonCalls = {
        MakeEvent("<built-in wrong thread>", "PYTHON_CALL", 100, 200, 0, 12),
        MakeEvent("<built-in outer>", "PYTHON_CALL", 90, 210, 0, 11),
        MakeEvent("<built-in inner>", "PYTHON_CALL", 120, 180, 0, 11),
    };

    auto result = KernelE2EAnalyzer::FindEnclosingPythonCall(enqueue, pythonCalls);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("<built-in inner>", result->name);
}

TEST_F(KernelE2EAnalyzerTest, FindLaunchInsideReturnsLatestContainedLaunch) {
    auto cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 200, 300);
    std::vector<KernelE2EEvent> launches = {
        MakeEvent("launchEarly", "LAUNCH", 210, 220),
        MakeEvent("launchLate", "LAUNCH", 250, 290),
        MakeEvent("launchOutside", "LAUNCH", 280, 320),
    };

    auto result = KernelE2EAnalyzer::FindLaunchInside(cannApi, launches);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("launchLate", result->name);
}

TEST_F(KernelE2EAnalyzerTest, FindEnclosingAclnnCannApiReturnsSmallestContainingEvent) {
    auto launch = MakeEvent("launch", "LAUNCH", 250, 270);
    std::vector<KernelE2EEvent> cannApis = {
        MakeEvent("aclnnOuter", "CANN_API", 200, 300),
        MakeEvent("aclnnInner", "CANN_API", 240, 280),
        MakeEvent("aclopCompileAndExecute", "CANN_API", 230, 290),
        MakeEvent("aclnnMiss", "CANN_API", 260, 320),
    };

    auto result = KernelE2EAnalyzer::FindEnclosingAclnnCannApi(launch, cannApis);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ("aclnnInner", result->name);
}

class FakeParentChildRepo : public KernelE2ERepoInterface {
  public:
    std::vector<KernelE2EEvent> QueryPythonApiEvents(const KernelE2EQuery &query) override {
        (void)query;
        auto pythonCall = KernelE2EAnalyzerTest::MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 400, 0, 1);
        pythonCall.id = 10;
        auto firstEnqueue = KernelE2EAnalyzerTest::MakeEvent("Enqueue1", "ENQUEUE", 130, 140, 11, 1);
        firstEnqueue.id = 11;
        auto firstDequeue = KernelE2EAnalyzerTest::MakeEvent("Dequeue1", "DEQUEUE", 160, 220, 11, 1);
        firstDequeue.id = 12;
        auto secondEnqueue = KernelE2EAnalyzerTest::MakeEvent("Enqueue2", "ENQUEUE", 230, 240, 12, 1);
        secondEnqueue.id = 13;
        auto secondDequeue = KernelE2EAnalyzerTest::MakeEvent("Dequeue2", "DEQUEUE", 260, 320, 12, 1);
        secondDequeue.id = 14;
        return {pythonCall, firstEnqueue, firstDequeue, secondEnqueue, secondDequeue};
    }

    std::vector<KernelE2EEvent> QueryCannApiEvents(const KernelE2EQuery &query) override {
        (void)query;
        auto firstCANNApi = KernelE2EAnalyzerTest::MakeEvent("aclopCompileAndExecute", "CANN_API", 170, 210, 21, 1);
        firstCANNApi.id = 21;
        auto firstLaunch = KernelE2EAnalyzerTest::MakeEvent("launch", "LAUNCH", 190, 195, 21, 1);
        firstLaunch.id = 22;
        auto secondLaunch = KernelE2EAnalyzerTest::MakeEvent("launch", "LAUNCH", 200, 205, 21, 1);
        secondLaunch.id = 23;
        auto secondCANNApi = KernelE2EAnalyzerTest::MakeEvent("aclopCompileAndExecute", "CANN_API", 270, 310, 22, 1);
        secondCANNApi.id = 24;
        auto thirdLaunch = KernelE2EAnalyzerTest::MakeEvent("launch", "LAUNCH", 290, 305, 22, 1);
        thirdLaunch.id = 25;
        return {firstCANNApi, firstLaunch, secondLaunch, secondCANNApi, thirdLaunch};
    }

    std::vector<KernelE2EEvent> QueryHardwareTaskEvents(const KernelE2EQuery &query,
        const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) override {
        (void)query;
        (void)pythonEvents;
        (void)cannEvents;
        return {};
    }

    std::vector<KernelE2EFlow> QueryFlows(const KernelE2EQuery &query, const std::vector<KernelE2EEvent> &pythonEvents,
        const std::vector<KernelE2EEvent> &cannEvents, const std::vector<KernelE2EEvent> &hardwareTasks) override {
        (void)query;
        (void)cannEvents;
        (void)hardwareTasks;
        KernelE2EFlow firstFlow;
        firstFlow.cat = KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE;
        firstFlow.from = pythonEvents[1];
        firstFlow.to = pythonEvents[2];
        KernelE2EFlow secondFlow;
        secondFlow.cat = KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE;
        secondFlow.from = pythonEvents[3];
        secondFlow.to = pythonEvents[4];
        return {firstFlow, secondFlow};
    }
};

TEST_F(KernelE2EAnalyzerTest, BuildParentChildChainsGroupsCannApiWithoutPythonCall) {
    KernelE2EChain firstChild;
    firstChild.pathType = "ACLOP";
    firstChild.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 100, 200);
    firstChild.cannApi->id = 21;
    firstChild.launch = MakeEvent("launch", "LAUNCH", 120, 130);
    firstChild.launch->id = 22;

    KernelE2EChain secondChild = firstChild;
    secondChild.launch = MakeEvent("launch", "LAUNCH", 150, 160);
    secondChild.launch->id = 23;

    auto chains = KernelE2EAnalyzer::BuildParentChildChains({firstChild, secondChild});

    ASSERT_EQ(1, chains.size());
    EXPECT_TRUE(chains[0].isParent);
    ASSERT_TRUE(chains[0].cannApi.has_value());
    EXPECT_EQ(21, chains[0].cannApi->id);
    ASSERT_EQ(2, chains[0].children.size());
    EXPECT_EQ(22, chains[0].children[0].launch->id);
    EXPECT_EQ("unknown:CANN_API:21", chains[0].children[0].parentId);
    EXPECT_EQ(23, chains[0].children[1].launch->id);
    EXPECT_EQ("unknown:CANN_API:21", chains[0].children[1].parentId);
}

TEST_F(KernelE2EAnalyzerTest, AnalyzeChainsBuildsParentChildRowsForOnePythonCall) {
    KernelE2EAnalyzer analyzer(std::make_unique<FakeParentChildRepo>());
    KernelE2EQuery query;

    auto chains = KernelE2EAnalyzer::BuildParentChildChains(analyzer.AnalyzeChains(query));

    ASSERT_EQ(1, chains.size());
    EXPECT_TRUE(chains[0].isParent);
    ASSERT_TRUE(chains[0].pythonCall.has_value());
    ASSERT_EQ(2, chains[0].children.size());
    ASSERT_TRUE(chains[0].children[0].isParent);
    ASSERT_TRUE(chains[0].children[0].cannApi.has_value());
    EXPECT_EQ(21, chains[0].children[0].cannApi->id);
    EXPECT_EQ("unknown:PYTHON_CALL:10", chains[0].children[0].parentId);
    ASSERT_EQ(2, chains[0].children[0].children.size());
    EXPECT_FALSE(chains[0].children[0].children[0].isParent);
    EXPECT_EQ(22, chains[0].children[0].children[0].launch->id);
    EXPECT_EQ("unknown:CANN_API:21", chains[0].children[0].children[0].parentId);
    EXPECT_FALSE(chains[0].children[0].children[1].isParent);
    EXPECT_EQ(23, chains[0].children[0].children[1].launch->id);
    EXPECT_EQ("unknown:CANN_API:21", chains[0].children[0].children[1].parentId);
    EXPECT_FALSE(chains[0].children[1].isParent);
    EXPECT_EQ("unknown:PYTHON_CALL:10", chains[0].children[1].parentId);
}
