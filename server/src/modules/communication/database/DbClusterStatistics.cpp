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
#include "NumDefs.h"
#include <set>

namespace Dic::Module::FullDb {
namespace {
using namespace Communication;

std::unique_ptr<SqlitePreparedStatement> PrepareRankStatistics(
    Database &database, const std::string &step, const std::string &groupName, const RankStatisticsOptions &options) {
    const std::string direction = options.order == "ascend" ? "ASC" : "DESC";
    if (options.orderBy == "rankId") {
        const std::string sql = "SELECT rank_id, band_type, bandwidth FROM TransitStatistic "
                                "WHERE step = ? AND group_name = ? ORDER BY rank_id " +
            direction + ", band_type COLLATE BINARY";
        return PrepareStatisticsQuery(database, sql, {step, groupName});
    }

    const std::string sql =
        "WITH sort_values AS (SELECT rank_id, MAX(CASE WHEN band_type COLLATE BINARY = ?3 THEN bandwidth END) "
        "AS sort_value FROM TransitStatistic WHERE step = ?1 AND group_name = ?2 GROUP BY rank_id) "
        "SELECT t.rank_id, t.band_type, t.bandwidth FROM TransitStatistic t "
        "LEFT JOIN sort_values s ON s.rank_id IS t.rank_id WHERE t.step = ?1 AND t.group_name = ?2 "
        "ORDER BY s.sort_value IS NULL ASC, s.sort_value " +
        direction + ", t.rank_id ASC, t.band_type COLLATE BINARY";
    return PrepareStatisticsQuery(database, sql, {step, groupName, options.bandType});
}

RankStatisticsResult ReadRankPage(sqlite3_stmt *row, const RankStatisticsOptions &options) {
    const uint64_t offset = static_cast<uint64_t>(options.currentPage - 1) * options.pageSize;
    const uint64_t pageEnd = offset + options.pageSize;
    std::vector<RankStatistic> items;
    std::optional<int64_t> previousRank;
    std::string previousType;
    uint64_t total = 0;
    int status;

    // Scan all ranks to compute the total and validate rows outside the requested page.
    while ((status = sqlite3_step(row)) == SQLITE_ROW) {
        int64_t rank = 0;
        TransportBandwidth transport;
        if (!ReadStatisticsRank(row, 0, rank) || !ReadStatisticsText(row, 1, transport.bandType) ||
            !ReadStatisticsMetric(row, 2, transport.bandwidth)) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }

        const bool newRank = previousRank != rank;
        if (!newRank && previousType == transport.bandType) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
        if (newRank) {
            ++total;
        }
        previousRank = rank;
        previousType = transport.bandType;

        if (total > offset && total <= pageEnd) {
            if (newRank) {
                items.push_back({rank, {}});
            }
            items.back().bandwidths.push_back(std::move(transport));
        }
    }
    if (status != SQLITE_DONE) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    return {StatisticsStatus::SUCCESS, std::move(items), total};
}
}

StatisticsResult<OperatorFluctuation> DbClusterDataBase::QueryOperatorFluctuation(
    const std::string &step, const std::string &groupName) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!isOpen || db == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    if (!HasStatisticsColumns(*this, "CommOpStatistic", {"step", "group_name", "op_name", "min_time", "max_time"})) {
        return {};
    }
    auto statement = PrepareStatisticsQuery(*this,
        "SELECT op_name, min_time, max_time FROM CommOpStatistic WHERE step = ? AND group_name = ?", {step, groupName});
    if (statement == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }

    std::vector<OperatorFluctuation> items;
    std::set<std::string> names;
    auto *row = statement->stmt;
    int status;
    while ((status = sqlite3_step(row)) == SQLITE_ROW) {
        std::string name;
        double minimum = 0;
        double maximum = 0;
        if (!ReadStatisticsText(row, 0, name) || !ReadStatisticsMetric(row, 1, minimum) ||
            !ReadStatisticsMetric(row, 2, maximum) || maximum < minimum || !names.insert(name).second) {
            return {StatisticsStatus::INVALID_DATA, {}};
        }
        items.push_back({std::move(name), static_cast<double>(static_cast<long double>(maximum) - minimum)});
    }
    if (status != SQLITE_DONE) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    std::sort(items.begin(), items.end(), [](const auto &left, const auto &right) {
        return left.rangeTime != right.rangeTime ? left.rangeTime > right.rangeTime : left.opName < right.opName;
    });
    return {StatisticsStatus::SUCCESS, std::move(items)};
}

RankStatisticsResult DbClusterDataBase::QueryRankStatistics(
    const std::string &step, const std::string &groupName, const RankStatisticsOptions &options) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!isOpen || db == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    if (!HasStatisticsColumns(*this, "TransitStatistic", {"step", "rank_id", "group_name", "band_type", "bandwidth"})) {
        return {};
    }
    if (options.currentPage <= MIN_CURRENT_PAGE || options.currentPage > MAX_CURRENT_PAGE ||
        options.pageSize <= MIN_PAGESIZE || options.pageSize > MAX_PAGESIZE || !options.IsRankSortValid()) {
        return {StatisticsStatus::INVALID_DATA, {}};
    }
    auto statement = PrepareRankStatistics(*this, step, groupName, options);
    if (statement == nullptr) {
        return {StatisticsStatus::QUERY_FAILED, {}};
    }
    return ReadRankPage(statement->stmt, options);
}
}
