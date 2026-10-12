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
#include <limits>
#include "KernelDetailOverallDatabaseAccesser.h"
#include "TextTraceDatabase.h"
#include "TraceTime.h"
using namespace Dic::Module::Timeline;
namespace {
class OverallQueryDatabase : public TextTraceDatabase {
  public:
    explicit OverallQueryDatabase(std::recursive_mutex &mutex) : TextTraceDatabase(mutex) {}
    bool result = true;
    size_t calls = 0;
    Dic::Protocol::KernelDetailsBody rows;
    Dic::Protocol::KernelDetailsParams received;
    uint64_t receivedBase = 0;
    bool QueryKernelDetailData(const Dic::Protocol::KernelDetailsParams &params, Dic::Protocol::KernelDetailsBody &body,
        uint64_t base) override {
        ++calls;
        received = params;
        receivedBase = base;
        body = rows;
        return result;
    }
};
class KernelDetailOverallDatabaseAccesserTest : public ::testing::Test {
  protected:
    std::recursive_mutex mutex;
    std::shared_ptr<OverallQueryDatabase> db = std::make_shared<OverallQueryDatabase>(mutex);
    void SetUp() override {
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(100, 1000);
    }
    void TearDown() override { TraceTime::Instance().Reset(); }
    void Add(const std::string &type, const std::string &core, double duration) {
        Dic::Protocol::KernelDetail row;
        row.type = type;
        row.acceleratorCore = core;
        row.duration = duration;
        db->rows.kernelDetails.push_back(row);
    }
};
TEST_F(KernelDetailOverallDatabaseAccesserTest, AggregatesByTypeAndCoreAndForwardsWholeQuery) {
    Add("MatMul", "AI_CORE", 4);
    Add("MatMul", "AI_CORE", 8);
    Add("MatMul", "VECTOR", 3);
    KernelDetailOverallDatabaseAccesser accesser(db, "file");
    Dic::Protocol::KernelOverallRequest::Params params;
    std::vector<KernelDetailOverallRecord> rows;
    ASSERT_TRUE(accesser.GetKernelDetailOverallRecords(10, 90, "7", params, rows));
    ASSERT_EQ(rows.size(), 2U);
    EXPECT_EQ(rows[0].count, 2U);
    EXPECT_DOUBLE_EQ(rows[0].totalDuration, 12);
    EXPECT_DOUBLE_EQ(rows[0].avgDuration, 6);
    EXPECT_DOUBLE_EQ(rows[0].minDuration, 4);
    EXPECT_DOUBLE_EQ(rows[0].maxDuration, 8);
    EXPECT_EQ(rows[1].acceleratorCore, "VECTOR");
    EXPECT_EQ(db->received.startTime, 10U);
    EXPECT_EQ(db->received.endTime, 90U);
    EXPECT_EQ(db->received.deviceId, "7");
    EXPECT_TRUE(db->received.computingOnly);
    EXPECT_EQ(db->received.current, 1U);
    EXPECT_EQ(db->received.pageSize, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(db->receivedBase, 100U);
}
TEST_F(KernelDetailOverallDatabaseAccesserTest, AppliesCaseInsensitiveTypeAndCoreFilters) {
    Add("MatMul", "AI_CORE", 4);
    Add("Add", "AI_CORE", 8);
    Add("MatMul", "VECTOR", 3);
    KernelDetailOverallDatabaseAccesser accesser(db, "file");
    Dic::Protocol::KernelOverallRequest::Params params;
    params.filters = {{"type", "MAT"}, {"acceleratorCore", "core"}};
    std::vector<KernelDetailOverallRecord> rows;
    ASSERT_TRUE(accesser.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].type, "MatMul");
    EXPECT_DOUBLE_EQ(rows[0].totalDuration, 4);
    params.filters = {{"type", "absent"}};
    ASSERT_TRUE(accesser.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
    EXPECT_TRUE(rows.empty());
}
TEST_F(KernelDetailOverallDatabaseAccesserTest, SortsEverySupportedFieldBothDirections) {
    Add("A", "CORE_A", 1);
    Add("B", "CORE_B", 3);
    Add("B", "CORE_B", 5);
    KernelDetailOverallDatabaseAccesser accesser(db, "file");
    for (const auto *field :
        {"type", "acceleratorCore", "number", "totalTime", "avgTime", "minTime", "maxTime", "ratio", "unknown"}) {
        for (const auto *direction : {"ascend", "descend"}) {
            Dic::Protocol::KernelOverallRequest::Params params;
            params.orderBy = field;
            params.order = direction;
            std::vector<KernelDetailOverallRecord> rows;
            ASSERT_TRUE(accesser.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
            ASSERT_EQ(rows.size(), 2U);
            EXPECT_EQ(rows[0].type, std::string(direction) == "ascend" ? "A" : "B") << field;
        }
    }
}
TEST_F(KernelDetailOverallDatabaseAccesserTest, EmptyRowsClearPreviousResultsAndFailuresDoNotClaimSuccess) {
    KernelDetailOverallDatabaseAccesser accesser(db, "file");
    Dic::Protocol::KernelOverallRequest::Params params;
    std::vector<KernelDetailOverallRecord> rows(1);
    EXPECT_TRUE(accesser.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
    EXPECT_TRUE(rows.empty());
    db->result = false;
    EXPECT_FALSE(accesser.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
    KernelDetailOverallDatabaseAccesser missing(nullptr, "file");
    EXPECT_FALSE(missing.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
    KernelDetailOverallDatabaseAccesser unnamed(db, "");
    EXPECT_FALSE(unnamed.GetKernelDetailOverallRecords(0, 0, "0", params, rows));
}
TEST_F(KernelDetailOverallDatabaseAccesserTest, TimestampOverflowDoesNotQueryButExactMaximumDoes) {
    KernelDetailOverallDatabaseAccesser accesser(db, "file");
    Dic::Protocol::KernelOverallRequest::Params params;
    std::vector<KernelDetailOverallRecord> rows;
    const auto max = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(accesser.GetKernelDetailOverallRecords(max, 0, "0", params, rows));
    EXPECT_FALSE(accesser.GetKernelDetailOverallRecords(0, max, "0", params, rows));
    EXPECT_EQ(db->calls, 0U);
    EXPECT_TRUE(accesser.GetKernelDetailOverallRecords(max - 100, max - 100, "0", params, rows));
    EXPECT_EQ(db->calls, 1U);
}
}
