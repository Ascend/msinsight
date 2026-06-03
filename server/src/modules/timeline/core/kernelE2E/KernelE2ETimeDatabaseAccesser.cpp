/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "pch.h"
#include "DbKernelE2ERepo.h"
#include "KernelE2EAnalyzer.h"
#include "KernelE2ECalculator.h"
#include "KernelE2ETimeDatabaseAccesser.h"
#include "ServerLog.h"
#include "SystemUtil.h"
#include "TextKernelE2ERepo.h"

#include <algorithm>
#include <cctype>

namespace Dic::Module::Timeline {

namespace {
constexpr uint64_t KERNEL_E2E_RESERVED_MEMORY_BYTES = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr uint64_t KERNEL_E2E_CACHE_PARAM_DENOMINATOR = 3;
}

std::mutex KernelE2ETimeDatabaseAccesser::cacheMutex_;
std::optional<KernelE2ETimeDatabaseAccesser::KernelE2ECacheValue> KernelE2ETimeDatabaseAccesser::cacheValue_;

bool KernelE2ETimeDatabaseAccesser::KernelE2ECacheKey::operator==(const KernelE2ECacheKey &other) const {
    return fileId == other.fileId && rankId == other.rankId && startNs == other.startNs && endNs == other.endNs;
}

DataType KernelE2ETimeDatabaseAccesser::GetDatabaseType() const {
    if (fileId_.empty()) {
        return DataType::TEXT;
    }
    return DataBaseManager::Instance().GetDataType(fileId_);
}

bool KernelE2ETimeDatabaseAccesser::SafeAddUint64(uint64_t a, uint64_t b, uint64_t &result) {
    if (a > std::numeric_limits<uint64_t>::max() - b) {
        return false;
    }
    result = a + b;
    return true;
}

bool KernelE2ETimeDatabaseAccesser::GetKernelE2ETimeRecords(
    const Protocol::KernelE2ETimeParams &params, Protocol::KernelE2ETimeBody &body) const {
    if (!database_ || fileId_.empty()) {
        return false;
    }

    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    KernelE2EQuery query;
    query.rankId = params.rankId;
    if (!SafeAddUint64(params.startTime, minTimestamp, query.startNs) ||
        !SafeAddUint64(params.endTime, minTimestamp, query.endNs)) {
        Server::ServerLog::Error("KernelE2E: time conversion overflow.");
        return false;
    }

    const DataType dataType = GetDatabaseType();
    if (dataType == DataType::DB) {
        return GetKernelE2ETimeRecordsFromDb(query, params, body);
    }
    if (dataType == DataType::TEXT) {
        return GetKernelE2ETimeRecordsFromText(query, params, body);
    }
    return false;
}

bool KernelE2ETimeDatabaseAccesser::GetKernelE2ETimeRecordsFromDb(
    const KernelE2EQuery &query, const Protocol::KernelE2ETimeParams &params, Protocol::KernelE2ETimeBody &body) const {
    const auto cacheKey = BuildCacheKey(fileId_, query);
    KernelE2ECacheValue cacheValue;
    if (TryGetCachedValue(cacheKey, cacheValue)) {
        ApplyCachedStats(cacheValue, body);
    } else {
        ClearCacheIfKeyChanged(cacheKey);
        auto repo = std::make_unique<DbKernelE2ERepo>();
        KernelE2EAnalyzer analyzer(std::move(repo));
        const auto chains = std::make_shared<std::vector<KernelE2EChain>>(analyzer.AnalyzeChains(query));
        cacheValue.key = cacheKey;
        cacheValue.chains = chains;
        cacheValue.totalCount = chains->size();
        KernelE2ECalculator statsCalculator;
        for (const auto &chain : *chains) {
            const auto record = statsCalculator.Calculate(chain);
            if (record.status == "normal") {
                ++cacheValue.normalCount;
            } else if (record.status == "fallback") {
                ++cacheValue.fallbackCount;
            } else if (record.status == "incomplete") {
                ++cacheValue.incompleteCount;
            }
        }
        cacheValue.launchMatchRate = cacheValue.totalCount == 0
            ? 0.0
            : static_cast<double>(cacheValue.normalCount) / static_cast<double>(cacheValue.totalCount);
        TryUpdateCache(cacheValue);
    }

    auto responseChains = PrepareChainsForResponse(*cacheValue.chains, params);
    KernelE2ECalculator calculator;
    body.records.clear();
    body.records.reserve(responseChains.size());
    const uint64_t totalCount = responseChains.size();
    const auto pagedChains = GetPagedChains(responseChains, params.current, params.pageSize);
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    for (const auto &chain : pagedChains) {
        Protocol::KernelE2ETimeRecordDto dto;
        if (!ConvertChainToDto(chain, calculator, minTimestamp, dto)) {
            return false;
        }
        body.records.emplace_back(std::move(dto));
    }
    UpdateStatistics(responseChains, calculator, body);
    body.totalCount = totalCount;
    return true;
}

bool KernelE2ETimeDatabaseAccesser::GetKernelE2ETimeRecordsFromText(
    const KernelE2EQuery &query, const Protocol::KernelE2ETimeParams &params, Protocol::KernelE2ETimeBody &body) const {
    const auto cacheKey = BuildCacheKey(fileId_, query);
    KernelE2ECacheValue cacheValue;
    if (TryGetCachedValue(cacheKey, cacheValue)) {
        ApplyCachedStats(cacheValue, body);
    } else {
        ClearCacheIfKeyChanged(cacheKey);
        auto repo = std::make_unique<TextKernelE2ERepo>(database_, fileId_);
        KernelE2EAnalyzer analyzer(std::move(repo));
        const auto chains = std::make_shared<std::vector<KernelE2EChain>>(analyzer.AnalyzeChains(query));
        cacheValue.key = cacheKey;
        cacheValue.chains = chains;
        cacheValue.totalCount = chains->size();
        KernelE2ECalculator statsCalculator;
        for (const auto &chain : *chains) {
            const auto record = statsCalculator.Calculate(chain);
            if (record.status == "normal") {
                ++cacheValue.normalCount;
            } else if (record.status == "fallback") {
                ++cacheValue.fallbackCount;
            } else if (record.status == "incomplete") {
                ++cacheValue.incompleteCount;
            }
        }
        cacheValue.launchMatchRate = cacheValue.totalCount == 0
            ? 0.0
            : static_cast<double>(cacheValue.normalCount) / static_cast<double>(cacheValue.totalCount);
        TryUpdateCache(cacheValue);
    }

    auto responseChains = PrepareChainsForResponse(*cacheValue.chains, params);
    KernelE2ECalculator calculator;
    body.records.clear();
    body.records.reserve(responseChains.size());
    const uint64_t totalCount = responseChains.size();
    const auto pagedChains = GetPagedChains(responseChains, params.current, params.pageSize);
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    for (const auto &chain : pagedChains) {
        Protocol::KernelE2ETimeRecordDto dto;
        if (!ConvertChainToDto(chain, calculator, minTimestamp, dto)) {
            return false;
        }
        body.records.emplace_back(std::move(dto));
    }
    UpdateStatistics(responseChains, calculator, body);
    body.totalCount = totalCount;
    return true;
}

void KernelE2ETimeDatabaseAccesser::ConvertRecordToDto(
    const KernelE2ETimeRecord &record, Protocol::KernelE2ETimeRecordDto &dto) {
    dto.id = record.id;
    dto.opName = record.opName;
    dto.pathType = record.pathType;
    dto.isParent = record.isParent;
    dto.prepareTime = record.prepareTime;
    dto.pythonApiTime = record.pythonApiTime;
    dto.enqueueTime = record.enqueueTime;
    dto.queueTime = record.queueTime;
    dto.pipeline2Time = record.pipeline2Time;
    dto.launchTime = record.launchTime;
    dto.endToEndTime = record.endToEndTime;
    dto.status = record.status;
    dto.diagnostic = record.diagnostic;
}

bool KernelE2ETimeDatabaseAccesser::ConvertChainToDto(const KernelE2EChain &chain, KernelE2ECalculator &calculator,
    uint64_t minTimestamp, Protocol::KernelE2ETimeRecordDto &dto) {
    const auto record = calculator.Calculate(chain);
    ConvertRecordToDto(record, dto);
    if (!BuildHighlightSlices(chain, record, minTimestamp, dto.highlightSlices)) {
        return false;
    }
    dto.children.clear();
    dto.children.reserve(chain.children.size());
    for (const auto &child : chain.children) {
        Protocol::KernelE2ETimeRecordDto childDto;
        if (!ConvertChainToDto(child, calculator, minTimestamp, childDto)) {
            return false;
        }
        dto.children.emplace_back(std::move(childDto));
    }
    return true;
}

Protocol::KernelE2EHighlightSliceDto KernelE2ETimeDatabaseAccesser::ToHighlightSliceDto(
    const KernelE2EEvent &event, const std::string &role, uint64_t minTimestamp) {
    Protocol::KernelE2EHighlightSliceDto dto;
    dto.role = role;
    dto.name = event.name;
    dto.startTime = event.startNs >= minTimestamp ? event.startNs - minTimestamp : 0;
    dto.duration = event.endNs > event.startNs ? event.endNs - event.startNs : 0;
    dto.pid = event.pid;
    dto.tid = event.tid;
    dto.id = event.id == 0 ? "" : std::to_string(event.id);
    return dto;
}

Protocol::KernelE2EHighlightSliceDto KernelE2ETimeDatabaseAccesser::MakeMissingHighlightSlice(
    const std::string &role, const std::string &missingReason) {
    Protocol::KernelE2EHighlightSliceDto dto;
    dto.role = role;
    dto.name = role;
    dto.missingReason = missingReason;
    return dto;
}

bool KernelE2ETimeDatabaseAccesser::BuildHighlightSlices(const KernelE2EChain &chain, const KernelE2ETimeRecord &record,
    uint64_t minTimestamp, std::vector<Protocol::KernelE2EHighlightSliceDto> &highlightSlices) {
    highlightSlices.clear();
    auto addEvent = [&](const std::optional<KernelE2EEvent> &event, const std::string &role) {
        if (event.has_value()) {
            highlightSlices.emplace_back(ToHighlightSliceDto(event.value(), role, minTimestamp));
        }
    };
    auto addMissing = [&](const std::string &role, const std::string &reason) {
        highlightSlices.emplace_back(MakeMissingHighlightSlice(role, reason));
    };

    addEvent(chain.pythonCall, "PYTHON_CALL");
    if (chain.isParent && !chain.cannApi.has_value()) {
        return true;
    }
    addEvent(chain.pythonOp, "PYTHON_OP");
    addEvent(chain.enqueue, "ENQUEUE");
    addEvent(chain.dequeue, "DEQUEUE");
    addEvent(chain.cannApi, "CANN_API");
    if (chain.isParent) {
        return true;
    }
    addEvent(chain.launch, "LAUNCH");
    addEvent(chain.hardwareTask, "HARDWARE_TASK");

    if (!chain.pythonCall.has_value()) {
        addMissing("PYTHON_CALL", "missing parent python call");
    }
    if (!chain.enqueue.has_value()) {
        addMissing("ENQUEUE", "missing enqueue");
    }
    if (!chain.dequeue.has_value()) {
        addMissing("DEQUEUE", "missing dequeue");
    }
    if (!chain.launch.has_value() && record.status == "fallback") {
        addMissing("LAUNCH", "CANN Launch not found, fallback to DEQUEUE_END");
    } else if (!chain.launch.has_value() && record.status == "incomplete") {
        addMissing("LAUNCH", "missing launch");
    }
    return true;
}

KernelE2ETimeDatabaseAccesser::KernelE2ECacheKey KernelE2ETimeDatabaseAccesser::BuildCacheKey(
    const std::string &fileId, const KernelE2EQuery &query) {
    KernelE2ECacheKey key;
    key.fileId = fileId;
    key.rankId = query.rankId;
    key.startNs = query.startNs;
    key.endNs = query.endNs;
    return key;
}

bool KernelE2ETimeDatabaseAccesser::TryGetCachedValue(const KernelE2ECacheKey &key, KernelE2ECacheValue &value) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (!cacheValue_.has_value() || !(cacheValue_->key == key)) {
        return false;
    }
    value = cacheValue_.value();
    Server::ServerLog::Info("KernelE2E: cache hit.");
    return true;
}

void KernelE2ETimeDatabaseAccesser::ClearCacheIfKeyChanged(const KernelE2ECacheKey &key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (cacheValue_.has_value() && !(cacheValue_->key == key)) {
        cacheValue_.reset();
        Server::ServerLog::Info("KernelE2E: cache cleared for new query key.");
    }
}

void KernelE2ETimeDatabaseAccesser::TryUpdateCache(KernelE2ECacheValue value) {
    if (value.chains == nullptr || !CanCacheChains(*value.chains)) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (cacheValue_.has_value() && cacheValue_->key == value.key) {
            cacheValue_.reset();
        }
        Server::ServerLog::Info("KernelE2E: cache skipped because estimated size exceeds memory budget.");
        return;
    }
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cacheValue_ = std::move(value);
    Server::ServerLog::Info("KernelE2E: cache updated.");
}

bool KernelE2ETimeDatabaseAccesser::CanCacheChains(const std::vector<KernelE2EChain> &chains) {
    const uint64_t availableBytes = Dic::SystemUtil::GetAvailablePhysicalMemoryBytes();
    if (availableBytes <= KERNEL_E2E_RESERVED_MEMORY_BYTES) {
        return false;
    }
    const uint64_t estimatedBytes = EstimateChainsBytes(chains);
    Server::ServerLog::Info("KernelE2E: cache bytes: %.", estimatedBytes);
    const uint64_t ratioBudget = availableBytes / KERNEL_E2E_CACHE_PARAM_DENOMINATOR;
    const uint64_t reserveBudget = availableBytes - KERNEL_E2E_RESERVED_MEMORY_BYTES;
    return estimatedBytes <= std::min(ratioBudget, reserveBudget);
}

uint64_t KernelE2ETimeDatabaseAccesser::EstimateChainsBytes(const std::vector<KernelE2EChain> &chains) {
    uint64_t bytes = sizeof(KernelE2EChain) * chains.capacity();
    for (const auto &chain : chains) {
        bytes += EstimateChainBytes(chain);
    }
    return bytes;
}

uint64_t KernelE2ETimeDatabaseAccesser::EstimateChainBytes(const KernelE2EChain &chain) {
    uint64_t bytes = chain.pathType.capacity() + chain.parentId.capacity() + chain.diagnostic.capacity();
    bytes += EstimateEventBytes(chain.pythonCall) + EstimateEventBytes(chain.pythonOp) +
        EstimateEventBytes(chain.enqueue) + EstimateEventBytes(chain.dequeue) + EstimateEventBytes(chain.cannApi) +
        EstimateEventBytes(chain.launch) + EstimateEventBytes(chain.hardwareTask);
    bytes += sizeof(KernelE2EChain) * chain.children.capacity();
    for (const auto &child : chain.children) {
        bytes += EstimateChainBytes(child);
    }
    return bytes;
}

uint64_t KernelE2ETimeDatabaseAccesser::EstimateEventBytes(const std::optional<KernelE2EEvent> &event) {
    if (!event.has_value()) {
        return 0;
    }
    return sizeof(KernelE2EEvent) + event->name.capacity() + event->eventType.capacity() + event->pathType.capacity() +
        event->pid.capacity() + event->tid.capacity() + event->rankId.capacity();
}

void KernelE2ETimeDatabaseAccesser::ApplyCachedStats(
    const KernelE2ECacheValue &cacheValue, Protocol::KernelE2ETimeBody &body) {
    body.totalCount = cacheValue.totalCount;
    body.normalCount = cacheValue.normalCount;
    body.fallbackCount = cacheValue.fallbackCount;
    body.incompleteCount = cacheValue.incompleteCount;
    body.launchMatchRate = cacheValue.launchMatchRate;
}

std::vector<KernelE2EChain> KernelE2ETimeDatabaseAccesser::PrepareChainsForResponse(
    const std::vector<KernelE2EChain> &chains, const Protocol::KernelE2ETimeParams &params) {
    KernelE2ECalculator calculator;
    auto responseChains = FilterChainsByPathType(chains, params.pathType);
    // Path filtering and duration sorting use intrinsic child-chain fields, so they run before tree grouping.
    SortChains(responseChains, params.sortField, params.sortOrder, calculator);
    const auto treeChains = KernelE2EAnalyzer::BuildParentChildChains(responseChains);
    // Operator search uses final display names, which differ between parent rows, single chains, and child rows.
    return FilterChainsByOpName(treeChains, params.opName, calculator);
}

std::vector<KernelE2EChain> KernelE2ETimeDatabaseAccesser::FilterChainsByPathType(
    const std::vector<KernelE2EChain> &chains, const std::string &pathType) {
    if (pathType.empty() || pathType == "all") {
        return chains;
    }
    std::vector<KernelE2EChain> filteredChains;
    for (const auto &chain : chains) {
        if (ChainMatchesPathType(chain, pathType)) {
            filteredChains.emplace_back(chain);
            continue;
        }
        KernelE2EChain filteredChain = chain;
        filteredChain.children.clear();
        for (const auto &child : chain.children) {
            if (ChainMatchesPathType(child, pathType)) {
                filteredChain.children.emplace_back(child);
            }
        }
        if (!filteredChain.children.empty()) {
            filteredChains.emplace_back(std::move(filteredChain));
        }
    }
    return filteredChains;
}

bool KernelE2ETimeDatabaseAccesser::ChainMatchesPathType(const KernelE2EChain &chain, const std::string &pathType) {
    return chain.pathType == pathType;
}

std::vector<KernelE2EChain> KernelE2ETimeDatabaseAccesser::FilterChainsByOpName(
    const std::vector<KernelE2EChain> &chains, const std::string &opName, KernelE2ECalculator &calculator) {
    if (opName.empty()) {
        return chains;
    }
    std::vector<KernelE2EChain> filteredChains;
    for (const auto &chain : chains) {
        if (ChainMatchesOpName(chain, opName, calculator)) {
            filteredChains.emplace_back(chain);
            continue;
        }
        KernelE2EChain filteredChain = chain;
        filteredChain.children.clear();
        for (const auto &child : chain.children) {
            if (ChainMatchesOpName(child, opName, calculator)) {
                filteredChain.children.emplace_back(child);
            }
        }
        if (!filteredChain.children.empty()) {
            filteredChains.emplace_back(std::move(filteredChain));
        }
    }
    return filteredChains;
}

bool KernelE2ETimeDatabaseAccesser::ChainMatchesOpName(
    const KernelE2EChain &chain, const std::string &opName, KernelE2ECalculator &calculator) {
    return ContainsIgnoreCase(calculator.Calculate(chain).opName, opName);
}

void KernelE2ETimeDatabaseAccesser::SortChains(std::vector<KernelE2EChain> &chains, const std::string &sortField,
    const std::string &sortOrder, KernelE2ECalculator &calculator) {
    const auto normalizedField = NormalizeSortField(sortField);
    const bool isAscending = IsAscendingSort(sortOrder);
    std::stable_sort(chains.begin(), chains.end(), [&](const auto &left, const auto &right) {
        const auto leftValue = GetSortableValue(calculator.Calculate(left), normalizedField);
        const auto rightValue = GetSortableValue(calculator.Calculate(right), normalizedField);
        if (!leftValue.has_value() && !rightValue.has_value()) {
            return false;
        }
        if (!leftValue.has_value()) {
            return false;
        }
        if (!rightValue.has_value()) {
            return true;
        }
        if (leftValue.value() == rightValue.value()) {
            return false;
        }
        return isAscending ? leftValue.value() < rightValue.value() : leftValue.value() > rightValue.value();
    });
}

std::optional<uint64_t> KernelE2ETimeDatabaseAccesser::GetSortableValue(
    const KernelE2ETimeRecord &record, const std::string &sortField) {
    if (sortField == "prepareTime") {
        return record.prepareTime;
    }
    if (sortField == "pythonApiTime") {
        return record.pythonApiTime;
    }
    if (sortField == "enqueueTime") {
        return record.enqueueTime;
    }
    if (sortField == "queueTime") {
        return record.queueTime;
    }
    if (sortField == "pipeline2Time") {
        return record.pipeline2Time;
    }
    if (sortField == "launchTime") {
        return record.launchTime;
    }
    return record.endToEndTime;
}

std::string KernelE2ETimeDatabaseAccesser::NormalizeSortField(const std::string &sortField) {
    static const std::vector<std::string> supportedFields = {
        "prepareTime",
        "pythonApiTime",
        "enqueueTime",
        "queueTime",
        "pipeline2Time",
        "launchTime",
        "endToEndTime",
    };
    return std::find(supportedFields.begin(), supportedFields.end(), sortField) == supportedFields.end()
        ? "endToEndTime"
        : sortField;
}

bool KernelE2ETimeDatabaseAccesser::IsAscendingSort(const std::string &sortOrder) { return sortOrder == "asc"; }

bool KernelE2ETimeDatabaseAccesser::ContainsIgnoreCase(const std::string &value, const std::string &pattern) {
    auto lowerValue = value;
    auto lowerPattern = pattern;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowerValue.find(lowerPattern) != std::string::npos;
}

std::vector<KernelE2EChain> KernelE2ETimeDatabaseAccesser::GetPagedChains(
    const std::vector<KernelE2EChain> &chains, uint64_t current, uint64_t pageSize) {
    if (current == 0 || pageSize == 0 || chains.empty()) {
        return {};
    }
    if (current - 1 > static_cast<uint64_t>(chains.size()) / pageSize) {
        return {};
    }
    const uint64_t startIndex = (current - 1) * pageSize;
    if (startIndex >= chains.size()) {
        return {};
    }
    const uint64_t endIndex = std::min(startIndex + pageSize, static_cast<uint64_t>(chains.size()));
    return {chains.begin() + startIndex, chains.begin() + endIndex};
}

void KernelE2ETimeDatabaseAccesser::UpdateStatistics(
    const std::vector<Protocol::KernelE2ETimeRecordDto> &records, Protocol::KernelE2ETimeBody &body) {
    UpdateStatistics(records, records.size(), body);
}

void KernelE2ETimeDatabaseAccesser::UpdateStatistics(const std::vector<Protocol::KernelE2ETimeRecordDto> &records,
    uint64_t totalCount, Protocol::KernelE2ETimeBody &body) {
    body.totalCount = totalCount;
    body.normalCount = 0;
    body.fallbackCount = 0;
    body.incompleteCount = 0;
    for (const auto &record : records) {
        if (record.status == "normal") {
            ++body.normalCount;
        } else if (record.status == "fallback") {
            ++body.fallbackCount;
        } else if (record.status == "incomplete") {
            ++body.incompleteCount;
        }
    }
    body.launchMatchRate =
        body.totalCount == 0 ? 0.0 : static_cast<double>(body.normalCount) / static_cast<double>(body.totalCount);
}

void KernelE2ETimeDatabaseAccesser::UpdateStatistics(
    const std::vector<KernelE2EChain> &chains, KernelE2ECalculator &calculator, Protocol::KernelE2ETimeBody &body) {
    body.totalCount = chains.size();
    body.normalCount = 0;
    body.fallbackCount = 0;
    body.incompleteCount = 0;
    for (const auto &chain : chains) {
        const auto record = calculator.Calculate(chain);
        if (record.status == "normal") {
            ++body.normalCount;
        } else if (record.status == "fallback") {
            ++body.fallbackCount;
        } else if (record.status == "incomplete") {
            ++body.incompleteCount;
        }
    }
    body.launchMatchRate =
        body.totalCount == 0 ? 0.0 : static_cast<double>(body.normalCount) / static_cast<double>(body.totalCount);
}

} // namespace Dic::Module::Timeline
