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

#include "pch.h"

#include <algorithm>
#include <tuple>

#include "NumaNodeTopology.h"

namespace Dic::Module::Numa {
namespace {
using Dic::Protocol::NumaMetricData;
using Dic::Protocol::NumaNodeData;

const NumaMetricData *FindMetric(const std::vector<NumaMetricData> &metrics, const std::string &key) {
    const auto iter =
        std::find_if(metrics.begin(), metrics.end(), [&key](const auto &metric) { return metric.key == key; });
    return iter == metrics.end() ? nullptr : &(*iter);
}

NumaMetricData UnavailableMetric(const std::string &key, const std::string &label, const std::string &unit) {
    return {key, label, "", unit, 0.0, false};
}

void AddDramConnections(
    const Protocol::NumaSocketData &socket, std::vector<Protocol::NumaConnectionData> &connections) {
    for (const auto &numa : socket.numas) {
        Protocol::NumaConnectionData memory;
        memory.id = "memory-" + std::to_string(numa.id);
        memory.type = "memory";
        memory.source = "numa-" + std::to_string(numa.id);
        memory.target = memory.id;
        memory.label = numa.name + " <-> DRAM";
        memory.description = "Traffic between the NUMA node and its directly attached memory.";
        for (const auto &key : {"dramRead", "dramWrite"}) {
            const auto *metric = FindMetric(numa.metrics, key);
            if (metric != nullptr) {
                memory.metrics.emplace_back(*metric);
            } else if (std::string(key) == "dramRead") {
                memory.metrics.emplace_back(UnavailableMetric("dramRead", "DRAM Read Traffic", "GB"));
            } else {
                memory.metrics.emplace_back(UnavailableMetric("dramWrite", "DRAM Write Traffic", "GB"));
            }
        }
        connections.emplace_back(std::move(memory));
    }
}

void AccumulateMetric(const NumaMetricData *metric, double &value, NumaMetricData &result, bool &found) {
    if (metric == nullptr || !metric->hasValue) {
        return;
    }
    value += metric->value;
    if (!found) {
        result = *metric;
        found = true;
    }
}

void AddCrossScclConnection(
    const Protocol::NumaSocketData &socket, std::vector<Protocol::NumaConnectionData> &connections) {
    // 数据库没有提供显式的 SCCL 对端字段。只有两个 NUMA 时配对关系才唯一；
    // 数量更多时不能依据数组位置猜测，因此直接不生成 NUMA 间链路。
    if (socket.numas.size() != 2) {
        return;
    }

    const NumaNodeData *left = &socket.numas[0];
    const NumaNodeData *right = &socket.numas[1];
    if (std::tie(right->name, right->id) < std::tie(left->name, left->id)) {
        std::swap(left, right);
    }

    // 两个 NUMA 的 Cross SCCL Read Traffic 均表示所选时间范围内的累计流量，
    // 链路详情按业务约定展示两端指标之和。
    double value = 0;
    NumaMetricData metric;
    bool found = false;
    AccumulateMetric(FindMetric(left->metrics, "crossScclRead"), value, metric, found);
    AccumulateMetric(FindMetric(right->metrics, "crossScclRead"), value, metric, found);
    if (found) {
        metric.value = value;
    } else {
        metric = UnavailableMetric("crossScclRead", "Cross SCCL Read Traffic", "GOps");
    }

    Protocol::NumaConnectionData connection;
    connection.id = "numa-link-" + std::to_string(left->id) + "-" + std::to_string(right->id);
    connection.type = "numa";
    connection.source = "numa-" + std::to_string(left->id);
    connection.target = "numa-" + std::to_string(right->id);
    connection.label = left->name + " <-> " + right->name;
    connection.description = "Bidirectional cross-SCCL traffic inside one socket.";
    connection.metrics.emplace_back(std::move(metric));
    connections.emplace_back(std::move(connection));
}
} // namespace

void AddNumaNodeConnections(
    const Protocol::NumaSocketData &socket, std::vector<Protocol::NumaConnectionData> &connections) {
    AddDramConnections(socket, connections);
    AddCrossScclConnection(socket, connections);
}

} // namespace Dic::Module::Numa
