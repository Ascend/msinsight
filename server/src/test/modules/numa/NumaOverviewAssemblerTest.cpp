/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>

#include <algorithm>

#include "NumaOverviewAssembler.h"

namespace Dic::Module::Numa {
namespace {
NumaMetricRecord Record(int64_t id, int depth, const std::string &socket, const std::string &numa,
    const std::string &metric, double value = 1.0, const std::string &unit = "GOps/s", uint64_t start = 0,
    uint64_t end = 1000000000) {
    return {id, depth, socket, numa, metric, metric + " description", unit, true, 2, value, start, end};
}

const Protocol::NumaMetricData *Metric(const std::vector<Protocol::NumaMetricData> &metrics, const std::string &key) {
    const auto iter = std::find_if(metrics.begin(), metrics.end(), [&](const auto &item) { return item.key == key; });
    return iter == metrics.end() ? nullptr : &*iter;
}

const Protocol::NumaConnectionData *Connection(const NumaOverviewData &result, const std::string &type) {
    const auto iter = std::find_if(
        result.connections.begin(), result.connections.end(), [&](const auto &item) { return item.type == type; });
    return iter == result.connections.end() ? nullptr : &*iter;
}

std::vector<NumaMetricRecord> CompleteRecords() {
    return {
        Record(1, 0, "Total External Traffic Impact", "", "Total External Traffic Impact", 12, "%"),
        Record(2, 1, "Socket 0", "External Traffic Impact", "External Traffic Impact", 5, "%"),
        Record(3, 2, "Socket 0", "NUMA Node 0", "Cross Socket Read Bandwidth", 2),
        Record(4, 2, "Socket 0", "NUMA Node 0", "Cross SCCL Read Bandwidth", 3),
        Record(5, 2, "Socket 0", "NUMA Node 0", "DRAM Read Bandwidth", 4, "GB/s"),
        Record(6, 2, "Socket 0", "NUMA Node 1", "Cross SCCL Read Bandwidth", 5),
        Record(7, 2, "Socket 0", "NUMA Node 1", "DRAM Write Bandwidth", 6, "GB/s"),
        Record(8, 2, "Socket 1", "Cross Socket Bandwidth", "Outgoing to Socket 0", 7, "GB/s"),
        Record(9, 2, "Socket 1", "NUMA Node 2", "Total Read Bandwidth", 8),
    };
}
} // namespace

TEST(NumaOverviewAssemblerTest, BuildsCompleteTopology) {
    const auto result = NumaOverviewAssembler().Build(CompleteRecords());
    ASSERT_EQ(result.sockets.size(), 2U);
    EXPECT_EQ(result.sockets[0].name, "Socket 0");
    ASSERT_EQ(result.sockets[0].numas.size(), 2U);
    EXPECT_DOUBLE_EQ(Metric(result.sockets[0].metrics, "totalCrossSocketRead")->value, 2.0);
    EXPECT_NE(Connection(result, "memory"), nullptr);
    EXPECT_DOUBLE_EQ(Connection(result, "numa")->metrics[0].value, 8.0);
    const auto *socket = Connection(result, "socket");
    ASSERT_NE(socket, nullptr);
    EXPECT_FALSE(socket->metrics[0].hasValue);
    EXPECT_DOUBLE_EQ(socket->metrics[1].value, 7.0);
}

TEST(NumaOverviewAssemblerTest, KeepsStableIdsWhenRecordOrderChanges) {
    auto reversed = CompleteRecords();
    const auto forward = NumaOverviewAssembler().Build(reversed);
    std::reverse(reversed.begin(), reversed.end());
    const auto reverse = NumaOverviewAssembler().Build(reversed);
    ASSERT_EQ(forward.sockets.size(), reverse.sockets.size());
    for (size_t i = 0; i < forward.sockets.size(); ++i) {
        EXPECT_EQ(forward.sockets[i].id, reverse.sockets[i].id);
        ASSERT_EQ(forward.sockets[i].numas.size(), reverse.sockets[i].numas.size());
        for (size_t j = 0; j < forward.sockets[i].numas.size(); ++j) {
            EXPECT_EQ(forward.sockets[i].numas[j].id, reverse.sockets[i].numas[j].id);
        }
    }
}

TEST(NumaOverviewAssemblerTest, PreservesUnavailableSlotsWithoutInventingAmbiguousLinks) {
    auto unavailable = Record(1, 2, "Socket 0", "NUMA Node 0", "Total Read Bandwidth");
    unavailable.hasValue = false;
    unavailable.sampleCount = 0;
    auto records = std::vector<NumaMetricRecord>{
        unavailable,
        Record(2, 2, "Socket 0", "NUMA Node 1", "Cross SCCL Read Bandwidth"),
        Record(3, 2, "Socket 0", "NUMA Node 2", "Cross SCCL Read Bandwidth"),
    };
    const auto result = NumaOverviewAssembler().Build(records);
    ASSERT_EQ(result.sockets[0].numas.size(), 3U);
    for (const auto &key : {"llcTraffic", "innerRead", "totalRead", "crossSocketRead"}) {
        const auto *metric = Metric(result.sockets[0].numas[0].metrics, key);
        ASSERT_NE(metric, nullptr);
        EXPECT_FALSE(metric->hasValue);
    }
    EXPECT_EQ(Connection(result, "numa"), nullptr);
    EXPECT_EQ(Connection(result, "socket"), nullptr);
    EXPECT_NE(Connection(result, "memory"), nullptr);
}

TEST(NumaOverviewAssemblerTest, CalculatesActualMetricTimeRange) {
    auto ignored = Record(3, 2, "Socket 0", "NUMA Node 1", "LLC Bandwidth", 3, "GB/s", 80, 300);
    ignored.hasValue = false;
    const auto result = NumaOverviewAssembler().Build({
        Record(1, 2, "Socket 0", "NUMA Node 0", "Total Read Bandwidth", 1, "GOps/s", 120, 180),
        Record(2, 2, "Socket 0", "NUMA Node 1", "Inner Read Bandwidth", 2, "GOps/s", 90, 250),
        ignored,
    });
    EXPECT_EQ(result.rangeStart, 90U);
    EXPECT_EQ(result.rangeEnd, 250U);
}

} // namespace Dic::Module::Numa
