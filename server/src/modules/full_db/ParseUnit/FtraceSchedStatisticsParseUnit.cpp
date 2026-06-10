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
    if (!StringUtil::IsAllDigits(numPart)) {
        return -1;
    }
    return static_cast<int32_t>(std::stoi(numPart));
}

bool FtraceSchedStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateTraceTaskSummaryTable();
}

bool FtraceSchedStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"sched_switch"};
    auto allSchedSlice =
        RenderEngine::Instance()->QuerySliceDetailByNameList(params.dbId, DataType::TEXT, "CPU Scheduling", nameList);

    if (allSchedSlice.empty()) {
        return true;
    }

    auto threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});

    // 构建 trackId → tid 映射，用于从 CPU 线程 track 提取 cpu_id
    std::unordered_map<uint64_t, std::string> trackIdToTid;
    for (const auto &pair : threadInfoMap) {
        trackIdToTid[pair.first] = pair.second.second; // pair.second.second = tid
    }

    // 构建 tid → trackId 映射，用于通过 prev_comm:prev_pid 找到对应的 trackId
    std::unordered_map<std::string, uint64_t> tidToTrackId;
    for (const auto &pair : threadInfoMap) {
        tidToTrackId[pair.second.second] = pair.first;
    }

    // 统计 Key：(comm, pid, cpuId)
    struct CsStats {
        uint64_t csCount = 0;
        uint64_t csInvoluntaryCount = 0;
    };
    std::map<std::tuple<std::string, uint64_t, int32_t>, CsStats> statsMap;

    for (const auto &slice : allSchedSlice) {
        auto argsMap = JsonUtil::JsonStrToMap(slice.args);

        std::string prevComm = argsMap["prev_comm"];
        std::string prevPidStr = argsMap["prev_pid"];
        std::string prevState = argsMap["prev_state"];

        if (prevComm.empty() || prevPidStr.empty()) {
            continue;
        }

        // 过滤内核进程
        if (IsKernelProcess(prevComm)) {
            continue;
        }

        uint64_t prevPid = 0;
        if (!StringUtil::IsAllDigits(prevPidStr)) {
            continue;
        }
        prevPid = std::stoull(prevPidStr);

        // 查找 prev 进程对应的 trackId
        std::string tid = prevComm + ":" + prevPidStr;
        auto it = tidToTrackId.find(tid);
        if (it == tidToTrackId.end()) {
            continue;
        }
        uint64_t trackId = it->second;

        // 通过 CPU 线程 track 获取 cpu_id
        auto tidIt = trackIdToTid.find(trackId);
        int32_t cpuId = -1;
        if (tidIt != trackIdToTid.end()) {
            cpuId = ParseCpuIdFromTid(tidIt->second);
        }

        auto &stat = statsMap[std::make_tuple(prevComm, prevPid, cpuId)];

        // prev_state == 'R' → 非自愿（被抢占）
        if (prevState == "R") {
            stat.csInvoluntaryCount++;
        }
        stat.csCount++;
    }

    for (auto &pair : statsMap) {
        const auto &key = pair.first;
        const auto &stat = pair.second;

        database->UpdateTraceTaskSummaryCsCount(
            std::get<0>(key), std::get<1>(key), std::get<2>(key), stat.csCount, stat.csInvoluntaryCount);
    }

    return true;
}

ParseUnitRegistrar<FtraceSchedStatisticsParseUnit> unitRegFtraceSched(Dic::FTRACE_SCHED_STATISTICS_UNIT);

}
