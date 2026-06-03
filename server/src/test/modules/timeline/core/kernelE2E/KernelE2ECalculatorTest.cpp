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
#include "KernelE2ECalculator.h"

using namespace Dic::Module::Timeline;

class KernelE2ECalculatorTest : public ::testing::Test {
  protected:
    static KernelE2EEvent MakeEvent(
        const std::string &name, const std::string &eventType, uint64_t startNs, uint64_t endNs) {
        KernelE2EEvent event;
        event.id = startNs;
        event.name = name;
        event.eventType = eventType;
        event.startNs = startNs;
        event.endNs = endNs;
        event.globalTid = 1;
        event.connectionId = 100;
        return event;
    }
};

TEST_F(KernelE2ECalculatorTest, CalculateNormalRecord) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("normal", record.status);
    EXPECT_EQ("ACLOP", record.pathType);
    EXPECT_EQ("<built-in method add>", record.opName);
    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(30, record.prepareTime.value());
    ASSERT_TRUE(record.pythonApiTime.has_value());
    EXPECT_EQ(120, record.pythonApiTime.value());
    ASSERT_TRUE(record.enqueueTime.has_value());
    EXPECT_EQ(20, record.enqueueTime.value());
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(30, record.queueTime.value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(55, record.pipeline2Time.value());
    ASSERT_TRUE(record.launchTime.has_value());
    EXPECT_EQ(25, record.launchTime.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(135, record.endToEndTime.value());
}

TEST_F(KernelE2ECalculatorTest, CalculateLeafUsesHardwareTaskNameBeforeLaunch) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.parentId = "unknown:CANN_API:190";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);
    chain.hardwareTask = MakeEvent("hardware_kernel", "HARDWARE", 215, 230);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("hardware_kernel", record.opName);
}

TEST_F(KernelE2ECalculatorTest, CalculatePythonOpLeafUsesPythonOpNameBeforeHardwareTask) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.parentId = ":PYTHON_CALL:100";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);
    chain.hardwareTask = MakeEvent("hardware_kernel", "HARDWARE", 215, 230);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("aten::add", record.opName);
}

TEST_F(KernelE2ECalculatorTest, CalculateTopLevelCannParentUsesPythonCallName) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.isParent = true;
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("<built-in method add>", record.opName);
}

TEST_F(KernelE2ECalculatorTest, CalculateCannParentUsesPythonOpNameBeforeCannApi) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.isParent = true;
    chain.parentId = ":PYTHON_CALL:100";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("aten::add", record.opName);
}

TEST_F(KernelE2ECalculatorTest, CalculateFallsBackDownstreamWhenPythonOpMissing) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.parentId = "unknown:CANN_API:190";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("launch", record.opName);
}

TEST_F(KernelE2ECalculatorTest, CalculateFallbackRecordWhenLaunchMissing) {
    KernelE2EChain chain;
    chain.pathType = "Unknown";
    chain.pythonCall = MakeEvent("<built-in method special_op>", "PYTHON_CALL", 100, 210);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 125, 140);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 175, 260);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("fallback", record.status);
    EXPECT_EQ("CANN Launch not found, fallback to DEQUEUE_END", record.diagnostic);
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(85, record.pipeline2Time.value());
    EXPECT_FALSE(record.launchTime.has_value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(160, record.endToEndTime.value());
}

TEST_F(KernelE2ECalculatorTest, CalculateIncompleteWhenRequiredEventMissing) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("incomplete", record.status);
    EXPECT_EQ("missing python call/enqueue/dequeue", record.diagnostic);
}

TEST_F(KernelE2ECalculatorTest, CalculateIncompleteWhenTimestampsAreNonMonotonic) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 200);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 260);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_EQ("incomplete", record.status);
    EXPECT_EQ("non-monotonic timestamps", record.diagnostic);
}

TEST_F(KernelE2ECalculatorTest, CalculateLaunchHardwareLeafClearsUpstreamTimes) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.parentId = "unknown:CANN_API:190";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    EXPECT_FALSE(record.prepareTime.has_value());
    EXPECT_FALSE(record.pythonApiTime.has_value());
    EXPECT_FALSE(record.enqueueTime.has_value());
    EXPECT_FALSE(record.queueTime.has_value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(55, record.pipeline2Time.value());
    ASSERT_TRUE(record.launchTime.has_value());
    EXPECT_EQ(25, record.launchTime.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(135, record.endToEndTime.value());
}

TEST_F(KernelE2ECalculatorTest, CalculatePythonOpLeafKeepsFullChainTimes) {
    KernelE2EChain chain;
    chain.pathType = "ACLOP";
    chain.parentId = ":PYTHON_CALL:100";
    chain.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    chain.launch = MakeEvent("launch", "LAUNCH", 210, 235);

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(chain);

    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(30, record.prepareTime.value());
    ASSERT_TRUE(record.pythonApiTime.has_value());
    EXPECT_EQ(120, record.pythonApiTime.value());
    ASSERT_TRUE(record.enqueueTime.has_value());
    EXPECT_EQ(20, record.enqueueTime.value());
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(30, record.queueTime.value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(55, record.pipeline2Time.value());
    ASSERT_TRUE(record.launchTime.has_value());
    EXPECT_EQ(25, record.launchTime.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(135, record.endToEndTime.value());
}

TEST_F(KernelE2ECalculatorTest, CalculateCannParentKeepsUpstreamTimesAndAggregatesLeafTimes) {
    KernelE2EChain child;
    child.pathType = "ACLOP";
    child.parentId = ":CANN_API:190";
    child.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 220);
    child.enqueue = MakeEvent("Enqueue", "ENQUEUE", 130, 150);
    child.dequeue = MakeEvent("Dequeue", "DEQUEUE", 180, 240);
    child.launch = MakeEvent("launch", "LAUNCH", 210, 235);

    KernelE2EChain parent;
    parent.pathType = "ACLOP";
    parent.isParent = true;
    parent.parentId = ":PYTHON_CALL:100";
    parent.pythonCall = child.pythonCall;
    parent.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 110, 210);
    parent.enqueue = child.enqueue;
    parent.dequeue = child.dequeue;
    parent.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 190, 230);
    parent.children = {child};

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(parent);

    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(30, record.prepareTime.value());
    ASSERT_TRUE(record.pythonApiTime.has_value());
    EXPECT_EQ(120, record.pythonApiTime.value());
    ASSERT_TRUE(record.enqueueTime.has_value());
    EXPECT_EQ(20, record.enqueueTime.value());
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(30, record.queueTime.value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(55, record.pipeline2Time.value());
    ASSERT_TRUE(record.launchTime.has_value());
    EXPECT_EQ(25, record.launchTime.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(135, record.endToEndTime.value());
}

TEST_F(KernelE2ECalculatorTest, CalculateParentRecordAggregatesChildren) {
    KernelE2EEvent pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 300);

    KernelE2EChain firstChild;
    firstChild.pathType = "ACLOP";
    firstChild.parentId = std::to_string(pythonCall.id);
    firstChild.pythonCall = pythonCall;
    firstChild.enqueue = MakeEvent("Enqueue1", "ENQUEUE", 130, 140);
    firstChild.dequeue = MakeEvent("Dequeue1", "DEQUEUE", 160, 180);
    firstChild.launch = MakeEvent("launch1", "LAUNCH", 170, 190);

    KernelE2EChain secondChild;
    secondChild.pathType = "ACLOP";
    secondChild.parentId = std::to_string(pythonCall.id);
    secondChild.pythonCall = pythonCall;
    secondChild.enqueue = MakeEvent("Enqueue2", "ENQUEUE", 200, 215);
    secondChild.dequeue = MakeEvent("Dequeue2", "DEQUEUE", 240, 260);
    secondChild.launch = MakeEvent("launch2", "LAUNCH", 250, 280);

    KernelE2EChain parent;
    parent.pathType = "ACLOP";
    parent.pythonCall = pythonCall;
    parent.isParent = true;
    parent.children = {firstChild, secondChild};

    KernelE2ECalculator calculator;
    auto record = calculator.Calculate(parent);

    EXPECT_TRUE(record.isParent);
    EXPECT_EQ("unknown:PYTHON_CALL:" + std::to_string(pythonCall.id), record.id);
    EXPECT_EQ("<built-in method add>", record.opName);
    EXPECT_EQ("normal", record.status);
    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(30, record.prepareTime.value());
    ASSERT_TRUE(record.pythonApiTime.has_value());
    EXPECT_EQ(200, record.pythonApiTime.value());
    ASSERT_TRUE(record.enqueueTime.has_value());
    EXPECT_EQ(25, record.enqueueTime.value());
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(45, record.queueTime.value());
    ASSERT_TRUE(record.pipeline2Time.has_value());
    EXPECT_EQ(70, record.pipeline2Time.value());
    ASSERT_TRUE(record.launchTime.has_value());
    EXPECT_EQ(50, record.launchTime.value());
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(180, record.endToEndTime.value());
}
