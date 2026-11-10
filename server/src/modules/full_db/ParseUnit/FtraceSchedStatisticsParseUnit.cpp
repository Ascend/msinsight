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

#include "FtraceSchedStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"
#include "StringUtil.h"
#include "FtraceStatisticsArgs.h"
#include "FtraceStatisticsSliceReader.h"

namespace Dic::Module::FullDb {

std::string FtraceSchedStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_SCHED_STATISTICS_UNIT; }

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

bool FtraceSchedStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateTraceTaskSummaryTable();
}

bool FtraceSchedStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"sched_switch"};
    std::unordered_map<uint64_t, std::string> trackIdToTid;
    std::unordered_map<std::string, uint64_t> tidToTrackId;
    bool threadInfoLoaded = false;

    // 统计 Key：(comm, pid, cpuId)
    struct CsStats {
        uint64_t csCount = 0;
        uint64_t csInvoluntaryCount = 0;
        bool msprof = false;
    };
    std::map<std::tuple<std::string, uint64_t, int32_t>, CsStats> statsMap;

    const bool read = FtraceStatistics::ForEachSlice(
        *database, params.dbId, "CPU Scheduling", nameList,
        [&](const CompeteSliceDomain &slice) {
            auto argsMap = FtraceStatistics::ParseArgs(slice.args);
            const bool msprof = FtraceStatistics::IsMsprof(argsMap);

            std::string prevComm = argsMap["prev_comm"];
            std::string prevPidStr = argsMap["prev_pid"];
            std::string prevState = argsMap["prev_state"];

            if (prevComm.empty() || prevPidStr.empty()) {
                return true;
            }

            // 过滤内核进程
            if (!msprof && IsKernelProcess(prevComm)) {
                return true;
            }

            uint64_t prevPid = 0;
            if (!FtraceStatistics::ParseTid(prevPidStr, prevPid)) {
                return true;
            }

            int32_t cpuId = -1;
            if (msprof) {
                cpuId = FtraceStatistics::Cpu(argsMap);
            } else {
                // Keep the track-label compatibility path for existing converted libraries.
                if (!threadInfoLoaded) {
                    const auto threads = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});
                    for (const auto &pair : threads) {
                        trackIdToTid[pair.first] = pair.second.second;
                        tidToTrackId[pair.second.second] = pair.first;
                    }
                    threadInfoLoaded = true;
                }
                std::string tid = prevComm + ":" + prevPidStr;
                auto it = tidToTrackId.find(tid);
                if (it == tidToTrackId.end()) {
                    return true;
                }
                auto tidIt = trackIdToTid.find(it->second);
                if (tidIt != trackIdToTid.end()) {
                    cpuId = ParseCpuIdFromTid(tidIt->second);
                }
            }

            auto &stat = statsMap[std::make_tuple(prevComm, prevPid, cpuId)];

            stat.msprof = stat.msprof || msprof;
            // New sources provide a validated preemption bit. R alone is not proof.
            if (msprof ? FtraceStatistics::Preempted(argsMap) : prevState == "R") {
                stat.csInvoluntaryCount++;
            }
            stat.csCount++;
            return true;
        },
        error);
    if (!read) {
        return false;
    }

    for (auto &pair : statsMap) {
        const auto &key = pair.first;
        const auto &stat = pair.second;

        if (stat.msprof &&
            !FtraceStatistics::EnsureTaskSummaryRow(*database, std::get<0>(key), std::get<1>(key), std::get<2>(key))) {
            error = "Failed to initialize task summary for msprof context switches.";
            return false;
        }
        if (!database->UpdateTraceTaskSummaryCsCount(
                std::get<0>(key), std::get<1>(key), std::get<2>(key), stat.csCount, stat.csInvoluntaryCount)) {
            error = "Failed to update ftrace context switch statistics.";
            return false;
        }
    }

    if (!database->CommitTraceStatisticsChecked()) {
        error = "Failed to flush ftrace context switch statistics.";
        return false;
    }
    return true;
}

ParseUnitRegistrar<FtraceSchedStatisticsParseUnit> unitRegFtraceSched(Dic::FTRACE_SCHED_STATISTICS_UNIT);

}
