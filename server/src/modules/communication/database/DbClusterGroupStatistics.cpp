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
#include "DbClusterDataBase.h"
#include "ClusterStatisticsQuery.h"
#include <map>
#include <set>
#include <tuple>

namespace Dic::Module::FullDb {
namespace {
using namespace Communication;

const std::string GROUP_MAPPING_SQL =
    "WITH mapping AS (SELECT group_name, MIN(group_id) AS group_id, COUNT(*) AS mapping_count "
    "FROM CommunicationGroupMapping GROUP BY group_name) ";
const std::string GROUP_FILTER_SQL = " s LEFT JOIN mapping m ON m.group_name = s.group_name WHERE s.step = ?";

struct GroupKey {
    std::string name;
    std::string id;
    int64_t rank = 0;
    std::string step;
};

using SourceKeys = std::set<std::tuple<std::string, int64_t, std::string>>;
using GroupResult = StatisticsResult<GroupStatistic>;

bool ReadUniqueGroupRow(sqlite3_stmt *row, SourceKeys &keys, GroupKey &group) {
    return ReadStatisticsText(row, 0, group.name) && ReadStatisticsText(row, 1, group.id) &&
        ReadStatisticsRank(row, 2, group.rank) && ReadStatisticsText(row, 3, group.step) &&
        sqlite3_column_int64(row, 4) == 1 && keys.emplace(group.name, group.rank, group.step).second;
}

// Compensated summation preserves small contributions to a large total.
struct MetricSum {
    long double value = 0;
    long double correction = 0;

    void Add(double metric) {
        const long double adjusted = static_cast<long double>(metric) - correction;
        const long double next = value + adjusted;
        correction = (next - value) - adjusted;
        value = next;
    }
};

struct GroupTimes {
    std::string id;
    std::map<int64_t, MetricSum> byRank;
};

struct GroupTraffic {
    std::string id;
    MetricSum size;
    MetricSum time;
};

bool AppendGroupRatio(std::vector<GroupStatistic> &items, const std::string &name, const std::string &id,
    long double numerator, long double denominator) {
    if (!std::isfinite(numerator) || !std::isfinite(denominator)) {
        return false;
    }
    std::optional<double> value;
    if (denominator > 0) {
        value = static_cast<double>(numerator / denominator);
        if (!std::isfinite(*value)) {
            return false;
        }
    }
    items.push_back({name, id, value});
    return true;
}

enum class GroupSortOrder { ASCENDING, DESCENDING };

GroupResult SortGroupResults(std::vector<GroupStatistic> items, GroupSortOrder order) {
    std::sort(items.begin(), items.end(), [order](const GroupStatistic &left, const GroupStatistic &right) {
        if (left.value == right.value) {
            return left.groupName < right.groupName;
        }
        if (!left.value || !right.value) {
            return left.value.has_value();
        }
        return order == GroupSortOrder::ASCENDING ? *left.value < *right.value : *left.value > *right.value;
    });
    return {StatisticsStatus::SUCCESS, std::move(items)};
}

GroupResult ReadGroupRangeRatios(sqlite3_stmt *row) {
    std::map<std::string, GroupTimes> groups;
    SourceKeys keys;
    int status;
    while ((status = sqlite3_step(row)) == SQLITE_ROW) {
        GroupKey key;
        double time = 0;
        if (!ReadUniqueGroupRow(row, keys, key) || !ReadStatisticsMetric(row, 5, time)) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
        auto &group = groups[key.name];
        group.id = std::move(key.id);
        group.byRank[key.rank].Add(time);
    }
    if (status != SQLITE_DONE) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }

    std::vector<GroupStatistic> items;
    for (const auto &[name, group] : groups) {
        const auto [minimum, maximum] = std::minmax_element(group.byRank.begin(), group.byRank.end(),
            [](const auto &left, const auto &right) { return left.second.value < right.second.value; });
        if (!AppendGroupRatio(items, name, group.id, maximum->second.value, minimum->second.value)) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
    }
    return SortGroupResults(std::move(items), GroupSortOrder::DESCENDING);
}

GroupResult ReadGroupBandwidths(sqlite3_stmt *row) {
    std::map<std::string, GroupTraffic> groups;
    SourceKeys keys;
    int status;
    while ((status = sqlite3_step(row)) == SQLITE_ROW) {
        GroupKey key;
        double size = 0;
        double time = 0;
        if (!ReadUniqueGroupRow(row, keys, key) || !ReadStatisticsMetric(row, 5, size) ||
            !ReadStatisticsMetric(row, 6, time)) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
        auto &group = groups[key.name];
        group.id = std::move(key.id);
        group.size.Add(size);
        group.time.Add(time);
    }
    if (status != SQLITE_DONE) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }

    std::vector<GroupStatistic> items;
    for (const auto &[name, group] : groups) {
        if (!AppendGroupRatio(items, name, group.id, group.size.value, group.time.value)) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
    }
    return SortGroupResults(std::move(items), GroupSortOrder::ASCENDING);
}
}

GroupResult DbClusterDataBase::QueryGroupRangeRatio(const std::string &step) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!isOpen || db == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    if (!HasStatisticsColumns(*this, "CommTimeStatistic", {"step", "rank_id", "group_name", "comm_total_time"}) ||
        !HasStatisticsColumns(*this, TABLE_COMM_GROUP, {"group_name", "group_id"})) {
        return {};
    }
    const std::string sql = GROUP_MAPPING_SQL +
        "SELECT s.group_name, m.group_id, s.rank_id, s.step, m.mapping_count, s.comm_total_time "
        "FROM CommTimeStatistic" +
        GROUP_FILTER_SQL + " ORDER BY s.group_name, s.rank_id, s.step";
    auto statement = PrepareStatisticsQuery(*this, sql, {step});
    if (statement == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    return ReadGroupRangeRatios(statement->stmt);
}

GroupResult DbClusterDataBase::QueryGroupBandwidth(const std::string &step, const std::string &transportType) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!isOpen || db == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    if (!HasStatisticsColumns(*this, "TransitStatistic",
            {"step", "rank_id", "group_name", "band_type", "transit_size", "transit_time"}) ||
        !HasStatisticsColumns(*this, TABLE_COMM_GROUP, {"group_name", "group_id"})) {
        return {};
    }
    const std::string sql = GROUP_MAPPING_SQL +
        "SELECT s.group_name, m.group_id, s.rank_id, s.step, m.mapping_count, s.transit_size, s.transit_time "
        "FROM TransitStatistic" +
        GROUP_FILTER_SQL + " AND s.band_type = ? ORDER BY s.group_name, s.rank_id, s.step";
    auto statement = PrepareStatisticsQuery(*this, sql, {step, transportType});
    if (statement == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    return ReadGroupBandwidths(statement->stmt);
}
}
