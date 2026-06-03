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

#ifndef PROFILER_SERVER_KERNELE2EREPOINTERFACE_H
#define PROFILER_SERVER_KERNELE2EREPOINTERFACE_H

#include <vector>
#include "KernelE2EDef.h"

namespace Dic::Module::Timeline {

class KernelE2ERepoInterface {
  public:
    virtual ~KernelE2ERepoInterface() = default;

    // Query Python API events (Enqueue, Dequeue, Python call)
    virtual std::vector<KernelE2EEvent> QueryPythonApiEvents(const KernelE2EQuery &query) = 0;

    // Query CANN API events (aclopCompileAndExecute, aclnn*, launch)
    virtual std::vector<KernelE2EEvent> QueryCannApiEvents(const KernelE2EQuery &query) = 0;

    // Query hardware task events (TASK + COMPUTE_TASK_INFO)
    virtual std::vector<KernelE2EEvent> QueryHardwareTaskEvents(const KernelE2EQuery &query,
        const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) = 0;

    // Query explicit flow relationships. TEXT format reads FLOW_TABLE; DB format may derive equivalent flows from connectionId.
    virtual std::vector<KernelE2EFlow> QueryFlows(const KernelE2EQuery &query,
        const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents,
        const std::vector<KernelE2EEvent> &hardwareTasks) = 0;
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNELE2EREPOINTERFACE_H
