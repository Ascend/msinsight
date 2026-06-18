/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
#include "TableDefs.h"
#include "TraceTime.h"
#include "DbClusterDataBase.h"
// LCOV_EXCL_BR_START

namespace Dic {
using namespace Server;
namespace Module {
namespace FullDb {
DbClusterDataBase::~DbClusterDataBase() {}

bool DbClusterDataBase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Cluster Database(DB) is not open.");
        return false;
    }
    std::string sql = "CREATE TABLE " + TABLE_CLUSTER_BASE_INFO + " (key VARCHAR(50) PRIMARY KEY, value TEXT); ";
    sql += commonSql;
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool DbClusterDataBase::DropTable() {
    std::vector<std::string> tables = {TABLE_CLUSTER_BASE_INFO};
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return DropSomeTables(tables);
}

std::string DbClusterDataBase::QueryParseClusterStatus() { return parseStatus; }

void DbClusterDataBase::UpdateClusterParseStatus(std::string status) { parseStatus = status; }

bool DbClusterDataBase::QueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo) {
    baseInfo.filePath = GetDbPath();
    baseInfo.dataSize = static_cast<double>(FileUtil::GetFileSize(baseInfo.filePath.c_str())) / MB_SIZE;

    // try to get base info
    std::string tryToGetBaseInfoSql = "SELECT key, value FROM " + TABLE_CLUSTER_BASE_INFO +
        " WHERE key IN ('ranks', 'steps', 'collect_start_time', 'collect_duration');";
    ExecuteQueryBaseInfo(baseInfo, tryToGetBaseInfoSql);
    if (baseInfo.stepNum && baseInfo.rankCount && baseInfo.collectStartTime && baseInfo.collectDuration != 0) {
        return true;
    }

    // query base info from ClusterStepTraceTime and update ClusterBaseInfo
    std::string baseInfoSql = "INSERT OR REPLACE INTO " + TABLE_CLUSTER_BASE_INFO +
        " (key, value) VALUES "
        " ('ranks', (select json_group_array(\"index\") "
        "            from (select DISTINCT \"index\" "
        "                  from " +
        TABLE_STEP_TRACE_TIME +
        "                  where \"index\" !='' AND type = 'rank'))), "
        " ('steps', (select json_group_array(step) "
        "            from (select DISTINCT step "
        "                  from " +
        TABLE_STEP_TRACE_TIME + "                  where \"index\" !='')));";
    std::unique_lock<std::recursive_mutex> lock(mutex);
    sqlite3_stmt *stmtBaseInfo = nullptr;
    int baseInfoResult = sqlite3_prepare_v2(db, baseInfoSql.c_str(), -1, &stmtBaseInfo, nullptr);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query base info from ClusterStepTraceTime. error:", sqlite3_errmsg(db));
        return false;
    }
    auto res = sqlite3_step(stmtBaseInfo);
    if (res != SQLITE_DONE) {
        ServerLog::Error("Failed to update base info for db cluster. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_finalize(stmtBaseInfo);
    return ExecuteQueryBaseInfo(baseInfo, tryToGetBaseInfoSql); // 更新后再次查询
}

std::map<std::string, std::string> DbClusterDataBase::QueryBaseInfoByKeys(const std::vector<std::string> &keys) {
    return ExecuteQueryBaseInfoByKeys(keys, TABLE_CLUSTER_BASE_INFO);
}

bool DbClusterDataBase::InsertDuplicateUpdateBaseInfo(const std::map<std::string, std::string> &baseInfoMap) {
    return ExecuteInsertDuplicateUpdateBaseInfo(baseInfoMap, TABLE_CLUSTER_BASE_INFO);
}

bool DbClusterDataBase::GetGroups(std::vector<GroupInfoDo> &groupList) {
    // 兼容老数据，直接导入db场景的cluster_analysis_output场景下，可能会没有pg_name这个字段
    std::string pgNameSelect = "pg_name";
    if (!HasColumn(TABLE_COMM_GROUP, "pg_name")) {
        pgNameSelect = " '' as pg_name ";
    }
    std::string sql =
        "SELECT rank_set as rank, group_name as group_id_hash," + pgNameSelect + " FROM " + TABLE_COMM_GROUP;
    return ExecuteGetGroups(groupList, sql);
}

bool DbClusterDataBase::QueryMatrixList(
    Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList) {
    std::string sql = "SELECT src_rank as srcRank, dst_rank as dstRank, "
                      "transport_type as transportType, "
                      "ROUND(transit_size, 4) as transitSize, "
                      "ROUND(transit_time, 4) as transitTime, "
                      "ROUND(bandwidth, 4) as bandwidth ,"
                      "hccl_op_name as opName "
                      "FROM " +
        TABLE_COMM_ANALYZER_MATRIX +
        " t"
        " WHERE group_name = ? AND step = ? AND hccl_op_name = ? ";
    param.iterationId = "step" + param.iterationId;
    return ExecuteQueryMatrixList(param, matrixInfoDoList, sql);
}

bool DbClusterDataBase::QueryExtremumTimestamp(uint64_t &min, uint64_t &max) {
    std::string sql = "SELECT MIN(start_timestamp) * 1000 as minTime, MAX(start_timestamp) * 1000 as maxTime "
                      "FROM " +
        TABLE_COMM_ANALYZER_TIME + " WHERE start_timestamp != 0";
    return ExecuteQueryExtremumTimestamp(sql, min, max);
}

bool DbClusterDataBase::UpdateCollectTimeInfo(const Protocol::SummaryBaseInfo &baseInfo) {
    std::string sql = "INSERT OR REPLACE INTO " + TABLE_CLUSTER_BASE_INFO +
        " (key, value) values "
        " ('collect_start_time', ?), ('collect_duration', ?);";
    return ExecuteUpdateCollectTimeInfo(baseInfo, sql);
}

bool DbClusterDataBase::QueryIterationAndCommunicationGroup(
    Protocol::CommunicationKernelParams &params, Protocol::CommunicationKernelBody &responseBody) {
    std::string sql = "select step, m.rank_set from " + TABLE_COMM_ANALYZER_TIME +
        " t"
        " LEFT JOIN " +
        TABLE_COMM_GROUP +
        " m ON m.group_name = t.group_name"
        " where hccl_op_name = ? and rank_id = ?";

    if (ExecuteQueryIterationAndCommunicationGroup(
            sql, params.name, params.rankId, responseBody.step, responseBody.group)) {
        // 形如"step0"处理为"0"
        std::string stepIdPrefix = "step";
        size_t found = responseBody.step.find(stepIdPrefix);
        if (found != std::string::npos) {
            responseBody.step.erase(found, stepIdPrefix.length());
        }
        return true;
    }
    return false;
}

bool DbClusterDataBase::QueryAllOperators(
    Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) {
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    std::string sql = "SELECT t.hccl_op_name as operatorName, "
                      "ROUND((start_timestamp - ?/1000.0) / 1000.0, 3) as startTime, "
                      "ROUND(elapsed_time, 4) as elapseTime, "
                      "ROUND(transit_time, 4) as transitTime, "
                      "ROUND(synchronization_time, 4) as synchronizationTime, "
                      "ROUND(wait_time, 4) as waitTime, "
                      "ROUND(idle_time, 4) as idleTime, "
                      "CASE WHEN synchronization_time = 0 THEN 0 ELSE ROUND(synchronization_time "
                      " / (synchronization_time + transit_time), 4) END AS synchronizationTimeRatio, "
                      "CASE WHEN wait_time = 0 THEN 0 ELSE "
                      "ROUND(wait_time / (wait_time + transit_time), 4) END AS waitTimeRatio, "
                      "bw.sdma_bw as sdmaBw, bw.rdma_bw as rdmaBw "
                      "FROM " +
        TABLE_COMM_ANALYZER_TIME +
        " t "
        "LEFT JOIN ( "
        "    SELECT hccl_op_name, "
        "    MAX(CASE WHEN band_type = 'SDMA' THEN bandwidth ELSE 0 END) AS sdma_bw, "
        "    MAX(CASE WHEN band_type = 'RDMA' THEN bandwidth ELSE 0 END) AS rdma_bw "
        "    FROM " +
        TABLE_COMM_ANALYZER_BANDWIDTH +
        " b"
        "    WHERE b.step = ? AND b.rank_id = ? AND b.group_name = ? AND hccl_op_name != 'Total Op Info'"
        "    GROUP BY hccl_op_name "
        ") bw ON t.hccl_op_name = bw.hccl_op_name "
        "WHERE t.step = ? AND t.rank_id = ? AND t.group_name = ? AND t.hccl_op_name != 'Total Op Info'";
    return ExecuteQueryAllOperators(param, resBody, sql, startTime);
}

bool DbClusterDataBase::QueryOperatorsCount(
    Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) {
    std::string sql = "SELECT count(*), step as iteration_id, group_name as op_suffix FROM " +
        TABLE_COMM_ANALYZER_TIME + " WHERE hccl_op_name != 'Total Op Info' ";
    param.iterationId = "step" + param.iterationId;
    return ExecuteQueryOperatorsCount(param, resBody, sql);
}

bool DbClusterDataBase::QueryBandwidthData(
    Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) {
    std::string sql = "SELECT band_type, ROUND(transit_size, 4) as transit_size,"
                      "ROUND(transit_time, 4) as transit_time,"
                      "ROUND(bandwidth, 4) as bandwidth_size,"
                      "ROUND(large_packet_ratio, 4)  as large_packet_ratio from "
                      "(SELECT*,ROW_NUMBER() OVER (PARTITION BY band_type) AS rn FROM (SELECT * "
                      "FROM " +
        TABLE_COMM_ANALYZER_BANDWIDTH +
        " t"
        " WHERE step = ? AND rank_id = ? AND group_name = ? AND hccl_op_name = ?))t "
        "WHERE rn = 1";
    param.iterationId = "step" + param.iterationId;
    return ExecuteQueryBandwidthData(param, resBody, sql);
}

bool DbClusterDataBase::QueryDistributionData(
    Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) {
    std::string sql = "SELECT package_size, count, total_duration FROM " + TABLE_COMM_ANALYZER_BANDWIDTH +
        " t"
        " WHERE step = ? "
        "AND rank_id = ? "
        "AND group_name = ? "
        "AND hccl_op_name = ? "
        "AND band_type = ? ;";
    param.iterationId = "step" + param.iterationId;
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Server::ServerLog::Error("Failed to prepare a statement to query distributed data. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.rankId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.operatorName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index, param.transportType.c_str(), -1, SQLITE_STATIC);
    std::string distribution = "{";
    int num = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string packageSize = sqlite3_column_string(stmt, col++);
        std::string count = sqlite3_column_string(stmt, col++);
        std::string totalDuration = sqlite3_column_string(stmt, col++);
        std::string value = "\"" + packageSize + "\":[" + count + "," + totalDuration + "],";
        num += 1;
        distribution += value;
    }
    if (num == 0) {
        distribution = "";
    } else {
        distribution.erase(distribution.size() - 1);
        distribution += "}";
    }
    resBody.distributionData = distribution;
    sqlite3_finalize(stmt);
    return true;
}

bool DbClusterDataBase::QueryRanksHandler(std::vector<Protocol::IterationsOrRanksObject> &responseBody) {
    std::string sql = "SELECT json_group_array (ranks) FROM ( "
                      "SELECT DISTINCT \"index\" as ranks FROM " +
        TABLE_STEP_TRACE_TIME + " WHERE ranks != '' AND type = 'rank')";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query ranks statement. error:", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string ranks = sqlite3_column_string(stmt, col++);
        GetStepsOrRanksObject(ranks, responseBody);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DbClusterDataBase::QueryOperatorNames(
    Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) {
    std::vector<std::string> rankList = requestParams.rankList;
    std::string sql;
    if (rankList.empty()) {
        sql = "SELECT DISTINCT hccl_op_name FROM (SELECT hccl_op_name FROM " + TABLE_COMM_ANALYZER_TIME +
            " t"
            " LEFT JOIN " +
            TABLE_COMM_GROUP +
            " m ON m.group_name = t.group_name"
            " WHERE step = ?" +
            " AND m.group_name = ?" + " ORDER BY hccl_op_name)";
    } else {
        std::string ranks = GetRanksSql(rankList);
        sql = "SELECT DISTINCT hccl_op_name FROM (SELECT hccl_op_name FROM " + TABLE_COMM_ANALYZER_TIME +
            " t"
            " LEFT JOIN " +
            TABLE_COMM_GROUP +
            " m ON m.group_name = t.group_name"
            " WHERE step = ?" +
            " AND m.group_name = ?" + " AND rank_id IN " + ranks + " ORDER BY hccl_op_name)";
    }
    requestParams.iterationId = "step" + requestParams.iterationId;
    return ExecuteQueryOperatorNames(requestParams, responseBody, sql);
}

bool DbClusterDataBase::QueryIterations(std::vector<Protocol::IterationsOrRanksObject> &responseBody) {
    std::string sql = "select json_group_array(step) from ("
                      "select DISTINCT step from " +
        TABLE_STEP_TRACE_TIME + " where \"index\" !='')";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query iterations statement. error:", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string steps = sqlite3_column_string(stmt, col++);
        GetStepsOrRanksObject(steps, responseBody);
    }
    if (responseBody.empty()) {
        ServerLog::Warn("Failed to obtain the number of iteration ids. At least one id must be contained. "
                        "Check whether communication data files exist in the directory.");
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DbClusterDataBase::QueryDurationList(
    Protocol::DurationListParams &requestParams, std::vector<DurationDo> &durationDoList) {
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    std::string rankSql;
    std::string rankSqlTime;
    if (!requestParams.rankList.empty()) {
        std::string ranks = GetRanksSql(requestParams.rankList);
        rankSql = " AND rank_id IN " + ranks;
        rankSqlTime = " AND t.rank_id IN " + ranks;
    }

    std::string sql =
        "SELECT t.rank_id as rank_id, "
        "CASE WHEN start_timestamp = 0 THEN 0 ELSE ROUND((start_timestamp - ?/1000.0) / 1000.0, 4) END, "
        "ROUND(elapsed_time, 4) as elapse_time, ROUND(transit_time, 4) as transit_time, "
        "ROUND(synchronization_time, 4) as synchronization_time, ROUND(wait_time, 4) as wait_time, "
        "ROUND(idle_time, 4) as idle_time, "
        "CASE WHEN synchronization_time = 0 THEN 0 ELSE ROUND(synchronization_time "
        " / (synchronization_time + transit_time), 4) END AS synchronization_time_ratio, "
        "CASE WHEN wait_time = 0 THEN 0 ELSE "
        " ROUND(wait_time / (wait_time + transit_time), 4) END AS wait_time_ratio, "
        "bw.sdma_bw as sdma_bw, bw.rdma_bw as rdma_bw, bw.sdma_time as sdma_time, bw.rdma_time as rdma_time "
        "FROM " +
        TABLE_COMM_ANALYZER_TIME +
        " t "
        " LEFT JOIN " +
        TABLE_COMM_GROUP +
        " map ON map.group_name = t.group_name"
        " LEFT JOIN ("
        "    SELECT rank_id, "
        "    MAX(CASE WHEN band_type = 'SDMA' THEN bandwidth ELSE 0 END) AS sdma_bw, "
        "    MAX(CASE WHEN band_type = 'RDMA' THEN bandwidth ELSE 0 END) AS rdma_bw, "
        "    MAX(CASE WHEN band_type = 'SDMA' THEN transit_time ELSE 0 END) AS sdma_time, "
        "    MAX(CASE WHEN band_type = 'RDMA' THEN transit_time ELSE 0 END) AS rdma_time "
        "    FROM " +
        TABLE_COMM_ANALYZER_BANDWIDTH +
        " b"
        "    LEFT JOIN " +
        TABLE_COMM_GROUP +
        " m ON m.group_name = b.group_name"
        "    WHERE step = ? AND m.group_name = ? AND hccl_op_name = ? " +
        rankSql +
        "    GROUP BY rank_id "
        ") bw ON t.rank_id = bw.rank_id "
        " WHERE t.step = ? AND map.group_name = ? AND t.hccl_op_name = ? " +
        rankSqlTime;
    requestParams.iterationId = "step" + requestParams.iterationId;
    return ExecuteQueryDurationList(requestParams, durationDoList, sql, startTime);
}

bool DbClusterDataBase::QueryOperatorList(
    Protocol::DurationListParams &requestParams, std::vector<OperatorTimeDo> &operatorTimeDoList) {
    std::string sql =
        "SELECT rank_id, hccl_op_name as op_name,"
        " CASE WHEN start_timestamp = 0 THEN 0 ELSE (start_timestamp - ?/1000.0)*1000.0 END as start_time, "
        " (elapsed_time * 1000000) as elapse_time From " +
        TABLE_COMM_ANALYZER_TIME +
        " t"
        " LEFT JOIN " +
        TABLE_COMM_GROUP +
        " m ON m.group_name = t.group_name"
        " WHERE step = ? AND m.group_name = ? AND hccl_op_name <> 'Total Op Info'";
    std::vector<std::string> rankList = requestParams.rankList;
    if (!rankList.empty()) {
        std::string ranks = GetRanksSql(rankList);
        sql += " AND rank_id IN " + ranks;
    }
    if (requestParams.operatorName != totalOpInfo) {
        sql += " AND hccl_op_name = ?";
    }
    sql += " ORDER by CAST(rank_id AS UNSIGNED) ASC";
    requestParams.iterationId = "step" + requestParams.iterationId;
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    return ExecuteQueryOperatorList(requestParams, operatorTimeDoList, sql, startTime);
}

bool DbClusterDataBase::QueryMatrixSortOpNames(
    Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) {
    std::string sql = "SELECT DISTINCT t.hccl_op_name  FROM " + TABLE_COMM_ANALYZER_MATRIX +
        " t"
        " WHERE t.step = ? AND t.group_name = ?" +
        " ORDER BY t.hccl_op_name";
    requestParams.iterationId = "step" + requestParams.iterationId;
    return ExecuteQueryMatrixSortOpNames(requestParams, responseBody, sql);
}

void DbClusterDataBase::InsertClusterBaseInfo(ClusterBaseInfo &baseInfo) {
    sqlite3_stmt *stmt;
    std::string sql = "INSERT INTO " + TABLE_CLUSTER_BASE_INFO +
        " (key, value) VALUES ('ranks', NULL), ('steps', NULL), "
        " ('collect_start_time', NULL), ('collect_duration', NULL), "
        " ('algorithm', ?), ('dp_size', ?), ('pp_size', ?), "
        " ('tp_size', ?), ('cp_size', ?), ('ep_size', ?), ('moe_tp_size', ?), ('level', ?); ";
    auto result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare inserting cluster base info statement. error:", sqlite3_errmsg(db));
        return;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    int idx = bindStartIndex;
    std::string stringDpSize = std::to_string(baseInfo.config.dpSize);
    std::string stringPpSize = std::to_string(baseInfo.config.ppSize);
    std::string stringTpSize = std::to_string(baseInfo.config.tpSize);
    std::string stringCpSize = std::to_string(baseInfo.config.cpSize);
    std::string stringEpSize = std::to_string(baseInfo.config.epSize);
    std::string stringMoeTpSize = std::to_string(baseInfo.config.moeTpSize);
    sqlite3_bind_text(
        stmt, idx++, baseInfo.config.algorithm.c_str(), baseInfo.config.algorithm.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringDpSize.c_str(), stringDpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringPpSize.c_str(), stringPpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringTpSize.c_str(), stringTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringCpSize.c_str(), stringCpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringEpSize.c_str(), stringEpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringMoeTpSize.c_str(), stringMoeTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, baseInfo.level.c_str(), baseInfo.level.length(), SQLITE_TRANSIENT);
    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Failed to insert cluster base info. ", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

bool DbClusterDataBase::QueryParallelStrategyConfig(ParallelStrategyConfig &config, std::string &level) {
    std::string sql = "SELECT key, value FROM " + TABLE_CLUSTER_BASE_INFO + " WHERE key IN " +
        "('algorithm', 'dp_size', 'pp_size', 'tp_size', 'cp_size', 'ep_size', 'moe_tp_size', 'level');";
    return ExecuteQueryParallelStrategyConfig(sql, config, level);
}

bool DbClusterDataBase::UpdateParallelStrategyConfig(
    const ParallelStrategyConfig &config, std::string &level, std::string &msg) {
    std::string sql = "UPDATE " + TABLE_CLUSTER_BASE_INFO +
        " SET value = "
        " CASE WHEN key = 'algorithm' THEN ?"
        " WHEN key = 'dp_size' THEN ?"
        " WHEN key = 'pp_size' THEN ?"
        " WHEN key = 'tp_size' THEN ?"
        " WHEN key = 'cp_size' THEN ?"
        " WHEN key = 'ep_size' THEN ?"
        " WHEN key = 'moe_tp_size' THEN ?"
        " WHEN key = 'level' THEN ?"
        " ELSE value END;";
    return ExecuteSetParallelStrategyConfig(sql, config, level);
}

bool DbClusterDataBase::GetParallelConfigFromStepTrace(ParallelStrategyConfig &config, std::string &level) {
    static const std::vector<std::string> validHeads = {"dp_index", "pp_index", "tp_index"};
    for (auto &item : validHeads) {
        if (!HasColumn(TABLE_STEP_TRACE_TIME, item)) {
            ServerLog::Warn(TABLE_STEP_TRACE_TIME, " didn't have parallel strategy config.");
            return false;
        }
    }
    std::string sql = "select dp_index, pp_index, tp_index from " + TABLE_STEP_TRACE_TIME +
        " "
        "where type = 'rank' and step = (select DISTINCT step FROM " +
        TABLE_STEP_TRACE_TIME +
        " limit 1) "
        "order by step asc, CAST(`index` AS INTEGER) asc";
    return ExecuteGetParallelConfigFromStepTrace(sql, config, level);
}

bool DbClusterDataBase::QueryAllPerformanceDataByStep(
    const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) {
    std::string sql;
    if (step.empty() || step == "All") {
        sql = "select \"index\" as rank, round(sum(computing), 3) as compute, "
              "round(sum(communication_not_overlapped), 3) as not_overlap, round(sum(overlapped), 3) as overlap, "
              "round(sum(communication), 3) as communication, round(sum(free), 3) as free, "
              "round(sum(preparing), 3) as preparing, "
              "round(sum(communication_not_overlapped_and_exclude_receive), 3) as exclude_receive "
              "FROM " +
            TABLE_STEP_TRACE_TIME + " Where type = 'rank' Group By \"index\"";
    } else {
        sql = "select \"index\" as rank, computing as compute, communication_not_overlapped as not_overlap, "
              "overlapped as overlap, communication, free, preparing,"
              "communication_not_overlapped_and_exclude_receive as exclude_receive FROM " +
            TABLE_STEP_TRACE_TIME +
            " "
            "Where type = 'rank' and step = ?";
    }

    return ExecuteQueryAllPerformanceDataByStep(sql, step, data);
}

bool DbClusterDataBase::HasClusterBaseInfoTable() const { return hasClusterBaseInfoTable; }

void DbClusterDataBase::SetHasClusterBaseInfoTable() {
    hasClusterBaseInfoTable = CheckTableExist(TABLE_CLUSTER_BASE_INFO);
}

bool DbClusterDataBase::QueryDistributedArgs(ParallelStrategyConfig &config, std::string &level) {
    // 考虑兼容性，既支持从列名为distributed_args的表中查询，也支持列名为key value的表中查询，以后profiling使用的列名为key value
    std::string sql;
    if (HasColumn(TABLE_CLUSTER_BASE_INFO, "distributed_args")) {
        sql = "SELECT distributed_args FROM " + TABLE_CLUSTER_BASE_INFO;
    } else if (HasColumn(TABLE_CLUSTER_BASE_INFO, "key") && HasColumn(TABLE_CLUSTER_BASE_INFO, "value")) {
        sql = "SELECT value FROM " + TABLE_CLUSTER_BASE_INFO + " WHERE key = 'distributed_args'";
    } else {
        Server::ServerLog::Error("Format of table ClusterBaseInfo is not supported.");
        return false;
    }
    return ExecuteQueryDistributedArgs(config, level, sql);
}

bool DbClusterDataBase::ExecuteQueryDistributedArgs(
    Dic::Module::ParallelStrategyConfig &config, std::string &level, std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        Server::ServerLog::Error("Failed to prepare a statement to query distributed args. error:", sqlite3_errmsg(db));
        return false;
    }
    std::string argsString;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        argsString = sqlite3_column_string(stmt, col++);
    }
    sqlite3_finalize(stmt);
    DistributedArgs args;
    Dic::document_t json;
    json.Parse(argsString.c_str());
    if (!json.IsObject()) {
        ServerLog::Error(TABLE_CLUSTER_BASE_INFO, " table distributed_args column is not valid json format.");
        return false;
    }
    for (const auto &item : DISTRIBUTED_ARGS_INT_KEY) {
        if (!json.HasMember(item.c_str()) || !json[item.c_str()].IsInt64()) {
            ServerLog::Error(TABLE_CLUSTER_BASE_INFO, " table distributed_args column lacks ", item,
                " key or "
                "value of this key is not of int type.");
            return false;
        }
    }
    for (const auto &item : DISTRIBUTED_ARGS_BOOL_KEY) {
        if (!json.HasMember(item.c_str()) || !json[item.c_str()].IsBool()) {
            ServerLog::Error(TABLE_CLUSTER_BASE_INFO, " table distributed_args column lacks ", item,
                " key or "
                "value of this key is not of bool type.");
            return false;
        }
    }
    args.config.tpSize = NumberUtil::IntToUint32(json["tensor_model_parallel_size"].GetInt());
    args.config.ppSize = NumberUtil::IntToUint32(json["pipeline_model_parallel_size"].GetInt());
    args.config.dpSize = NumberUtil::IntToUint32(json["data_parallel_size"].GetInt());
    args.config.cpSize = NumberUtil::IntToUint32(json["context_parallel_size"].GetInt());
    args.config.epSize = NumberUtil::IntToUint32(json["expert_model_parallel_size"].GetInt());
    args.worldSize = NumberUtil::IntToUint32(json["world_size"].GetInt());
    args.sequenceParallel = json["sequence_parallel"].GetBool();
    config = args.config;
    level = PARALLEL_CONFIG_LEVEL_COLLECTED;
    return true;
}

std::vector<std::string> DbClusterDataBase::GetAllRankFromStepStatisticInfo() {
    std::string sql = "SELECT DISTINCT \"index\" as rankId From " + TABLE_STEP_TRACE_TIME + " WHERE type = 'rank'";
    return ExecuteGetAllRankFromStepStatisticInfo(sql);
}

/**
 * 按快卡rankId查询通信时间，得到fast表；按慢卡rankId查询通信时间，得到slow表；
 * 通过opName join两表，计算得到差值diffElapseTime、算子名称、开始时间、持续时间等算子信息
 * 时间单位统一返回us
 * DB场景原表中, start_timestamp时间单位为us，Timeline::TraceTime::Instance().GetStartTime();得到时间戳单位为ns（占位符绑定）
 * 因此算子开始时间startTime为CASE WHEN start_timestamp = 0 THEN 0 ELSE ROUND((start_timestamp - ?/1000.0), 3)
 * elapseTime原表中时间单位为ms，查询时折算为ROUND(t.elapse_time * 1000, 3)
 */
bool DbClusterDataBase::QuerySlowOpByCommDuration(const Protocol::DurationListParams &params,
    const std::string &fastestRankId, Protocol::RankDetailsForSlowRank &slowRank) {
    std::string sql =
        " select "
        "    fast.opName, slow.startTime as startTimeOfSlow, slow.elapseTime AS elapseTimeOfSlow, "
        "    (fast.elapseTime - slow.elapseTime) AS diffElapseTime, "
        "    fast.elapseTime AS elapseTimeOfFast, fast.startTime as startTimeOfFast"
        " from ( "
        "    select t.rank_id as rankId, t.hccl_op_name AS opName, "
        "    ROUND(t.elapsed_time * 1000, 3) AS elapseTime, "
        "    CASE WHEN start_timestamp = 0 THEN 0 ELSE ROUND((start_timestamp - ?/1000.0), 3) END as startTime from " +
        TABLE_COMM_ANALYZER_TIME +
        " t "
        "    where t.rank_id = ? and step = 'step' || ? and t.group_name = ? and opName != 'Total Op Info' "
        " ) fast "
        " join ( "
        "    select t.rank_id as rankId, t.hccl_op_name AS opName, "
        "    ROUND(t.elapsed_time * 1000, 3) AS elapseTime, "
        "    CASE WHEN start_timestamp = 0 THEN 0 ELSE ROUND((start_timestamp - ?/1000.0), 3) END as startTime from " +
        TABLE_COMM_ANALYZER_TIME +
        " t "
        "    where t.rank_id = ? and step = 'step' || ? and t.group_name = ? and opName != 'Total Op Info' "
        " ) slow "
        " on fast.opName = slow.opName ORDER BY diffElapseTime DESC;";
    return ExecuteQuerySlowOpByCommDuration(sql, params, fastestRankId, slowRank);
}

std::vector<CommInfoUnderRank> DbClusterDataBase::GetCommTimeForRankDim(const std::string &stepId) {
    std::string sql = "SELECT t.rank_id as rankId, ROUND(sum(t.elapsed_time) * 1000, 3) as commTime, "
                      "g.rank_set as rankSet, g.group_name as groupIdHash, g.pg_name as pgName "
                      "FROM " +
        TABLE_COMM_ANALYZER_TIME + " as t LEFT JOIN (SELECT rank_set, group_name, pg_name FROM " + TABLE_COMM_GROUP +
        " group by group_name) as g ON t.group_name = g.group_name "
        "WHERE hccl_op_name = 'Total Op Info'";
    if (!stepId.empty() && stepId != "All") {
        sql += " AND step = ? ";
    }
    sql += " group by t.rank_id, g.group_name";
    std::string stepAfterDeal = stepId.empty() || stepId == "All" ? stepId : "step" + stepId;
    return ExecuteGetCommTimeForRankDim(sql, stepAfterDeal);
}

bool DbClusterDataBase::QueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data) {
    std::string sql = "SELECT band_type, transit_size, transit_time FROM " + TABLE_COMM_ANALYZER_BANDWIDTH +
        " WHERE (band_type = 'RDMA' OR band_type = 'SDMA') AND transit_size > 0 AND hccl_op_name != 'Total Op Info';";
    return ExecuteQueryPacketAnalyzerData(data, sql);
}

bool DbClusterDataBase::QueryBandwidthContentionAnalyzerData(
    std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId) {
    std::string sql = "SELECT t1.hccl_op_name,"
                      " ROUND(start_timestamp, 3) AS startTime, ROUND(elapsed_time * 1000.0, 3), bandwidth"
                      " FROM " +
        TABLE_COMM_ANALYZER_BANDWIDTH + " AS t1 INNER JOIN " + TABLE_COMM_ANALYZER_TIME +
        " AS t2 ON"
        " t1.step = t2.step AND t1.group_name = t2.group_name AND "
        " t1.rank_id = t2.rank_id AND t1.hccl_op_name = t2.hccl_op_name " +
        " WHERE t1.rank_id = ? AND band_type = 'SDMA' AND transit_size > 0 ORDER BY startTime;";
    return ExecuteQueryBandwidthContentionAnalyzerData(res, rankId, sql);
}

bool DbClusterDataBase::QueryRetransmissionAnalyzerData(std::vector<RetransmissionClassificationInfo> &data) {
    std::string sql = "SELECT t1.step, t3.rank_set, t1.hccl_op_name, MIN(t2.elapsed_time), MAX(t1.transit_time) "
                      " FROM " +
        TABLE_COMM_ANALYZER_BANDWIDTH + " AS t1 INNER JOIN " + TABLE_COMM_ANALYZER_TIME +
        " AS t2 ON "
        " t1.step = t2.step AND t1.group_name = t2.group_name AND "
        " t1.hccl_op_name = t2.hccl_op_name AND t1.rank_id = t2.rank_id INNER JOIN " +
        TABLE_COMM_GROUP +
        " AS t3 ON "
        " t1.group_name = t3.group_name WHERE band_type = 'RDMA' AND t1.hccl_op_name != 'Total Op Info' "
        " GROUP BY t1.step, t3.rank_set, t1.hccl_op_name";
    return ExecuteQueryRetransmissionAnalyzerData(data, sql);
}

std::vector<OpTypeStatistics> DbClusterDataBase::GetOpStatByStepId(const std::string &stepId) {
    return opTypeStatisticsCache_.GetOrFetch(stepId, [&stepId, this]() -> std::vector<OpTypeStatistics> {
        const std::string sql =
            "SELECT hccl_op_name as op_type, "
            "group_name FROM ClusterCommunicationTime WHERE hccl_op_name != 'Total Op Info' AND step = ?";
        return ExecuteGetOpStatByStepId("step" + stepId, sql);
    });
}
}
}
}
// LCOV_EXCL_BR_STOP
