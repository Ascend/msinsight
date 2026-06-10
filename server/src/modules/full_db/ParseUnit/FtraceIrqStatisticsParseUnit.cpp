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
    if (!StringUtil::IsAllDigits(numPart)) {
        return -1;
    }
    return static_cast<int32_t>(std::stoi(numPart));
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
    if (!StringUtil::IsAllDigits(tid.substr(lastColon + 1))) {
        return false;
    }
    pid = std::stoull(tid.substr(lastColon + 1));
    return true;
}

bool FtraceIrqStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateTraceIrqDetailTable();
}

bool FtraceIrqStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"irq", "softirq"};
    auto allIrqSlice =
        RenderEngine::Instance()->QuerySliceDetailByNameList(params.dbId, DataType::TEXT, "CPU Scheduling", nameList);

    if (allIrqSlice.empty()) {
        return true;
    }

    auto threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});

    // 构建 trackId → tid 映射，用于从 CPU 线程 track 提取 cpu_id
    std::unordered_map<uint64_t, std::string> trackIdToTid;
    for (const auto &pair : threadInfoMap) {
        trackIdToTid[pair.first] = pair.second.second; // pair.second.second = tid
    }

    // 统计 Key：(comm, pid, cpu_id, irq_type, irq_name)
    struct IrqStats {
        uint64_t count = 0;
        uint64_t timeNs = 0;
    };
    std::map<std::tuple<std::string, uint64_t, int32_t, std::string, std::string>, IrqStats> statsMap;

    for (const auto &slice : allIrqSlice) {
        auto argsMap = JsonUtil::JsonStrToMap(slice.args);

        // 提取 task（被中断的进程）
        std::string task = argsMap["task"];
        if (task.empty()) {
            continue;
        }

        // 过滤 idle 进程
        if (task == "<idle>") {
            continue;
        }

        // 解析 comm 和 pid
        std::string comm;
        uint64_t pid = 0;
        if (!ParseCommAndPid(task, comm, pid)) {
            continue;
        }

        // 过滤内核进程
        if (IsKernelProcess(comm)) {
            continue;
        }

        // 提取 irq_name：irq 类型取 name，softirq 类型取 action
        std::string irqType = slice.name;
        std::string irqName = (irqType == "irq") ? argsMap["name"] : argsMap["action"];
        if (irqName.empty()) {
            continue;
        }

        // 通过 CPU 线程 track 获取 cpu_id
        int32_t cpuId = -1;
        auto tidIt = trackIdToTid.find(slice.trackId);
        if (tidIt != trackIdToTid.end()) {
            cpuId = ParseCpuIdFromTid(tidIt->second);
        }

        auto key = std::make_tuple(comm, pid, cpuId, irqType, irqName);
        auto &stat = statsMap[key];
        stat.count++;
        stat.timeNs += slice.duration;
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

        database->InsertTraceIrqDetail(data);
    }

    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceIrqStatisticsParseUnit> unitRegFtraceIrq(Dic::FTRACE_IRQ_STATISTICS_UNIT);

}
