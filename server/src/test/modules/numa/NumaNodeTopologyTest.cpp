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

#include <algorithm>

#include "NumaNodeTopology.h"

namespace Dic::Module::Numa {
namespace {
Protocol::NumaMetricData Metric(const std::string &key, double value = 1.0, bool hasValue = true) {
    return {key, key, key + " description", "GB", value, hasValue};
}

Protocol::NumaNodeData Numa(int id, std::vector<Protocol::NumaMetricData> metrics) {
    return {id, "NUMA Node " + std::to_string(id), std::move(metrics)};
}

const Protocol::NumaConnectionData *Find(const std::vector<Protocol::NumaConnectionData> &connections,
    const std::string &type, const std::string &source = "") {
    const auto iter = std::find_if(connections.begin(), connections.end(), [&](const auto &connection) {
        return connection.type == type && (source.empty() || connection.source == source);
    });
    return iter == connections.end() ? nullptr : &*iter;
}
} // namespace

TEST(NumaNodeTopologyTest, BuildsStableNumaAndDramConnections) {
    Protocol::NumaSocketData socket{2, "Socket 2", {},
        {
            Numa(42, {Metric("crossScclRead", 3.0), Metric("dramRead", 2.0)}),
            Numa(9, {Metric("crossScclRead", 2.0), Metric("dramWrite", 1.0)}),
        }};
    std::vector<Protocol::NumaConnectionData> connections;
    AddNumaNodeConnections(socket, connections);

    const auto *numa = Find(connections, "numa");
    ASSERT_NE(numa, nullptr);
    EXPECT_EQ(numa->id, "numa-link-42-9");
    EXPECT_EQ(numa->source, "numa-42");
    EXPECT_EQ(numa->target, "numa-9");
    ASSERT_EQ(numa->metrics.size(), 1U);
    EXPECT_DOUBLE_EQ(numa->metrics[0].value, 5.0);

    for (const auto &source : {"numa-9", "numa-42"}) {
        const auto *memory = Find(connections, "memory", source);
        ASSERT_NE(memory, nullptr);
        ASSERT_EQ(memory->metrics.size(), 2U);
        EXPECT_EQ(memory->metrics[0].key, "dramRead");
        EXPECT_EQ(memory->metrics[1].key, "dramWrite");
    }
}

TEST(NumaNodeTopologyTest, AvoidsAmbiguousLinksAndKeepsUnavailableSlots) {
    const auto crossSccl = Metric("crossScclRead");
    Protocol::NumaSocketData ambiguous{0, "Socket 0", {},
        {
            Numa(0, {crossSccl}),
            Numa(1, {crossSccl}),
            Numa(2, {crossSccl}),
        }};
    std::vector<Protocol::NumaConnectionData> connections;
    AddNumaNodeConnections(ambiguous, connections);
    EXPECT_EQ(Find(connections, "numa"), nullptr);

    connections.clear();
    Protocol::NumaSocketData pair{0, "Socket 0", {},
        {
            Numa(0, {Metric("totalRead")}),
            Numa(1, {Metric("innerRead")}),
        }};
    AddNumaNodeConnections(pair, connections);
    const auto *numa = Find(connections, "numa");
    ASSERT_NE(numa, nullptr);
    ASSERT_EQ(numa->metrics.size(), 1U);
    EXPECT_EQ(numa->metrics[0].key, "crossScclRead");
    EXPECT_FALSE(numa->metrics[0].hasValue);
}

} // namespace Dic::Module::Numa
