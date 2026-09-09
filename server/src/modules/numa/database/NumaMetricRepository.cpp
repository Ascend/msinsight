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

#include "NumaMetricRepository.h"
#include "ServerLog.h"

namespace Dic::Module::Numa {
namespace {
std::string BuildMetricQuery(bool useTimeRange);
NumaMetricRecord ReadMetricRecord(SqliteResultSet &resultSet);
} // namespace

NumaMetricRepository::NumaMetricRepository(std::shared_ptr<Database> database) : database_(std::move(database)) {}

bool NumaMetricRepository::QueryMetricRecords(
    uint64_t startTime, uint64_t endTime, std::vector<NumaMetricRecord> &records) const {
    if (database_ == nullptr) {
        Server::ServerLog::Error("NumaMetricRepository: database is null");
        return false;
    }

    const bool useTimeRange = startTime < endTime;
    auto statement = database_->CreatPreparedStatement(BuildMetricQuery(useTimeRange));
    if (statement == nullptr) {
        Server::ServerLog::Error("NumaMetricRepository: failed to create prepared statement");
        return false;
    }
    if (useTimeRange) {
        statement->BindParams(startTime, endTime);
    }

    auto resultSet = statement->ExecuteQuery();
    if (resultSet == nullptr) {
        Server::ServerLog::Error("NumaMetricRepository: failed to execute metric query");
        return false;
    }
    while (resultSet->Next()) {
        records.emplace_back(ReadMetricRecord(*resultSet));
    }
    return true;
}

namespace {
std::string BuildMetricQuery(bool useTimeRange) {
    // title0/title1/title2 分别表示从外到内的数据库层级。LEFT JOIN 保留“层级存在但暂无采样”的记录，
    // 这样上层组装算法仍能识别数据库真实拓扑，而不会因为所选时间段没有数据而丢失节点定义。
    std::string query =
        "WITH metric_origin AS (SELECT MIN(ts) AS min_ts FROM NUMA_METRICS) "
        "SELECT levels.rowid AS level_id, "
        "CASE WHEN title2.unique_id IS NOT NULL THEN 2 WHEN title1.unique_id IS NOT NULL THEN 1 ELSE 0 END "
        "AS hierarchy_depth, "
        "title0.name AS socket_name, "
        "COALESCE(title1.name, '') AS numa_name, "
        "COALESCE(title2.name, title1.name, title0.name) AS metric_name, "
        "COALESCE(NULLIF(title2.description, ''), NULLIF(title1.description, ''), title0.description, '') "
        "AS description, "
        "COALESCE(NULLIF(title2.measurement_unit, ''), NULLIF(title1.measurement_unit, ''), "
        "title0.measurement_unit, '') AS measurement_unit, "
        "COUNT(metrics.value) AS sample_count, "
        "CASE WHEN LOWER(TRIM(COALESCE(NULLIF(title2.measurement_unit, ''), "
        "NULLIF(title1.measurement_unit, ''), title0.measurement_unit, ''))) IN ('gb/s', 'gops/s') "
        "THEN SUM(metrics.value) ELSE AVG(metrics.value) END AS metric_value, "
        "MIN(metrics.ts - metric_origin.min_ts) AS min_ts, "
        "MAX(metrics.ts - metric_origin.min_ts) AS max_ts "
        "FROM NUMA_LEVELS_HIERARCHY_NAMES levels "
        "CROSS JOIN metric_origin "
        "JOIN NUMA_TITLES_NAMES title0 ON title0.unique_id = levels.title0_id "
        "LEFT JOIN NUMA_TITLES_NAMES title1 ON title1.unique_id = NULLIF(levels.title1_id, 0) "
        "LEFT JOIN NUMA_TITLES_NAMES title2 ON title2.unique_id = NULLIF(levels.title2_id, 0) "
        "LEFT JOIN NUMA_METRICS metrics ON metrics.levels_id = levels.rowid ";

    if (useTimeRange) {
        // 时间值使用占位符绑定，既保持 SQLite 类型语义，也避免外部参数进入 SQL 文本。
        query += "AND metrics.ts - metric_origin.min_ts >= ? "
                 "AND metrics.ts - metric_origin.min_ts <= ? ";
    }
    // 这两个比例指标不属于当前 NUMA 概览，先过滤层级再聚合，避免无用的 AVG 计算和响应数据。
    query += "WHERE COALESCE(title2.name, title1.name, title0.name) NOT IN "
             "('External Traffic Impact', 'Total External Traffic Impact') ";
    query += "GROUP BY levels.rowid ORDER BY levels.rowid";
    return query;
}

NumaMetricRecord ReadMetricRecord(SqliteResultSet &resultSet) {
    NumaMetricRecord record;
    record.levelId = resultSet.GetInt64("level_id");
    record.hierarchyDepth = static_cast<int>(resultSet.GetInt64("hierarchy_depth"));
    record.socketName = resultSet.GetString("socket_name");
    record.numaName = resultSet.GetString("numa_name");
    record.metricName = resultSet.GetString("metric_name");
    record.description = resultSet.GetString("description");
    record.measurementUnit = resultSet.GetString("measurement_unit");
    record.sampleCount = static_cast<uint64_t>(resultSet.GetInt64("sample_count"));
    record.hasValue = record.sampleCount > 0;
    record.value = resultSet.GetDouble("metric_value");
    record.minTimestamp = static_cast<uint64_t>(resultSet.GetInt64("min_ts"));
    record.maxTimestamp = static_cast<uint64_t>(resultSet.GetInt64("max_ts"));
    return record;
}
} // namespace
} // namespace Dic::Module::Numa
