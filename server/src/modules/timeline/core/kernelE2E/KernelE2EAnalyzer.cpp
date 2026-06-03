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

#include "pch.h"
#include <algorithm>
#include <map>
#include <set>
#include "KernelE2EAnalyzer.h"
#include "ServerLog.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

namespace {
bool IsSameThreadContext(const KernelE2EEvent &parent, const KernelE2EEvent &child) {
    if (parent.globalTid != 0 || child.globalTid != 0) {
        return parent.globalTid == child.globalTid;
    }
    return parent.trackId != 0 && child.trackId != 0 && parent.trackId == child.trackId;
}

std::string BuildEventRecordId(const KernelE2EEvent &event) {
    const auto rankId = event.rankId.empty() ? "unknown" : event.rankId;
    return rankId + ":" + event.eventType + ":" + std::to_string(event.id);
}
}

KernelE2EAnalyzer::KernelE2EAnalyzer(std::unique_ptr<KernelE2ERepoInterface> repo) : repo(std::move(repo)) {}

std::vector<KernelE2ETimeRecord> KernelE2EAnalyzer::Analyze(const KernelE2EQuery &query) {
    auto chains = AnalyzeChains(query);
    std::vector<KernelE2ETimeRecord> records;
    records.reserve(chains.size());
    for (const auto &chain : chains) {
        records.emplace_back(calculator.Calculate(chain));
    }
    ServerLog::Info("KernelE2E: recovered ", chains.size(), " chains, ", records.size(), " records");
    return records;
}

std::vector<KernelE2EChain> KernelE2EAnalyzer::AnalyzeChains(const KernelE2EQuery &query) {
    auto pythonEvents = repo->QueryPythonApiEvents(query);
    auto cannEvents = repo->QueryCannApiEvents(query);
    auto taskEvents = repo->QueryHardwareTaskEvents(query, pythonEvents, cannEvents);
    auto flows = repo->QueryFlows(query, pythonEvents, cannEvents, taskEvents);

    ServerLog::Info("KernelE2E: queried ", pythonEvents.size(), " python events, ", cannEvents.size(), " CANN events, ",
        taskEvents.size(), " task events, ", flows.size(), " flows");

    auto index = BuildIndex(pythonEvents, cannEvents, taskEvents, flows);
    std::vector<KernelE2EChain> chains;

    auto aclopChains = RecoverAclopChains(index);
    chains.insert(chains.end(), aclopChains.begin(), aclopChains.end());

    auto aclnnChains = RecoverAclnnChains(index);
    chains.insert(chains.end(), aclnnChains.begin(), aclnnChains.end());
    return chains;
}

KernelE2EAnalyzer::EventIndex KernelE2EAnalyzer::BuildIndex(const std::vector<KernelE2EEvent> &pythonEvents,
    const std::vector<KernelE2EEvent> &cannEvents, const std::vector<KernelE2EEvent> &taskEvents,
    const std::vector<KernelE2EFlow> &flows) {
    EventIndex index;
    const auto addUniqueEvent = [](std::vector<KernelE2EEvent> &events, const KernelE2EEvent &event) {
        const auto iter = std::find_if(events.begin(), events.end(), [&event](const KernelE2EEvent &existing) {
            return existing.id == event.id && existing.eventType == event.eventType &&
                existing.rankId == event.rankId && existing.name == event.name && existing.startNs == event.startNs &&
                existing.endNs == event.endNs;
        });
        if (iter == events.end()) {
            events.push_back(event);
        }
    };
    const auto addEventToIndex = [&index, &addUniqueEvent](const KernelE2EEvent &event) {
        if (event.eventType == "PYTHON_CALL") {
            addUniqueEvent(index.pythonCalls, event);
        } else if (event.eventType == "PYTHON_OP") {
            addUniqueEvent(index.pythonOps, event);
        } else if (event.eventType == "ENQUEUE") {
            addUniqueEvent(index.enqueues, event);
        } else if (event.eventType == "DEQUEUE") {
            addUniqueEvent(index.dequeues, event);
        } else if (event.eventType == "LAUNCH") {
            addUniqueEvent(index.launches, event);
        } else if (event.eventType == "CANN_API") {
            addUniqueEvent(index.cannApis, event);
        } else if (event.eventType == "HARDWARE") {
            addUniqueEvent(index.hardwareTasks, event);
        }
    };
    for (const auto &event : pythonEvents) {
        addEventToIndex(event);
    }
    for (const auto &event : cannEvents) {
        addEventToIndex(event);
    }
    for (const auto &event : taskEvents) {
        addEventToIndex(event);
    }
    for (const auto &flow : flows) {
        // addEventToIndex(flow.from);
        // addEventToIndex(flow.to);
        if (flow.cat == KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE) {
            index.asyncTaskQueueFlows.push_back(flow);
        } else if (flow.cat == KERNEL_E2E_FLOW_HOST_TO_DEVICE) {
            index.hostToDeviceFlows.push_back(flow);
        } else if (flow.cat == KERNEL_E2E_FLOW_ASYNC_NPU) {
            index.asyncNpuFlows.push_back(flow);
        }
    }
    return index;
}

std::vector<KernelE2EChain> KernelE2EAnalyzer::BuildParentChildChains(const std::vector<KernelE2EChain> &chains) {
    std::map<uint64_t, size_t> cannChildCounts;
    for (const auto &chain : chains) {
        if (chain.cannApi.has_value()) {
            ++cannChildCounts[chain.cannApi->id];
        }
    }

    std::vector<KernelE2EChain> cannGroupedChains;
    cannGroupedChains.reserve(chains.size() + cannChildCounts.size());
    std::map<uint64_t, size_t> cannParentIndexes;
    for (const auto &chain : chains) {
        if (!chain.cannApi.has_value() || cannChildCounts[chain.cannApi->id] <= 1) {
            cannGroupedChains.emplace_back(chain);
            continue;
        }

        const auto cannParentId = chain.cannApi->id;
        const auto cannParentIter = cannParentIndexes.find(cannParentId);
        if (cannParentIter == cannParentIndexes.end()) {
            KernelE2EChain cannParent;
            cannParent.pathType = chain.pathType;
            cannParent.pythonCall = chain.pythonCall;
            cannParent.pythonOp = chain.pythonOp;
            cannParent.enqueue = chain.enqueue;
            cannParent.dequeue = chain.dequeue;
            cannParent.cannApi = chain.cannApi;
            cannParent.isParent = true;
            cannParent.children.reserve(cannChildCounts[cannParentId]);
            cannGroupedChains.emplace_back(std::move(cannParent));
            cannParentIndexes[cannParentId] = cannGroupedChains.size() - 1;
        }

        auto child = chain;
        child.parentId = BuildEventRecordId(chain.cannApi.value());
        child.isParent = false;
        cannGroupedChains[cannParentIndexes[cannParentId]].children.emplace_back(std::move(child));
    }

    std::map<uint64_t, size_t> pythonChildCounts;
    for (const auto &chain : cannGroupedChains) {
        if (chain.pythonCall.has_value()) {
            ++pythonChildCounts[chain.pythonCall->id];
        }
    }

    std::map<uint64_t, size_t> pythonParentIndexes;
    std::vector<KernelE2EChain> result;
    result.reserve(cannGroupedChains.size() + pythonChildCounts.size());
    for (const auto &chain : cannGroupedChains) {
        if (!chain.pythonCall.has_value() || pythonChildCounts[chain.pythonCall->id] <= 1) {
            result.emplace_back(chain);
            continue;
        }

        const auto pythonParentId = chain.pythonCall->id;
        const auto pythonParentIter = pythonParentIndexes.find(pythonParentId);
        if (pythonParentIter == pythonParentIndexes.end()) {
            KernelE2EChain pythonParent;
            pythonParent.pathType = chain.pathType;
            pythonParent.pythonCall = chain.pythonCall;
            pythonParent.isParent = true;
            pythonParent.children.reserve(pythonChildCounts[pythonParentId]);
            result.emplace_back(std::move(pythonParent));
            pythonParentIndexes[pythonParentId] = result.size() - 1;
        }

        auto child = chain;
        child.parentId = BuildEventRecordId(chain.pythonCall.value());
        child.isParent = chain.isParent;
        result[pythonParentIndexes[pythonParentId]].children.emplace_back(std::move(child));
    }
    return result;
}

std::vector<KernelE2EChain> KernelE2EAnalyzer::RecoverAclopChains(const EventIndex &index) {
    std::vector<KernelE2EChain> chains;
    for (const auto &cannApi : index.cannApis) {
        if (!IsAclopEvent(cannApi.name)) {
            continue;
        }

        KernelE2EChain chain;
        chain.pathType = "ACLOP";
        chain.cannApi = cannApi;
        chain.dequeue = FindEnclosingDequeue(cannApi, index.dequeues);
        if (chain.dequeue.has_value()) {
            chain.enqueue = FindFlowFrom(chain.dequeue.value(), index.asyncTaskQueueFlows);
            if (chain.enqueue.has_value()) {
                chain.pythonOp = FindEnclosingPythonOp(chain.enqueue.value(), index.pythonOps);
                chain.pythonCall = chain.pythonOp.has_value()
                    ? FindEnclosingPythonCall(chain.pythonOp.value(), index.pythonCalls)
                    : FindEnclosingPythonCall(chain.enqueue.value(), index.pythonCalls);
            }
        }

        const auto launches = FindLaunchesInside(cannApi, index.launches);
        if (launches.empty()) {
            if (!chain.dequeue.has_value()) {
                chain.diagnostic = "missing dequeue for aclopCompileAndExecute";
            } else if (!chain.enqueue.has_value()) {
                chain.diagnostic = "missing enqueue (async_task_queue not found)";
            } else if (!chain.pythonCall.has_value()) {
                chain.diagnostic = "missing python call";
            }
            chains.push_back(chain);
            continue;
        }

        for (const auto &launch : launches) {
            auto launchChain = chain;
            launchChain.launch = launch;
            launchChain.hardwareTask = FindFlowTo(launch, index.hostToDeviceFlows);
            if (!launchChain.dequeue.has_value()) {
                launchChain.diagnostic = "missing dequeue for aclopCompileAndExecute";
            } else if (!launchChain.enqueue.has_value()) {
                launchChain.diagnostic = "missing enqueue (async_task_queue not found)";
            } else if (!launchChain.pythonCall.has_value()) {
                launchChain.diagnostic = "missing python call";
            }
            chains.push_back(std::move(launchChain));
        }
    }
    return chains;
}

std::vector<KernelE2EChain> KernelE2EAnalyzer::RecoverAclnnChains(const EventIndex &index) {
    std::vector<KernelE2EChain> chains;
    for (const auto &task : index.hardwareTasks) {
        if (!IsAclnnEvent(task.name)) {
            continue;
        }

        KernelE2EChain chain;
        chain.pathType = "ACLNN";
        chain.hardwareTask = task;

        chain.launch = FindFlowFrom(task, index.hostToDeviceFlows);
        if (chain.launch.has_value()) {
            chain.cannApi = FindEnclosingAclnnCannApi(chain.launch.value(), index.cannApis);
        }
        if (chain.cannApi.has_value()) {
            chain.dequeue = FindEnclosingDequeue(chain.cannApi.value(), index.dequeues);
            if (chain.dequeue.has_value()) {
                chain.enqueue = FindFlowFrom(chain.dequeue.value(), index.asyncTaskQueueFlows);
                if (chain.enqueue.has_value()) {
                    chain.pythonOp = FindEnclosingPythonOp(chain.enqueue.value(), index.pythonOps);
                    chain.pythonCall = chain.pythonOp.has_value()
                        ? FindEnclosingPythonCall(chain.pythonOp.value(), index.pythonCalls)
                        : FindEnclosingPythonCall(chain.enqueue.value(), index.pythonCalls);
                }
            }
        }

        if (!chain.launch.has_value()) {
            chain.diagnostic = "missing launch for aclnn hardware task";
        } else if (!chain.cannApi.has_value()) {
            chain.diagnostic = "missing CANN aclnn API for launch";
        } else if (!chain.dequeue.has_value()) {
            chain.diagnostic = "missing dequeue for aclnn API";
        } else if (!chain.enqueue.has_value()) {
            chain.diagnostic = "missing enqueue (async_task_queue not found)";
        } else if (!chain.pythonCall.has_value()) {
            chain.diagnostic = "missing python call";
        }
        chains.push_back(chain);
    }
    return chains;
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingDequeue(
    const KernelE2EEvent &target, const std::vector<KernelE2EEvent> &dequeues) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    for (const auto &dequeue : dequeues) {
        if (!Contains(dequeue, target)) {
            continue;
        }
        uint64_t duration = dequeue.endNs - dequeue.startNs;
        if (duration < bestDuration) {
            bestDuration = duration;
            best = &dequeue;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindFlowFrom(
    const KernelE2EEvent &to, const std::vector<KernelE2EFlow> &flows) {
    for (const auto &flow : flows) {
        if (flow.to.id == to.id && flow.to.eventType == to.eventType) {
            return flow.from;
        }
    }
    return std::nullopt;
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindFlowTo(
    const KernelE2EEvent &from, const std::vector<KernelE2EFlow> &flows) {
    for (const auto &flow : flows) {
        if (flow.from.id == from.id && flow.from.eventType == from.eventType) {
            return flow.to;
        }
    }
    return std::nullopt;
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingPythonCall(
    const KernelE2EEvent &child, const std::vector<KernelE2EEvent> &pythonCalls) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    for (const auto &pythonCall : pythonCalls) {
        if (!Contains(pythonCall, child)) {
            continue;
        }
        if (!IsSameThreadContext(pythonCall, child)) {
            continue;
        }
        uint64_t duration = pythonCall.endNs - pythonCall.startNs;
        if (duration < bestDuration) {
            bestDuration = duration;
            best = &pythonCall;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingPythonOp(
    const KernelE2EEvent &child, const std::vector<KernelE2EEvent> &pythonOps) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    for (const auto &pythonOp : pythonOps) {
        if (!Contains(pythonOp, child)) {
            continue;
        }
        if (!IsSameThreadContext(pythonOp, child)) {
            continue;
        }
        uint64_t duration = pythonOp.endNs - pythonOp.startNs;
        if (duration < bestDuration) {
            bestDuration = duration;
            best = &pythonOp;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindLaunchInside(
    const KernelE2EEvent &cannApi, const std::vector<KernelE2EEvent> &launches) {
    const auto containedLaunches = FindLaunchesInside(cannApi, launches);
    if (containedLaunches.empty()) {
        return std::nullopt;
    }
    return containedLaunches.back();
}

std::vector<KernelE2EEvent> KernelE2EAnalyzer::FindLaunchesInside(
    const KernelE2EEvent &cannApi, const std::vector<KernelE2EEvent> &launches) {
    std::vector<KernelE2EEvent> containedLaunches;
    for (const auto &launch : launches) {
        if (Contains(cannApi, launch)) {
            containedLaunches.emplace_back(launch);
        }
    }
    std::sort(containedLaunches.begin(), containedLaunches.end(), [](const auto &left, const auto &right) {
        if (left.startNs != right.startNs) {
            return left.startNs < right.startNs;
        }
        if (left.endNs != right.endNs) {
            return left.endNs < right.endNs;
        }
        return left.id < right.id;
    });
    return containedLaunches;
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingAclnnCannApi(
    const KernelE2EEvent &launch, const std::vector<KernelE2EEvent> &cannApis) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    for (const auto &cannApi : cannApis) {
        if (!IsAclnnEvent(cannApi.name) || !Contains(cannApi, launch)) {
            continue;
        }
        const auto duration = cannApi.endNs - cannApi.startNs;
        if (duration < bestDuration) {
            bestDuration = duration;
            best = &cannApi;
        }
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}

} // namespace Dic::Module::Timeline
