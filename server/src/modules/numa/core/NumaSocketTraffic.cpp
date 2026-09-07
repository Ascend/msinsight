/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#include "pch.h"

#include <cctype>
#include <optional>
#include <regex>

#include "NumaSocketTraffic.h"

namespace Dic::Module::Numa {
namespace {
struct ParsedSocketTrafficDirection {
    std::string sourceSocket;
    std::string targetSocket;
    bool outgoing = false;
};

std::string NormalizeName(const std::string &name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const unsigned char character : name) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return normalized;
}

const SocketTrafficMetric *FindSocketTrafficMetric(
    const SocketTrafficMetricMap &metrics, const std::string &source, const std::string &target) {
    const auto iter = metrics.find({NormalizeName(source), NormalizeName(target)});
    return iter == metrics.end() ? nullptr : &iter->second;
}

int MetricPriority(const SocketTrafficMetric &metric) {
    // 有采样值优先于无采样值；同为可用或不可用时，Incoming 优先于 Outgoing。
    return (metric.metric.hasValue ? 2 : 0) + (metric.outgoing ? 0 : 1);
}

Protocol::NumaMetricData DirectionMetric(
    const SocketTrafficMetric *sourceMetric, const std::string &key, const std::string &label) {
    Protocol::NumaMetricData metric = sourceMetric == nullptr
        ? Protocol::NumaMetricData{"socketTraffic", "", "", "GB", 0.0, false}
        : sourceMetric->metric;
    metric.key = key;
    metric.label = label;
    return metric;
}
std::optional<ParsedSocketTrafficDirection> ParseSocketTrafficDirection(const NumaMetricRecord &record) {
    static const std::regex outgoingPattern(R"(^\s*outgoing\s+to\s+(.+)\s*$)", std::regex::icase);
    static const std::regex incomingPattern(R"(^\s*incoming\s+from\s+(.+)\s*$)", std::regex::icase);
    std::smatch match;
    if (std::regex_match(record.metricName, match, outgoingPattern)) {
        return ParsedSocketTrafficDirection{record.socketName, match[1].str(), true};
    }
    if (std::regex_match(record.metricName, match, incomingPattern)) {
        return ParsedSocketTrafficDirection{match[1].str(), record.socketName, false};
    }
    return std::nullopt;
}
} // namespace

bool IsSocketTrafficHierarchy(const NumaMetricRecord &record) {
    return NormalizeName(record.numaName) == "crosssocketbandwidth";
}

void RecordSocketTrafficMetric(
    const NumaMetricRecord &record, Protocol::NumaMetricData metric, SocketTrafficMetricMap &metrics) {
    const auto direction = ParseSocketTrafficDirection(record);
    if (!direction.has_value()) {
        return;
    }
    const SocketTrafficKey key = {NormalizeName(direction->sourceSocket), NormalizeName(direction->targetSocket)};
    if (key.first.empty() || key.second.empty() || key.first == key.second) {
        return;
    }
    metric.key = "socketTraffic";
    SocketTrafficMetric candidate{std::move(metric), direction->outgoing};
    const auto iter = metrics.find(key);
    // 同一方向优先使用目标端 Incoming；Incoming 无可用采样时才回退到源端 Outgoing。
    if (iter == metrics.end() || MetricPriority(candidate) > MetricPriority(iter->second)) {
        metrics[key] = std::move(candidate);
    }
}

void AddSocketConnections(const std::vector<Protocol::NumaSocketData> &sockets,
    const SocketTrafficMetricMap &trafficMetrics, std::vector<Protocol::NumaConnectionData> &connections) {
    for (size_t leftIndex = 0; leftIndex < sockets.size(); ++leftIndex) {
        for (size_t rightIndex = leftIndex + 1; rightIndex < sockets.size(); ++rightIndex) {
            const auto &left = sockets[leftIndex];
            const auto &right = sockets[rightIndex];
            const auto *leftToRight = FindSocketTrafficMetric(trafficMetrics, left.name, right.name);
            const auto *rightToLeft = FindSocketTrafficMetric(trafficMetrics, right.name, left.name);
            Protocol::NumaConnectionData connection;
            connection.id = "socket-link-" + std::to_string(left.id) + "-" + std::to_string(right.id);
            connection.type = "socket";
            connection.source = "socket-" + std::to_string(left.id);
            connection.target = "socket-" + std::to_string(right.id);
            connection.label = left.name + " <-> " + right.name;
            connection.description =
                "Directional socket traffic reported by the platform Cross Socket Bandwidth counters.";
            connection.metrics.emplace_back(
                DirectionMetric(leftToRight, "sourceToTarget", "From " + left.name + " to " + right.name));
            connection.metrics.emplace_back(
                DirectionMetric(rightToLeft, "targetToSource", "From " + right.name + " to " + left.name));
            connections.emplace_back(std::move(connection));
        }
    }
}

} // namespace Dic::Module::Numa
