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

// 在按 startNs 排序的事件指针列表上，用二分查找 + 剪枝扫描找最小包含区间
std::optional<KernelE2EEvent> FindSmallestEnclosing(
    const KernelE2EEvent &target, const std::vector<const KernelE2EEvent *> &sorted) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    auto upper = std::upper_bound(sorted.begin(), sorted.end(), target.startNs,
        [](uint64_t startNs, const KernelE2EEvent *e) { return startNs < e->startNs; });
    for (auto it = (upper == sorted.begin() ? sorted.end() : upper - 1); it != sorted.end() && it >= sorted.begin();) {
        const auto &candidate = **it;
        if (best != nullptr && target.endNs >= candidate.startNs && target.endNs - candidate.startNs >= bestDuration) {
            break;
        }
        if (Contains(candidate, target)) {
            uint64_t duration = candidate.endNs - candidate.startNs;
            if (duration < bestDuration) {
                bestDuration = duration;
                best = &candidate;
            }
        }
        if (it == sorted.begin()) {
            break;
        }
        --it;
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}

// 带过滤谓词的重载版本，返回 false 的事件跳过
template <typename Filter>
std::optional<KernelE2EEvent> FindSmallestEnclosing(
    const KernelE2EEvent &target, const std::vector<const KernelE2EEvent *> &sorted, Filter &&filter) {
    const KernelE2EEvent *best = nullptr;
    uint64_t bestDuration = UINT64_MAX;
    auto upper = std::upper_bound(sorted.begin(), sorted.end(), target.startNs,
        [](uint64_t startNs, const KernelE2EEvent *e) { return startNs < e->startNs; });
    for (auto it = (upper == sorted.begin() ? sorted.end() : upper - 1); it != sorted.end() && it >= sorted.begin();) {
        const auto &candidate = **it;
        if (best != nullptr && target.endNs >= candidate.startNs && target.endNs - candidate.startNs >= bestDuration) {
            break;
        }
        if (Contains(candidate, target) && filter(candidate)) {
            uint64_t duration = candidate.endNs - candidate.startNs;
            if (duration < bestDuration) {
                bestDuration = duration;
                best = &candidate;
            }
        }
        if (it == sorted.begin()) {
            break;
        }
        --it;
    }
    return best == nullptr ? std::nullopt : std::optional<KernelE2EEvent>(*best);
}
} // namespace

// 优化2：去重键，用于 unordered_set 替代 O(n²) 的 find_if 线性扫描
struct EventDedupKey {
    uint64_t id;
    std::string eventType;
    std::string rankId;
    std::string name;
    uint64_t startNs;
    uint64_t endNs;
    bool operator==(const EventDedupKey &other) const {
        return id == other.id && eventType == other.eventType && rankId == other.rankId && name == other.name &&
            startNs == other.startNs && endNs == other.endNs;
    }
};

// 优化2：Boost hash_combine 惯用写法：每轮将已累积的哈希值做位移混合，加上新字段的哈希值
// 和黄金比例常数 (2^32/phi)，再异或回去。确保每个字段都影响结果的所有位，
// 且字段顺序不同结果也不同（不同于简单相加）。
struct EventDedupKeyHash {
    size_t operator()(const EventDedupKey &k) const {
        auto h = std::hash<uint64_t>{}(k.id);
        h ^= std::hash<std::string>{}(k.eventType) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.rankId) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(k.name) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint64_t>{}(k.startNs) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint64_t>{}(k.endNs) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// 优化1：构建 Flow 哈希索引的 key
std::string MakeFlowKey(const KernelE2EEvent &event) { return event.eventType + ":" + std::to_string(event.id); }

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
    // 优化2：使用 unordered_set 替代 O(n²) 的 find_if 去重
    std::unordered_set<EventDedupKey, EventDedupKeyHash> seen;
    const auto addUniqueEvent = [&seen](std::vector<KernelE2EEvent> &events, const KernelE2EEvent &event) {
        EventDedupKey key{event.id, event.eventType, event.rankId, event.name, event.startNs, event.endNs};
        if (seen.insert(key).second) {
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
    // 优化1：构建 Flow 哈希索引，FindFlowFrom/FindFlowTo 从 O(F) 改为 O(1)
    for (const auto &flow : flows) {
        if (flow.cat == KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE) {
            index.asyncTaskQueueFlows.push_back(flow);
            index.asyncTaskQueueFlowIndex.fromIndex.emplace(MakeFlowKey(flow.to), flow.from);
            index.asyncTaskQueueFlowIndex.toIndex.emplace(MakeFlowKey(flow.from), flow.to);
        } else if (flow.cat == KERNEL_E2E_FLOW_HOST_TO_DEVICE) {
            index.hostToDeviceFlows.push_back(flow);
            index.hostToDeviceFlowIndex.fromIndex.emplace(MakeFlowKey(flow.to), flow.from);
            index.hostToDeviceFlowIndex.toIndex.emplace(MakeFlowKey(flow.from), flow.to);
        } else if (flow.cat == KERNEL_E2E_FLOW_ASYNC_NPU) {
            index.asyncNpuFlows.push_back(flow);
            index.asyncNpuFlowIndex.fromIndex.emplace(MakeFlowKey(flow.to), flow.from);
            index.asyncNpuFlowIndex.toIndex.emplace(MakeFlowKey(flow.from), flow.to);
        }
    }

    // 优化4：构建指针列表和按线程分桶的索引
    // SQL 按 globalTid ASC, startNs ASC 排序，不是全局按 startNs 有序，需要重新排序
    auto buildSortedPtrs = [](const std::vector<KernelE2EEvent> &events) -> std::vector<const KernelE2EEvent *> {
        std::vector<const KernelE2EEvent *> ptrs;
        ptrs.reserve(events.size());
        for (const auto &e : events) {
            ptrs.push_back(&e);
        }
        std::sort(ptrs.begin(), ptrs.end(), [](const auto *a, const auto *b) { return a->startNs < b->startNs; });
        return ptrs;
    };
    index.sortedDequeues = buildSortedPtrs(index.dequeues);
    index.sortedCannApis = buildSortedPtrs(index.cannApis);
    index.sortedLaunches = buildSortedPtrs(index.launches);

    // 去重可能打乱 SQL 原始排序，桶内需要重新排序保证 startNs 有序
    auto buildThreadBuckets = [](const std::vector<KernelE2EEvent> &events)
        -> std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> {
        std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> buckets;
        for (const auto &e : events) {
            const uint64_t globalTid = e.globalTid != 0 ? e.globalTid : e.trackId;
            if (globalTid != 0) {
                buckets[globalTid].push_back(&e);
            }
        }
        for (auto &[globalTid, ptrs] : buckets) {
            std::sort(ptrs.begin(), ptrs.end(), [](const auto *a, const auto *b) { return a->startNs < b->startNs; });
        }
        return buckets;
    };
    index.pythonCallsByGlobalTid = buildThreadBuckets(index.pythonCalls);
    index.pythonOpsByGlobalTid = buildThreadBuckets(index.pythonOps);

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
        chain.dequeue = FindEnclosingDequeue(cannApi, index.sortedDequeues);
        if (chain.dequeue.has_value()) {
            chain.enqueue = FindFlowFrom(chain.dequeue.value(), index.asyncTaskQueueFlowIndex);
            if (chain.enqueue.has_value()) {
                chain.pythonOp = FindEnclosingPythonOp(chain.enqueue.value(), index.pythonOpsByGlobalTid);
                chain.pythonCall = chain.pythonOp.has_value()
                    ? FindEnclosingPythonCall(chain.pythonOp.value(), index.pythonCallsByGlobalTid)
                    : FindEnclosingPythonCall(chain.enqueue.value(), index.pythonCallsByGlobalTid);
            }
        }

        const auto launches = FindLaunchesInside(cannApi, index.sortedLaunches);
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
            launchChain.hardwareTask = FindFlowTo(launch, index.hostToDeviceFlowIndex);
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

        chain.launch = FindFlowFrom(task, index.hostToDeviceFlowIndex);
        if (chain.launch.has_value()) {
            chain.cannApi = FindEnclosingAclnnCannApi(chain.launch.value(), index.sortedCannApis);
        }
        if (chain.cannApi.has_value()) {
            chain.dequeue = FindEnclosingDequeue(chain.cannApi.value(), index.sortedDequeues);
            if (chain.dequeue.has_value()) {
                chain.enqueue = FindFlowFrom(chain.dequeue.value(), index.asyncTaskQueueFlowIndex);
                if (chain.enqueue.has_value()) {
                    chain.pythonOp = FindEnclosingPythonOp(chain.enqueue.value(), index.pythonOpsByGlobalTid);
                    chain.pythonCall = chain.pythonOp.has_value()
                        ? FindEnclosingPythonCall(chain.pythonOp.value(), index.pythonCallsByGlobalTid)
                        : FindEnclosingPythonCall(chain.enqueue.value(), index.pythonCallsByGlobalTid);
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

// 优化4：在按 startNs 排序的指针列表上，用二分查找 + 剪枝扫描找最小包含区间
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingDequeue(
    const KernelE2EEvent &target, const std::vector<const KernelE2EEvent *> &sortedDequeues) {
    return FindSmallestEnclosing(target, sortedDequeues);
}

// 优化1：Flow 哈希查找，O(F) 线性扫描 -> O(1) 哈希查找
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindFlowFrom(const KernelE2EEvent &to, const FlowIndex &flowIndex) {
    const auto it = flowIndex.fromIndex.find(MakeFlowKey(to));
    return it != flowIndex.fromIndex.end() ? std::optional<KernelE2EEvent>(it->second) : std::nullopt;
}

// 优化1：Flow 哈希查找，O(F) 线性扫描 -> O(1) 哈希查找
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindFlowTo(const KernelE2EEvent &from, const FlowIndex &flowIndex) {
    const auto it = flowIndex.toIndex.find(MakeFlowKey(from));
    return it != flowIndex.toIndex.end() ? std::optional<KernelE2EEvent>(it->second) : std::nullopt;
}

// 优化4：先按线程定位桶，再在桶内找最小包含区间
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingPythonCall(const KernelE2EEvent &child,
    const std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> &pythonCallsByGlobalTid) {
    const uint64_t globalTid = child.globalTid != 0 ? child.globalTid : child.trackId;
    if (globalTid == 0) {
        return std::nullopt;
    }
    auto it = pythonCallsByGlobalTid.find(globalTid);
    if (it == pythonCallsByGlobalTid.end()) {
        return std::nullopt;
    }
    return FindSmallestEnclosing(child, it->second);
}

std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingPythonOp(const KernelE2EEvent &child,
    const std::unordered_map<uint64_t, std::vector<const KernelE2EEvent *>> &pythonOpsByGlobalTid) {
    const uint64_t globalTid = child.globalTid != 0 ? child.globalTid : child.trackId;
    if (globalTid == 0) {
        return std::nullopt;
    }
    auto it = pythonOpsByGlobalTid.find(globalTid);
    if (it == pythonOpsByGlobalTid.end()) {
        return std::nullopt;
    }
    return FindSmallestEnclosing(child, it->second);
}

// 优化4：在排序的 launches 列表上，用二分查找定位 cannApi.startNs 附近的候选，
// 再向右扫描收集所有被 cannApi 包含的 launch
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindLaunchInside(
    const KernelE2EEvent &cannApi, const std::vector<const KernelE2EEvent *> &sortedLaunches) {
    const auto containedLaunches = FindLaunchesInside(cannApi, sortedLaunches);
    if (containedLaunches.empty()) {
        return std::nullopt;
    }
    return containedLaunches.back();
}

std::vector<KernelE2EEvent> KernelE2EAnalyzer::FindLaunchesInside(
    const KernelE2EEvent &cannApi, const std::vector<const KernelE2EEvent *> &sortedLaunches) {
    std::vector<KernelE2EEvent> containedLaunches;
    auto lower = std::lower_bound(sortedLaunches.begin(), sortedLaunches.end(), cannApi.startNs,
        [](const KernelE2EEvent *e, uint64_t startNs) { return e->startNs < startNs; });
    for (auto it = lower; it != sortedLaunches.end(); ++it) {
        const auto &launch = **it;
        if (launch.startNs > cannApi.endNs) {
            break;
        }
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

// 优化4：在排序的 cannApis 列表上找最小包含区间（仅匹配 aclnn 事件）
std::optional<KernelE2EEvent> KernelE2EAnalyzer::FindEnclosingAclnnCannApi(
    const KernelE2EEvent &launch, const std::vector<const KernelE2EEvent *> &sortedCannApis) {
    return FindSmallestEnclosing(launch, sortedCannApis, [](const KernelE2EEvent &e) { return IsAclnnEvent(e.name); });
}

} // namespace Dic::Module::Timeline
