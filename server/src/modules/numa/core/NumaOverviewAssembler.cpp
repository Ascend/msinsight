/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#include "pch.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "NumaMetricConverter.h"
#include "NumaNodeTopology.h"
#include "NumaOverviewAssembler.h"
#include "NumaSocketTraffic.h"

namespace Dic::Module::Numa {
namespace {
using Dic::Protocol::NumaMetricData;
using Dic::Protocol::NumaNodeData;
using Dic::Protocol::NumaSocketData;

NumaMetricData UnavailableMetric(const std::string &key, const std::string &label, const std::string &unit) {
    return {key, label, "", unit, 0.0, false};
}

void EnsureMetric(std::vector<NumaMetricData> &metrics, NumaMetricData fallback) {
    const auto iter = std::find_if(
        metrics.begin(), metrics.end(), [&fallback](const auto &metric) { return metric.key == fallback.key; });
    if (iter == metrics.end()) {
        metrics.emplace_back(std::move(fallback));
    }
}

void EnsureNumaMetricSlots(NumaNodeData &numa) {
    EnsureMetric(numa.metrics, UnavailableMetric("llcTraffic", "LLC Traffic", "GB"));
    EnsureMetric(numa.metrics, UnavailableMetric("innerRead", "Inner Read Traffic", "GOps"));
    // Total Read 使用数据库指标本身，不在这里重复相加；其业务语义为
    // Total Read = Inner Read + Cross Socket Read + Cross SCCL Read。
    EnsureMetric(numa.metrics, UnavailableMetric("totalRead", "Total Read Traffic", "GOps"));
    EnsureMetric(numa.metrics, UnavailableMetric("crossSocketRead", "Cross Socket Read Traffic", "GOps"));
}

void ReplaceOrAppendMetric(std::vector<NumaMetricData> &metrics, NumaMetricData replacement) {
    const auto iter = std::find_if(
        metrics.begin(), metrics.end(), [&replacement](const auto &metric) { return metric.key == replacement.key; });
    if (iter == metrics.end()) {
        metrics.emplace_back(std::move(replacement));
        return;
    }
    *iter = std::move(replacement);
}

struct SocketAssembly {
    NumaSocketData data; // Socket 节点数据
    std::vector<NumaMetricData> directMetrics; //直接挂在 Socket 层级上的原始指标
    std::map<std::string, size_t> numaIndexes; //NUMA 名称到 data.numas 数组下标的映射 "NUMA Node 0" -> 0
};

class OverviewBuilder {
  public:
    NumaOverviewData Build(const std::vector<NumaMetricRecord> &records);

  private:
    void PrepareTopology(const std::vector<NumaMetricRecord> &records);
    void CollectMetricRecord(const NumaMetricRecord &record);
    void ExpandMetricTimeRange(const NumaMetricRecord &record);
    void BuildSocketSummaryMetrics(SocketAssembly &socket);
    void BuildSystemSummaryMetrics();

    const NumaMetricData *FindMetric(const std::vector<NumaMetricData> &metrics, const std::string &key) const;
    static void AccumulateMetric(
        const NumaMetricData *metric, double &value, std::string &unit, std::string &description, bool &found);

    NumaOverviewData result_;
    std::vector<SocketAssembly> sockets_;
    std::map<std::string, size_t> socketIndexes_;
    SocketTrafficMetricMap socketTrafficMetrics_;
    bool hasMetricRange_ = false;
};
} // namespace

NumaOverviewData NumaOverviewAssembler::Build(const std::vector<NumaMetricRecord> &records) const {
    return OverviewBuilder().Build(records);
}

namespace {
NumaOverviewData OverviewBuilder::Build(const std::vector<NumaMetricRecord> &records) {
    PrepareTopology(records);

    // Repository 通常按 level_id 返回，但 Assembler 不依赖查询顺序。排序后再处理，
    // 可确保指标顺序、ID 和连接 ID 在输入记录重排后仍保持稳定。
    std::vector<const NumaMetricRecord *> orderedRecords;
    orderedRecords.reserve(records.size());
    for (const auto &record : records) {
        orderedRecords.emplace_back(&record);
    }
    std::sort(orderedRecords.begin(), orderedRecords.end(), [](const auto *left, const auto *right) {
        return std::tie(left->levelId, left->socketName, left->numaName, left->metricName) <
            std::tie(right->levelId, right->socketName, right->numaName, right->metricName);
    });
    for (const auto *record : orderedRecords) {
        ExpandMetricTimeRange(*record);
        CollectMetricRecord(*record);
    }

    // 此时每个 Socket 的 NUMA 子节点已经完整，才能安全计算汇总指标和节点链路。
    for (auto &socket : sockets_) {
        for (auto &numa : socket.data.numas) {
            EnsureNumaMetricSlots(numa);
        }
        BuildSocketSummaryMetrics(socket);
        AddNumaNodeConnections(socket.data, result_.connections);
        result_.sockets.emplace_back(std::move(socket.data));
    }
    BuildSystemSummaryMetrics();

    // Socket 链路依赖稳定 Socket ID 和全部方向指标，因此最后统一生成。
    AddSocketConnections(result_.sockets, socketTrafficMetrics_, result_.connections);
    return std::move(result_);
}

void OverviewBuilder::PrepareTopology(const std::vector<NumaMetricRecord> &records) {
    std::map<std::string, std::set<std::string>> topologyNames;
    for (const auto &record : records) {
        if (record.hierarchyDepth <= 0 || record.socketName.empty()) {
            continue;
        }
        auto &numaNames = topologyNames[record.socketName];
        if (record.hierarchyDepth > 1 && !record.numaName.empty() && !IsSocketTrafficHierarchy(record)) {
            numaNames.emplace(record.numaName);
        }
    }

    int nextNumaId = 0;
    for (const auto &[socketName, numaNames] : topologyNames) {
        SocketAssembly socket;
        socket.data.id = static_cast<int>(sockets_.size());
        socket.data.name = socketName;

        // 首次发现 Socket 时一次性初始化其 NUMA 列表。名称来自数据库层级，
        // 使用有序集合分配 ID，避免把数据库返回顺序误当成拓扑关系。
        for (const auto &numaName : numaNames) {
            socket.numaIndexes.emplace(numaName, socket.data.numas.size());
            socket.data.numas.push_back({nextNumaId++, numaName, {}});
        }
        socketIndexes_.emplace(socketName, sockets_.size());
        sockets_.emplace_back(std::move(socket));
    }
}

void OverviewBuilder::CollectMetricRecord(const NumaMetricRecord &record) {
    if (record.hierarchyDepth <= 0) {
        result_.totalMetrics.emplace_back(ConvertNumaMetric(record));
        return;
    }

    const auto socketIter = socketIndexes_.find(record.socketName);
    if (socketIter == socketIndexes_.end()) {
        return;
    }
    auto &socket = sockets_[socketIter->second];

    // Socket 间通信层级只用于构造链路，不应再被误画成一个 NUMA 节点。
    if (IsSocketTrafficHierarchy(record)) {
        RecordSocketTrafficMetric(record, ConvertNumaMetric(record), socketTrafficMetrics_);
        return;
    }

    // hierarchyDepth == 1 表示数据库指标直接挂载在 Socket 自身。
    if (record.hierarchyDepth == 1) {
        socket.directMetrics.emplace_back(ConvertNumaMetric(record));
        return;
    }

    const auto numaIter = socket.numaIndexes.find(record.numaName);
    if (numaIter != socket.numaIndexes.end()) {
        // NUMA 指标定义即使没有采样也需要保留，前端通过 hasValue 区分 0 和未采集。
        socket.data.numas[numaIter->second].metrics.emplace_back(ConvertNumaMetric(record));
    }
}

void OverviewBuilder::ExpandMetricTimeRange(const NumaMetricRecord &record) {
    if (!record.hasValue) {
        return;
    }
    if (!hasMetricRange_) {
        result_.rangeStart = record.minTimestamp;
        result_.rangeEnd = record.maxTimestamp;
        hasMetricRange_ = true;
        return;
    }
    result_.rangeStart = std::min(result_.rangeStart, record.minTimestamp);
    result_.rangeEnd = std::max(result_.rangeEnd, record.maxTimestamp);
}

void OverviewBuilder::BuildSocketSummaryMetrics(SocketAssembly &socket) {
    socket.data.metrics.insert(socket.data.metrics.end(), socket.directMetrics.begin(), socket.directMetrics.end());

    double value = 0;
    std::string unit;
    std::string description;
    bool found = false;
    for (const auto &numa : socket.data.numas) {
        AccumulateMetric(FindMetric(numa.metrics, "crossSocketRead"), value, unit, description, found);
    }
    socket.data.metrics.push_back(
        {"totalCrossSocketRead", "Total Cross Socket Read Traffic", description, found ? unit : "GOps", value, found});
}

void OverviewBuilder::BuildSystemSummaryMetrics() {
    double crossSocketValue = 0;
    double dramValue = 0;
    std::string crossSocketUnit;
    std::string dramUnit;
    std::string crossSocketDescription;
    std::string dramDescription;
    bool hasCrossSocket = false;
    bool hasDram = false;

    for (const auto &socket : result_.sockets) {
        for (const auto &numa : socket.numas) {
            AccumulateMetric(FindMetric(numa.metrics, "crossSocketRead"), crossSocketValue, crossSocketUnit,
                crossSocketDescription, hasCrossSocket);
            AccumulateMetric(FindMetric(numa.metrics, "dramRead"), dramValue, dramUnit, dramDescription, hasDram);
            AccumulateMetric(FindMetric(numa.metrics, "dramWrite"), dramValue, dramUnit, dramDescription, hasDram);
        }
    }

    const auto *existingCrossSocket = FindMetric(result_.totalMetrics, "totalCrossSocketRead");
    if (existingCrossSocket == nullptr || !existingCrossSocket->hasValue) {
        ReplaceOrAppendMetric(result_.totalMetrics,
            {"totalCrossSocketRead", "Total Cross Socket Read Operations",
                "Total cross-socket read operations for all NUMA nodes during the selected time range.",
                hasCrossSocket ? crossSocketUnit : "GOps", crossSocketValue, hasCrossSocket});
    }

    const auto *existingDram = FindMetric(result_.totalMetrics, "totalDramTraffic");
    if (existingDram == nullptr || !existingDram->hasValue) {
        ReplaceOrAppendMetric(result_.totalMetrics,
            {"totalDramTraffic", "Total DRAM traffic",
                "Total data read from and written to DRAM by all NUMA nodes during the selected time range.",
                hasDram ? dramUnit : "GB", dramValue, hasDram});
    }
}

const NumaMetricData *OverviewBuilder::FindMetric(
    const std::vector<NumaMetricData> &metrics, const std::string &key) const {
    const auto iter =
        std::find_if(metrics.begin(), metrics.end(), [&key](const auto &metric) { return metric.key == key; });
    return iter == metrics.end() ? nullptr : &(*iter);
}

void OverviewBuilder::AccumulateMetric(
    const NumaMetricData *metric, double &value, std::string &unit, std::string &description, bool &found) {
    if (metric == nullptr || !metric->hasValue) {
        return;
    }
    value += metric->value;
    if (!found) {
        unit = metric->unit;
        description = metric->description;
        found = true;
    }
}
} // namespace
} // namespace Dic::Module::Numa
