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

#include "FtraceTimeStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"
#include "ServerLog.h"
#include "StringUtil.h"
#include "FtraceStatisticsArgs.h"
#include "FtraceStatisticsSliceReader.h"

namespace Dic::Module::FullDb {

std::string FtraceTimeStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_TIME_STATISTICS_UNIT; }

bool FtraceTimeStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateTraceTaskSummaryTable();
}

/**
 * 从 tid 字符串中提取 comm 和 pid。
 * tid 格式通常为 "comm:pid"，但 comm 本身可能包含冒号（如 "kworker/0:1"），
 * 因此从最后一个冒号处分割，前面全部为 comm，最后一段为 pid。
 */
static bool ParseCommAndPid(const std::string &tid, std::string &comm, uint64_t &pid) {
    if (tid.empty()) {
        return false;
    }
    size_t lastColon = tid.rfind(':');
    if (lastColon == std::string::npos || lastColon == 0 || lastColon == tid.size() - 1) {
        return false;
    }
    comm = tid.substr(0, lastColon);
    return FtraceStatistics::ParseTid(tid.substr(lastColon + 1), pid);
}

/**
 * 从 slice.args JSON 中提取 cpu 字段。
 * 缺失时返回 -1，并通过 missingCount 引用计数，由调用方统一打印 WARNING。
 */
static int32_t ExtractCpuId(const FtraceStatistics::Args &args, int &missingCount) {
    const int32_t cpu = FtraceStatistics::Cpu(args);
    if (cpu < 0) {
        missingCount++;
    }
    return cpu;
}

bool FtraceTimeStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"Sleeping", "Runnable", "Running"};
    std::unordered_map<uint64_t, std::pair<std::string, std::string>> threadInfoMap;
    bool threadInfoLoaded = false;

    // 统计 Key：(comm, pid, cpuId)
    struct TimeStats {
        uint64_t runningNs = 0;
        uint64_t sleepingNs = 0;
        uint64_t runnableNs = 0;
    };
    std::map<std::tuple<std::string, uint64_t, int32_t>, TimeStats> statsMap;

    int cpuMissingCount = 0;

    const bool read = FtraceStatistics::ForEachSlice(
        *database, params.dbId, "Process Scheduling", nameList,
        [&](const CompeteSliceDomain &slice) {
            uint64_t trackId = slice.trackId;
            std::string timeType = slice.name;
            uint64_t duration = slice.duration;

            const auto args = FtraceStatistics::ParseArgs(slice.args);
            int32_t cpuId = ExtractCpuId(args, cpuMissingCount);
            std::string comm;
            uint64_t pid = 0;
            if (FtraceStatistics::IsMsprof(args)) {
                if (!FtraceStatistics::Task(args, comm, pid)) {
                    return true;
                }
            } else {
                // Existing converted ftrace libraries encode the identity in the track label.
                if (!threadInfoLoaded) {
                    threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});
                    threadInfoLoaded = true;
                }
                auto it = threadInfoMap.find(trackId);
                if (it == threadInfoMap.end() || !ParseCommAndPid(it->second.second, comm, pid)) {
                    return true;
                }
            }

            auto key = std::make_tuple(comm, pid, cpuId);
            auto &stat = statsMap[key];

            if (timeType == "Running") {
                stat.runningNs += duration;
            } else if (timeType == "Sleeping") {
                stat.sleepingNs += duration;
            } else if (timeType == "Runnable") {
                stat.runnableNs += duration;
            }
            return true;
        },
        error);
    if (!read) {
        return false;
    }

    if (cpuMissingCount > 0) {
        ServerLog::Warn("slice.args.cpu missing for", std::to_string(cpuMissingCount),
            " slices during ftrace time statistics, cpu_id set to -1");
    }

    for (auto &pair : statsMap) {
        const auto &key = pair.first;
        const auto &stat = pair.second;

        TraceTaskSummaryData data;
        data.comm = std::get<0>(key);
        data.pid = std::get<1>(key);
        data.cpuId = std::get<2>(key);
        data.runningNs = stat.runningNs;
        data.sleepingNs = stat.sleepingNs;
        data.runnableNs = stat.runnableNs;

        if (!database->InsertTraceTaskSummary(data)) {
            error = "Failed to insert ftrace task time statistics.";
            return false;
        }
    }

    if (!database->CommitTraceStatisticsChecked()) {
        error = "Failed to flush ftrace task time statistics.";
        return false;
    }
    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceTimeStatisticsParseUnit> unitRegFtraceTime(Dic::FTRACE_TIME_STATISTICS_UNIT);

}
