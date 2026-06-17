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

#ifndef PROFILER_SERVER_KERNELE2EANALYZER_H
#define PROFILER_SERVER_KERNELE2EANALYZER_H

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "KernelE2EDef.h"
#include "KernelE2ERepoInterface.h"
#include "KernelE2ECalculator.h"

namespace Dic::Module::Timeline {

class KernelE2EAnalyzer {
  public:
    explicit KernelE2EAnalyzer(std::unique_ptr<KernelE2ERepoInterface> repo);

    // Main entry: run full E2E analysis for a query
    std::vector<KernelE2ETimeRecord> Analyze(const KernelE2EQuery &query);

    // Returns recovered chains before time calculation so the accesser can generate UI highlight slices.
    std::vector<KernelE2EChain> AnalyzeChains(const KernelE2EQuery &query);

    // Find the parent Dequeue that contains a CANN API event (by time containment)
    static std::optional<KernelE2EEvent> FindEnclosingDequeue(
        const KernelE2EEvent &cannEvent, const std::vector<const KernelE2EEvent *> &sortedDequeues);

    // 优化1：Flow 哈希索引，FindFlowFrom/FindFlowTo 从 O(F) 线性扫描改为 O(1) 哈希查找
    struct FlowIndex {
        std::unordered_map<std::string, KernelE2EEvent> fromIndex;
        std::unordered_map<std::string, KernelE2EEvent> toIndex;
    };

    static std::optional<KernelE2EEvent> FindFlowFrom(const KernelE2EEvent &to, const FlowIndex &flowIndex);

    static std::optional<KernelE2EEvent> FindFlowTo(const KernelE2EEvent &from, const FlowIndex &flowIndex);

    // Find the parent Python call that contains an event (by time containment)
    static std::optional<KernelE2EEvent> FindEnclosingPythonCall(const KernelE2EEvent &child,
        const std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> &pythonCallsByGlobalTid);

    // Find the concrete Python op that contains an Enqueue event.
    static std::optional<KernelE2EEvent> FindEnclosingPythonOp(const KernelE2EEvent &child,
        const std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> &pythonOpsByGlobalTid);

    // Find the launch inside a CANN API (by time containment)
    static std::optional<KernelE2EEvent> FindLaunchInside(
        const KernelE2EEvent &cannApi, const std::vector<const KernelE2EEvent *> &sortedLaunches);

    static std::vector<KernelE2EEvent> FindLaunchesInside(
        const KernelE2EEvent &cannApi, const std::vector<const KernelE2EEvent *> &sortedLaunches);

    static std::optional<KernelE2EEvent> FindEnclosingAclnnCannApi(
        const KernelE2EEvent &launch, const std::vector<const KernelE2EEvent *> &sortedCannApis);

    static std::vector<KernelE2EChain> BuildParentChildChains(const std::vector<KernelE2EChain> &chains);

  private:
    std::unique_ptr<KernelE2ERepoInterface> repo;
    KernelE2ECalculator calculator;

    // Build lookup indexes from events
    struct EventIndex {
        std::vector<KernelE2EEvent> pythonCalls;
        std::vector<KernelE2EEvent> pythonOps;
        std::vector<KernelE2EEvent> enqueues;
        std::vector<KernelE2EEvent> dequeues;
        std::vector<KernelE2EEvent> cannApis;
        std::vector<KernelE2EEvent> launches;
        std::vector<KernelE2EEvent> hardwareTasks;
        std::vector<KernelE2EFlow> asyncTaskQueueFlows;
        std::vector<KernelE2EFlow> hostToDeviceFlows;
        std::vector<KernelE2EFlow> asyncNpuFlows;
        // 优化1：Flow 哈希索引，按 flow 类别分别建索引
        FlowIndex asyncTaskQueueFlowIndex;
        FlowIndex hostToDeviceFlowIndex;
        FlowIndex asyncNpuFlowIndex;

        // 优化4：按 startNs 排序的指针列表，用于二分查找加速 FindEnclosing* / FindLaunchesInside
        std::vector<const KernelE2EEvent *> sortedDequeues;
        std::vector<const KernelE2EEvent *> sortedCannApis;
        std::vector<const KernelE2EEvent *> sortedLaunches;
        // 按线程分桶的指针列表，用于 FindEnclosingPythonCall / FindEnclosingPythonOp
        // key: globalTid（globalTid 为 0 时用 trackId 兜底）
        std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> pythonCallsByGlobalTid;
        std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> pythonOpsByGlobalTid;
    };

    EventIndex BuildIndex(const std::vector<KernelE2EEvent> &pythonEvents,
        const std::vector<KernelE2EEvent> &cannEvents, const std::vector<KernelE2EEvent> &taskEvents,
        const std::vector<KernelE2EFlow> &flows);

    // ACLOP chain recovery: from aclopCompileAndExecute
    std::vector<KernelE2EChain> RecoverAclopChains(const EventIndex &index);

    // ACLNN chain recovery: from aclnn* hardware task
    std::vector<KernelE2EChain> RecoverAclnnChains(const EventIndex &index);
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNELE2EANALYZER_H
