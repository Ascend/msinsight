/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#ifndef PROFILER_SERVER_FREEATTRIBUTIONSERVICE_H
#define PROFILER_SERVER_FREEATTRIBUTIONSERVICE_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Dic::Module::Timeline {
struct FreeAttributionParams {
    std::string rankId;
    std::string dbPath;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    uint64_t current = 1;
    uint64_t pageSize = 100;
    std::string orderBy = "totalFreeDuration";
    std::string order = "desc";
    std::string groupId;
    std::string searchName;
    // Omitted by legacy clients: equal bounds still mean all events. Explicit
    // range mode preserves point selections, including the point at time zero.
    std::string rangeMode;
};

struct FreeAttributionTarget {
    std::string id;
    std::string name;
    std::string cardId;
    std::string dbPath;
    std::string pid;
    std::string tid;
    uint64_t depth = 0;
    std::string metaType;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
};

struct FreeAttributionRecord {
    std::string detailId;
    uint64_t freeDuration = 0;
    std::string nextBehaviorType;
    std::string attributionStatus;
    std::string reason;
    std::optional<FreeAttributionTarget> hostApiTarget;
    FreeAttributionTarget freeTarget;
    std::vector<FreeAttributionTarget> candidateApiTargets;
};

struct FreeAttributionGroup {
    std::string groupId;
    std::string groupType;
    std::string apiName;
    uint64_t totalFreeDuration = 0;
    uint64_t freeCount = 0;
};

struct FreeAttributionSummary {
    std::vector<FreeAttributionGroup> groups;
    uint64_t totalGroups = 0;
    uint64_t totalFreeDuration = 0;
    uint64_t totalFreeCount = 0;
};

struct FreeAttributionDetails {
    std::vector<FreeAttributionRecord> records;
    uint64_t totalRecords = 0;
};

struct FreeAttributionEvidence {
    bool targetFound = false;
    bool flowFound = false;
    bool complete = true;
    std::vector<FreeAttributionTarget> apiTargets;
};

// The wire values use Timeline's relative nanosecond time base. Query ranges select
// complete Free events; they never trim the event or limit the evidence chain.
class FreeAttributionService {
  public:
    bool QuerySummary(const FreeAttributionParams &params, FreeAttributionSummary &result, std::string &error);
    bool QueryDetails(const FreeAttributionParams &params, FreeAttributionDetails &result, std::string &error);

    // Kept independent of database access so grouping, conservation and pagination
    // can be verified using deterministic event fixtures.
    static void Summarize(const std::vector<FreeAttributionRecord> &records, const FreeAttributionParams &params,
        FreeAttributionSummary &result);
    static void SelectDetails(const std::vector<FreeAttributionRecord> &records, const FreeAttributionParams &params,
        FreeAttributionDetails &result);
    static FreeAttributionRecord AttributeFree(const FreeAttributionTarget &freeTarget,
        const std::string &nextBehaviorType, const std::vector<FreeAttributionEvidence> &evidence,
        const std::string &initialReason = "");
};
}
#endif
