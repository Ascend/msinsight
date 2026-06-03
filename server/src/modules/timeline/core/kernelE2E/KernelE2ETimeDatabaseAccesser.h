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

#ifndef PROFILER_SERVER_KERNELE2ETIMEDATABASEACCESSER_H
#define PROFILER_SERVER_KERNELE2ETIMEDATABASEACCESSER_H

#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include "DataBaseManager.h"
#include "KernelE2ECalculator.h"
#include "KernelE2EDef.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {

class KernelE2ETimeDatabaseAccesser {
  public:
    explicit KernelE2ETimeDatabaseAccesser(
        const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &fileId)
        : database_(database), fileId_(fileId) {}

    bool GetKernelE2ETimeRecords(const Protocol::KernelE2ETimeParams &params, Protocol::KernelE2ETimeBody &body) const;

  private:
    DataType GetDatabaseType() const;
    bool GetKernelE2ETimeRecordsFromDb(const KernelE2EQuery &query, const Protocol::KernelE2ETimeParams &params,
        Protocol::KernelE2ETimeBody &body) const;
    bool GetKernelE2ETimeRecordsFromText(const KernelE2EQuery &query, const Protocol::KernelE2ETimeParams &params,
        Protocol::KernelE2ETimeBody &body) const;
    static bool SafeAddUint64(uint64_t a, uint64_t b, uint64_t &result);
    static void ConvertRecordToDto(const KernelE2ETimeRecord &record, Protocol::KernelE2ETimeRecordDto &dto);
    static bool ConvertChainToDto(const KernelE2EChain &chain, KernelE2ECalculator &calculator, uint64_t minTimestamp,
        Protocol::KernelE2ETimeRecordDto &dto);
    static Protocol::KernelE2EHighlightSliceDto ToHighlightSliceDto(
        const KernelE2EEvent &event, const std::string &role, uint64_t minTimestamp);
    static Protocol::KernelE2EHighlightSliceDto MakeMissingHighlightSlice(
        const std::string &role, const std::string &missingReason);
    // Converts recovered chain nodes into frontend Timeline coordinates and appends missing critical nodes.
    static bool BuildHighlightSlices(const KernelE2EChain &chain, const KernelE2ETimeRecord &record,
        uint64_t minTimestamp, std::vector<Protocol::KernelE2EHighlightSliceDto> &highlightSlices);
    static std::vector<KernelE2EChain> PrepareChainsForResponse(
        const std::vector<KernelE2EChain> &chains, const Protocol::KernelE2ETimeParams &params);
    static std::vector<KernelE2EChain> FilterChainsByPathType(
        const std::vector<KernelE2EChain> &chains, const std::string &pathType);
    static bool ChainMatchesPathType(const KernelE2EChain &chain, const std::string &pathType);
    static std::vector<KernelE2EChain> FilterChainsByOpName(
        const std::vector<KernelE2EChain> &chains, const std::string &opName, KernelE2ECalculator &calculator);
    static bool ChainMatchesOpName(
        const KernelE2EChain &chain, const std::string &opName, KernelE2ECalculator &calculator);
    static void SortChains(std::vector<KernelE2EChain> &chains, const std::string &sortField,
        const std::string &sortOrder, KernelE2ECalculator &calculator);
    static std::optional<uint64_t> GetSortableValue(const KernelE2ETimeRecord &record, const std::string &sortField);
    static std::string NormalizeSortField(const std::string &sortField);
    static bool IsAscendingSort(const std::string &sortOrder);
    static bool ContainsIgnoreCase(const std::string &value, const std::string &pattern);
    static std::vector<KernelE2EChain> GetPagedChains(
        const std::vector<KernelE2EChain> &chains, uint64_t current, uint64_t pageSize);
    static void UpdateStatistics(
        const std::vector<Protocol::KernelE2ETimeRecordDto> &records, Protocol::KernelE2ETimeBody &body);
    static void UpdateStatistics(const std::vector<Protocol::KernelE2ETimeRecordDto> &records, uint64_t totalCount,
        Protocol::KernelE2ETimeBody &body);
    static void UpdateStatistics(
        const std::vector<KernelE2EChain> &chains, KernelE2ECalculator &calculator, Protocol::KernelE2ETimeBody &body);

    struct KernelE2ECacheKey {
        std::string fileId;
        std::string rankId;
        uint64_t startNs = 0;
        uint64_t endNs = 0;

        bool operator==(const KernelE2ECacheKey &other) const;
    };

    struct KernelE2ECacheValue {
        KernelE2ECacheKey key;
        std::shared_ptr<const std::vector<KernelE2EChain>> chains;
        uint64_t totalCount = 0;
        uint64_t normalCount = 0;
        uint64_t fallbackCount = 0;
        uint64_t incompleteCount = 0;
        double launchMatchRate = 0.0;
    };

    static KernelE2ECacheKey BuildCacheKey(const std::string &fileId, const KernelE2EQuery &query);
    static bool TryGetCachedValue(const KernelE2ECacheKey &key, KernelE2ECacheValue &value);
    static void ClearCacheIfKeyChanged(const KernelE2ECacheKey &key);
    static void TryUpdateCache(KernelE2ECacheValue value);
    static bool CanCacheChains(const std::vector<KernelE2EChain> &chains);
    static uint64_t EstimateChainsBytes(const std::vector<KernelE2EChain> &chains);
    static uint64_t EstimateChainBytes(const KernelE2EChain &chain);
    static uint64_t EstimateEventBytes(const std::optional<KernelE2EEvent> &event);
    static void ApplyCachedStats(const KernelE2ECacheValue &cacheValue, Protocol::KernelE2ETimeBody &body);

    static std::mutex cacheMutex_;
    static std::optional<KernelE2ECacheValue> cacheValue_;

    std::shared_ptr<VirtualTraceDatabase> database_;
    std::string fileId_;
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNELE2ETIMEDATABASEACCESSER_H
