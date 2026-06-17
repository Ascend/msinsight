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
#include "KernelE2ECalculator.h"

namespace Dic::Module::Timeline {
namespace {
std::string BuildEventRecordId(const KernelE2EEvent &event) {
    const auto rankId = event.rankId.empty() ? "unknown" : event.rankId;
    return rankId + ":" + event.eventType + ":" + std::to_string(event.id);
}

bool IsLaunchHardwareLeaf(const KernelE2EChain &chain) {
    return !chain.isParent && chain.cannApi.has_value() && chain.parentId == BuildEventRecordId(chain.cannApi.value());
}

std::string ResolveRecordId(const KernelE2EChain &chain) {
    if (chain.isParent && chain.cannApi.has_value()) {
        return BuildEventRecordId(chain.cannApi.value());
    }
    if (chain.isParent && chain.pythonCall.has_value()) {
        return BuildEventRecordId(chain.pythonCall.value());
    }
    if (chain.hardwareTask.has_value()) {
        return BuildEventRecordId(chain.hardwareTask.value());
    }
    if (chain.launch.has_value()) {
        return BuildEventRecordId(chain.launch.value());
    }
    if (chain.cannApi.has_value()) {
        return BuildEventRecordId(chain.cannApi.value());
    }
    if (chain.dequeue.has_value()) {
        return BuildEventRecordId(chain.dequeue.value());
    }
    if (chain.pythonCall.has_value()) {
        return BuildEventRecordId(chain.pythonCall.value());
    }
    return chain.pathType;
}

// 顶层行表示 PythonCall 分组，优先显示 Python call 名称，缺失时沿链路向下兜底。
std::string ResolvePythonParentOpName(const KernelE2EChain &chain) {
    if (chain.pythonCall.has_value() && !chain.pythonCall->name.empty()) {
        return chain.pythonCall->name;
    }
    if (chain.pythonOp.has_value() && !chain.pythonOp->name.empty()) {
        return chain.pythonOp->name;
    }
    if (chain.cannApi.has_value() && !chain.cannApi->name.empty()) {
        return chain.cannApi->name;
    }
    if (chain.launch.has_value() && !chain.launch->name.empty()) {
        return chain.launch->name;
    }
    if (chain.hardwareTask.has_value() && !chain.hardwareTask->name.empty()) {
        return chain.hardwareTask->name;
    }
    return "Unknown";
}

// 中间层行把同一个 CANN API 下的多个 launch 分组，但更有业务意义的算子名是 Python op。
std::string ResolveCannParentOpName(const KernelE2EChain &chain) {
    if (chain.pythonOp.has_value() && !chain.pythonOp->name.empty()) {
        return chain.pythonOp->name;
    }
    if (chain.cannApi.has_value() && !chain.cannApi->name.empty()) {
        return chain.cannApi->name;
    }
    if (chain.launch.has_value() && !chain.launch->name.empty()) {
        return chain.launch->name;
    }
    if (chain.hardwareTask.has_value() && !chain.hardwareTask->name.empty()) {
        return chain.hardwareTask->name;
    }
    if (chain.dequeue.has_value() && !chain.dequeue->name.empty()) {
        return chain.dequeue->name;
    }
    if (chain.enqueue.has_value() && !chain.enqueue->name.empty()) {
        return chain.enqueue->name;
    }
    return "Unknown";
}

// PythonCall 下的叶子行表示完整 PythonOp 链路，优先显示 Python op 名称。
std::string ResolvePythonOpLeafName(const KernelE2EChain &chain) {
    if (chain.pythonOp.has_value() && !chain.pythonOp->name.empty()) {
        return chain.pythonOp->name;
    }
    if (chain.cannApi.has_value() && !chain.cannApi->name.empty()) {
        return chain.cannApi->name;
    }
    if (chain.launch.has_value() && !chain.launch->name.empty()) {
        return chain.launch->name;
    }
    if (chain.hardwareTask.has_value() && !chain.hardwareTask->name.empty()) {
        return chain.hardwareTask->name;
    }
    if (chain.dequeue.has_value() && !chain.dequeue->name.empty()) {
        return chain.dequeue->name;
    }
    if (chain.enqueue.has_value() && !chain.enqueue->name.empty()) {
        return chain.enqueue->name;
    }
    return "Unknown";
}

// CANN API 下的叶子行表示 launch/hardware 明细，优先显示 hardware task 名称。
std::string ResolveLaunchHardwareLeafName(const KernelE2EChain &chain) {
    if (chain.hardwareTask.has_value() && !chain.hardwareTask->name.empty()) {
        return chain.hardwareTask->name;
    }
    if (chain.launch.has_value() && !chain.launch->name.empty()) {
        return chain.launch->name;
    }
    if (chain.cannApi.has_value() && !chain.cannApi->name.empty()) {
        return chain.cannApi->name;
    }
    if (chain.pythonOp.has_value() && !chain.pythonOp->name.empty()) {
        return chain.pythonOp->name;
    }
    if (chain.dequeue.has_value() && !chain.dequeue->name.empty()) {
        return chain.dequeue->name;
    }
    if (chain.enqueue.has_value() && !chain.enqueue->name.empty()) {
        return chain.enqueue->name;
    }
    return "Unknown";
}

std::string ResolveOpName(const KernelE2EChain &chain) {
    // 顶层 CANN parent 代表 pythonCall 到 cannApi 的单分支链路，优先显示 PythonCall。
    // 挂在 PythonCall parent 下的 CANN parent 代表其中一个 PythonOp 分支，优先显示 PythonOp。
    if (chain.isParent && chain.cannApi.has_value()) {
        return chain.parentId.empty() ? ResolvePythonParentOpName(chain) : ResolveCannParentOpName(chain);
    }
    // PythonCall 命名规则用于显式 Python 父行，也用于没有生成父树的顶层单行记录。
    // 顶层单行没有 parentId，但它代表完整 op，而不是某个 launch/hardware 叶子明细。
    if (chain.isParent || chain.parentId.empty()) {
        return ResolvePythonParentOpName(chain);
    }
    if (IsLaunchHardwareLeaf(chain)) {
        return ResolveLaunchHardwareLeafName(chain);
    }
    return ResolvePythonOpLeafName(chain);
}
}

KernelE2ETimeRecord KernelE2ECalculator::Calculate(const KernelE2EChain &chain) {
    auto record = CreateBaseRecord(chain);
    if (chain.isParent && chain.cannApi.has_value()) {
        return CalculateCannParentRecord(chain, std::move(record));
    }
    if (chain.isParent) {
        return CalculatePythonParentRecord(chain, std::move(record));
    }
    FillRecoveredEventTimes(chain, record);
    if (IsLaunchHardwareLeaf(chain)) {
        ClearLeafUpstreamTimes(record);
    }
    return FinalizeChildRecord(chain, std::move(record));
}

// 优化3：优先读缓存，未命中时调用 Calculate 并存入 chain.cachedRecord
const KernelE2ETimeRecord &KernelE2ECalculator::GetOrCalculate(const KernelE2EChain &chain) {
    if (!chain.cachedRecord.has_value()) {
        chain.cachedRecord = Calculate(chain);
    }
    return chain.cachedRecord.value();
}

KernelE2ETimeRecord KernelE2ECalculator::CreateBaseRecord(const KernelE2EChain &chain) {
    KernelE2ETimeRecord record;
    record.pathType = chain.pathType;
    record.id = ResolveRecordId(chain);
    record.opName = ResolveOpName(chain);
    record.parentId = chain.parentId;
    record.isParent = chain.isParent;
    return record;
}

KernelE2ETimeRecord KernelE2ECalculator::CalculatePythonParentRecord(
    const KernelE2EChain &chain, KernelE2ETimeRecord record) {
    if (chain.pythonCall.has_value()) {
        record.cCallTs = chain.pythonCall->startNs;
        record.cReturnTs = chain.pythonCall->endNs;
        if (chain.pythonCall->endNs >= chain.pythonCall->startNs) {
            record.pythonApiTime = chain.pythonCall->endNs - chain.pythonCall->startNs;
        }
    }

    std::optional<uint64_t> maxChildEndToEndTime;
    std::vector<KernelE2ETimeRecord> childRecords;
    childRecords.reserve(chain.children.size());
    uint64_t firstEnqueueStart = UINT64_MAX;
    bool hasEnqueueStart = false;
    const auto accumulateLeaf = [&](const KernelE2EChain &leafChain, const auto &accumulateLeafRef) -> void {
        if (leafChain.children.empty()) {
            auto leafRecord = CreateBaseRecord(leafChain);
            FillRecoveredEventTimes(leafChain, leafRecord);
            leafRecord = FinalizeChildRecord(leafChain, std::move(leafRecord));
            AddChildDurations(leafRecord, record);
            if (leafChain.enqueue.has_value()) {
                firstEnqueueStart = std::min(firstEnqueueStart, leafChain.enqueue->startNs);
                hasEnqueueStart = true;
            }
            if (leafRecord.endToEndTime.has_value()) {
                maxChildEndToEndTime = std::max(maxChildEndToEndTime.value_or(0), leafRecord.endToEndTime.value());
            }
            childRecords.emplace_back(std::move(leafRecord));
            return;
        }
        for (const auto &child : leafChain.children) {
            accumulateLeafRef(child, accumulateLeafRef);
        }
    };
    for (const auto &childChain : chain.children) {
        accumulateLeaf(childChain, accumulateLeaf);
    }

    if (chain.pythonCall.has_value() && hasEnqueueStart && firstEnqueueStart >= chain.pythonCall->startNs) {
        record.prepareTime = firstEnqueueStart - chain.pythonCall->startNs;
    }
    if (maxChildEndToEndTime.has_value()) {
        record.endToEndTime = maxChildEndToEndTime.value();
    }
    UpdateParentStatus(childRecords, record);
    return record;
}

KernelE2ETimeRecord KernelE2ECalculator::CalculateCannParentRecord(
    const KernelE2EChain &chain, KernelE2ETimeRecord record) {
    if (chain.pythonCall.has_value()) {
        record.cCallTs = chain.pythonCall->startNs;
        record.cReturnTs = chain.pythonCall->endNs;
    }
    if (chain.pythonCall.has_value() && chain.pythonCall->endNs >= chain.pythonCall->startNs) {
        record.pythonApiTime = chain.pythonCall->endNs - chain.pythonCall->startNs;
    }
    if (chain.enqueue.has_value()) {
        record.enqueueStartTs = chain.enqueue->startNs;
        record.enqueueEndTs = chain.enqueue->endNs;
        if (chain.enqueue->endNs >= chain.enqueue->startNs) {
            record.enqueueTime = chain.enqueue->endNs - chain.enqueue->startNs;
        }
    }
    if (chain.dequeue.has_value()) {
        record.dequeueStartTs = chain.dequeue->startNs;
        record.dequeueEndTs = chain.dequeue->endNs;
    }
    if (chain.pythonCall.has_value() && chain.enqueue.has_value() &&
        chain.enqueue->startNs >= chain.pythonCall->startNs) {
        record.prepareTime = chain.enqueue->startNs - chain.pythonCall->startNs;
    }
    if (chain.enqueue.has_value() && chain.dequeue.has_value() && chain.dequeue->startNs >= chain.enqueue->endNs) {
        record.queueTime = chain.dequeue->startNs - chain.enqueue->endNs;
    }

    std::optional<uint64_t> maxChildEndToEndTime;
    std::vector<KernelE2ETimeRecord> childRecords;
    childRecords.reserve(chain.children.size());
    for (const auto &childChain : chain.children) {
        auto childRecord = Calculate(childChain);
        if (childRecord.pipeline2Time.has_value()) {
            record.pipeline2Time = record.pipeline2Time.value_or(0) + childRecord.pipeline2Time.value();
        }
        if (childRecord.launchTime.has_value()) {
            record.launchTime = record.launchTime.value_or(0) + childRecord.launchTime.value();
        }
        if (childRecord.endToEndTime.has_value()) {
            maxChildEndToEndTime = std::max(maxChildEndToEndTime.value_or(0), childRecord.endToEndTime.value());
        }
        childRecords.emplace_back(std::move(childRecord));
    }
    if (maxChildEndToEndTime.has_value()) {
        record.endToEndTime = maxChildEndToEndTime.value();
    }
    UpdateParentStatus(childRecords, record);
    return record;
}

void KernelE2ECalculator::FillRecoveredEventTimes(const KernelE2EChain &chain, KernelE2ETimeRecord &record) {
    if (chain.pythonCall.has_value()) {
        record.cCallTs = chain.pythonCall->startNs;
        record.cReturnTs = chain.pythonCall->endNs;
        if (chain.pythonCall->endNs >= chain.pythonCall->startNs) {
            record.pythonApiTime = chain.pythonCall->endNs - chain.pythonCall->startNs;
        }
    }
    if (chain.enqueue.has_value()) {
        record.enqueueStartTs = chain.enqueue->startNs;
        record.enqueueEndTs = chain.enqueue->endNs;
        if (chain.enqueue->endNs >= chain.enqueue->startNs) {
            record.enqueueTime = chain.enqueue->endNs - chain.enqueue->startNs;
        }
    }
    if (chain.dequeue.has_value()) {
        record.dequeueStartTs = chain.dequeue->startNs;
        record.dequeueEndTs = chain.dequeue->endNs;
    }
    if (chain.launch.has_value()) {
        record.launchStartTs = chain.launch->startNs;
        record.launchEndTs = chain.launch->endNs;
        if (chain.launch->endNs >= chain.launch->startNs) {
            record.launchTime = chain.launch->endNs - chain.launch->startNs;
        }
    }
    if (chain.pythonCall.has_value() && chain.enqueue.has_value() &&
        chain.enqueue->startNs >= chain.pythonCall->startNs) {
        record.prepareTime = chain.enqueue->startNs - chain.pythonCall->startNs;
    }
    if (chain.enqueue.has_value() && chain.dequeue.has_value() && chain.dequeue->startNs >= chain.enqueue->endNs) {
        record.queueTime = chain.dequeue->startNs - chain.enqueue->endNs;
    }
    if (chain.dequeue.has_value() && chain.launch.has_value() && chain.launch->endNs >= chain.dequeue->startNs) {
        record.pipeline2Time = chain.launch->endNs - chain.dequeue->startNs;
    } else if (chain.dequeue.has_value() && !chain.launch.has_value() &&
        chain.dequeue->endNs >= chain.dequeue->startNs) {
        record.pipeline2Time = chain.dequeue->endNs - chain.dequeue->startNs;
    }
    if (chain.pythonCall.has_value() && chain.launch.has_value() && chain.launch->endNs >= chain.pythonCall->startNs) {
        record.endToEndTime = chain.launch->endNs - chain.pythonCall->startNs;
    } else if (chain.pythonCall.has_value() && chain.dequeue.has_value() && !chain.launch.has_value() &&
        chain.dequeue->endNs >= chain.pythonCall->startNs) {
        record.endToEndTime = chain.dequeue->endNs - chain.pythonCall->startNs;
    }
}

void KernelE2ECalculator::ClearLeafUpstreamTimes(KernelE2ETimeRecord &record) {
    record.prepareTime = std::nullopt;
    record.pythonApiTime = std::nullopt;
    record.enqueueTime = std::nullopt;
    record.queueTime = std::nullopt;
}

KernelE2ETimeRecord KernelE2ECalculator::FinalizeChildRecord(const KernelE2EChain &chain, KernelE2ETimeRecord record) {
    const bool hasQueuePath = chain.pythonCall.has_value() && chain.enqueue.has_value() && chain.dequeue.has_value();
    if (hasQueuePath && chain.launch.has_value()) {
        if (!ValidateMonotonicTimestamps(chain, true)) {
            record.status = "incomplete";
            record.diagnostic = "non-monotonic timestamps";
            return record;
        }
        record.status = "normal";
        return record;
    }
    if (hasQueuePath && !chain.launch.has_value()) {
        if (!ValidateMonotonicTimestamps(chain, false)) {
            record.status = "incomplete";
            record.diagnostic = "non-monotonic timestamps";
            return record;
        }
        record.status = "fallback";
        record.diagnostic = "CANN Launch not found, fallback to DEQUEUE_END";
        return record;
    }
    record.status = "incomplete";
    record.diagnostic = chain.diagnostic.empty() ? "missing python call/enqueue/dequeue" : chain.diagnostic;
    return record;
}

void KernelE2ECalculator::AddChildDurations(const KernelE2ETimeRecord &childRecord, KernelE2ETimeRecord &parentRecord) {
    if (childRecord.enqueueTime.has_value()) {
        parentRecord.enqueueTime = parentRecord.enqueueTime.value_or(0) + childRecord.enqueueTime.value();
    }
    if (childRecord.queueTime.has_value()) {
        parentRecord.queueTime = parentRecord.queueTime.value_or(0) + childRecord.queueTime.value();
    }
    if (childRecord.pipeline2Time.has_value()) {
        parentRecord.pipeline2Time = parentRecord.pipeline2Time.value_or(0) + childRecord.pipeline2Time.value();
    }
    if (childRecord.launchTime.has_value()) {
        parentRecord.launchTime = parentRecord.launchTime.value_or(0) + childRecord.launchTime.value();
    }
}

void KernelE2ECalculator::UpdateParentStatus(
    const std::vector<KernelE2ETimeRecord> &childRecords, KernelE2ETimeRecord &record) {
    record.status = "normal";
    for (const auto &childRecord : childRecords) {
        if (childRecord.status == "incomplete") {
            record.status = "incomplete";
            record.diagnostic = childRecord.diagnostic;
            break;
        }
        if (childRecord.status == "fallback") {
            record.status = "fallback";
            record.diagnostic = childRecord.diagnostic;
        }
    }
}

bool KernelE2ECalculator::ValidateMonotonicTimestamps(const KernelE2EChain &chain, bool hasLaunch) {
    if (!chain.pythonCall.has_value() || !chain.enqueue.has_value() || !chain.dequeue.has_value()) {
        return false;
    }

    const auto &pythonCall = chain.pythonCall.value();
    const auto &enqueue = chain.enqueue.value();
    const auto &dequeue = chain.dequeue.value();

    if (pythonCall.startNs > enqueue.startNs) {
        return false;
    }
    if (enqueue.startNs > enqueue.endNs) {
        return false;
    }
    if (enqueue.endNs > dequeue.startNs) {
        return false;
    }

    if (hasLaunch) {
        return chain.launch.has_value() && dequeue.startNs <= chain.launch->endNs;
    }
    return dequeue.startNs <= dequeue.endNs;
}

} // namespace Dic::Module::Timeline
