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

#ifndef PROFILER_SERVER_KERNELE2EDEF_H
#define PROFILER_SERVER_KERNELE2EDEF_H

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace Dic::Module::Timeline {

constexpr auto KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE = "async_task_queue";
constexpr auto KERNEL_E2E_FLOW_ASYNC_NPU = "async_npu";
constexpr auto KERNEL_E2E_FLOW_HOST_TO_DEVICE = "HostToDevice";

// Query parameters for E2E analysis
struct KernelE2EQuery {
    std::string rankId;
    uint64_t startNs = 0;
    uint64_t endNs = 0;
};

// A single event in the kernel dispatch chain
struct KernelE2EEvent {
    uint64_t id = 0;
    std::string name;
    std::string eventType; // PYTHON_CALL / ENQUEUE / DEQUEUE / CANN_API / LAUNCH / HARDWARE
    std::string pathType; // ACLOP / ACLNN / Unknown

    uint64_t startNs = 0;
    uint64_t endNs = 0;

    uint64_t trackId = 0;
    std::string pid;
    std::string tid;
    std::string rankId;

    int64_t connectionId = -1;
    uint64_t globalTid = 0;
    uint64_t streamId = 0;
    uint64_t deviceId = 0;
};

// A flow relationship between two events
struct KernelE2EFlow {
    std::string cat;
    // TEXT format uses FLOW_TABLE.flow_id to connect flow points; DB format derives this from connectionId.
    std::string flowId;
    int64_t connectionId = -1;
    KernelE2EEvent from;
    KernelE2EEvent to;
};

// The final output record (defined before KernelE2EChain so it can be used as cachedRecord)
struct KernelE2ETimeRecord {
    std::string id;
    std::string opName;
    std::string pathType; // ACLOP / ACLNN / Unknown
    std::string parentId;
    bool isParent = false;

    uint64_t cCallTs = 0;
    uint64_t cReturnTs = 0;
    uint64_t enqueueStartTs = 0;
    uint64_t enqueueEndTs = 0;
    uint64_t dequeueStartTs = 0;
    uint64_t dequeueEndTs = 0;
    uint64_t launchStartTs = 0;
    uint64_t launchEndTs = 0;

    std::optional<uint64_t> prepareTime;
    std::optional<uint64_t> pythonApiTime;
    std::optional<uint64_t> enqueueTime;
    std::optional<uint64_t> queueTime;
    std::optional<uint64_t> pipeline2Time;
    std::optional<uint64_t> launchTime;
    std::optional<uint64_t> endToEndTime;

    std::string status; // normal / fallback / incomplete
    std::string diagnostic;
};

// A recovered chain of events for a single kernel
struct KernelE2EChain {
    std::optional<KernelE2EEvent> pythonCall;
    std::optional<KernelE2EEvent> pythonOp;
    std::optional<KernelE2EEvent> enqueue;
    std::optional<KernelE2EEvent> dequeue;
    std::optional<KernelE2EEvent> cannApi;
    std::optional<KernelE2EEvent> launch;
    std::optional<KernelE2EEvent> hardwareTask;
    std::vector<KernelE2EChain> children;

    std::string pathType = "Unknown";
    std::string parentId;
    bool isParent = false;
    std::string status = "incomplete";
    std::string diagnostic;

    // 优化3：缓存 Calculate 结果，避免排序、过滤、DTO 转换时重复计算
    mutable std::optional<KernelE2ETimeRecord> cachedRecord;
};

// Time containment helpers
inline bool Contains(const KernelE2EEvent &parent, const KernelE2EEvent &child) {
    return parent.startNs <= child.startNs && parent.endNs >= child.endNs;
}

inline bool ContainsTimestamp(const KernelE2EEvent &event, uint64_t timestamp) {
    return event.startNs <= timestamp && event.endNs >= timestamp;
}

// Event type classification
inline bool IsPytorchCallEvent(const std::string &name) { return name.rfind("<built-in", 0) == 0; }

inline bool IsEnqueueEvent(const std::string &name) { return name.rfind("Enqueue", 0) == 0; }

inline bool IsDequeueEvent(const std::string &name) { return name.rfind("Dequeue", 0) == 0; }

inline bool IsPytorchOpEvent(const std::string &name) {
    return name.rfind("aten::", 0) == 0 || name.rfind("npu::", 0) == 0 || name.rfind("vllm::", 0) == 0;
}

inline bool IsAclopEvent(const std::string &name) { return name.find("aclopCompileAndExecute") != std::string::npos; }

inline bool IsAclnnEvent(const std::string &name) {
    return name.rfind("aclnn", 0) == 0 || name.rfind("AscendCL@aclnn", 0) == 0;
}

inline bool IsCannLaunch(const std::string &name) {
    return name == "launch" || name == "Node@launch" || name.rfind("launch", 0) == 0;
}

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNELE2EDEF_H
