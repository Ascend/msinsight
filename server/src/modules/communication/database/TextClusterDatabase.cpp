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
#include "SummaryProtocolResponse.h"
#include "TimelineProtocolResponse.h"
#include "TableDefs.h"
#include "NumDefs.h"
#include "TraceTime.h"
#include "CollectionUtil.h"
#include "TextClusterDatabase.h"
// LCOV_EXCL_BR_START
namespace Dic {
namespace Module {
using namespace Server;
using namespace rapidjson;
TextClusterDatabase::~TextClusterDatabase() noexcept {
    SaveLastDataSafe();
    if (isInitStmt) {
        ReleaseStmt();
    }
}

bool TextClusterDatabase::SetConfig() { return Database::SetConfig(); }

bool TextClusterDatabase::SetDbVersion() {
    if (!isOpen) {
        ServerLog::Error("Failed to set db version. Database is not open.");
        return false;
    }
    std::string dbVersion = GetCompileDataBaseVersion();
    return ExecSql(" PRAGMA user_version = " + dbVersion + ";");
}

bool TextClusterDatabase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Cluster Database is not open.");
        return false;
    }
    std::string sql = "CREATE TABLE " + TABLE_TIME_INFO +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, iteration_id VARCHAR(50),"
        " stage_id VARCHAR(200), rank_id VARCHAR(50), op_name VARCHAR(100),"
        " op_suffix VARCHAR(100), start_time integer, elapse_time double, synchronization_time_ratio double, "
        "synchronization_time double, transit_time double, wait_time_ratio double, "
        "wait_time double, idle_time double);"
        "CREATE TABLE " +
        TABLE_BANDWIDTH +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, iteration_id VARCHAR(50), "
        "stage_id VARCHAR(200), rank_id VARCHAR(50), op_name VARCHAR(100), "
        "op_suffix VARCHAR(100), transport_type VARCHAR(20), bandwidth_size double,"
        " bandwidth_utilization double, large_package_ratio double, size_distribution json,"
        " transit_size double, transit_time double);" +
        "CREATE TABLE " + TABLE_BASE_INFO +
        " (key VARCHAR(50) PRIMARY KEY, value TEXT); "
        "CREATE TABLE " +
        TABLE_STEP_TRACE +
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, rank_id VARCHAR(50), step_id VARCHAR(50),"
        " stage_id VARCHAR(50), compute_time double, pure_communication_time double, "
        "overlap_communication_time double, communication_time double, free_time double, "
        "stage_time double, bubble_time double, pure_communication_exclude_receive_time double, preparing double, "
        "dp_index INTEGER, pp_index INTEGER, tp_index INTEGER); "
        "CREATE TABLE " +
        TABLE_COMMUNICATION_MATRIX +
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, group_id VARCHAR(100), iteration_id VARCHAR(50), "
        "op_name VARCHAR(100),op_sort VARCHAR(100), group_name VARCHAR(100), src_rank VARCHAR(50), "
        "dst_rank VARCHAR(50), "
        "transport_type VARCHAR(50), transit_size double, transit_time double, bandwidth double);" +
        "CREATE TABLE " + TABLE_GROUP_ID +
        "(id INTEGER PRIMARY KEY AUTOINCREMENT, rank_set VARCHAR(100), type VARCHAR(50), "
        "group_id_hash VARCHAR(100), group_id VARCHAR(100), pg_name VARCHAR(50));";
    sql += commonSql;
    return ExecSql(sql);
}

bool TextClusterDatabase::CreateIndex() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Cluster Database is not open.");
        return false;
    }
    std::string sql = "CREATE INDEX IF NOT EXISTS idx3 on communication_matrix(group_id, op_sort);";
    return ExecSql(sql);
}

bool TextClusterDatabase::CreateTimeIndex() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Cluster Database is not open.");
        return false;
    }
    std::string sql = "CREATE INDEX IF NOT EXISTS idx1 on communication_time_info(stage_id);"
                      "CREATE INDEX IF NOT EXISTS idx2 on communication_bandwidth_info(op_name);";
    return ExecSql(sql);
}

bool TextClusterDatabase::InitStmt() {
    if (isInitStmt) {
        return true;
    }
    std::string timeInfoSql = GetTimeInfoStmtSql(TABLE_CACHE_SIZE);
    if (sqlite3_prepare_v2(db, timeInfoSql.c_str(), -1, &insertTimeInfoStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare getting TimeInfoStmtSql statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::string bandSql = GetBandwidthStmtSql(TABLE_CACHE_SIZE);
    if (sqlite3_prepare_v2(db, bandSql.c_str(), -1, &insertBandwidthStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare inserting bandwidth statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::string matrixSql = GetMatrixStmtSql(TABLE_CACHE_SIZE);
    if (sqlite3_prepare_v2(db, matrixSql.c_str(), -1, &matrixStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare getting MatrixStmtSql statement. error:", sqlite3_errmsg(db));
        return false;
    }
    isInitStmt = true;
    return true;
}

std::string TextClusterDatabase::GetTimeInfoStmtSql(int len) {
    std::string sql = "INSERT INTO " + TABLE_TIME_INFO +
        " (iteration_id, stage_id, rank_id, op_name, op_suffix, start_time, elapse_time,"
        " synchronization_time_ratio,"
        "synchronization_time, transit_time, wait_time_ratio, wait_time, idle_time )"
        " VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)";
    for (int i = 0; i < len - 1; i++) {
        sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    }
    return sql;
}

std::string TextClusterDatabase::GetBandwidthStmtSql(int len) {
    std::string sql = "INSERT INTO " + TABLE_BANDWIDTH +
        " (iteration_id, stage_id, rank_id, op_name, op_suffix, transport_type,"
        " bandwidth_size, bandwidth_utilization,large_package_ratio, size_distribution,"
        " transit_size, transit_time) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)";
    for (int i = 0; i < len - 1; i++) {
        sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?)");
    }
    return sql;
}

void TextClusterDatabase::ReleaseStmt() {
    if (insertTimeInfoStmt != nullptr) {
        sqlite3_finalize(insertTimeInfoStmt);
    }
    if (insertBandwidthStmt != nullptr) {
        sqlite3_finalize(insertBandwidthStmt);
    }
    if (matrixStmt != nullptr) {
        sqlite3_finalize(matrixStmt);
    }
    if (stepStmt != nullptr) {
        sqlite3_finalize(stepStmt);
    }
}

void TextClusterDatabase::SaveLastData() {
    if (!timeInfoCache.empty()) {
        InsertTimeInfoList(timeInfoCache);
        timeInfoCache.clear();
    }
    if (!bandwidthCache.empty()) {
        InsertBandwidthList(bandwidthCache);
        bandwidthCache.clear();
    }
    if (!matrixCache.empty()) {
        InsertCommunicationMatrixInfo(matrixCache);
        matrixCache.clear();
    }
}

void TextClusterDatabase::SaveLastDataSafe() {
    try {
        SaveLastData();
    } catch (const std::exception &ex) {
        // 忽略所有异常，因为即使保存失败，也不会影响后续操作
    }
}

void TextClusterDatabase::InsertTimeInfo(CommunicationTimeInfo &timeInfo) {
    timeInfoCache.emplace_back(timeInfo);
    if (timeInfoCache.size() == TABLE_CACHE_SIZE) {
        InsertTimeInfoList(timeInfoCache);
        timeInfoCache.clear();
    }
}

void TextClusterDatabase::InsertTimeInfoList(std::vector<CommunicationTimeInfo> &timeInfoList) {
    if (timeInfoList.empty()) {
        return;
    }
    sqlite3_stmt *stmt = nullptr;
    if (timeInfoList.size() == TABLE_CACHE_SIZE && isInitStmt) {
        sqlite3_reset(insertTimeInfoStmt);
        stmt = insertTimeInfoStmt;
    } else {
        std::string sql = GetTimeInfoStmtSql(timeInfoList.size());
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare Inserting TimeInfoList statement. error:", sqlite3_errmsg(db));
            return;
        }
    }
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get timeInfo insert stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &timeInfo : timeInfoList) {
        sqlite3_bind_text(stmt, idx++, timeInfo.iterationId.c_str(), timeInfo.iterationId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, timeInfo.stageId.c_str(), timeInfo.stageId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, timeInfo.rankId.c_str(), timeInfo.rankId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, timeInfo.opName.c_str(), timeInfo.opName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, timeInfo.opSuffix.c_str(), timeInfo.opSuffix.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, idx++, NumberUtil::CeilingClamp(timeInfo.startTime, static_cast<uint64_t>(INT64_MAX)));
        sqlite3_bind_double(stmt, idx++, timeInfo.elapseTime);
        sqlite3_bind_double(stmt, idx++, timeInfo.synchronizationTimeRatio);
        sqlite3_bind_double(stmt, idx++, timeInfo.synchronizationTime);
        sqlite3_bind_double(stmt, idx++, timeInfo.transitTime);
        sqlite3_bind_double(stmt, idx++, timeInfo.waitTimeRatio);
        sqlite3_bind_double(stmt, idx++, timeInfo.waitTime);
        sqlite3_bind_double(stmt, idx++, timeInfo.idleTime);
    }

    auto result = sqlite3_step(stmt);
    if (!(timeInfoList.size() == TABLE_CACHE_SIZE && isInitStmt)) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert timeInfo data fail. ", sqlite3_errmsg(db));
    }
}

void TextClusterDatabase::InsertBandwidth(CommunicationBandWidth &bandWidth) {
    bandwidthCache.emplace_back(bandWidth);
    if (bandwidthCache.size() == TABLE_CACHE_SIZE) {
        InsertBandwidthList(bandwidthCache);
        bandwidthCache.clear();
    }
}

bool TextClusterDatabase::BatchInsertDuplicateUpdateGroupInfo(const std::vector<CommGroupParallelInfo> &groupInfoList) {
    if (groupInfoList.empty()) {
        ServerLog::Error("Failed to prepare batch insert duplicate update group info statement, invalid input.");
        return false;
    }
    std::string sql = "INSERT OR REPLACE INTO " + TABLE_GROUP_ID +
        "(id, rank_set, type, group_id_hash, group_id, "
        "pg_name) VALUES(?, ?, ?, ?, ?, ?)";
    for (size_t i = 1; i < groupInfoList.size(); ++i) {
        sql += ",(?, ?, ?, ?, ?, ?)";
    }
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare batch insert duplicate update group info statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int idx = bindStartIndex;
    for (const auto &info : groupInfoList) {
        sqlite3_bind_int64(stmt, idx++, info.id);
        sqlite3_bind_text(stmt, idx++, info.rankSetStr.c_str(), info.rankSetStr.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.type.c_str(), info.type.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.groupIdHash.c_str(), info.groupIdHash.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.groupId.c_str(), info.groupId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.pgName.c_str(), info.pgName.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Batch insert duplicate update group info data fail. ", sqlite3_errmsg(db));
        return false;
    }
    return true;
}

bool TextClusterDatabase::InsertGroupInfoReturnIndex(const CommGroupParallelInfo &groupInfo, uint64_t &index) {
    std::string sql = "INSERT INTO " + TABLE_GROUP_ID +
        " (rank_set, type, group_id_hash, group_id, pg_name) VALUES (?, ?, ?, ?, ?) RETURNING id;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare inserting group info statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int idx = bindStartIndex;
    sqlite3_bind_text(stmt, idx++, groupInfo.rankSetStr.c_str(), groupInfo.rankSetStr.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, groupInfo.type.c_str(), groupInfo.type.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, groupInfo.groupIdHash.c_str(), groupInfo.groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, groupInfo.groupId.c_str(), groupInfo.groupId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, groupInfo.pgName.c_str(), groupInfo.pgName.length(), SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int coll = resultStartIndex;
        int64_t id = sqlite3_column_int64(stmt, coll++);
        if (id < 0) {
            ServerLog::Error("Failed to get index after insert group info.");
            sqlite3_finalize(stmt);
            return false;
        }
        index = static_cast<uint64_t>(id);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool TextClusterDatabase::InsertGroupInfos(const std::vector<CommGroupParallelInfo> &groupInfos) {
    if (groupInfos.empty()) {
        ServerLog::Error("Group info is empty");
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    std::string sql =
        "INSERT INTO " + TABLE_GROUP_ID + " (rank_set, type, group_id_hash, group_id, pg_name) VALUES (?, ?, ?, ?, ?)";
    for (size_t i = 0; i < groupInfos.size() - 1; ++i) {
        sql.append(",(?, ?, ?, ?, ?)");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare inserting group infos statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int idx = bindStartIndex;
    for (const auto &info : groupInfos) {
        sqlite3_bind_text(stmt, idx++, info.rankSetStr.c_str(), info.rankSetStr.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.type.c_str(), info.type.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.groupIdHash.c_str(), info.groupIdHash.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.groupId.c_str(), info.groupId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.pgName.c_str(), info.pgName.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert GroupId data fail. ", sqlite3_errmsg(db));
        return false;
    }
    return true;
}

void TextClusterDatabase::InsertBandwidthList(std::vector<CommunicationBandWidth> &bandWidthList) {
    if (bandWidthList.empty()) {
        return;
    }
    sqlite3_stmt *stmt = nullptr;
    if (bandWidthList.size() == TABLE_CACHE_SIZE && isInitStmt) {
        sqlite3_reset(insertBandwidthStmt);
        stmt = insertBandwidthStmt;
    } else {
        std::string sql = GetBandwidthStmtSql(bandWidthList.size());
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare inserting bandwidth statement. error:", sqlite3_errmsg(db));
            return;
        }
    }
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get timeInfo stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &bandwidth : bandWidthList) {
        sqlite3_bind_text(stmt, idx++, bandwidth.iterationId.c_str(), bandwidth.iterationId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, bandwidth.stageId.c_str(), bandwidth.stageId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, bandwidth.rankId.c_str(), bandwidth.rankId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, bandwidth.opName.c_str(), bandwidth.opName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, bandwidth.opSuffix.c_str(), bandwidth.opSuffix.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(
            stmt, idx++, bandwidth.transportType.c_str(), bandwidth.transportType.length(), SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, idx++, bandwidth.bandwidthSize);
        sqlite3_bind_double(stmt, idx++, bandwidth.bandwidthUtilization);
        sqlite3_bind_double(stmt, idx++, bandwidth.largePackageRatio);
        sqlite3_bind_text(
            stmt, idx++, bandwidth.sizeDistribution.c_str(), bandwidth.sizeDistribution.length(), SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, idx++, bandwidth.transitSize);
        sqlite3_bind_double(stmt, idx++, bandwidth.transitTime);
    }
    auto result = sqlite3_step(stmt);
    if (!(bandWidthList.size() == TABLE_CACHE_SIZE && isInitStmt)) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert bandwidth data fail. ", sqlite3_errmsg(db));
    }
}

void TextClusterDatabase::InsertStepStatisticsInfo(StepStatistic &stepStatistic) {
    if (stepStmt == nullptr) {
        std::string sql = "INSERT INTO " + TABLE_STEP_TRACE +
            "(rank_id, step_id, stage_id, compute_time, pure_communication_time, overlap_communication_time, "
            "communication_time, free_time, stage_time, bubble_time, pure_communication_exclude_receive_time, "
            "preparing, dp_index, pp_index, tp_index) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stepStmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare stepTraceTable statement. error:", sqlite3_errmsg(db));
            return;
        }
        if (stepStmt == nullptr) {
            ServerLog::Error("Failed to get stepTraceTable stmt.");
            return;
        }
    } else {
        sqlite3_reset(stepStmt);
    }
    int idx = bindStartIndex;
    sqlite3_bind_text(stepStmt, idx++, stepStatistic.rankId.c_str(), stepStatistic.rankId.length(), SQLITE_STATIC);
    sqlite3_bind_text(stepStmt, idx++, stepStatistic.stepId.c_str(), stepStatistic.stepId.length(), SQLITE_STATIC);
    sqlite3_bind_text(stepStmt, idx++, stepStatistic.stageId.c_str(), stepStatistic.stageId.length(), SQLITE_STATIC);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.computingTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.pureCommunicationTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.overlapCommunicationTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.communicationTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.freeTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.stageTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.bubbleTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.pureCommunicationExcludeReceiveTime);
    sqlite3_bind_double(stepStmt, idx++, stepStatistic.prepareTime);
    sqlite3_bind_int64(stepStmt, idx++, stepStatistic.dpIndex);
    sqlite3_bind_int64(stepStmt, idx++, stepStatistic.ppIndex);
    sqlite3_bind_int64(stepStmt, idx++, stepStatistic.tpIndex);
    auto result = sqlite3_step(stepStmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Failed to insert step trace data. ", sqlite3_errmsg(db));
    }
}

void TextClusterDatabase::BindTextForClusterBaseInfo(ClusterBaseInfo &baseInfo, sqlite3_stmt *stmt) {
    std::string stringDpSize = std::to_string(baseInfo.config.dpSize);
    std::string stringPpSize = std::to_string(baseInfo.config.ppSize);
    std::string stringTpSize = std::to_string(baseInfo.config.tpSize);
    std::string stringCpSize = std::to_string(baseInfo.config.cpSize);
    std::string stringEpSize = std::to_string(baseInfo.config.epSize);
    std::string stringMoeTpSize = std::to_string(baseInfo.config.moeTpSize);
    int idx = bindStartIndex;
    sqlite3_bind_text(stmt, idx++, baseInfo.filePath.c_str(), baseInfo.filePath.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, baseInfo.stages.c_str(), baseInfo.stages.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, baseInfo.ppStages.c_str(), baseInfo.ppStages.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(
        stmt, idx++, baseInfo.config.algorithm.c_str(), baseInfo.config.algorithm.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringDpSize.c_str(), stringDpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringPpSize.c_str(), stringPpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringTpSize.c_str(), stringTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringCpSize.c_str(), stringCpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringEpSize.c_str(), stringEpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, stringMoeTpSize.c_str(), stringMoeTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, baseInfo.level.c_str(), baseInfo.level.length(), SQLITE_TRANSIENT);
}

void TextClusterDatabase::InsertClusterBaseInfo(ClusterBaseInfo &baseInfo) {
    sqlite3_stmt *stmt;
    std::string sql = "INSERT INTO " + TABLE_BASE_INFO +
        " (key, value) VALUES ('file_path', ?), "
        " ('ranks', (SELECT json_group_array(rank_id) FROM "
        " (SELECT DISTINCT rank_id FROM step_statistic_info WHERE rank_id !=''))), "
        " ('steps', (SELECT json_group_array(step_id) FROM "
        " (SELECT DISTINCT step_id FROM step_statistic_info WHERE rank_id !=''))), "
        " ('collect_start_time', NULL), ('collect_duration', NULL), "
        " ('stages', ?), ('pp_stages', ?), ('algorithm', ?), ('dp_size', ?), ('pp_size', ?), "
        " ('tp_size', ?), ('cp_size', ?), ('ep_size', ?), ('moe_tp_size', ?), "
        " ('level', ?), ('parse_status', NULL);";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare baseInfoTable statement. error:", sqlite3_errmsg(db));
        return;
    }
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get baseInfoTable stmt.");
        return;
    }
    BindTextForClusterBaseInfo(baseInfo, stmt);
    auto result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert baseInfoTable data fail. ", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
}

void TextClusterDatabase::InsertCommunicationMatrix(Dic::Module::CommunicationMatrixInfo &communicationMatrix) {
    matrixCache.emplace_back(communicationMatrix);
    if (matrixCache.size() == TABLE_CACHE_SIZE) {
        InsertCommunicationMatrixInfo(matrixCache);
        matrixCache.clear();
    }
}

void TextClusterDatabase::InsertCommunicationMatrixInfo(std::vector<CommunicationMatrixInfo> &matrixInfos) {
    if (matrixInfos.empty()) {
        return;
    }
    sqlite3_stmt *stmt = nullptr;
    if (matrixInfos.size() == TABLE_CACHE_SIZE && isInitStmt) {
        sqlite3_reset(matrixStmt);
        stmt = matrixStmt;
    } else {
        std::string sql = GetMatrixStmtSql(matrixInfos.size());
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare matrix table statement. error:", sqlite3_errmsg(db));
            return;
        }
    }
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get matrix stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &info : matrixInfos) {
        sqlite3_bind_text(stmt, idx++, info.groupId.c_str(), info.groupId.length(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, idx++, info.iterationId.c_str(), info.iterationId.length(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, idx++, info.opName.c_str(), info.opName.length(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, idx++, info.sortOp.c_str(), info.sortOp.length(), SQLITE_STATIC);
        sqlite3_bind_text(stmt, idx++, info.groupName.c_str(), info.groupName.length(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, idx++, info.srcRank);
        sqlite3_bind_int(stmt, idx++, info.dstRank);
        sqlite3_bind_text(stmt, idx++, info.transportType.c_str(), info.transportType.length(), SQLITE_STATIC);
        sqlite3_bind_double(stmt, idx++, info.transitSize);
        sqlite3_bind_double(stmt, idx++, info.transitTime);
        sqlite3_bind_double(stmt, idx++, info.bandwidth);
    }
    auto result = sqlite3_step(stmt);
    if (!(matrixInfos.size() == TABLE_CACHE_SIZE && isInitStmt)) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert matrix data fail. ", sqlite3_errmsg(db));
        return;
    }
}

std::string TextClusterDatabase::GetMatrixStmtSql(int len) {
    std::string sql = "INSERT INTO " + TABLE_COMMUNICATION_MATRIX +
        " (group_id, iteration_id, op_name, op_sort, group_name, src_rank, "
        "dst_rank, transport_type, transit_size, transit_time, bandwidth) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)";
    for (int i = 0; i < len - 1; i++) {
        sql.append(",(?,?,?,?,?,?,?,?,?,?,?)");
    }
    return sql;
}

bool TextClusterDatabase::QueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo) {
    baseInfo.filePath = GetDbPath();
    baseInfo.dataSize = static_cast<double>(FileUtil::GetFileSize(baseInfo.filePath.c_str())) / MB_SIZE;
    std::string baseInfoSql = "SELECT key, value FROM " + TABLE_BASE_INFO +
        " WHERE key IN ('ranks', 'steps', 'collect_start_time', 'collect_duration');";
    return ExecuteQueryBaseInfo(baseInfo, baseInfoSql);
}

std::map<std::string, std::string> TextClusterDatabase::QueryBaseInfoByKeys(const std::vector<std::string> &keys) {
    return ExecuteQueryBaseInfoByKeys(keys, TABLE_BASE_INFO);
}

bool TextClusterDatabase::InsertDuplicateUpdateBaseInfo(const std::map<std::string, std::string> &baseInfoMap) {
    return ExecuteInsertDuplicateUpdateBaseInfo(baseInfoMap, TABLE_BASE_INFO);
}

std::string TextClusterDatabase::QueryParseClusterStatus() {
    sqlite3_stmt *stmtBaseInfo = nullptr;
    std::string baseInfoSql = "SELECT key, value FROM " + TABLE_BASE_INFO + " WHERE key IN ('parse_status');";
    int baseInfoResult = sqlite3_prepare_v2(db, baseInfoSql.c_str(), -1, &stmtBaseInfo, nullptr);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Query parse status statement failed to prepare sql.", sqlite3_errmsg(db));
        return "";
    }
    std::map<std::string, std::string> info;
    std::string parseStatus;
    while (sqlite3_step(stmtBaseInfo) == SQLITE_ROW) {
        int coll = resultStartIndex;
        std::string key = sqlite3_column_string(stmtBaseInfo, coll++);
        std::string value = sqlite3_column_string(stmtBaseInfo, coll++);
        info.insert({key, value});
    }
    std::string valueParseStatus = CollectionUtil::FindValueByKey(info, "parse_status", CollectionUtil::EMPTY_STRING);
    parseStatus = valueParseStatus;
    sqlite3_finalize(stmtBaseInfo);
    return parseStatus;
}

void TextClusterDatabase::UpdateClusterParseStatus(std::string status) {
    ServerLog::Info("Update_Cluster_Parse_Status status: ", status);
    sqlite3_stmt *stmtBaseInfo = nullptr;
    int index = bindStartIndex;
    std::string baseInfoSql = "UPDATE " + TABLE_BASE_INFO + " SET value = ? WHERE key = 'parse_status';";
    int baseInfoResult = sqlite3_prepare_v2(db, baseInfoSql.c_str(), -1, &stmtBaseInfo, nullptr);
    sqlite3_bind_text(stmtBaseInfo, index, status.c_str(), status.length(), SQLITE_TRANSIENT);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Update cluster parse_status statement failed to prepare sql.", sqlite3_errmsg(db));
        return;
    }
    auto result = sqlite3_step(stmtBaseInfo);
    sqlite3_finalize(stmtBaseInfo);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Fail to update clusterParseStatus. ", sqlite3_errmsg(db));
    }
}

std::vector<std::string> TextClusterDatabase::GetAllRankFromStepStatisticInfo() {
    std::string sql = "SELECT DISTINCT rank_id as rankId FROM " + TABLE_STEP_TRACE + " WHERE rankId != ''";
    return ExecuteGetAllRankFromStepStatisticInfo(sql);
}

bool TextClusterDatabase::GetGroups(std::vector<GroupInfoDo> &groupList) {
    std::string sql = "SELECT DISTINCT rank_set as rank, group_id_hash, pg_name FROM " + TABLE_GROUP_ID;
    return ExecuteGetGroups(groupList, sql);
}

std::vector<CommGroupParallelInfo> TextClusterDatabase::GetAllGroupInfo() {
    std::string sql = "SELECT id, rank_set, type, group_id_hash, group_id, pg_name FROM " + TABLE_GROUP_ID;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to Get Groups info. error: ", sqlite3_errmsg(db));
        return {};
    }
    std::vector<CommGroupParallelInfo> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        CommGroupParallelInfo info;
        info.id = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        info.rankSetStr = sqlite3_column_string(stmt, col++);
        info.type = sqlite3_column_string(stmt, col++);
        info.groupIdHash = sqlite3_column_string(stmt, col++);
        info.groupId = sqlite3_column_string(stmt, col++);
        info.pgName = sqlite3_column_string(stmt, col++);
        res.push_back(info);
    }
    sqlite3_finalize(stmt);
    return res;
}

bool TextClusterDatabase::QueryMatrixList(
    Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList) {
    // 查询集群通信
    std::string sql = "SELECT src_rank as srcRank, dst_rank as dstRank, "
                      "transport_type as transportType, "
                      "ROUND(transit_size, 4) as transitSize, "
                      "ROUND(transit_time, 4) as transitTime, "
                      "ROUND(bandwidth, 4) as bandwidth ,"
                      "op_name as opName "
                      "FROM " +
        TABLE_COMMUNICATION_MATRIX +
        " CM"
        " WHERE group_name = ? AND iteration_id = ? AND op_sort = ? ";
    return ExecuteQueryMatrixList(param, matrixInfoDoList, sql);
}

bool TextClusterDatabase::QueryExtremumTimestamp(uint64_t &min, uint64_t &max) {
    std::string sql = "SELECT MIN(start_time) as minTime, MAX(start_time) as maxTime "
                      "FROM " +
        TABLE_TIME_INFO + " WHERE start_time != 0";
    return ExecuteQueryExtremumTimestamp(sql, min, max);
}

bool TextClusterDatabase::UpdateCollectTimeInfo(const Protocol::SummaryBaseInfo &baseInfo) {
    std::string sql = "INSERT OR REPLACE INTO " + TABLE_BASE_INFO +
        " (key, value) values "
        " ('collect_start_time', ?), ('collect_duration', ?);";
    return ExecuteUpdateCollectTimeInfo(baseInfo, sql);
}

bool TextClusterDatabase::QueryIterationAndCommunicationGroup(
    Protocol::CommunicationKernelParams &params, Protocol::CommunicationKernelBody &responseBody) {
    std::string sql = "select iteration_id, tg.rank_set as stage_id from " + TABLE_TIME_INFO +
        " t"
        " LEFT JOIN " +
        TABLE_GROUP_ID +
        " tg ON t.stage_id = tg.id"
        " where op_name = ? and rank_id = ?";
    return ExecuteQueryIterationAndCommunicationGroup(
        sql, params.name, params.rankId, responseBody.step, responseBody.group);
}

std::string TextClusterDatabase::GetAllOperatorsSql(
    const std::string &startTime, const std::string &bandwidthCondition, const std::string &timeCondition) const {
    std::string sql = "SELECT t.op_name as operatorName, "
                      "CASE WHEN start_time = 0 THEN 0 ELSE ROUND((start_time - " +
        startTime +
        ") / 1000000.0, 4) END as startTime, "
        "ROUND(elapse_time, 4) as elapseTime, "
        "ROUND(transit_time, 4) as transitTime, "
        "ROUND(synchronization_time, 4) as synchronizationTime, "
        "ROUND(wait_time, 4) as waitTime, "
        "ROUND(idle_time, 4) as idleTime, "
        "ROUND(synchronization_time_ratio, 4) as synchronizationTimeRatio, "
        "ROUND(wait_time_ratio, 4) as waitTimeRatio, "
        "bw.sdma_bw as sdmaBw, bw.rdma_bw as rdmaBw "
        "FROM " +
        TABLE_TIME_INFO +
        " t "
        "JOIN ( "
        "    SELECT op_name, "
        "    MAX(CASE WHEN transport_type = 'SDMA' THEN bandwidth_size ELSE 0 END) AS sdma_bw, "
        "    MAX(CASE WHEN transport_type = 'RDMA' THEN bandwidth_size ELSE 0 END) AS rdma_bw "
        "    FROM " +
        TABLE_BANDWIDTH + bandwidthCondition +
        "    GROUP BY op_name "
        ") bw ON t.op_name = bw.op_name " +
        timeCondition;
    return sql;
}

std::string TextClusterDatabase::GetAllOperatorsSql(uint64_t &startTime, const Protocol::OperatorDetailsParam &param) {
    std::string bandwidthCondition =
        " WHERE iteration_id = ? AND rank_id = ? AND op_suffix = ? AND op_name != 'Total Op Info' ";
    std::string timeCondition =
        " WHERE t.iteration_id = ? AND t.rank_id = ? AND t.op_suffix = ? AND t.op_name != 'Total Op Info'";
    std::string sql = GetAllOperatorsSql("?", bandwidthCondition, timeCondition);
    return sql;
}

bool TextClusterDatabase::QueryAllOperators(
    Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::string sql = GetAllOperatorsSql(startTime, param);
    // 深拷贝避免修改原param中的内容
    Protocol::OperatorDetailsParam copyParam(param);
    return ExecuteQueryAllOperators(copyParam, resBody, sql, startTime);
}

bool TextClusterDatabase::QueryOperatorsCount(
    Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody) {
    std::string sql = "SELECT count(*) AS nums from " + TABLE_TIME_INFO + " where op_name != 'Total Op Info' ";
    return ExecuteQueryOperatorsCount(param, resBody, sql);
}

bool TextClusterDatabase::QueryBandwidthData(
    Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody) {
    std::string sql = "SELECT transport_type,ROUND(transit_size, 4) as transit_size,"
                      "ROUND(transit_time, 4) as transit_time,"
                      "ROUND(bandwidth_size, 4) as bandwidth_size,"
                      "ROUND(large_package_ratio, 4)  as large_package_ratio from " +
        TABLE_BANDWIDTH +
        " b "
        " WHERE iteration_id = ? AND rank_id = ? AND op_suffix = ? AND op_name = ? ";
    return ExecuteQueryBandwidthData(param, resBody, sql);
}

bool TextClusterDatabase::QueryDistributionData(
    Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody) {
    std::string sql = "SELECT size_distribution FROM " + TABLE_BANDWIDTH +
        " b"
        " WHERE iteration_id = ? "
        "AND rank_id = ? "
        "AND b.op_suffix = ? "
        "AND op_name = ? "
        "AND transport_type = ? ;";
    return ExecuteQueryDistributionData(param, resBody, sql);
}

bool TextClusterDatabase::QueryRanksHandler(std::vector<Protocol::IterationsOrRanksObject> &responseBody) {
    std::string sql = "SELECT key, value FROM " + TABLE_BASE_INFO + " WHERE key IN ('ranks');";
    return ExecuteQueryRanksHandler(responseBody, sql);
}

bool TextClusterDatabase::QueryOperatorNames(
    Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) {
    std::vector<std::string> rankList = requestParams.rankList;
    std::string sql = "SELECT DISTINCT op_name FROM " + TABLE_TIME_INFO;
    std::string collectiveCondition = " WHERE iteration_id = ? AND op_suffix = ? ORDER BY op_name";
    Protocol::OperatorNamesParams copyParams(requestParams);
    return ExecuteQueryOperatorNames(copyParams, responseBody, sql + collectiveCondition);
}

bool TextClusterDatabase::QueryMatrixSortOpNames(
    Protocol::OperatorNamesParams &requestParams, std::vector<Protocol::OperatorNamesObject> &responseBody) {
    std::vector<Protocol::OperatorNamesObject> collectiveOpNameList;
    std::string sql = "SELECT DISTINCT op_sort  FROM " + TABLE_COMMUNICATION_MATRIX +
        " cm"
        " LEFT JOIN " +
        TABLE_GROUP_ID + " g ON cm.group_id = g.id";
    std::string collectiveCondition = " WHERE iteration_id = ? AND g.group_id_hash = ? ORDER BY op_sort";
    return ExecuteQueryMatrixSortOpNames(requestParams, responseBody, sql + collectiveCondition);
}

bool TextClusterDatabase::QueryIterations(std::vector<Protocol::IterationsOrRanksObject> &responseBody) {
    std::string sql = "SELECT key, value FROM " + TABLE_BASE_INFO + " WHERE key IN ('steps');";
    return ExecuteQueryIterations(responseBody, sql);
}

std::string TextClusterDatabase::GetDurationListSql(
    const std::string &bandwidthCondition, const std::string &timeCondition) const {
    std::string sql =
        "SELECT t.rank_id as rank_id, "
        "CASE WHEN start_time = 0 THEN 0 ELSE ROUND((start_time - ?) / 1000000.0, 4) END as startTime, "
        "ROUND(elapse_time, 4) as elapse_time, ROUND(transit_time, 4) as transit_time, "
        "ROUND(synchronization_time, 4) as synchronization_time, ROUND(wait_time, 4) as wait_time, "
        "ROUND(idle_time, 4) as idle_time, ROUND(synchronization_time_ratio, 4) as synchronization_time_ratio, "
        "ROUND(wait_time_ratio, 4) as wait_time_ratio, "
        "bw.sdma_bw as sdma_bw, bw.rdma_bw as rdma_bw, bw.sdma_time as sdma_time, bw.rdma_time as rdma_time "
        "FROM " +
        TABLE_TIME_INFO +
        " t "
        "JOIN ( "
        "    SELECT rank_id, "
        "    MAX(CASE WHEN transport_type = 'SDMA' THEN bandwidth_size ELSE 0 END) AS sdma_bw, "
        "    MAX(CASE WHEN transport_type = 'RDMA' THEN bandwidth_size ELSE 0 END) AS rdma_bw, "
        "    MAX(CASE WHEN transport_type = 'SDMA' THEN transit_time ELSE 0 END) AS sdma_time, "
        "    MAX(CASE WHEN transport_type = 'RDMA' THEN transit_time ELSE 0 END) AS rdma_time "
        "    FROM " +
        TABLE_BANDWIDTH + bandwidthCondition +
        "    GROUP BY rank_id "
        ") bw ON t.rank_id = bw.rank_id" +
        timeCondition;
    return sql;
}

bool TextClusterDatabase::QueryDurationList(
    Protocol::DurationListParams &requestParams, std::vector<DurationDo> &durationDoList) {
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    std::string collectiveCondition = " WHERE iteration_id = ? AND op_suffix = ? AND op_name = ? ";
    std::string collectiveSql = GetDurationListSql(collectiveCondition, collectiveCondition);
    Protocol::DurationListParams copyParams(requestParams);
    return ExecuteQueryDurationList(copyParams, durationDoList, collectiveSql, startTime);
}

std::string TextClusterDatabase::GetStageIdByGroupId(const std::string &groupId) {
    if (groupId.empty()) {
        return "";
    }
    std::string sql = "SELECT id From " + TABLE_GROUP_ID + " WHERE rank_set = ?";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Get stage id by group id failed to prepare sql.");
        return "";
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    resultSet = stmt->ExecuteQuery(groupId);
    if (!resultSet) {
        ServerLog::Error("Failed to execute query.");
        return "";
    }
    std::string stageId;
    if (resultSet->Next()) {
        stageId = resultSet->GetString("id");
    }
    return stageId;
}

bool TextClusterDatabase::QueryOperatorList(
    Protocol::DurationListParams &requestParams, std::vector<OperatorTimeDo> &operatorTimeDoList) {
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    // 基础sql
    std::string sql = "SELECT rank_id, op_name,"
                      " CASE WHEN start_time = 0 THEN 0 ELSE (start_time - ?) END as startTime,"
                      " (elapse_time * 1000000) as elapse_time From " +
        TABLE_TIME_INFO;
    // 拼接集群通信查询sql
    std::string collectiveCondition = " WHERE iteration_id = ? AND op_suffix = ? ";
    std::string opNameQuerySql =
        requestParams.operatorName == totalOpInfo ? " AND op_name <> 'Total Op Info'" : " AND op_name = ?";
    std::string collectiveSql = sql + collectiveCondition + opNameQuerySql + " ORDER by CAST(rank_id AS UNSIGNED) ASC";
    // 组装集群通信参数
    Protocol::DurationListParams copyParams(requestParams);
    copyParams.stage = GetStageIdByGroupId(copyParams.stage);
    // 集群通信数据查询
    return ExecuteQueryOperatorList(copyParams, operatorTimeDoList, collectiveSql, startTime);
}

bool TextClusterDatabase::QueryParallelStrategyConfig(ParallelStrategyConfig &config, std::string &level) {
    std::string sql = "SELECT key, value FROM " + TABLE_BASE_INFO + " WHERE key IN " +
        "('algorithm', 'dp_size', 'pp_size', 'tp_size', 'cp_size', 'ep_size', 'moe_tp_size', 'level');";
    return ExecuteQueryParallelStrategyConfig(sql, config, level);
}

bool TextClusterDatabase::UpdateParallelStrategyConfig(
    const ParallelStrategyConfig &config, std::string &level, std::string &msg) {
    std::string sql = "UPDATE " + TABLE_BASE_INFO +
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

bool TextClusterDatabase::GetParallelConfigFromStepTrace(ParallelStrategyConfig &config, std::string &level) {
    std::string sql = "select dp_index, pp_index, tp_index from " + TABLE_STEP_TRACE +
        " "
        "where rank_id != '' and step_id = (select DISTINCT step_id FROM " +
        TABLE_STEP_TRACE +
        " limit 1) "
        "order by step_id asc, CAST(rank_id AS INTEGER) asc";
    return ExecuteGetParallelConfigFromStepTrace(sql, config, level);
}

bool TextClusterDatabase::QueryAllPerformanceDataByStep(
    const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) {
    std::string sql;
    if (step.empty() || step == "All") {
        sql =
            "select rank_id as rank, round(sum(compute_time), 3) as compute, "
            "round(sum(pure_communication_time), 3) as not_overlap, "
            "round(sum(overlap_communication_time), 3) as overlap, round(sum(communication_time), 3) as communication, "
            "round(sum(free_time), 3) as free, "
            "round(sum(pure_communication_exclude_receive_time), 3) as exclude_receive, "
            "case WHEN sum(preparing) < 0 then 0 else round(sum(preparing), 3) end as preparing "
            "FROM " +
            TABLE_STEP_TRACE + " WHERE rank_id <> '' GROUP BY rank_id";
    } else {
        sql = "select rank_id as rank, compute_time as compute, pure_communication_time as not_overlap, "
              "overlap_communication_time as overlap, communication_time as communication, "
              "free_time as free, pure_communication_exclude_receive_time as exclude_receive, "
              "case WHEN preparing < 0 then 0 else preparing end as preparing FROM " +
            TABLE_STEP_TRACE +
            " "
            "WHERE rank_id <> '' and step_id = ?";
    }
    return ExecuteQueryAllPerformanceDataByStep(sql, step, data);
}

/**
 * 按快卡rankId查询通信时间，得到fast表；按慢卡rankId查询通信时间，得到slow表；
 * 通过opName join两表，计算得到差值diffElapseTime、算子名称、开始时间、持续时间等算子信息
 * 时间单位统一返回us
 * Text场景原表中, start_time时间单位为ns，Timeline::TraceTime::Instance().GetStartTime();得到时间戳单位为ns（占位符绑定）
 * 因此算子开始时间startTime为CASE WHEN t.start_time = 0 THEN 0 ELSE ROUND((start_time - ?) / 1000.0, 3)，
 * elapseTime原表中时间单位为ms，查询时折算为ROUND(t.elapse_time * 1000, 3)
 */
bool TextClusterDatabase::QuerySlowOpByCommDuration(const Protocol::DurationListParams &params,
    const std::string &fastestRankId, Protocol::RankDetailsForSlowRank &slowRank) {
    std::string sql = " select "
                      "    fast.opName, slow.startTime as startTimeOfSlow, slow.elapseTime AS elapseTimeOfSlow, "
                      "    (fast.elapseTime - slow.elapseTime) AS diffElapseTime, "
                      "    fast.elapseTime AS elapseTimeOfFast, fast.startTime as startTimeOfFast"
                      " from ( "
                      "    select t.rank_id as rankId, t.op_name AS opName, "
                      "    ROUND(t.elapse_time * 1000, 3) AS elapseTime, "
                      "    CASE WHEN start_time = 0 THEN 0 ELSE ROUND((start_time - ?) / 1000.0, 3) END as startTime "
                      "    from " +
        TABLE_TIME_INFO +
        " t "
        "    where t.rank_id = ? and iteration_id = ? and t.op_suffix = ? and opName != 'Total Op Info' "
        " ) fast "
        " join ( "
        "    select t.rank_id as rankId, t.op_name AS opName, "
        "    ROUND(t.elapse_time * 1000, 3) AS elapseTime, "
        "    CASE WHEN start_time = 0 THEN 0 ELSE ROUND((start_time - ?) / 1000.0, 3) END as startTime "
        "    from " +
        TABLE_TIME_INFO +
        " t "
        "    where t.rank_id = ? and iteration_id = ? and t.op_suffix = ? and opName != 'Total Op Info' "
        " ) slow "
        " on fast.opName = slow.opName ORDER BY diffElapseTime DESC;";
    return ExecuteQuerySlowOpByCommDuration(sql, params, fastestRankId, slowRank);
}

std::vector<CommInfoUnderRank> TextClusterDatabase::GetCommTimeForRankDim(const std::string &stepId) {
    std::string sql = "SELECT t.rank_id as rankId, ROUND(sum(t.elapse_time) * 1000, 3) as commTime, g.rank_set as "
                      " rankSet, g.group_id_hash as groupIdHash, g.pg_name as pgName FROM " +
        TABLE_TIME_INFO + " as t LEFT JOIN (SELECT rank_set, group_id_hash, pg_name FROM " + TABLE_GROUP_ID +
        " group by "
        "group_id_hash) as g ON t.op_suffix = g.group_id_hash WHERE op_name = 'Total Op Info'";
    if (!stepId.empty() && stepId != "All") {
        sql += " AND iteration_id = ? ";
    }
    sql += " group by t.rank_id, g.group_id_hash";
    return ExecuteGetCommTimeForRankDim(sql, stepId);
}

bool TextClusterDatabase::QueryPacketAnalyzerData(std::vector<PacketAnalyzerData> &data) {
    std::string sql = "SELECT transport_type, transit_size, transit_time FROM " + TABLE_BANDWIDTH +
        " WHERE (transport_type = 'RDMA' OR transport_type = 'SDMA') AND transit_size > 0 AND"
        " op_name != 'Total Op Info';";
    return ExecuteQueryPacketAnalyzerData(data, sql);
}

bool TextClusterDatabase::QueryBandwidthContentionAnalyzerData(
    std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId) {
    std::string sql = "SELECT t1.op_name, ROUND(start_time / 1000.0, 3) AS startTime,"
                      " ROUND(elapse_time * 1000.0, 3), bandwidth_size"
                      " FROM " +
        TABLE_BANDWIDTH + "  AS t1 INNER JOIN " + TABLE_TIME_INFO +
        " AS t2 ON "
        " t1.iteration_id = t2.iteration_id AND t1.rank_id = t2.rank_id AND"
        " t1.op_name = t2.op_name AND t1.op_suffix = t2.op_suffix "
        " WHERE t1.rank_id = ? AND transport_type = 'SDMA' AND transit_size > 0 ORDER BY startTime;";
    return ExecuteQueryBandwidthContentionAnalyzerData(res, rankId, sql);
}

bool TextClusterDatabase::QueryRetransmissionAnalyzerData(std::vector<RetransmissionClassificationInfo> &data) {
    std::string sql = "SELECT t1.iteration_id, t3.rank_set, t1.op_name, MIN(t2.elapse_time), MAX(t1.transit_time) "
                      " FROM " +
        TABLE_BANDWIDTH + " AS t1 INNER JOIN " + TABLE_TIME_INFO +
        " AS t2 ON "
        " t1.iteration_id = t2.iteration_id AND t1.op_suffix = t2.op_suffix AND "
        " t1.op_name = t2.op_name AND t1.rank_id = t2.rank_id INNER JOIN " +
        TABLE_GROUP_ID +
        " AS t3 ON "
        " t1.op_suffix = t3.group_id_hash WHERE transport_type = 'RDMA' AND t1.op_name != 'Total Op Info' "
        " GROUP BY t1.iteration_id, t3.rank_set, t1.op_name";
    return ExecuteQueryRetransmissionAnalyzerData(data, sql);
}

std::vector<OpTypeStatistics> TextClusterDatabase::GetOpStatByStepId(const std::string &stepId) {
    return opTypeStatisticsCache_.GetOrFetch(stepId, [&stepId, this]() -> std::vector<OpTypeStatistics> {
        std::string sql = "SELECT op_name as op_type, op_suffix FROM "
                          "communication_time_info WHERE op_name != 'Total Op Info' AND iteration_id = ?";
        return ExecuteGetOpStatByStepId(stepId, sql);
    });
}
} // end of namespace Module
} // end of namespace Dic
// LCOV_EXCL_BR_STOP
