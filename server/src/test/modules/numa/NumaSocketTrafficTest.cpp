/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

#include <gtest/gtest.h>

#include "NumaSocketTraffic.h"

namespace Dic::Module::Numa {
namespace {
Protocol::NumaMetricData Metric(double value, bool hasValue = true) {
    return {"socketTraffic", "", "Cross socket traffic.", "GB", value, hasValue};
}

NumaMetricRecord Direction(const std::string &socket, const std::string &name) {
    return {0, 0, socket, "Cross Socket Bandwidth", name};
}

std::vector<Protocol::NumaConnectionData> Connections(const SocketTrafficMetricMap &metrics,
    std::vector<Protocol::NumaSocketData> sockets = {{0, "Socket 0", {}, {}}, {1, "Socket 1", {}, {}}}) {
    std::vector<Protocol::NumaConnectionData> connections;
    AddSocketConnections(sockets, metrics, connections);
    return connections;
}
} // namespace

TEST(NumaSocketTrafficTest, CreatesEveryPairWithUnavailableDirectionsWhenCountersAreMissing) {
    const auto connections = Connections({},
        {
            {0, "Socket 0", {}, {}},
            {1, "Socket 1", {}, {}},
            {2, "Socket 2", {}, {}},
        });
    ASSERT_EQ(connections.size(), 3U);
    for (const auto &connection : connections) {
        ASSERT_EQ(connection.metrics.size(), 2U);
        EXPECT_FALSE(connection.metrics[0].hasValue);
        EXPECT_FALSE(connection.metrics[1].hasValue);
    }
}

TEST(NumaSocketTrafficTest, PrefersIncomingButFallsBackToAvailableOutgoing) {
    const auto incoming = Direction("Socket 1", "Incoming from Socket 0");
    const auto outgoing = Direction("Socket 0", "Outgoing to Socket 1");
    EXPECT_TRUE(IsSocketTrafficHierarchy(incoming));

    SocketTrafficMetricMap metrics;
    RecordSocketTrafficMetric(incoming, Metric(1.0), metrics);
    RecordSocketTrafficMetric(outgoing, Metric(2.0), metrics);
    auto connections = Connections(metrics);
    ASSERT_EQ(connections.size(), 1U);
    EXPECT_DOUBLE_EQ(connections[0].metrics[0].value, 1.0);

    metrics.clear();
    RecordSocketTrafficMetric(incoming, Metric(0.0, false), metrics);
    RecordSocketTrafficMetric(outgoing, Metric(2.0), metrics);
    connections = Connections(metrics);
    EXPECT_TRUE(connections[0].metrics[0].hasValue);
    EXPECT_DOUBLE_EQ(connections[0].metrics[0].value, 2.0);
}

TEST(NumaSocketTrafficTest, IgnoresNonDirectionalChildren) {
    SocketTrafficMetricMap metrics;
    RecordSocketTrafficMetric(Direction("Socket 1", "Maximum Bandwidth"), Metric(3.0), metrics);
    EXPECT_TRUE(metrics.empty());
}

} // namespace Dic::Module::Numa
