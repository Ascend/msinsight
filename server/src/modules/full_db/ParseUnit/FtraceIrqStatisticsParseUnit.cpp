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

#include "FtraceIrqStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"
#include "StringUtil.h"
#include "FtraceStatisticsArgs.h"
#include "FtraceStatisticsSliceReader.h"
#include <algorithm>

namespace Dic::Module::FullDb {

std::string FtraceIrqStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_IRQ_STATISTICS_UNIT; }

/**
 * 判断是否为内核进程。
 * 按进程名关键字匹配：migration / swapper / kworker
 */
static bool IsKernelProcess(const std::string &comm) {
    return StringUtil::Contains(comm, "migration") || StringUtil::Contains(comm, "swapper") ||
        StringUtil::Contains(comm, "kworker");
}

/**
 * 从 "CPU NNN" 格式的 tid 中提取 cpu_id。
 */
static int32_t ParseCpuIdFromTid(const std::string &tid) {
    if (!StringUtil::StartWith(tid, "CPU ")) {
        return -1;
    }
    std::string numPart = tid.substr(4);
    uint64_t cpu = 0;
    if (!FtraceStatistics::ParseTid(numPart, cpu) || cpu > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        return -1;
    }
    return static_cast<int32_t>(cpu);
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

bool FtraceIrqStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateTraceIrqDetailTable();
}

bool FtraceIrqStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"irq", "softirq"};
    std::unordered_map<uint64_t, std::string> trackIdToTid;
    bool threadInfoLoaded = false;

    // 统计 Key：(comm, pid, cpu_id, irq_type, irq_name)
    struct IrqStats {
        uint64_t count = 0;
        uint64_t timeNs = 0;
        bool msprof = false;
    };
    std::map<std::tuple<std::string, uint64_t, int32_t, std::string, std::string>, IrqStats> statsMap;

    const bool read = FtraceStatistics::ForEachSlice(
        *database, params.dbId, "CPU Scheduling", nameList,
        [&](const CompeteSliceDomain &slice) {
            auto argsMap = FtraceStatistics::ParseArgs(slice.args);
            const bool msprof = FtraceStatistics::IsMsprof(argsMap);
            std::string comm;
            uint64_t pid = 0;
            if (msprof) {
                if (!FtraceStatistics::Task(argsMap, comm, pid)) {
                    comm = "<unknown>";
                    pid = std::numeric_limits<uint64_t>::max();
                }
            } else {
                // Existing converted libraries expose only a display-label task identity.
                const std::string task = argsMap["task"];
                if (task.empty() || task == "<idle>" || !ParseCommAndPid(task, comm, pid) || IsKernelProcess(comm)) {
                    return true;
                }
            }

            // 提取 irq_name：irq 类型取 name，softirq 类型取 action
            std::string irqType = slice.name;
            std::string irqName = (irqType == "irq") ? argsMap["name"] : argsMap["action"];
            if (irqName.empty()) {
                if (!msprof) {
                    return true;
                }
                irqName = "<unknown>";
            }

            // 通过 CPU 线程 track 获取 cpu_id
            int32_t cpuId = -1;
            if (msprof) {
                cpuId = FtraceStatistics::Cpu(argsMap);
            } else {
                if (!threadInfoLoaded) {
                    const auto threads = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});
                    for (const auto &pair : threads) {
                        trackIdToTid[pair.first] = pair.second.second;
                    }
                    threadInfoLoaded = true;
                }
                auto tidIt = trackIdToTid.find(slice.trackId);
                if (tidIt != trackIdToTid.end()) {
                    cpuId = ParseCpuIdFromTid(tidIt->second);
                }
            }

            auto key = std::make_tuple(comm, pid, cpuId, irqType, irqName);
            auto &stat = statsMap[key];
            stat.count++;
            stat.timeNs += slice.duration;
            stat.msprof = stat.msprof || msprof;
            return true;
        },
        error);
    if (!read) {
        return false;
    }

    const bool hasMsprof =
        std::any_of(statsMap.begin(), statsMap.end(), [](const auto &entry) { return entry.second.msprof; });
    if (hasMsprof && !database->CreateTraceTaskSummaryTable()) {
        error = "Failed to initialize task summary for msprof IRQ statistics.";
        return false;
    }
    for (auto &pair : statsMap) {
        const auto &key = pair.first;
        const auto &stat = pair.second;

        TraceIrqDetailData data;
        data.comm = std::get<0>(key);
        data.pid = std::get<1>(key);
        data.cpuId = std::get<2>(key);
        data.irqType = std::get<3>(key);
        data.irqName = std::get<4>(key);
        data.count = stat.count;
        data.timeNs = stat.timeNs;

        if (stat.msprof && !FtraceStatistics::EnsureTaskSummaryRow(*database, data.comm, data.pid, data.cpuId)) {
            error = "Failed to initialize task summary row for msprof IRQ statistics.";
            return false;
        }
        if (!database->InsertTraceIrqDetail(data)) {
            error = "Failed to insert ftrace IRQ statistics.";
            return false;
        }
    }

    if (!database->CommitTraceStatisticsChecked()) {
        error = "Failed to flush ftrace IRQ statistics.";
        return false;
    }
    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceIrqStatisticsParseUnit> unitRegFtraceIrq(Dic::FTRACE_IRQ_STATISTICS_UNIT);

}
