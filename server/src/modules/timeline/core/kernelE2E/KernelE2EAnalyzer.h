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
        const KernelE2EEvent &cannEvent, const std::vector<KernelE2EEvent> &dequeues);

    static std::optional<KernelE2EEvent> FindFlowFrom(
        const KernelE2EEvent &to, const std::vector<KernelE2EFlow> &flows);

    static std::optional<KernelE2EEvent> FindFlowTo(
        const KernelE2EEvent &from, const std::vector<KernelE2EFlow> &flows);

    // Find the parent Python call that contains an event (by time containment)
    static std::optional<KernelE2EEvent> FindEnclosingPythonCall(
        const KernelE2EEvent &child, const std::vector<KernelE2EEvent> &pythonCalls);

    // Find the concrete Python op that contains an Enqueue event.
    static std::optional<KernelE2EEvent> FindEnclosingPythonOp(
        const KernelE2EEvent &child, const std::vector<KernelE2EEvent> &pythonOps);

    // Find the launch inside a CANN API (by time containment)
    static std::optional<KernelE2EEvent> FindLaunchInside(
        const KernelE2EEvent &cannApi, const std::vector<KernelE2EEvent> &launches);

    static std::vector<KernelE2EEvent> FindLaunchesInside(
        const KernelE2EEvent &cannApi, const std::vector<KernelE2EEvent> &launches);

    static std::optional<KernelE2EEvent> FindEnclosingAclnnCannApi(
        const KernelE2EEvent &launch, const std::vector<KernelE2EEvent> &cannApis);

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
