/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include "FreeAttributionService.h"
#include "StringUtil.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>

namespace Dic::Module::Timeline {
namespace {
std::string TargetKey(const FreeAttributionTarget &target) {
    // Length-prefixed fields prevent collisions when paths or names contain separators.
    std::string key;
    for (const auto &part :
        {target.dbPath, target.cardId, target.metaType, target.id, std::to_string(target.timestamp)}) {
        key += std::to_string(part.size()) + ":" + part;
    }
    return key;
}

std::string GroupId(const FreeAttributionRecord &record) {
    return record.attributionStatus == "matched" && record.hostApiTarget.has_value()
        ? "matched:" + record.hostApiTarget->name
        : "status:" + record.attributionStatus;
}

bool InRange(const FreeAttributionRecord &record, const FreeAttributionParams &params) {
    if (params.rangeMode == "empty") {
        return false;
    }
    if (params.rangeMode == "all" || (params.rangeMode.empty() && params.startTime == params.endTime)) {
        return true;
    }
    const auto &target = record.freeTarget;
    const uint64_t end = target.duration > std::numeric_limits<uint64_t>::max() - target.timestamp
        ? std::numeric_limits<uint64_t>::max()
        : target.timestamp + target.duration;
    return end >= params.startTime && target.timestamp <= params.endTime;
}

bool MatchesApiName(const FreeAttributionRecord &record, const std::string &searchName) {
    if (searchName.empty()) {
        return true;
    }
    if (record.hostApiTarget.has_value()) {
        return StringUtil::ContainsIgnoreCase(record.hostApiTarget->name, searchName);
    }
    return std::any_of(record.candidateApiTargets.begin(), record.candidateApiTargets.end(),
        [&searchName](const auto &api) { return StringUtil::ContainsIgnoreCase(api.name, searchName); });
}

template <typename T> void Paginate(std::vector<T> &rows, const FreeAttributionParams &params) {
    if (params.current == 0 || params.pageSize == 0 ||
        params.current - 1 > std::numeric_limits<uint64_t>::max() / params.pageSize) {
        rows.clear();
        return;
    }
    const uint64_t first = (params.current - 1) * params.pageSize;
    if (first >= rows.size()) {
        rows.clear();
        return;
    }
    const auto begin = rows.begin() + static_cast<std::ptrdiff_t>(first);
    const auto count = std::min<uint64_t>(params.pageSize, rows.size() - first);
    std::vector<T> page(begin, begin + static_cast<std::ptrdiff_t>(count));
    rows = std::move(page);
}

uint64_t DetailNumber(const FreeAttributionRecord &record, const std::string &field) {
    if (field == "freeDuration") {
        return record.freeDuration;
    }
    return record.freeTarget.timestamp;
}

struct ApiSortKey {
    std::vector<std::string> names;
    std::vector<uint64_t> numbers;
};

ApiSortKey MakeApiSortKey(const FreeAttributionRecord &record, const std::string &field) {
    ApiSortKey key;
    const auto append = [&](const FreeAttributionTarget &api) {
        if (field == "apiName") {
            key.names.push_back(api.name);
        } else {
            key.numbers.push_back(field == "apiStartTime" ? api.timestamp : api.duration);
        }
    };
    if (record.hostApiTarget.has_value()) {
        append(*record.hostApiTarget);
    } else {
        for (const auto &api : record.candidateApiTargets) {
            append(api);
        }
    }
    // Compare every displayed candidate without selecting a primary API or
    // changing its display order. Equal values retain candidate multiplicity.
    std::sort(key.names.begin(), key.names.end());
    std::sort(key.numbers.begin(), key.numbers.end());
    return key;
}

template <typename T> int CompareValues(const T &left, const T &right) {
    return left < right ? -1 : (right < left ? 1 : 0);
}
}

FreeAttributionRecord FreeAttributionService::AttributeFree(const FreeAttributionTarget &freeTarget,
    const std::string &nextBehaviorType, const std::vector<FreeAttributionEvidence> &evidence,
    const std::string &initialReason) {
    FreeAttributionRecord record;
    record.freeTarget = freeTarget;
    record.freeDuration = freeTarget.duration;
    record.detailId = TargetKey(freeTarget);
    record.nextBehaviorType = nextBehaviorType;
    record.attributionStatus = "unresolved";
    if (!initialReason.empty()) {
        record.reason = initialReason;
        return record;
    }
    bool complete = !evidence.empty();
    bool hasTarget = false;
    bool hasFlow = false;
    std::map<std::string, FreeAttributionTarget> candidates;
    for (const auto &item : evidence) {
        hasTarget = hasTarget || item.targetFound;
        hasFlow = hasFlow || item.flowFound;
        complete = complete && item.complete && item.targetFound && item.flowFound && !item.apiTargets.empty();
        for (const auto &api : item.apiTargets) {
            candidates.emplace(TargetKey(api), api);
        }
    }
    for (const auto &[key, api] : candidates) {
        record.candidateApiTargets.push_back(api);
    }
    if (candidates.empty()) {
        record.reason = !hasTarget ? "noTargetOperator" : (!hasFlow ? "noFlow" : "noPytorchApi");
    } else if (!complete) {
        record.attributionStatus = "incomplete";
        record.reason = "incompleteFlow";
    } else if (candidates.size() > 1) {
        record.attributionStatus = "ambiguous";
        record.reason = "multipleApis";
    } else {
        record.attributionStatus = "matched";
        record.hostApiTarget = candidates.begin()->second;
        record.candidateApiTargets.clear();
    }
    return record;
}

void FreeAttributionService::Summarize(const std::vector<FreeAttributionRecord> &records,
    const FreeAttributionParams &params, FreeAttributionSummary &result) {
    result = {};
    std::map<std::string, FreeAttributionGroup> groups;
    std::set<std::string> seenFree;
    for (const auto &record : records) {
        if (!InRange(record, params) || !MatchesApiName(record, params.searchName) ||
            !seenFree.insert(TargetKey(record.freeTarget)).second) {
            continue;
        }
        const auto id = GroupId(record);
        auto &group = groups[id];
        group.groupId = id;
        group.groupType = record.attributionStatus;
        group.apiName = record.hostApiTarget.has_value() ? record.hostApiTarget->name : "";
        group.totalFreeDuration += record.freeDuration;
        ++group.freeCount;
        result.totalFreeDuration += record.freeDuration;
        ++result.totalFreeCount;
    }
    for (auto &[id, group] : groups) {
        result.groups.emplace_back(std::move(group));
    }
    result.totalGroups = result.groups.size();
    std::sort(result.groups.begin(), result.groups.end(), [&params](const auto &left, const auto &right) {
        int compare = 0;
        if (params.orderBy == "apiName") {
            compare = left.apiName.compare(right.apiName);
        } else {
            const auto a = params.orderBy == "freeCount" ? left.freeCount : left.totalFreeDuration;
            const auto b = params.orderBy == "freeCount" ? right.freeCount : right.totalFreeDuration;
            compare = a < b ? -1 : (a > b ? 1 : 0);
        }
        if (compare != 0) {
            return params.order == "asc" ? compare < 0 : compare > 0;
        }
        return left.groupId < right.groupId;
    });
    Paginate(result.groups, params);
}

void FreeAttributionService::SelectDetails(const std::vector<FreeAttributionRecord> &records,
    const FreeAttributionParams &params, FreeAttributionDetails &result) {
    result = {};
    std::set<std::string> seenFree;
    for (const auto &record : records) {
        if (InRange(record, params) && MatchesApiName(record, params.searchName) && GroupId(record) == params.groupId &&
            seenFree.insert(TargetKey(record.freeTarget)).second) {
            result.records.push_back(record);
        }
    }
    result.totalRecords = result.records.size();
    const bool sortByApi =
        params.orderBy == "apiName" || params.orderBy == "apiStartTime" || params.orderBy == "apiDuration";
    std::unordered_map<std::string, ApiSortKey> apiSortKeys;
    if (sortByApi) {
        apiSortKeys.reserve(result.records.size());
        for (const auto &record : result.records) {
            apiSortKeys.emplace(record.detailId, MakeApiSortKey(record, params.orderBy));
        }
    }
    std::sort(result.records.begin(), result.records.end(),
        [&params, sortByApi, &apiSortKeys](const auto &left, const auto &right) {
            int compare;
            if (sortByApi) {
                const auto &a = apiSortKeys.at(left.detailId);
                const auto &b = apiSortKeys.at(right.detailId);
                compare =
                    params.orderBy == "apiName" ? CompareValues(a.names, b.names) : CompareValues(a.numbers, b.numbers);
            } else {
                compare = CompareValues(DetailNumber(left, params.orderBy), DetailNumber(right, params.orderBy));
            }
            if (compare != 0) {
                return params.order == "desc" ? compare > 0 : compare < 0;
            }
            return left.detailId < right.detailId;
        });
    Paginate(result.records, params);
}
}
