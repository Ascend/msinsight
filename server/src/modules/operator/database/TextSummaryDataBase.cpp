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
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "TraceTime.h"
#include "OperatorProtocol.h"
#include "ConstantDefs.h"
#include "DataBaseManager.h"
#include "TextSummaryDataBase.h"

namespace Dic::Module::Summary {
using namespace Server;
TextSummaryDataBase::TextSummaryDataBase(std::recursive_mutex &sqlMutex) : VirtualSummaryDataBase(sqlMutex) {}

TextSummaryDataBase::~TextSummaryDataBase() {
    if (hasInitStmt) {
        ReleaseStmt();
    }
    CloseDb();
}

bool TextSummaryDataBase::SetConfig() { return Database::SetConfig(); }

bool TextSummaryDataBase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Database is not open.");
        return false;
    }
    std::string sql = "CREATE TABLE " + TABLE_KERNEL +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, deviceId TEXT, step_id TEXT, " +
        "task_id TEXT, name TEXT, op_type TEXT, accelerator_core TEXT, start_time INTEGER, duration INTEGER, " +
        "wait_time INTEGER, block_num INTEGER, input_shapes TEXT, input_data_types TEXT, input_formats TEXT, " +
        "output_shapes TEXT, output_data_types TEXT, output_formats TEXT);" + "CREATE INDEX rank_index ON " +
        TABLE_KERNEL + " (deviceId);";
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool TextSummaryDataBase::DropTable() {
    std::vector<std::string> tables = {TABLE_KERNEL};
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return DropSomeTables(tables);
}

bool TextSummaryDataBase::InitStmt(const std::vector<std::string> &columns) {
    if (hasInitStmt) {
        return true;
    }
    std::string columnSql;
    std::string placeHolder;
    if (!columns.empty()) {
        for (const auto &column : columns) {
            columnSql += ", " + column;
            placeHolder += ",?";
        }
    }
    std::string sql = "INSERT INTO " + TABLE_KERNEL +
        " (deviceId, step_id, task_id, name, op_type, accelerator_core, " +
        "start_time, duration, wait_time, block_num, input_shapes, input_data_types, input_formats, " +
        "output_shapes, output_data_types, output_formats" + columnSql + ") " +
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" + placeHolder + ")";
    for (size_t i = 0; i < cacheSize - 1; ++i) {
        sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" + placeHolder + ")");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &insertKernelStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert kernel detail statement. error:", sqlite3_errmsg(db));
        return false;
    }
    hasInitStmt = true;
    return true;
}

void TextSummaryDataBase::ReleaseStmt() {
    if (insertKernelStmt != nullptr) {
        sqlite3_finalize(insertKernelStmt);
        insertKernelStmt = nullptr;
        hasInitStmt = false;
    }
}

void TextSummaryDataBase::InsertKernelDetailList(
    const std::vector<Kernel> &kernelVec, const std::vector<std::string> &columns) {
    sqlite3_stmt *stmt = GetKernelStmt(kernelVec.size(), columns);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get kernel stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &event : kernelVec) {
        sqlite3_bind_text(stmt, idx++, event.rankId.c_str(), event.rankId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.stepId.c_str(), event.stepId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.taskId.c_str(), event.taskId.length(), SQLITE_TRANSIENT),
            sqlite3_bind_text(stmt, idx++, event.name.c_str(), event.name.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.type.c_str(), event.type.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.acceleratorCore.c_str(), event.acceleratorCore.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, idx++, event.startTime);
        sqlite3_bind_double(stmt, idx++, event.duration);
        sqlite3_bind_double(stmt, idx++, event.waitTime);
        sqlite3_bind_int64(stmt, idx++, event.blockNum);
        sqlite3_bind_text(stmt, idx++, event.inputShapes.c_str(), event.inputShapes.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.inputDataTypes.c_str(), event.inputDataTypes.length(), SQLITE_TRANSIENT);

        sqlite3_bind_text(stmt, idx++, event.inputFormats.c_str(), event.inputFormats.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.outputShapes.c_str(), event.outputShapes.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.outputDataTypes.c_str(), event.outputDataTypes.length(), SQLITE_TRANSIENT);

        sqlite3_bind_text(stmt, idx++, event.outputFormats.c_str(), event.outputFormats.length(), SQLITE_TRANSIENT);
        // 此处的size比较为冗余判断，前面的逻辑能够保证二者相等，否则会丢弃相关数据
        if (columns.empty() || columns.size() != event.utilizationInfo.size()) {
            continue;
        }
        for (const auto &item : event.utilizationInfo) {
            sqlite3_bind_text(stmt, idx++, item.c_str(), item.length(), SQLITE_TRANSIENT);
        }
    }
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto result = sqlite3_step(stmt);
    if (kernelVec.size() != cacheSize) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert kernel detail fail. ", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        return;
    }
}

void TextSummaryDataBase::InsertKernelDetail(const Kernel &kernel, const std::vector<std::string> &columns) {
    std::lock_guard lock(mutex);
    kernelCache.emplace_back(kernel);
    if (kernelCache.size() == cacheSize) {
        InsertKernelDetailList(kernelCache, columns);
        kernelCache.clear();
    }
}

void TextSummaryDataBase::SaveKernelDetail(const std::vector<std::string> &columns) {
    if (kernelCache.size() > 0) {
        InsertKernelDetailList(kernelCache, columns);
        kernelCache.clear();
    }
}

sqlite3_stmt *TextSummaryDataBase::GetKernelStmt(uint64_t paramLen, const std::vector<std::string> &columns) {
    sqlite3_stmt *stmt = nullptr;
    if (paramLen == cacheSize) {
        stmt = insertKernelStmt;
        sqlite3_reset(stmt);
    } else {
        std::string columnSql;
        std::string placeHolder;
        if (!columns.empty()) {
            for (const auto &column : columns) {
                columnSql += ", " + column;
                placeHolder += ",?";
            }
        }
        std::string sql = "INSERT INTO " + TABLE_KERNEL +
            " (deviceId, step_id, task_id, name, op_type, accelerator_core, " +
            "start_time, duration, wait_time, block_num, input_shapes, input_data_types, input_formats, " +
            "output_shapes, output_data_types, output_formats" + columnSql + ") " +
            "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" + placeHolder + ")";
        for (uint64_t i = 0; i < paramLen - 1; ++i) {
            sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?" + placeHolder + ")");
        }
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare for insert kernel. length:", paramLen, " . error:", sqlite3_errmsg(db));
            return nullptr;
        }
    }
    return stmt;
}

bool TextSummaryDataBase::UpdateParseStatus(const std::string &status) {
    return UpdateValueIntoStatusInfoTable(kernelParseState, status);
}

bool TextSummaryDataBase::HasFinishedParseLastTime() {
    return CheckValueFromStatusInfoTable(kernelParseState, FINISH_STATUS);
}

bool TextSummaryDataBase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    if (!Database::OpenDb(dbPath, clearAllTable)) {
        return false;
    }
    return SetConfig() && CheckAndResetDatabaseOnVersionChange();
}

uint64_t TextSummaryDataBase::QueryMinStartTime() {
    std::string sql = "Select MIN(start_time) FROM " + TABLE_KERNEL + " WHERE start_time > 0";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query minimum start time.", sqlite3_errmsg(db));
        return 0;
    }
    int64_t min = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        min = sqlite3_column_int64(stmt, col++);
    }
    sqlite3_finalize(stmt);
    if (min <= 0) {
        min = INT64_MAX;
    }
    return min;
}

bool TextSummaryDataBase::QueryComputeOpDetail(
    Protocol::ComputeDetailParams params, std::vector<Protocol::ComputeDetail> &computeDetails) {
    std::string sql = GenComputeSql(params);
    std::string timeFlag = params.timeFlag;
    uint64_t startTime = NumberUtil::CeilingClamp(Timeline::TraceTime::Instance().GetStartTime(), (uint64_t)INT64_MAX);
    int64_t offset = (params.currentPage - 1) * params.pageSize;
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;

    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query operator detail failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, startTime);
    sqlite3_bind_text(stmt, index++, params.timeFlag.c_str(), params.timeFlag.length(), nullptr);
    sqlite3_bind_int64(stmt, index++, params.pageSize);
    sqlite3_bind_int64(stmt, index++, offset);
    std::vector<Protocol::ComputeDetail> computeVec;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::ComputeDetail computeDetail{};
        computeDetail.name = sqlite3_column_string(stmt, col++);
        computeDetail.type = sqlite3_column_string(stmt, col++);
        computeDetail.startTime = sqlite3_column_string(stmt, col++);
        computeDetail.duration = sqlite3_column_double(stmt, col++);
        computeDetail.waitTime = sqlite3_column_double(stmt, col++);
        computeDetail.blockNum = sqlite3_column_int64(stmt, col++);
        computeDetail.inputShapes = sqlite3_column_string(stmt, col++);
        computeDetail.inputDataTypes = sqlite3_column_string(stmt, col++);
        computeDetail.inputFormats = sqlite3_column_string(stmt, col++);
        computeDetail.outputShapes = sqlite3_column_string(stmt, col++);
        computeDetail.outputDataTypes = sqlite3_column_string(stmt, col++);
        computeDetail.outputFormats = sqlite3_column_string(stmt, col++);
        computeVec.emplace_back(computeDetail);
    }
    computeDetails = computeVec;

    sqlite3_finalize(stmt);
    return true;
}

std::string TextSummaryDataBase::GenSortSql(std::string orderBy, std::string order) {
    std::string orderBySql;
    if (!StringUtil::CheckSqlValid(orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy to generate sort sql.");
        return orderBySql;
    }
    if (order == "descend") {
        orderBySql = " ORDER BY " + orderBy + " DESC";
    } else {
        orderBySql = " ORDER BY " + orderBy + " ASC";
    }
    return orderBySql;
}

std::string TextSummaryDataBase::GenComputeSql(Protocol::ComputeDetailParams request) {
    std::string orderBy = GenSortSql(request.orderBy, request.order);
    std::string sql;
    if (request.orderBy.empty()) {
        sql = "SELECT name, op_type as type, "
              "CASE WHEN start_time == 0 THEN 0 ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 4) END AS startTime, "
              "duration, wait_time as waitTime, block_num as blockNum, "
              "input_shapes as inputShapes, input_data_types as inputDataTypes, input_formats as inputFormats, "
              "output_shapes as outputShapes, output_data_types as outputDataTypes, output_formats as outputFormats "
              "FROM " +
            TABLE_KERNEL + " WHERE accelerator_core = ?  LIMIT ? offset ?";
    } else {
        sql = "SELECT name, op_type as type, "
              "CASE WHEN start_time == 0 THEN 0 ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 4) END AS startTime, "
              "duration, wait_time as waitTime, block_num as blockNum, "
              "input_shapes as inputShapes, input_data_types as inputDataTypes, input_formats as inputFormats, "
              "output_shapes as outputShapes, output_data_types as outputDataTypes, output_formats as outputFormats "
              "FROM " +
            TABLE_KERNEL + " WHERE accelerator_core = ?  " + orderBy + " LIMIT ? offset ?";
    }
    return sql;
}

bool TextSummaryDataBase::QueryTotalNumByAcceleratorCore(std::string name, int64_t &totalNum) {
    sqlite3_stmt *stmt = nullptr;
    std::string sql;
    if (name == "Communication") {
        sql = "SELECT count(*) as nums FROM " + TABLE_KERNEL + " WHERE accelerator_core " + GetAcceleratorCoreSql(true);
    } else {
        sql = "SELECT count(*) as nums FROM " + TABLE_KERNEL + " WHERE accelerator_core = ?";
    }
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Get total num failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    if (name != "Communication") {
        int index = bindStartIndex;
        sqlite3_bind_text(stmt, index++, name.c_str(), name.length(), nullptr);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        totalNum = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string TextSummaryDataBase::GetCommSql(Protocol::CommunicationDetailParams request) {
    std::string orderBy = GenSortSql(request.orderBy, request.order);
    std::string sql;
    if (request.orderBy.empty()) {
        sql = "SELECT name, op_type as type, CASE WHEN start_time == 0 THEN 'NA' "
              "ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 4) END AS startTime, "
              "ROUND(duration, 4) as duration, ROUND(wait_time, 4) as waitTime FROM " +
            TABLE_KERNEL + " WHERE accelerator_core " + GetAcceleratorCoreSql(true) + "  LIMIT ? offset ?";
    } else {
        sql = "SELECT name, op_type as type, CASE WHEN start_time == 0 THEN 'NA' "
              "ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 4) END AS startTime, "
              "ROUND(duration, 4) as duration, ROUND(wait_time, 4) as waitTime FROM " +
            TABLE_KERNEL + " WHERE accelerator_core " + GetAcceleratorCoreSql(true) + orderBy + " LIMIT ? offset ?";
    }
    return sql;
}

bool TextSummaryDataBase::QueryCommunicationOpDetail(
    Protocol::CommunicationDetailParams params, std::vector<Protocol::CommunicationDetail> &computeDetails) {
    std::string sql = GetCommSql(params);
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    int64_t offset = (params.currentPage - 1) * params.pageSize;
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;

    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query common detail failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, (uint64_t)INT64_MAX));
    sqlite3_bind_int64(stmt, index++, params.pageSize);
    sqlite3_bind_int64(stmt, index++, offset);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::CommunicationDetail computeDetail{};
        computeDetail.name = sqlite3_column_string(stmt, col++);
        computeDetail.type = sqlite3_column_string(stmt, col++);
        computeDetail.startTime = sqlite3_column_string(stmt, col++);
        computeDetail.duration = sqlite3_column_double(stmt, col++);
        computeDetail.waitTime = sqlite3_column_double(stmt, col++);
        computeDetails.emplace_back(computeDetail);
    }

    sqlite3_finalize(stmt);
    return true;
}

std::string TextSummaryDataBase::GenerateQueryCategoryDurationSql(Protocol::OperatorDurationReqParams &reqParams) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::UNKNOWN) {
        ServerLog::Error("Category duration sql generate failed, unknown operator group.");
        return "";
    }
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);

    std::string sql;
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_NAME_GROUP) {
        std::string name = "name";
        sql = " SELECT " + name + " as name , ROUND(duration, 2) as duration FROM " + TABLE_KERNEL +
            " WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
            " ORDER BY duration DESC LIMIT ?";
    } else {
        std::string name;
        std::string group;
        if (IsOperatorGroupInType(operatorGroup)) {
            name = "op_type";
            group = "op_type || accelerator_core";
        } else {
            name = R"(name || '[' || input_shapes || ']')";
            group = R"(name || '[' || input_shapes || ']' || accelerator_core)";
        }
        sql = " SELECT " + name + " as name, ROUND(sum(duration), 2) as duration FROM " + TABLE_KERNEL +
            " WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) + " GROUP by " +
            group + " ORDER BY duration DESC LIMIT ?";
    }
    return sql;
}

std::string TextSummaryDataBase::GenerateQueryComputeUnitDurationSql(Protocol::OperatorDurationReqParams &reqParams) {
    std::string group;
    if (reqParams.group == Protocol::OP_TYPE_GROUP) {
        group = "op_type || accelerator_core";
    } else if (reqParams.group == Protocol::OPERATOR_GROUP) {
        group = "name || accelerator_core";
    } else {
        group = R"(name || '[' || input_shapes || ']' || accelerator_core)";
    }

    std::string sql = " SELECT accelerator_core as name, ROUND(SUM(duration), 2) as duration"
                      " FROM ("
                      "     SELECT " +
        group + ", accelerator_core," +
        (reqParams.group == Protocol::OPERATOR_GROUP ? "     ROUND(duration, 2) as duration"
                                                     : " ROUND(SUM(duration), 2) as duration") +
        "     FROM " + TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(false) +
        (reqParams.group == Protocol::OPERATOR_GROUP ? "     " : (" GROUP BY " + group)) +
        "     ORDER BY duration DESC LIMIT ?"
        " ) subquery" +
        " GROUP by accelerator_core"
        " ORDER BY duration DESC";
    return sql;
}

bool TextSummaryDataBase::QueryOperatorDurationInfo(Protocol::OperatorDurationReqParams &reqParams,
    Protocol::QueryType type, std::vector<Protocol::OperatorDurationRes> &data) {
    std::string sql;
    if (type == Protocol::QueryType::CATEGORY) {
        sql = GenerateQueryCategoryDurationSql(reqParams);
    } else {
        sql = GenerateQueryComputeUnitDurationSql(reqParams);
    }

    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Duration Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }

    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), reqParams.deviceId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, index++, reqParams.topK);

    std::vector<Protocol::OperatorDurationRes> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Protocol::OperatorDurationRes one{};
        int col = 0;
        one.name = sqlite3_column_string(stmt, col++);
        one.duration = sqlite3_column_double(stmt, col++);
        // 限制能够显示的最大数目为50
        if (res.size() >= maxCategorySize) {
            res[maxCategorySize - 1].name = "Others";
            res[maxCategorySize - 1].duration += one.duration;
        } else {
            res.emplace_back(one);
        }
    }
    data = res;

    sqlite3_finalize(stmt);
    return true;
}

bool TextSummaryDataBase::QueryStatisticTotalNum(Protocol::OperatorStatisticReqParams &reqParams, int64_t &total) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);

    std::string group;
    if (IsOperatorGroupInType(operatorGroup)) {
        group = "op_type || accelerator_core";
    } else {
        group = R"(name || '[' || input_shapes || ']' || accelerator_core)";
    }

    std::string sql = " SELECT COUNT(*) as nums"
                      " FROM ("
                      "     SELECT accelerator_core as accCore, op_type as opType, name as opName FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     GROUP by " + group +
        "     ORDER by ROUND(SUM(duration), 2) DESC LIMIT ?"
        " ) subquery";
    GenerateQueryFiltersSql<Protocol::OperatorStatisticReqParams>(reqParams, sql);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Statistic Num. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, index++, reqParams.topK);
    BindQueryFilters(reqParams, stmt, index);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string TextSummaryDataBase::GenerateQueryStatisticSql(Protocol::OperatorStatisticReqParams &reqParams) {
    std::string sql = GetQueryBaseStaticSql(reqParams);
    if (sql.empty()) {
        return "";
    }
    if (!StringUtil::CheckSqlValid(reqParams.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy"
                         "to generate query statistic sql.");
    } else if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + reqParams.orderBy + " " + (reqParams.order == "ascend" ? "ASC" : "DESC");
    }

    sql += " LIMIT ? OFFSET ?";
    return sql;
}

std::string TextSummaryDataBase::GetQueryBaseStaticSql(Protocol::OperatorStatisticReqParams &reqParams) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);

    std::string name;
    std::string group;
    if (IsOperatorGroupInType(operatorGroup)) {
        group = "op_type || accelerator_core";
        name = "''";
    } else {
        group = R"(name || input_shapes || accelerator_core)";
        name = "name";
    }
    std::string sql = GetQuerySqlNofilter(reqParams, isCommunication, group, name);
    GenerateQueryFiltersSql<Protocol::OperatorStatisticReqParams>(reqParams, sql);
    return sql;
}

std::string TextSummaryDataBase::GetAcceleratorCoreSql(const bool isCommunication) {
    if (isCommunication) {
        return " IN ('HCCL', 'COMMUNICATION') ";
    } else {
        return " NOT IN ('HCCL', 'COMMUNICATION') ";
    }
}

std::string TextSummaryDataBase::GetQuerySqlNofilter(Protocol::OperatorStatisticReqParams &reqParams,
    const bool isCommunication, const std::string &group, const std::string &name) {
    std::string limitSql = " SELECT * FROM ("
                           "     SELECT op_type as opType, " +
        name +
        " as opName, input_shapes as inputShape, "
        "     accelerator_core as accCore,"
        "     ROUND(SUM(duration), 2) as totalTime, COUNT(0) as count,"
        "     ROUND(SUM(duration) / COUNT(0), 2) as avgTime,"
        "     ROUND(max(duration), 2) as maxTime,"
        "     ROUND(min(duration), 2) as minTime"
        "     FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     GROUP by " + group +
        "     ORDER by totalTime DESC LIMIT ?"
        " ) subquery ";

    std::string baseNolimitSql = " SELECT * FROM ("
                                 "     SELECT op_type as opType, " +
        name +
        " as opName, input_shapes as inputShape, "
        "     accelerator_core as accCore,"
        "     ROUND(SUM(duration), 2) as totalTime, COUNT(0) as count,"
        "     ROUND(SUM(duration) / COUNT(0), 2) as avgTime,"
        "     ROUND(max(duration), 2) as maxTime,"
        "     ROUND(min(duration), 2) as minTime"
        "     FROM " +
        TABLE_KERNEL + "     WHERE accelerator_core " + GetAcceleratorCoreSql(isCommunication) + "     GROUP by " +
        group + " ) subquery";

    std::string noLimitsql = " SELECT * FROM ("
                             "     SELECT op_type as opType, " +
        name +
        " as opName, input_shapes as inputShape, accelerator_core as accCore,"
        "     ROUND(SUM(duration), 2) as totalTime, COUNT(0) as count,"
        "     ROUND(SUM(duration) / COUNT(0), 2) as avgTime,"
        "     ROUND(max(duration), 2) as maxTime,"
        "     ROUND(min(duration), 2) as minTime"
        "     FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     GROUP by " + group + " ) subquery";

    if (!reqParams.isCompare) {
        return limitSql;
    }
    if (reqParams.isCompare && !reqParams.deviceId.empty()) {
        return noLimitsql;
    }
    if (reqParams.isCompare && reqParams.deviceId.empty()) {
        return baseNolimitSql;
    }
    return "";
}

bool TextSummaryDataBase::QueryAllOperatorStatisticInfo(
    Protocol::OperatorStatisticReqParams &reqParams, std::vector<Protocol::OperatorStatisticInfoRes> &res) {
    // 比对场景全量查询
    std::string sql = GetQueryBaseStaticSql(reqParams);
    return ExecSqlGetStaticInfo(sql, reqParams, res);
}

bool TextSummaryDataBase::QueryOperatorStatisticInfo(
    Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorStatisticInfoResponse &response) {
    // 非比对场景条件查询
    if (!QueryStatisticTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of statistic info.");
        return false;
    }
    std::string sql = GenerateQueryStatisticSql(reqParams);
    std::vector<Protocol::OperatorStatisticInfoRes> res;
    if (!ExecSqlGetStaticInfo(sql, reqParams, res)) {
        return false;
    }

    std::vector<Protocol::OperatorStatisticCmpInfoRes> cmpRes;
    for (auto &data : res) {
        OperatorStatisticCmpInfoRes tmpInfo;
        tmpInfo.compare = data;
        cmpRes.emplace_back(tmpInfo);
    }
    response.data = cmpRes;
    return true;
}

bool TextSummaryDataBase::ExecSqlGetStaticInfo(const std::string &sql, Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorStatisticInfoRes> &res) {
    if (sql.empty()) {
        ServerLog::Error("Failed to generate query statistic sql.");
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Statistic Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }

    int index = bindStartIndex;
    if (!reqParams.deviceId.empty()) {
        sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), reqParams.deviceId.length(), SQLITE_TRANSIENT);
    }

    if (!reqParams.isCompare) {
        // 非比对场景条件查询
        sqlite3_bind_int64(stmt, index++, reqParams.topK);
        BindQueryFilters(reqParams, stmt, index);
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize * (reqParams.current - 1));
    } else {
        BindQueryFilters(reqParams, stmt, index);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        Protocol::OperatorStatisticInfoRes one{};
        one.opType = sqlite3_column_string(stmt, col++);
        one.opName = sqlite3_column_string(stmt, col++);
        one.inputShape = sqlite3_column_string(stmt, col++);
        one.accCore = sqlite3_column_string(stmt, col++);
        one.totalTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.count = Sqlite3ColumnConvertStr(SQLITE_INTEGER, stmt, col++);
        one.avgTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.maxTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.minTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        res.emplace_back(one);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool TextSummaryDataBase::QueryDetailTotalNum(Protocol::OperatorStatisticReqParams &reqParams, int64_t &total) {
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    sqlite3_stmt *stmt = nullptr;
    std::string sql = " SELECT COUNT(*) as nums"
                      " FROM ("
                      "     SELECT name, op_type as type, accelerator_core as accCore FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     ORDER BY duration DESC LIMIT ?"
        " ) subquery";
    GenerateQueryFiltersSql<Protocol::OperatorStatisticReqParams>(reqParams, sql);
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Detail Total Num. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, index++, reqParams.topK);
    BindQueryFilters(reqParams, stmt, index);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::set<std::string> TextSummaryDataBase::FetchPmuColumnNames() {
    if (!pmuColumns_.empty()) {
        return pmuColumns_;
    }
    std::string queryColumnSql = "SELECT name "
                                 "FROM pragma_table_info('" +
        TABLE_KERNEL +
        "') "
        "WHERE cid >= ( "
        "    SELECT cid "
        "    FROM pragma_table_info('" +
        TABLE_KERNEL +
        "') "
        "    WHERE name = 'aicore_time_us_'"
        ");";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, queryColumnSql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Detail Info. Msg:", sqlite3_errmsg(db), " ", result);
        return pmuColumns_;
    }
    // 执行SQL查询并处理结果
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string colName = sqlite3_column_string(stmt, 0);
        // 表头只能是字母、数字、下划线、短线、空格，不匹配不要跳过，直接返回空
        if (!RegexUtil::RegexMatch(colName, PMU_HEADER_WHITE_LIST_REG)) {
            ServerLog::Error("There is an SQL injection attack on this parameter. error param: %", colName);
            sqlite3_finalize(stmt);
            return {};
        }
        pmuColumns_.insert(colName);
    }
    // 释放资源
    sqlite3_finalize(stmt);
    return pmuColumns_;
}

std::string TextSummaryDataBase::GetQueryDetailBaseSql(Protocol::OperatorStatisticReqParams &reqParams, bool isLimit) {
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    // 获取pmu数据的列用来做查询，如果pmuColumnNames为空，就表示没有pmu列需要查找
    std::string pmuColumnNames;
    if (!isCommunication) {
        std::set<std::string> pmuClos = FetchPmuColumnNames();
        pmuColumnNames = JoinExtraColName(std::vector<std::string>(pmuClos.begin(), pmuClos.end()));
    }
    std::string sql;
    std::string sqlTab =
        " SELECT deviceId, step_id , name, op_type as type, accelerator_core as accCore,"
        " CASE WHEN start_time == 0 THEN 'NA' ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 2)"
        " END AS startTime, duration, wait_time as waitTime, block_num as blockNum,"
        " input_shapes as inputShape, input_data_types as inputType, input_formats as inputFormat, "
        " output_shapes as outputShape, output_data_types as outputType, output_formats as outputFormat " +
        pmuColumnNames;
    std::string conditionalQuerySql = " FROM ("
                                      "     SELECT * FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     ORDER by duration DESC LIMIT ?"
        " ) subquery ";
    std::string allQuerySql = " FROM ("
                              "     SELECT * FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     ORDER by start_time DESC"
        "     ) subquery ";
    std::string baseAllQuerySql = " FROM ("
                                  "     SELECT * FROM " +
        TABLE_KERNEL + "     WHERE accelerator_core " + GetAcceleratorCoreSql(isCommunication) +
        "     ORDER by start_time DESC"
        "     ) subquery ";

    if (isLimit && !reqParams.deviceId.empty()) {
        sql = sqlTab + conditionalQuerySql;
    } else {
        if (reqParams.deviceId.empty()) {
            sql = sqlTab + baseAllQuerySql;
        } else {
            sql = sqlTab + allQuerySql;
        }
    }
    GenerateQueryFiltersSql<Protocol::OperatorStatisticReqParams>(reqParams, sql);
    return sql;
}

std::string TextSummaryDataBase::GenerateQueryDetailSql(Protocol::OperatorStatisticReqParams &reqParams) {
    std::string sql = GetQueryDetailBaseSql(reqParams, true);
    if (sql.empty()) {
        return "";
    }

    if (!StringUtil::CheckSqlValid(reqParams.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy"
                         "to generate query detail sql.");
    } else if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + reqParams.orderBy + " " + (reqParams.order == "ascend" ? "ASC" : "DESC");
    }
    sql += " LIMIT ? OFFSET ?";
    return sql;
}

bool TextSummaryDataBase::ExecSqlGetDetailInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) {
    if (sql.empty()) {
        ServerLog::Error("Failed to generate query statistic sql.");
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Detail Info. Msg:", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_int64(
        stmt, index++, NumberUtil::CeilingClamp(Timeline::TraceTime::Instance().GetStartTime(), (uint64_t)INT64_MAX));
    if (!reqParams.deviceId.empty()) {
        sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), reqParams.deviceId.length(), SQLITE_TRANSIENT);
    }
    if (!reqParams.isCompare) {
        sqlite3_bind_int64(stmt, index++, reqParams.topK);
        BindQueryFilters(reqParams, stmt, index);
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
        sqlite3_bind_int64(stmt, index++, (reqParams.current - 1) * reqParams.pageSize);
    } else {
        BindQueryFilters(reqParams, stmt, index);
    }
    ExecSqlGetRes(stmt, res);
    level = OperatorGetLevel(res);
    sqlite3_finalize(stmt);
    return true;
}

bool TextSummaryDataBase::ExecSqlGetRes(sqlite3_stmt *stmt, std::vector<Protocol::OperatorDetailInfoRes> &res) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        Protocol::OperatorDetailInfoRes one{};
        one.rankId = sqlite3_column_string(stmt, col++);
        one.stepId = sqlite3_column_string(stmt, col++);
        one.name = sqlite3_column_string(stmt, col++);
        one.type = sqlite3_column_string(stmt, col++);
        one.accCore = sqlite3_column_string(stmt, col++);
        one.startTime = sqlite3_column_string(stmt, col++);
        // 这三个界面上都呈现string类型
        one.duration = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.waitTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.blockNum = Sqlite3ColumnConvertStr(SQLITE_INTEGER, stmt, col++);
        one.inputShape = sqlite3_column_string(stmt, col++);
        one.inputType = sqlite3_column_string(stmt, col++);
        one.inputFormat = sqlite3_column_string(stmt, col++);
        one.outputShape = sqlite3_column_string(stmt, col++);
        one.outputType = sqlite3_column_string(stmt, col++);
        one.outputFormat = sqlite3_column_string(stmt, col++);
        for (const auto &pmuCol : FetchPmuColumnNames()) {
            // 注意这里不要判空，有多少存储多少，防止和pmuheaders错行
            one.pmuDatas[pmuCol] = sqlite3_column_string(stmt, col++);
        }
        res.emplace_back(one);
    }
    return true;
}

bool TextSummaryDataBase::QueryAllOperatorDetailInfo(Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) {
    std::string sql = GetQueryDetailBaseSql(reqParams, false);
    return ExecSqlGetDetailInfo(sql, reqParams, res, level);
}

bool TextSummaryDataBase::QueryOperatorDetailInfo(
    Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorDetailInfoResponse &response) {
    if (!QueryDetailTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of detail info.");
        return false;
    }
    std::string sql = GenerateQueryDetailSql(reqParams);
    std::vector<Protocol::OperatorDetailInfoRes> sqlRes;
    if (!ExecSqlGetDetailInfo(sql, reqParams, sqlRes, response.level)) {
        return false;
    }
    std::vector<Protocol::OperatorDetailCmpInfoRes> resultData;
    for (auto &data : sqlRes) {
        OperatorDetailCmpInfoRes tmpInfo;
        tmpInfo.compare = data;
        resultData.emplace_back(tmpInfo);
    }
    response.data = resultData;
    response.pmuHeaders = FetchPmuColumnNames();
    return true;
}

bool TextSummaryDataBase::QueryMoreInfoTotalNum(Protocol::OperatorMoreInfoReqParams &reqParams, int64_t &total) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    std::string condition;
    if (IsOperatorGroupInType(operatorGroup)) {
        condition = " op_type = ?";
    } else {
        condition = " name = ? AND input_shapes = ?";
    }
    std::string sql = " SELECT COUNT(*) as nums"
                      " FROM ("
                      "     SELECT name, op_type as type, accelerator_core as accCore"
                      "     FROM " +
        TABLE_KERNEL + "     WHERE deviceId = ? AND accelerator_core = ? AND" + condition + " ) subquery";

    GenerateQueryFiltersSql<Protocol::OperatorMoreInfoReqParams>(reqParams, sql);

    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get More Total Num. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), reqParams.deviceId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, reqParams.accCore.c_str(), reqParams.accCore.length(), SQLITE_TRANSIENT);
    if (IsOperatorGroupInType(operatorGroup)) {
        sqlite3_bind_text(stmt, index++, reqParams.opType.c_str(), reqParams.opType.length(), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, index++, reqParams.opName.c_str(), reqParams.opName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, index++, reqParams.shape.c_str(), reqParams.shape.length(), SQLITE_TRANSIENT);
    }
    BindQueryFilters(reqParams, stmt, index);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string TextSummaryDataBase::GenerateQueryMoreInfoSql(Protocol::OperatorMoreInfoReqParams &reqParams) {
    // 获取pmu数据的列用来做查询，如果pmuColumnNames为空，就表示没有pmu列需要查找
    std::set<std::string> pmuClos = FetchPmuColumnNames();
    std::string pmuColumnNames = JoinExtraColName(std::vector<std::string>(pmuClos.begin(), pmuClos.end()));
    std::string sql =
        " SELECT deviceId, step_id, name, op_type as type, accelerator_core as accCore,"
        " CASE WHEN start_time == 0 THEN 'NA' ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 2)"
        " END AS startTime, duration, wait_time as waitTime, block_num as blockNum,"
        " input_shapes as inputShape, input_data_types as inputType, input_formats as inputFormat, "
        " output_shapes as outputShape, output_data_types as outputType, output_formats as outputFormat " +
        pmuColumnNames +
        " FROM ("
        "     SELECT * FROM " +
        TABLE_KERNEL +
        "     WHERE deviceId = ? AND accelerator_core = ?"
        "     ORDER by duration DESC"
        " ) subquery ";

    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    if (IsOperatorGroupInType(operatorGroup)) {
        sql += " WHERE type = ?";
    } else {
        sql += " WHERE name = ? AND inputShape = ?";
    }

    for (const auto &filter : reqParams.filters) {
        sql += " AND " + filter.first + " LIKE ? ";
    }

    if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + reqParams.orderBy + " " + (reqParams.order == "ascend" ? "ASC" : "DESC");
    }

    sql += " LIMIT ? OFFSET ?";
    return sql;
}

bool TextSummaryDataBase::QueryOperatorMoreInfo(
    Protocol::OperatorMoreInfoReqParams &reqParams, Protocol::OperatorMoreInfoResponse &response) {
    if (!QueryMoreInfoTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of more info.");
        return false;
    }

    std::string sql = GenerateQueryMoreInfoSql(reqParams);
    if (sql.empty()) {
        ServerLog::Error("Failed to generate query statistic sql.");
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Op More Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    if (reqParams.current <= 0) {
        ServerLog::Error("The current page is less than or equal to 0");
        sqlite3_finalize(stmt);
        return false;
    }
    BindSqliteParam(stmt, reqParams);

    std::vector<Protocol::OperatorDetailInfoRes> res = ExecSqlGetMoreInfo(stmt);
    response.level = OperatorGetLevel(res);
    response.data = res;
    response.pmuHeaders = pmuColumns_;
    sqlite3_finalize(stmt);
    return true;
}

std::vector<Protocol::OperatorDetailInfoRes> TextSummaryDataBase::ExecSqlGetMoreInfo(sqlite3_stmt *stmt) {
    std::vector<Protocol::OperatorDetailInfoRes> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        Protocol::OperatorDetailInfoRes one{};
        one.rankId = sqlite3_column_string(stmt, col++);
        one.stepId = sqlite3_column_string(stmt, col++);
        one.name = sqlite3_column_string(stmt, col++);
        one.type = sqlite3_column_string(stmt, col++);
        one.accCore = sqlite3_column_string(stmt, col++);
        one.startTime = sqlite3_column_string(stmt, col++);
        one.duration = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.waitTime = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        one.blockNum = Sqlite3ColumnConvertStr(SQLITE_INTEGER, stmt, col++);
        one.inputShape = sqlite3_column_string(stmt, col++);
        one.inputType = sqlite3_column_string(stmt, col++);
        one.inputFormat = sqlite3_column_string(stmt, col++);
        one.outputShape = sqlite3_column_string(stmt, col++);
        one.outputType = sqlite3_column_string(stmt, col++);
        one.outputFormat = sqlite3_column_string(stmt, col++);
        for (const auto &pmuCol : pmuColumns_) {
            // 注意这里不要判空，有多少存储多少，防止和pmuheaders错行
            one.pmuDatas[pmuCol] = sqlite3_column_string(stmt, col++);
        }
        res.emplace_back(one);
    }
    return res;
}

void TextSummaryDataBase::BindSqliteParam(sqlite3_stmt *stmt, Protocol::OperatorMoreInfoReqParams &reqParams) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    int index = bindStartIndex;
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, (uint64_t)INT64_MAX));
    sqlite3_bind_text(stmt, index++, reqParams.deviceId.c_str(), reqParams.deviceId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, reqParams.accCore.c_str(), reqParams.accCore.length(), SQLITE_TRANSIENT);
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    if (IsOperatorGroupInType(operatorGroup)) {
        sqlite3_bind_text(stmt, index++, reqParams.opType.c_str(), reqParams.opType.length(), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, index++, reqParams.opName.c_str(), reqParams.opName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, index++, reqParams.shape.c_str(), reqParams.shape.length(), SQLITE_TRANSIENT);
    }
    BindQueryFilters(reqParams, stmt, index);
    sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
    sqlite3_bind_int64(stmt, index++, (reqParams.current - 1) * reqParams.pageSize);
}

template <typename T> void TextSummaryDataBase::GenerateQueryFiltersSql(T &reqParams, std::string &sql) {
    if (reqParams.filters.empty()) {
        return;
    }
    sql += " WHERE ";
    for (uint64_t index = 0; index < reqParams.filters.size(); index++) {
        std::pair<std::string, std::string> filter = reqParams.filters[index];
        if (index != 0) {
            sql += " AND ";
        }
        sql += filter.first + " LIKE ? ";
    }
}

template <typename T> void TextSummaryDataBase::BindQueryFilters(T &reqParams, sqlite3_stmt *stmt, int &index) {
    if (reqParams.filters.empty()) {
        return;
    }
    for (uint64_t i = 0; i < reqParams.filters.size(); i++) {
        std::pair<std::string, std::string> filter = reqParams.filters[i];
        std::string filterParam = "%" + filter.second + "%";
        sqlite3_bind_text(stmt, index++, filterParam.c_str(), filterParam.length(), SQLITE_TRANSIENT);
    }
}

std::set<std::string> TextSummaryDataBase::QueryRankIds() {
    std::set<std::string> rankIds = {};
    std::string sql = "SELECT deviceId FROM " + TABLE_KERNEL + " GROUP BY deviceId";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare stmt for QueryRankIds. Msg: ", sqlite3_errmsg(db));
        return rankIds;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;

        std::string rank = sqlite3_column_string(stmt, col++);
        rankIds.emplace(rank);
    }

    sqlite3_finalize(stmt);
    return rankIds;
}

bool TextSummaryDataBase::IsOperatorGroupInType(OperatorGroupConverter::OperatorGroup operatorGroup) {
    return operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP;
}

bool TextSummaryDataBase::QueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res) {
    std::string sql = "SELECT name, ROUND(start_time / 1000.0, 3) AS startTime, duration FROM " + TABLE_KERNEL +
        " WHERE name LIKE '%matmul%' ORDER BY startTime";
    return ExecuteQueryBandwidthContentionMatMulData(res, sql);
}

} // end of namespace Summary
// end of namespace Module
// end of namespace Dic
