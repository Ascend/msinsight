/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_DOMAINOBJECT_H
#define PROFILER_SERVER_DOMAINOBJECT_H
#include <string>
#include <vector>
#include <cstdint>

#include "JsonUtil.h"

namespace Dic::Module::Timeline {
struct SliceDomain {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t endTime = 0;
    uint32_t depth = 0;
    std::string groupId;
    bool operator<(const SliceDomain &right) const {
        if (depth < right.depth) {
            return true;
        }
        return depth == right.depth && timestamp < right.timestamp;
    }

    static bool CompareTimestampASC(const SliceDomain &first, const SliceDomain &second) {
        if (first.timestamp < second.timestamp) {
            return true;
        }
        if (first.timestamp == second.timestamp && first.id < second.id) {
            return true;
        }
        return false;
    }
};

struct SliceShape {
    std::string inputShapes;
    std::string inputDataTypes;
    std::string inputFormats;
    std::string outputShapes;
    std::string outputDataTypes;
    std::string outputFormats;
    std::string attrInfo;
};

struct CompeteSliceDomain {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    uint64_t endTime = 0;
    uint32_t depth = 0;
    std::string name;
    uint64_t trackId = 0;
    std::string cname;
    std::string tid;
    std::string pid;
    std::string metaType;
    std::string flagId;
    SliceShape sliceShape;
    uint64_t dataSize{};
    std::string memcpyDirection;
    std::string bandwidth;
    std::string args;
    std::string cardId;
    bool operator<(const CompeteSliceDomain &right) const {
        if (depth < right.depth) {
            return true;
        }
        return depth == right.depth && timestamp < right.timestamp;
    }
};

struct Thread {
    uint64_t trackId = 0;
    std::string tid;
    std::string pid;
    std::string threadName;
    uint32_t sortIndex = 0;
};

struct Process {
    std::string pid;
    std::string name;
    std::string label;
    uint32_t sortIndex = 0;
    std::string parentPid = "0";
};

struct FlowPoint {
    uint64_t id = 0;
    std::string flowId;
    std::string name;
    std::string cat;
    uint64_t trackId = 0;
    uint64_t timestamp = 0;
    std::string type;
    uint32_t depth = 0;
    uint64_t duration = 0;
    std::string tid;
    std::string pid;
    std::string rankId;
    bool operator<(const FlowPoint &right) const {
        if (trackId < right.trackId) {
            return true;
        }
        if (trackId > right.trackId) {
            return false;
        }
        if (trackId == right.trackId && timestamp < right.timestamp) {
            return true;
        }
        if (trackId == right.trackId && timestamp > right.timestamp) {
            return false;
        }
        return timestamp == right.timestamp && id < right.id;
    }

    static bool CompareTimestampASC(const FlowPoint &first, const FlowPoint &second) {
        if (first.timestamp == second.timestamp) {
            return first.trackId < second.trackId;
        }
        return first.timestamp < second.timestamp;
    }
};

// PROCESS_TYPE 仅仅给 DB 场景使用，Text 场景最好不要使用此枚举值
enum class PROCESS_TYPE {
    ASCEND_HARDWARE,
    HCCL,
    CCU,
    OVERLAP_ANALYSIS,
    CANN_API,
    API,
    HBM,
    LLC,
    DDR,
    ACC_PMU,
    STARS_SOC,
    NPU_MEM,
    HCCS,
    QOS,
    PCIE,
    NIC,
    SAMPLE_PMU,
    AI_CORE,
    CPU_USAGE,
    HOST_DISK_USAGE,
    HOST_MEM_USAGE,
    HOST_NETWORK_USAGE,
    OSRT_API,
    MS_TX,
    TEXT,
    NONE,
    DB,
    PYTHON_GC,
    PYTHON_STACK,
    PROCESS,
};

struct ParallelGroupInfo {
    std::string group;
    std::string groupName;
    std::vector<std::string> globalRanks;
};

enum class PYTHON_FUNCTION_STATUS {
    UNKNOWN,
    EXIST,
    NOT_EXIST,
};

enum class FtraceDataType : int8_t {
    TIME = 0, // 耗时信息
    IRQ = 1, // 中断信息
    SCHED = 2, // 上下文切换信息
    UNKOWN = 3, //未知
};

struct FtraceStatisticsData {
    uint64_t trackId = 0;
    FtraceDataType dataType = FtraceDataType::TIME;
    std::unordered_map<std::string, std::string> data;

    // 获取JSON字符串（兼容旧接口）
    std::string GetArgs() const { return JsonUtil::MapToJsonStr(data); };
    // 设置JSON字符串（兼容旧接口）
    void SetArgs(const std::string &jsonStr) { data = JsonUtil::JsonStrToMap(jsonStr); };
};

struct FtraceStatistics {
    int64_t totalCount = 0;
    std::vector<FtraceStatisticsData> data;
};

// trace_task_summary 表的一行：按 (comm, pid, cpu_id) 聚合的耗时与上下文切换统计
struct TraceTaskSummaryData {
    std::string comm;
    uint64_t pid = 0;
    int32_t cpuId = 0;
    uint64_t runningNs = 0;
    uint64_t sleepingNs = 0;
    uint64_t runnableNs = 0;
    uint64_t csCount = 0;
    uint64_t csInvoluntaryCount = 0;
};

// trace_irq_detail 表的一行：按 (comm, pid, cpu_id, irq_type, irq_name) 聚合的中断统计（进程视角）
struct TraceIrqDetailData {
    std::string comm;
    uint64_t pid = 0;
    int32_t cpuId = 0;
    std::string irqType; // "irq" 或 "softirq"
    std::string irqName; // 中断名称（如 "eth0", "timer"）
    uint64_t count = 0;
    uint64_t timeNs = 0;
};

struct TraceTaskSummaryResult {
    int64_t totalCount = 0;
    std::vector<TraceTaskSummaryData> data;
};

struct TraceIrqDetailResult {
    int64_t totalCount = 0;
    std::vector<TraceIrqDetailData> data;
};
}
#endif // PROFILER_SERVER_DOMAINOBJECT_H
