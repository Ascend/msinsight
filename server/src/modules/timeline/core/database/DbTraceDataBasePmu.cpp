/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "DbTraceDataBase.h"
#include "PmuCounterRepository.h"
#include "RankLaneMergeCoordinator.h"
#include <filesystem>
#include <map>

namespace Dic::Module::FullDb {
void DbTraceDataBase::ExtendPmuTimeRange(uint64_t &min, uint64_t &max) {
    Timeline::PmuCounterRepository repository(*this);
    if (!repository.DetectCapabilities().available) {
        return;
    }
    auto statement = CreatPreparedStatement(
        "SELECT MIN(s.timestamp),MAX(s.timestamp) FROM HOST_PMU_COUNTER_DATA s "
        "JOIN HOST_PMU_COUNTER_INFO c ON c.counterId=s.counterId JOIN HOST_PMU_EVENTS e ON e.eventId=c.eventId "
        "WHERE typeof(s.timestamp)='integer' AND s.timestamp>=0 "
        "AND typeof(c.counterId)='integer' AND c.counterId>=0 "
        "AND typeof(c.eventId)='integer' AND c.eventId>=0 AND typeof(e.eventId)='integer' AND e.eventId>=0 "
        "AND typeof(c.pid)='integer' AND c.pid=-1 AND typeof(c.cpu)='integer' AND c.cpu>=0");
    if (statement == nullptr) {
        return;
    }
    auto result = statement->ExecuteQuery();
    if (result != nullptr && result->Next() && sqlite3_column_type(statement->stmt, 0) == SQLITE_INTEGER &&
        sqlite3_column_type(statement->stmt, 1) == SQLITE_INTEGER) {
        min = std::min(min, static_cast<uint64_t>(sqlite3_column_int64(statement->stmt, 0)));
        max = std::max(max, static_cast<uint64_t>(sqlite3_column_int64(statement->stmt, 1)));
    }
}

void DbTraceDataBase::ProcessPmuCounterMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    try {
        Timeline::PmuCounterRepository repository(*this);
        const auto capabilities = repository.DetectCapabilities();
        if (!capabilities.available) {
            if (capabilities.reason != "pmu_not_collected") {
                Server::ServerLog::Warn(
                    "PMU metadata unavailable. source: ", GetDbPath(), ", reason: ", capabilities.reason);
            }
            return;
        }
        std::vector<Timeline::PmuCounterInfo> counters;
        if (!repository.ListCounters(counters, capabilities)) {
            Server::ServerLog::Warn("Failed to query PMU counters. source: ", GetDbPath());
            return;
        }
        if (counters.empty()) {
            return;
        }
        const auto dbPath = GetDbPath();
        const auto sourceKey = Timeline::RankLaneMergeCoordinator::NormalizeSourceFileId(dbPath);
        const auto sourcePath = std::filesystem::path(dbPath);
        const auto sourceLabel = sourcePath.parent_path().filename().string() + "/" + sourcePath.filename().string();
        auto group = GenerateBaseUnitTrack("label", fileId, sourceKey + ":pmu", "PMU", "PMU_COUNTER");
        auto initialize = [&](Protocol::UnitTrack &track, const std::string &kind, const std::string &key) {
            track.metaData.dbPath = dbPath;
            track.metaData.sourceLabel = sourceLabel;
            track.metaData.pmu.emplace();
            track.metaData.pmu->kind = kind;
            track.metaData.pmu->sourceKey = sourceKey;
            track.metaData.pmu->nodeKey = key;
        };
        initialize(*group, "group", sourceKey + ":pmu");
        group->metaData.headerTooltip = "System-wide CPU counters; source: " + dbPath;
        std::map<int64_t, Protocol::UnitTrack *> cpuTracks;
        for (const auto &counter : counters) {
            const auto cpuKey = sourceKey + ":pmu:cpu:" + std::to_string(counter.cpu);
            const auto cpuName = "CPU " + std::to_string(counter.cpu);
            if (cpuTracks.count(counter.cpu) == 0) {
                auto cpu = GenerateBaseUnitTrack("label", fileId, cpuKey, cpuName, "PMU_COUNTER");
                initialize(*cpu, "cpu", cpuKey);
                cpu->metaData.pmu->cpu = counter.cpu;
                cpuTracks.emplace(counter.cpu, cpu.get());
                group->children.emplace_back(std::move(cpu));
            }
            auto event = GenerateBaseUnitTrack("counter", fileId, cpuKey, cpuName, "PMU_COUNTER");
            initialize(*event, "event", sourceKey + ":pmu:counter:" + std::to_string(counter.counterId));
            event->metaData.threadId = std::to_string(counter.counterId);
            event->metaData.threadName = counter.name;
            event->metaData.dataType = {counter.name};
            auto &pmu = *event->metaData.pmu;
            pmu.counterId = std::to_string(counter.counterId);
            pmu.eventId = std::to_string(counter.eventId);
            pmu.cpu = counter.cpu;
            pmu.eventName = counter.name;
            pmu.eventSource = counter.source;
            pmu.unit = counter.unit;
            pmu.intervalNs = std::to_string(counter.intervalNs);
            pmu.hasSamples = counter.hasSamples;
            cpuTracks[counter.cpu]->children.emplace_back(std::move(event));
        }
        auto *metrics = GetOrCreateCpuMetricsUnitTrack(fileId, metaData);
        metrics->metaData.dbPath = dbPath;
        metrics->children.emplace_back(std::move(group));
    } catch (const std::exception &exception) {
        Server::ServerLog::Warn("Unable to read PMU metadata: ", exception.what());
    }
}
}
