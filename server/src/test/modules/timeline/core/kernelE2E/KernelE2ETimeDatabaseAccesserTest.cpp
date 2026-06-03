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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#include <gtest/gtest.h>
#include <limits>

#define private public
#include "KernelE2ETimeDatabaseAccesser.h"
#undef private

using namespace Dic::Module::Timeline;

class KernelE2ETimeDatabaseAccesserTest : public ::testing::Test {
  protected:
    static KernelE2EEvent MakeEvent(
        const std::string &name, const std::string &eventType, uint64_t startNs, uint64_t endNs, uint64_t id = 1) {
        KernelE2EEvent event;
        event.id = id;
        event.name = name;
        event.eventType = eventType;
        event.startNs = startNs;
        event.endNs = endNs;
        event.pid = "123";
        event.tid = "456";
        return event;
    }

    static KernelE2ETimeRecord MakeRecord(const std::string &id, const std::string &status, uint64_t baseTime) {
        KernelE2ETimeRecord record;
        record.id = id;
        record.opName = "aclopCompileAndExecute";
        record.pathType = "ACLOP";
        record.cCallTs = baseTime + 100;
        record.cReturnTs = baseTime + 180;
        record.enqueueStartTs = baseTime + 120;
        record.enqueueEndTs = baseTime + 130;
        record.dequeueStartTs = baseTime + 150;
        record.dequeueEndTs = baseTime + 190;
        record.launchStartTs = baseTime + 160;
        record.launchEndTs = baseTime + 175;
        record.prepareTime = 20;
        record.pythonApiTime = 80;
        record.enqueueTime = 10;
        record.queueTime = 20;
        record.pipeline2Time = 25;
        record.launchTime = 15;
        record.endToEndTime = 75;
        record.status = status;
        record.diagnostic = "ok";
        return record;
    }
};

TEST_F(KernelE2ETimeDatabaseAccesserTest, ConvertRecordToDtoCopiesRecordFields) {
    constexpr uint64_t minTimestamp = 1000;
    auto record = MakeRecord("record-1", "normal", minTimestamp);
    Dic::Protocol::KernelE2ETimeRecordDto dto;

    KernelE2ETimeDatabaseAccesser::ConvertRecordToDto(record, dto);

    EXPECT_EQ("record-1", dto.id);
    EXPECT_EQ("aclopCompileAndExecute", dto.opName);
    EXPECT_EQ("ACLOP", dto.pathType);
    ASSERT_TRUE(dto.prepareTime.has_value());
    EXPECT_EQ(20, dto.prepareTime.value());
    ASSERT_TRUE(dto.endToEndTime.has_value());
    EXPECT_EQ(75, dto.endToEndTime.value());
    EXPECT_EQ("normal", dto.status);
    EXPECT_EQ("ok", dto.diagnostic);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, ConvertRecordToDtoKeepsMissingDurationsEmpty) {
    constexpr uint64_t minTimestamp = 1000;
    auto record = MakeRecord("record-1", "fallback", minTimestamp);
    record.launchTime = std::nullopt;
    Dic::Protocol::KernelE2ETimeRecordDto dto;

    KernelE2ETimeDatabaseAccesser::ConvertRecordToDto(record, dto);

    EXPECT_FALSE(dto.launchTime.has_value());
    EXPECT_EQ("fallback", dto.status);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildHighlightSlicesReturnsRecoveredNormalChainSlices) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain chain;
    chain.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 1100, 1180, 10);
    chain.pythonOp = MakeEvent("aten::conv2d", "PYTHON_OP", 1110, 1170, 11);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 1120, 1130, 20);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 1150, 1190, 21);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 1160, 1185, 30);
    chain.launch = MakeEvent("launch", "LAUNCH", 1170, 1175, 31);
    chain.hardwareTask = MakeEvent("MatMul", "HARDWARE", 1200, 1220, 40);
    auto record = MakeRecord("record-1", "normal", minTimestamp);
    std::vector<Dic::Protocol::KernelE2EHighlightSliceDto> slices;

    bool result = KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(chain, record, minTimestamp, slices);

    ASSERT_TRUE(result);
    ASSERT_EQ(7, slices.size());
    EXPECT_EQ("PYTHON_CALL", slices[0].role);
    EXPECT_EQ(100, slices[0].startTime);
    EXPECT_EQ(80, slices[0].duration);
    EXPECT_EQ("10", slices[0].id);
    EXPECT_EQ("PYTHON_OP", slices[1].role);
    EXPECT_EQ("ENQUEUE", slices[2].role);
    EXPECT_EQ("DEQUEUE", slices[3].role);
    EXPECT_EQ("CANN_API", slices[4].role);
    EXPECT_EQ("LAUNCH", slices[5].role);
    EXPECT_EQ("HARDWARE_TASK", slices[6].role);
    for (const auto &slice : slices) {
        EXPECT_GT(slice.startTime, 0);
        EXPECT_GT(slice.duration, 0);
        EXPECT_TRUE(slice.missingReason.empty());
    }
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildHighlightSlicesClampsSliceBeforeMinTimestamp) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain chain;
    chain.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 900, 980, 10);
    auto record = MakeRecord("record-1", "normal", minTimestamp);
    std::vector<Dic::Protocol::KernelE2EHighlightSliceDto> slices;

    bool result = KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(chain, record, minTimestamp, slices);

    ASSERT_TRUE(result);
    ASSERT_GE(slices.size(), 1);
    EXPECT_EQ("PYTHON_CALL", slices[0].role);
    EXPECT_EQ(0, slices[0].startTime);
    EXPECT_EQ(80, slices[0].duration);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, ConvertChainToDtoKeepsChildrenUnderParent) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain child;
    child.parentId = ":PYTHON_CALL:10";
    child.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 1100, 1180, 10);
    child.enqueue = MakeEvent("Enqueue", "ENQUEUE", 1120, 1130, 20);
    child.dequeue = MakeEvent("Dequeue", "DEQUEUE", 1150, 1190, 21);
    child.launch = MakeEvent("launch", "LAUNCH", 1170, 1175, 31);

    KernelE2EChain parent;
    parent.isParent = true;
    parent.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 1100, 1180, 10);
    parent.children = {child};

    KernelE2ECalculator calculator;
    Dic::Protocol::KernelE2ETimeRecordDto dto;
    bool result = KernelE2ETimeDatabaseAccesser::ConvertChainToDto(parent, calculator, minTimestamp, dto);

    ASSERT_TRUE(result);
    EXPECT_TRUE(dto.isParent);
    ASSERT_EQ(1, dto.children.size());
    EXPECT_FALSE(dto.children[0].id.empty());
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildHighlightSlicesReturnsOnlyPythonCallForParentChain) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain chain;
    chain.isParent = true;
    chain.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 1100, 1180, 10);
    auto record = MakeRecord("10", "normal", minTimestamp);
    record.isParent = true;
    std::vector<Dic::Protocol::KernelE2EHighlightSliceDto> slices;

    bool result = KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(chain, record, minTimestamp, slices);

    ASSERT_TRUE(result);
    ASSERT_EQ(1, slices.size());
    EXPECT_EQ("PYTHON_CALL", slices[0].role);
    EXPECT_TRUE(slices[0].missingReason.empty());
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildHighlightSlicesAddsMissingLaunchForFallback) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain chain;
    chain.pythonCall = MakeEvent("<built-in method conv2d>", "PYTHON_CALL", 1100, 1180, 10);
    chain.enqueue = MakeEvent("Enqueue", "ENQUEUE", 1120, 1130, 20);
    chain.dequeue = MakeEvent("Dequeue", "DEQUEUE", 1150, 1190, 21);
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 1160, 1185, 30);
    auto record = MakeRecord("record-1", "fallback", minTimestamp);
    std::vector<Dic::Protocol::KernelE2EHighlightSliceDto> slices;

    bool result = KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(chain, record, minTimestamp, slices);

    ASSERT_TRUE(result);
    ASSERT_EQ(5, slices.size());
    EXPECT_EQ("LAUNCH", slices.back().role);
    EXPECT_EQ(0, slices.back().startTime);
    EXPECT_EQ(0, slices.back().duration);
    EXPECT_EQ("CANN Launch not found, fallback to DEQUEUE_END", slices.back().missingReason);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildHighlightSlicesAddsMissingRequiredNodesForIncomplete) {
    constexpr uint64_t minTimestamp = 1000;
    KernelE2EChain chain;
    chain.cannApi = MakeEvent("aclopCompileAndExecute", "CANN_API", 1160, 1185, 30);
    auto record = MakeRecord("record-1", "incomplete", minTimestamp);
    std::vector<Dic::Protocol::KernelE2EHighlightSliceDto> slices;

    bool result = KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(chain, record, minTimestamp, slices);

    ASSERT_TRUE(result);
    ASSERT_EQ(5, slices.size());
    EXPECT_EQ("CANN_API", slices[0].role);
    EXPECT_EQ("PYTHON_CALL", slices[1].role);
    EXPECT_EQ("missing parent python call", slices[1].missingReason);
    EXPECT_EQ("ENQUEUE", slices[2].role);
    EXPECT_EQ("missing enqueue", slices[2].missingReason);
    EXPECT_EQ("DEQUEUE", slices[3].role);
    EXPECT_EQ("missing dequeue", slices[3].missingReason);
    EXPECT_EQ("LAUNCH", slices[4].role);
    EXPECT_EQ("missing launch", slices[4].missingReason);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, GetPagedChainsReturnsRequestedPage) {
    std::vector<KernelE2EChain> chains(5);

    auto page = KernelE2ETimeDatabaseAccesser::GetPagedChains(chains, 2, 2);

    EXPECT_EQ(2, page.size());
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, GetPagedChainsReturnsEmptyWhenPageOutOfRange) {
    std::vector<KernelE2EChain> chains(2);

    auto page = KernelE2ETimeDatabaseAccesser::GetPagedChains(chains, 3, 2);

    EXPECT_TRUE(page.empty());
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, GetPagedChainsReturnsEmptyWhenStartIndexExceedsSize) {
    std::vector<KernelE2EChain> chains(2);

    auto page = KernelE2ETimeDatabaseAccesser::GetPagedChains(chains, std::numeric_limits<uint64_t>::max(), 2);

    EXPECT_TRUE(page.empty());
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, BuildCacheKeyUsesFullQueryIdentity) {
    KernelE2EQuery query;
    query.rankId = "rank0";
    query.startNs = 100;
    query.endNs = 200;

    auto key = KernelE2ETimeDatabaseAccesser::BuildCacheKey("file.db", query);

    EXPECT_EQ("file.db", key.fileId);
    EXPECT_EQ("rank0", key.rankId);
    EXPECT_EQ(100, key.startNs);
    EXPECT_EQ(200, key.endNs);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, EstimateChainsBytesIncludesChildren) {
    KernelE2EChain parent;
    parent.pythonCall = MakeEvent("<built-in method add>", "PYTHON_CALL", 100, 200, 1);
    KernelE2EChain child;
    child.pythonOp = MakeEvent("aten::add", "PYTHON_OP", 120, 180, 2);
    parent.children = {child};
    std::vector<KernelE2EChain> chains = {parent};

    auto bytes = KernelE2ETimeDatabaseAccesser::EstimateChainsBytes(chains);

    EXPECT_GT(bytes, sizeof(KernelE2EChain));
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, UpdateStatisticsCountsStatusesAndLaunchMatchRate) {
    std::vector<Dic::Protocol::KernelE2ETimeRecordDto> records(4);
    records[0].status = "normal";
    records[1].status = "normal";
    records[2].status = "fallback";
    records[3].status = "incomplete";
    Dic::Protocol::KernelE2ETimeBody body;

    KernelE2ETimeDatabaseAccesser::UpdateStatistics(records, body);

    EXPECT_EQ(4, body.totalCount);
    EXPECT_EQ(2, body.normalCount);
    EXPECT_EQ(1, body.fallbackCount);
    EXPECT_EQ(1, body.incompleteCount);
    EXPECT_DOUBLE_EQ(0.5, body.launchMatchRate);
}

TEST_F(KernelE2ETimeDatabaseAccesserTest, UpdateStatisticsHandlesEmptyRecords) {
    Dic::Protocol::KernelE2ETimeBody body;

    KernelE2ETimeDatabaseAccesser::UpdateStatistics({}, body);

    EXPECT_EQ(0, body.totalCount);
    EXPECT_EQ(0, body.normalCount);
    EXPECT_EQ(0, body.fallbackCount);
    EXPECT_EQ(0, body.incompleteCount);
    EXPECT_DOUBLE_EQ(0.0, body.launchMatchRate);
}
