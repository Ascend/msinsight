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
#include "SummaryDef.h"
#include "OperatorProtocolRequest.h"
#include "OperatorGroupConverter.h"
#include "OperatorProtocolResponse.h"
#include "OperatorProtocol.h"
#include "WsSessionManager.h"
#include "OperatorProtocolEvent.h"
#include "TraceTime.h"
#include "DataBaseManager.h"
#include "TrackInfoManager.h"
#include "DbSummaryDataBase.h"

namespace Dic::Module::FullDb {
using namespace Server;
using namespace Dic::Module::Timeline;
bool DbSummaryDataBase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    auto result =
        Database::OpenDb(dbPath, clearAllTable) && QueryMetaVersion() && AddCommunicationOpTableOpTypeIfNotExists();
    if (CheckColumnExist(TABLE_COMPUTE_TASK_INFO, "blockNum")) {
        blockNumColumnName = "blockNum";
    } else {
        blockNumColumnName = "blockDim";
    }
    return result;
}

bool DbSummaryDataBase::QueryComputeOpDetail(
    Protocol::ComputeDetailParams params, std::vector<Protocol::ComputeDetail> &computeDetails) {
    std::string sql = GenComputeSql(params);
    std::string timeFlag = params.timeFlag;
    uint64_t startTime = NumberUtil::CeilingClamp(Timeline::TraceTime::Instance().GetStartTime(), (uint64_t)INT64_MAX);
    double offset = (params.currentPage - 1) * params.pageSize;
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;

    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query operator detail failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, startTime);
    sqlite3_bind_text(stmt, index++, params.timeFlag.c_str(), params.timeFlag.length(), nullptr);
    sqlite3_bind_double(stmt, index++, params.pageSize);
    sqlite3_bind_double(stmt, index++, offset);
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

// LCOV_EXCL_BR_START
std::string DbSummaryDataBase::GenComputeSql(const Protocol::ComputeDetailParams &request) {
    std::string sql = "SELECT NAME.value AS name, "
                      "OP_TYPE.value as type, "
                      "CASE WHEN startNs == 0 THEN 0 ELSE ROUND((startNs - ?) /(1000.0 * 1000.0), 4) END AS startTime, "
                      "ROUND((endNs - startNs)/1000.0, 2) as duration, "
                      "ROUND((waitNs)/1000.0, 3) as waitTime, " +
        blockNumColumnName +
        " as blockNum, "
        "INPUTSHAPES.value as inputShape, "
        "INPUTDATATYPES.value as inputDataType, "
        "INPUTFORMATS.value as inputFormat, "
        "OUTPUTSHAPES.value as outputShape, "
        "OUTPUTDATATYPES.value as outputDataType, "
        "OUTPUTFORMATS.value as outputFormat "
        "FROM " +
        TABLE_COMPUTE_TASK_INFO +
        " JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        " JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name "
        " JOIN STRING_IDS AS OP_TYPE ON OP_TYPE.id = COMPUTE_TASK_INFO.opType "
        " JOIN STRING_IDS AS INPUTSHAPES ON INPUTSHAPES.id = COMPUTE_TASK_INFO.inputShapes "
        " JOIN STRING_IDS AS INPUTDATATYPES ON INPUTDATATYPES.id = COMPUTE_TASK_INFO.inputDataTypes "
        " JOIN STRING_IDS AS INPUTFORMATS ON INPUTFORMATS.id = COMPUTE_TASK_INFO.inputFormats "
        " JOIN STRING_IDS AS OUTPUTSHAPES ON OUTPUTSHAPES.id = COMPUTE_TASK_INFO.outputShapes "
        " JOIN STRING_IDS AS OUTPUTDATATYPES ON OUTPUTDATATYPES.id = COMPUTE_TASK_INFO.outputDataTypes "
        " JOIN STRING_IDS AS OUTPUTFORMATS ON OUTPUTFORMATS.id = COMPUTE_TASK_INFO.outputFormats "
        " JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType "
        " WHERE TASKTYPE.value = ? ";

    if (!StringUtil::CheckSqlValid(request.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy to generate compute sql.");
    } else if (!request.orderBy.empty() && !request.order.empty()) {
        sql += " ORDER by " + request.orderBy + " " + (request.order == "ascend" ? "ASC" : "DESC");
    }
    sql += " LIMIT ? offset ?";
    return sql;
}

bool DbSummaryDataBase::QueryTotalNumByAcceleratorCore(std::string name, int64_t &totalNum) {
    sqlite3_stmt *stmt = nullptr;
    // 这里JOIN一次TASK表的原因是查询具体算子时也JOIN了，当出现COMPUTE_TASK_INFO表中globalTaskId不在TASK表中的情况，不JOIN两者会不一致，但正常数据一般不会出现这种情况
    std::string sql = "SELECT count(*) as nums FROM " + TABLE_COMPUTE_TASK_INFO +
        " JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        " WHERE COMPUTE_TASK_INFO.taskType = (select id from STRING_IDS WHERE value = ?)";
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result == SQLITE_OK) {
        int index = bindStartIndex;
        sqlite3_bind_text(stmt, index++, name.c_str(), name.length(), nullptr);
    } else {
        ServerLog::Error("Get total num failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        totalNum = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string DbSummaryDataBase::GetGroupNameByIdListStr(const std::string &idListStr) {
    if (idListStr.empty()) {
        return "";
    }
    std::string res;
    std::vector<std::string> idList = StringUtil::Split(idListStr, "_");
    if (idList.size() == 1) {
        res = DbTraceDataBase::GetStringCacheValue(path, idList[0]);
    } else if (idList.size() > 1) {
        res = DbTraceDataBase::GetStringCacheValue(path, idList[0]);
        for (size_t i = 1; i < idList.size(); ++i) {
            res += "[" + DbTraceDataBase::GetStringCacheValue(path, idList[i]) + "]";
        }
    }
    return res;
}

bool DbSummaryDataBase::QueryOperatorDurationInfo(Protocol::OperatorDurationReqParams &reqParams,
    Protocol::QueryType type, std::vector<Protocol::OperatorDurationRes> &data) {
    if (!CheckOperatorTableExist(reqParams.group, "operator duration info")) {
        return true;
    }
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
    int deviceId = StringUtil::StringToInt(reqParams.deviceId);
    sqlite3_bind_int64(stmt, index++, deviceId);
    sqlite3_bind_int64(stmt, index++, reqParams.topK);

    std::vector<Protocol::OperatorDurationRes> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Protocol::OperatorDurationRes one{};
        int col = 0;
        one.name = GetGroupNameByIdListStr(sqlite3_column_string(stmt, col++));
        one.duration = sqlite3_column_double(stmt, col++);
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

bool DbSummaryDataBase::ExecSqlGetStatisticInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorStatisticInfoRes> &res) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Duration Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    if (!reqParams.deviceId.empty()) {
        int deviceId = StringUtil::StringToInt(reqParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }
    sqlite3_bind_int64(stmt, index++, reqParams.isCompare ? -1 : reqParams.topK);
    BindIdList(reqParams.rangeFilters, stmt, index);
    if (!reqParams.isCompare) {
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize * (reqParams.current - 1));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Protocol::OperatorStatisticInfoRes one{};
        int col = 0;
        one.opType = DbTraceDataBase::GetStringCacheValue(GetDbPath(), sqlite3_column_string(stmt, col++));
        one.opName = DbTraceDataBase::GetStringCacheValue(GetDbPath(), sqlite3_column_string(stmt, col++));
        one.inputShape = DbTraceDataBase::GetStringCacheValue(GetDbPath(), sqlite3_column_string(stmt, col++));
        one.accCore = DbTraceDataBase::GetStringCacheValue(GetDbPath(), sqlite3_column_string(stmt, col++));
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

bool DbSummaryDataBase::QueryOperatorStatisticInfo(
    Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorStatisticInfoResponse &response) {
    if (!CheckOperatorTableExist(reqParams.group, "operator statistic info")) {
        return true;
    }
    reqParams.rangeFilters = ConvertFiltersToRangeFilters(reqParams.filters);
    if (!QueryStatisticTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of statistic info.");
        return false;
    }
    std::string sql = GenerateQueryStatisticSql(reqParams);
    std::vector<Protocol::OperatorStatisticInfoRes> res;
    if (!ExecSqlGetStatisticInfo(sql, reqParams, res)) {
        ServerLog::Error("Failed to exec query detail sql.");
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

bool DbSummaryDataBase::QueryAllOperatorStatisticInfo(
    Protocol::OperatorStatisticReqParams &reqParams, std::vector<Protocol::OperatorStatisticInfoRes> &res) {
    reqParams.rangeFilters = ConvertFiltersToRangeFilters(reqParams.filters);
    std::string sql = GenerateQueryStatisticSql(reqParams);
    if (!ExecSqlGetStatisticInfo(sql, reqParams, res)) {
        ServerLog::Error("Failed to exec query detail sql.");
        return false;
    }
    return true;
}

std::string DbSummaryDataBase::GenerateQueryStatisticSql(Protocol::OperatorStatisticReqParams &reqParams) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    std::string sql;
    if (isCommunication) {
        sql = GenStatSqlWithCommunication();
    } else {
        std::string group = operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP
            ? "opType, accCore"
            : "opName, inputShape, accCore";
        if (!reqParams.deviceId.empty()) {
            sql = GenStatSqlWithDeviceId(group);
        } else {
            sql = GenStatSql(group);
        }
    }
    GenerateRangeQueryFiltersSql(reqParams.rangeFilters, sql);

    if (!StringUtil::CheckSqlValid(reqParams.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy"
                         "to generate query statistic sql.");
    } else if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + reqParams.orderBy + " " + (reqParams.order == "ascend" ? "ASC" : "DESC");
    }
    if (!reqParams.isCompare) {
        sql += " LIMIT ? OFFSET ?";
    }
    return sql;
}

std::string DbSummaryDataBase::GenStatSqlWithCommunication() {
    return " SELECT * FROM (SELECT "
           " COMMUNICATION_OP.opType as opType, "
           " COMMUNICATION_OP.opName as opName,"
           " NULL AS inputShape,"
           " NULL as accCore,"
           " ROUND(SUM(COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs) / 1000.0, 2) as totalTime, COUNT(0) as count,"
           " ROUND(SUM(COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs) / 1000.0 / COUNT(0), 2) as avgTime,"
           " ROUND(max(COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs) / 1000.0, 2) as maxTime,"
           " ROUND(min(COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs) / 1000.0, 2) as minTime "
           " FROM COMMUNICATION_OP "
           " JOIN (SELECT DISTINCT deviceId, connectionId FROM " +
        TABLE_TASK +
        ") "
        " AS NTASK ON NTASK.connectionId = COMMUNICATION_OP.connectionId "
        " WHERE NTASK.deviceId = ? "
        " GROUP BY opType "
        " ORDER by totalTime DESC LIMIT ?) subquery ";
}

std::string DbSummaryDataBase::GenStatSqlWithDeviceId(const std::string group) {
    return " SELECT * FROM ("
           "     SELECT "
           "     COMPUTE_TASK_INFO.opType as opType,"
           "     COMPUTE_TASK_INFO.name AS opName,"
           "     COMPUTE_TASK_INFO.inputShapes AS inputShape,"
           "     COMPUTE_TASK_INFO.taskType as accCore, "
           "     ROUND(SUM(TASK.endNs - TASK.startNs) / 1000.0, 2) as totalTime, COUNT(0) as count,"
           "     ROUND(SUM(endNs - startNs) / 1000.0 / COUNT(0), 2) as avgTime,"
           "     ROUND(max(endNs - startNs) / 1000.0, 2) as maxTime,"
           "     ROUND(min(endNs - startNs) / 1000.0, 2) as minTime"
           "     FROM  COMPUTE_TASK_INFO "
           "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
           "     WHERE TASK.deviceId = ? "
           "     GROUP BY " +
        group +
        " ORDER by totalTime DESC LIMIT ? "
        "     ) subquery ";
}

std::string DbSummaryDataBase::GenStatSql(const std::string group) {
    return " SELECT * FROM ("
           "     COMPUTE_TASK_INFO.opType as opType,"
           "     COMPUTE_TASK_INFO.name AS opName,"
           "     COMPUTE_TASK_INFO.inputShapes AS inputShape,"
           "     COMPUTE_TASK_INFO.taskType as accCore, "
           "     ROUND(SUM(TASK.endNs - TASK.startNs) / 1000.0, 2) as totalTime, COUNT(0) as count,"
           "     ROUND(SUM(endNs - startNs) / 1000.0 / COUNT(0), 2) as avgTime,"
           "     ROUND(max(endNs - startNs) / 1000.0, 2) as maxTime,"
           "     ROUND(min(endNs - startNs) / 1000.0, 2) as minTime"
           "     FROM  COMPUTE_TASK_INFO "
           "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
           "     GROUP BY " +
        group +
        " ORDER by totalTime DESC LIMIT ? "
        "     ) subquery ";
}

bool DbSummaryDataBase::QueryStatisticTotalNum(Protocol::OperatorStatisticReqParams &reqParams, int64_t &total) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    std::string sql;
    if (Protocol::OperatorGroupConverter::IsCommunication(reqParams.group)) {
        sql = GenStatSqlWithCommunication();
    } else {
        std::string group = operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP
            ? "opType, accCore"
            : "opName, inputShapes, accCore";
        sql = " SELECT COUNT(*) as nums"
              " FROM ( "
              "     SELECT deviceId, startNs, endNs,"
              "     COMPUTE_TASK_INFO.taskType AS accCore, "
              "     COMPUTE_TASK_INFO.opType AS opType, COMPUTE_TASK_INFO.name AS opName FROM COMPUTE_TASK_INFO"
              "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
              "     WHERE TASK.deviceId = ? "
              "     GROUP by " +
            group +
            "     ORDER by ROUND(SUM(TASK.endNs - TASK.startNs) / 1000.0, 2) DESC LIMIT ?"
            " ) subquery";
    }
    GenerateRangeQueryFiltersSql(reqParams.rangeFilters, sql);
    if (Protocol::OperatorGroupConverter::IsCommunication(reqParams.group)) {
        sql = StringUtil::FormatString("SELECT COUNT(*) FROM ({})", sql);
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Duration Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_int64(stmt, index++, StringUtil::StringToInt(reqParams.deviceId));
    sqlite3_bind_int64(stmt, index++, reqParams.topK);
    BindIdList(reqParams.rangeFilters, stmt, index);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DbSummaryDataBase::QueryOperatorDetailInfo(
    Protocol::OperatorStatisticReqParams &reqParams, Protocol::OperatorDetailInfoResponse &response) {
    if (!CheckOperatorTableExist(reqParams.group, "operator detail info")) {
        return true;
    }
    if (!QueryDetailTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of detail info.");
        return false;
    }
    std::string sql = GenerateQueryDetailSql(reqParams);
    std::vector<Protocol::OperatorDetailInfoRes> sqlRes;
    if (!ExecSqlGetDetailInfo(sql, reqParams, sqlRes)) {
        ServerLog::Error("Failed to exec query detail sql.");
        return false;
    }
    std::vector<Protocol::OperatorDetailCmpInfoRes> resultData;
    for (auto &data : sqlRes) {
        OperatorDetailCmpInfoRes tmpInfo;
        tmpInfo.compare = data;
        resultData.emplace_back(tmpInfo);
    }
    response.pmuHeaders = FetchPmuColumnNames();
    response.data = resultData;
    response.level = OperatorGetLevel(sqlRes);
    return true;
}

bool DbSummaryDataBase::QueryAllOperatorDetailInfo(Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorDetailInfoRes> &res, std::string &level) {
    if (!CheckOperatorTableExist(reqParams.group, "all operator detail info")) {
        return true;
    }
    std::string sql = GenerateAllQueryDetailSql(reqParams);
    if (!ExecSqlGetDetailInfo(sql, reqParams, res)) {
        ServerLog::Error("Failed to exec query detail sql.");
        return false;
    } else {
        level = OperatorGetLevel(res);
    }
    return true;
}

bool DbSummaryDataBase::CheckOperatorTableExist(const std::string &group, const std::string &logInfo) {
    if (OperatorGroupConverter::IsCommunication(group) && !CheckTableExist(TABLE_COMMUNICATION_OP)) {
        Server::ServerLog::Warn(
            "Missing table % on querying %, nothing will be done.", TABLE_COMMUNICATION_OP, logInfo);
        return false;
    }

    if (OperatorGroupConverter::IsCommunication(group) && !CheckTableExist(TABLE_COMPUTE_TASK_INFO)) {
        Server::ServerLog::Warn(
            "Missing table % on querying %, nothing will be done.", TABLE_COMPUTE_TASK_INFO, logInfo);
        return false;
    }
    return true;
}

bool DbSummaryDataBase::ExecSqlGetDetailInfo(std::string sql, Protocol::OperatorStatisticReqParams &reqParams,
    std::vector<Protocol::OperatorDetailInfoRes> &res) {
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
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    int index = bindStartIndex;
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, (uint64_t)INT64_MAX));

    if (!isCommunication && !reqParams.deviceId.empty()) {
        sqlite3_bind_int64(stmt, index++, StringUtil::StringToInt(reqParams.deviceId));
    }
    sqlite3_bind_int64(stmt, index++, reqParams.isCompare ? -1 : reqParams.topK);
    BindQueryFilters(reqParams, stmt, index);
    if (!reqParams.isCompare) {
        sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
        sqlite3_bind_int64(stmt, index++, (reqParams.current - 1) * reqParams.pageSize);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OperatorDetailInfoRes one = GetOperatorDetailRow(stmt);
        res.emplace_back(one);
    }
    sqlite3_finalize(stmt);
    return true;
}

OperatorDetailInfoRes DbSummaryDataBase::GetOperatorDetailRow(sqlite3_stmt *stmt) {
    int col = 0;
    OperatorDetailInfoRes one{};
    one.rankId = sqlite3_column_string(stmt, col++);
    one.stepId = sqlite3_column_string(stmt, col++);
    one.name = sqlite3_column_string(stmt, col++);
    one.type = sqlite3_column_string(stmt, col++);
    one.accCore = sqlite3_column_string(stmt, col++);
    one.startTime = sqlite3_column_string(stmt, col++);
    one.duration = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
    std::string waitTime = Sqlite3ColumnConvertStrReturnNull(SQLITE_FLOAT, stmt, col++);
    one.waitTime = waitTime == "NULL" ? "Loading" : waitTime;
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
    return one;
}

bool DbSummaryDataBase::QueryMoreInfoTotalNum(OperatorMoreInfoReqParams &reqParams, int64_t &total) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    std::string sql;
    if (isCommunication) {
        sql = "SELECT COUNT(*) as nums FROM ( SELECT TYPE.value as type, Name.value as name FROM COMMUNICATION_OP"
              "  JOIN STRING_IDS AS NAME ON NAME.id = COMMUNICATION_OP.opName "
              "  JOIN STRING_IDS AS TYPE ON TYPE.id = COMMUNICATION_OP.opType"
              "  WHERE type = ? ) subquery";
    } else {
        GenerateMoreInfoTotalNumForOther(sql, operatorGroup);
    }
    GenerateQueryFiltersSql<OperatorMoreInfoReqParams>(reqParams, sql);

    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get More Total Num. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    if (!isCommunication) {
        int deviceId = StringUtil::StringToInt(reqParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
        sqlite3_bind_text(stmt, index++, reqParams.accCore.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP) {
        sqlite3_bind_text(stmt, index++, reqParams.opType.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, index++, reqParams.opName.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
        sqlite3_bind_text(stmt, index++, reqParams.shape.c_str(), -1, SQLITE_TRANSIENT);
    }
    BindQueryFilters(reqParams, stmt, index);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string DbSummaryDataBase::GenerateQueryMoreInfoSql(OperatorMoreInfoReqParams &reqParams) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    std::string sql;
    if (isCommunication) {
        sql = GenerateQueryMoreInfoSqlForHCCL(sql);
    } else {
        sql = GenerateQueryMoreInfoSqlForOther(sql);
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP) {
        sql += " WHERE type = ?";
    } else {
        sql += " WHERE name = ? ";
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
        sql += "AND inputShape = ?";
    }
    if (!GenerateQueryMoreInfoFilters(reqParams, sql)) {
        return "";
    }

    if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + (reqParams.orderBy == "blockNum" ? blockNumColumnName : reqParams.orderBy) + " " +
            (reqParams.order == "ascend" ? "ASC" : "DESC");
    }

    sql += " LIMIT ? OFFSET ?";
    return sql;
}

bool DbSummaryDataBase::QueryOperatorMoreInfo(
    OperatorMoreInfoReqParams &reqParams, OperatorMoreInfoResponse &response) {
    if (!QueryMoreInfoTotalNum(reqParams, response.total)) {
        ServerLog::Error("[Operator]Failed to query total num of more info.");
        return false;
    }
    std::string sql = GenerateQueryMoreInfoSql(reqParams);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Op More Info. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    BindSqliteParam(stmt, reqParams);

    std::vector<OperatorDetailInfoRes> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = 0;
        OperatorDetailInfoRes one{};
        one.rankId = sqlite3_column_string(stmt, col++);
        one.stepId = sqlite3_column_string(stmt, col++);
        one.name = sqlite3_column_string(stmt, col++);
        one.type = sqlite3_column_string(stmt, col++);
        one.accCore = sqlite3_column_string(stmt, col++);
        one.startTime = sqlite3_column_string(stmt, col++);
        one.duration = Sqlite3ColumnConvertStr(SQLITE_FLOAT, stmt, col++);
        std::string waitTime = Sqlite3ColumnConvertStrReturnNull(SQLITE_FLOAT, stmt, col++);
        one.waitTime = waitTime == "NULL" ? "Loading" : waitTime;
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
    response.level = OperatorGetLevel(res);
    response.data = res;
    response.pmuHeaders = pmuColumns_;
    sqlite3_finalize(stmt);
    return true;
}

void DbSummaryDataBase::BindSqliteParam(sqlite3_stmt *stmt, Protocol::OperatorMoreInfoReqParams &reqParams) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    int index = bindStartIndex;
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, (uint64_t)INT64_MAX));
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    int deviceId = StringUtil::StringToInt(reqParams.deviceId);
    sqlite3_bind_int64(stmt, index++, deviceId);
    if (!isCommunication) {
        sqlite3_bind_text(stmt, index++, reqParams.accCore.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP) {
        sqlite3_bind_text(stmt, index++, reqParams.opType.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, index++, reqParams.opName.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP ||
        operatorGroup == OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
        sqlite3_bind_text(stmt, index++, reqParams.shape.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int64(stmt, index++, reqParams.pageSize);
    sqlite3_bind_int64(stmt, index++, (reqParams.current - 1) * reqParams.pageSize);
}

bool DbSummaryDataBase::QueryCommunicationOpDetail(
    Protocol::CommunicationDetailParams params, std::vector<Protocol::CommunicationDetail> &commDetails) {
    std::string sql = GetCommSql(params);
    std::string timeFlag = params.timeFlag;
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    double offset = (params.currentPage - 1) * params.pageSize;
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query common detail failed! Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, (uint64_t)INT64_MAX));
    sqlite3_bind_text(stmt, index++, params.timeFlag.c_str(), params.timeFlag.length(), nullptr);
    sqlite3_bind_double(stmt, index++, params.pageSize);
    sqlite3_bind_double(stmt, index++, offset);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        CommunicationDetail computeDetail{};
        computeDetail.name = sqlite3_column_string(stmt, col++);
        computeDetail.type = sqlite3_column_string(stmt, col++);
        computeDetail.startTime = sqlite3_column_string(stmt, col++);
        computeDetail.duration = sqlite3_column_double(stmt, col++);
        computeDetail.waitTime = sqlite3_column_double(stmt, col++);
        commDetails.emplace_back(computeDetail);
    }

    sqlite3_finalize(stmt);
    return true;
}

std::string DbSummaryDataBase::GetCommSql(const CommunicationDetailParams &request) {
    std::string sql = "SELECT name, op_type as type, CASE WHEN start_time == 0 THEN 'NA' "
                      "ELSE ROUND((start_time - ?) / (1000.0 * 1000.0), 4) END AS startTime, "
                      "ROUND(duration, 4) as duration, ROUND(waitTime, 4) as waitTime FROM ( "
                      " SELECT NAME.value AS name, OPTYPE.value AS op_type, "
                      " startNs as start_time, ROUND((endNs - startNs)/1000.0, 3) as duration, "
                      " TASKTYPE.value AS taskTypes, 0 as waitTime FROM " +
        TABLE_COMPUTE_TASK_INFO +
        "     JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name"
        "     JOIN STRING_IDS AS OPTYPE ON OPTYPE.id = COMPUTE_TASK_INFO.opType"
        "     JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType"
        "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        "     WHERE taskTypes = ?"
        "     GROUP BY TASK.globalTaskId"
        " ) subquery ";

    if (!StringUtil::CheckSqlValid(request.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy to get common sql.");
    } else if (!request.orderBy.empty() && !request.order.empty()) {
        sql += " ORDER by " + request.orderBy + " " + (request.order == "ascend" ? "ASC" : "DESC");
    }

    sql += " LIMIT ? offset ?";
    return sql;
}

std::string DbSummaryDataBase::GenerateQueryCategoryDurationSql(Protocol::OperatorDurationReqParams &reqParams) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(reqParams.group);
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::UNKNOWN) {
        ServerLog::Error("Generate query category duration sql failed, unknown operator group.");
        return "";
    }
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    if (isCommunication) {
        return GenerateQueryCategoryDurationSqlForHCCL(operatorGroup);
    } else {
        std::string group;
        std::string name;
        std::string duration;
        if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP) {
            name = "COMPUTE_TASK_INFO.opType";
            group = " GROUP by op_type, task_type";
            duration = " ROUND(sum(endNs - startNs)/1000.0, 2) as duration";
        } else if (operatorGroup == OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP) {
            name = "COMPUTE_TASK_INFO.name";
            group = "";
            duration = " ROUND((endNs - startNs)/1000.0, 2) as duration";
        } else {
            name = R"(COMPUTE_TASK_INFO.name || '_' || COMPUTE_TASK_INFO.inputShapes)";
            group = " GROUP by name, input_shapes, task_type";
            duration = " ROUND(sum(endNs - startNs)/1000.0, 2) as duration";
        }
        std::string sql = " SELECT name, duration From ("
                          " SELECT " +
            name + " as name," + duration +
            " ,COMPUTE_TASK_INFO.taskType as task_type, COMPUTE_TASK_INFO.opType as op_type, "
            "COMPUTE_TASK_INFO.inputShapes as input_shapes"
            " FROM " +
            TABLE_COMPUTE_TASK_INFO +
            " JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
            " WHERE TASK.deviceId = ? " +
            group +
            " ORDER BY duration DESC LIMIT ?"
            " ) subquery";
        return sql;
    }
}

std::string DbSummaryDataBase::GenerateQueryComputeUnitDurationSql(Protocol::OperatorDurationReqParams &reqParams) {
    std::string group;
    if (reqParams.group == OP_TYPE_GROUP) {
        group = "COMPUTE_TASK_INFO.opType, COMPUTE_TASK_INFO.taskType";
    } else if (reqParams.group == OPERATOR_GROUP) {
        group = "COMPUTE_TASK_INFO.name, COMPUTE_TASK_INFO.taskType";
    } else {
        group = "COMPUTE_TASK_INFO.name, COMPUTE_TASK_INFO.inputShapes, COMPUTE_TASK_INFO.taskType";
    }

    std::string sql = " SELECT taskTypes as name, ROUND(SUM(duration), 2) as duration"
                      " FROM ("
                      "     SELECT " +
        group + ", COMPUTE_TASK_INFO.taskType as taskTypes, " +
        (reqParams.group == Protocol::OPERATOR_GROUP ? " ROUND((endNs - startNs)/1000.0, 2) as duration"
                                                     : " ROUND(sum(endNs - startNs)/1000.0, 2) as duration") +
        "     FROM " + TABLE_COMPUTE_TASK_INFO +
        "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        "     WHERE TASK.deviceId = ?"
        "     " +
        (reqParams.group == Protocol::OPERATOR_GROUP ? "" : "     GROUP BY " + group) +
        "     ORDER BY duration DESC LIMIT ?"
        " ) subquery" +
        " GROUP by taskTypes"
        " ORDER BY duration DESC";
    return sql;
}

bool DbSummaryDataBase::QueryDetailTotalNum(OperatorStatisticReqParams &reqParams, int64_t &total) {
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    std::string sql;
    if (isCommunication) {
        sql = GenerateQueryDetailSqlForHCCL(sql);
    } else {
        sql = " SELECT COUNT(*) as nums"
              " FROM ("
              "   SELECT ROUND((endNs - startNs)/1000.0, 3) as duration, " +
            blockNumColumnName +
            "     , deviceId, streamId as step_id,NAME.value AS name,"
            "     OPTYPE.value AS type,TASKTYPE.value as accCore, startNs as startTime"
            "   FROM " +
            TABLE_COMPUTE_TASK_INFO +
            "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
            "     JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType"
            "     JOIN STRING_IDS AS OPTYPE ON OPTYPE.id = COMPUTE_TASK_INFO.opType"
            "     JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name"
            "     WHERE TASK.deviceId = ? "
            "     ORDER BY duration DESC LIMIT ?"
            " ) subquery";
    }
    GenerateQueryFiltersSql<OperatorStatisticReqParams>(reqParams, sql);
    if (isCommunication) {
        sql = StringUtil::FormatString("SELECT COUNT(*) FROM ({})", sql);
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get Detail Total Num. Msg: ", sqlite3_errmsg(db), " ", result);
        return false;
    }
    int index = bindStartIndex;
    int deviceId = StringUtil::StringToInt(reqParams.deviceId);
    sqlite3_bind_int64(stmt, index++, deviceId);
    sqlite3_bind_int64(stmt, index++, reqParams.topK);
    BindQueryFilters(reqParams, stmt, index);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}
// LCOV_EXCL_BR_STOP

std::set<std::string> DbSummaryDataBase::FetchPmuColumnNames() {
    if (!CheckTableExist(TABLE_TASK_PMU_INFO) || !pmuColumns_.empty()) {
        return pmuColumns_;
    }
    std::string queryColumnSql = "SELECT STRING_IDS.value "
                                 "FROM STRING_IDS "
                                 "WHERE STRING_IDS.id IN ( "
                                 "    SELECT name "
                                 "    FROM TASK_PMU_INFO "
                                 "    WHERE globalTaskId = ( "
                                 "        SELECT globalTaskId "
                                 "        FROM TASK_PMU_INFO "
                                 "        ORDER BY RANDOM() "
                                 "        LIMIT 1 "
                                 "    ) "
                                 ");";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, queryColumnSql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to get pmu cols Info. Msg:", sqlite3_errmsg(db), " ", result);
        return pmuColumns_;
    }
    // 执行SQL查询并处理结果
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string colName = sqlite3_column_string(stmt, 0);
        // 表头只能是字母、数字、下划线、短线、空格
        if (!RegexUtil::RegexMatch(colName, Summary::PMU_HEADER_WHITE_LIST_REG)) {
            sqlite3_finalize(stmt);
            ServerLog::Error("There is an SQL injection attack on colName. error colName: %", colName);
            return {};
        }
        pmuColumns_.insert(colName);
    }

    // 释放资源
    sqlite3_finalize(stmt);
    return pmuColumns_;
}

// STRING_IDS 和 TASK_PMU_INFO表联查，用 globalTaskId分组
std::string DbSummaryDataBase::CreatPMUTmpTableSql(const std::set<std::string> &cols) {
    if (cols.empty()) {
        return "";
    }
    std::string convertPmuDataSql = "SELECT i.globalTaskId ";
    for (auto const &col : cols) {
        convertPmuDataSql += ", MAX(CASE WHEN s.value = \'" + col + "\' THEN i.value END) AS " + col;
    }
    convertPmuDataSql += " FROM TASK_PMU_INFO i "
                         " JOIN STRING_IDS s ON i.name = s.id "
                         " GROUP BY i.globalTaskId ";

    return " LEFT JOIN ( " + convertPmuDataSql + " ) AS PMU ON COMPUTE_TASK_INFO.globalTaskId = PMU.globalTaskId ";
}

std::string DbSummaryDataBase::GetPMUTmpTableColSql(const std::set<std::string> &cols) {
    if (cols.empty()) {
        return "";
    }
    std::vector<std::string> tmpCols;
    tmpCols.reserve(cols.size()); // 预留空间，避免多次分配内存
    for (const std::string &col : cols) {
        tmpCols.push_back("PMU." + col);
    }
    // 加上临时表的前缀
    return "," + StringUtil::join(tmpCols, ",");
}

// LCOV_EXCL_BR_START
std::string DbSummaryDataBase::GenerateQueryDetailSqlForOperator() {
    std::set<std::string> pmuClos = FetchPmuColumnNames();
    // JoinExtraColName、 GetPMUTmpTableColSql、 CreatPMUTmpTableSql 为PMU数据处理，如果没有返回""
    std::string sql =
        " SELECT deviceId, step_id, name, type, accCore,"
        " CASE WHEN startTime == 0 THEN 'NA' ELSE  ROUND((startTime - ?) / (1000.0 * 1000.0), 2) END AS startTime, "
        " duration, waitTime, " +
        blockNumColumnName +
        ","
        " inputShape, inputType, inputFormat, outputShape, outputType, outputFormat " +
        JoinExtraColName(std::vector<std::string>(pmuClos.begin(), pmuClos.end())) +
        " FROM ("
        "     SELECT " +
        blockNumColumnName +
        ", deviceId, streamId as step_id,NAME.value AS name,"
        "     OPTYPE.value AS type,TASKTYPE.value as accCore, startNs as startTime, "
        "     ROUND((endNs - startNs)/1000.0, 3) as duration, ROUND(waitNs/1000.0, 3) as waitTime, "
        "     INPUTSHAPES.value as inputShape, INPUTDATATYPES.value as inputType, "
        "     INPUTFORMATS.value as inputFormat, OUTPUTSHAPES.value as outputShape, "
        "     OUTPUTDATATYPES.value as outputType, OUTPUTFORMATS.value as outputFormat " +
        GetPMUTmpTableColSql(pmuClos) + " FROM " + TABLE_COMPUTE_TASK_INFO +
        "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        "     JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name"
        "     JOIN STRING_IDS AS OPTYPE ON OPTYPE.id = COMPUTE_TASK_INFO.opType"
        "     JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType"
        "     JOIN STRING_IDS AS INPUTSHAPES ON INPUTSHAPES.id = COMPUTE_TASK_INFO.inputShapes"
        "     JOIN STRING_IDS AS INPUTDATATYPES ON INPUTDATATYPES.id = COMPUTE_TASK_INFO.inputDataTypes"
        "     JOIN STRING_IDS AS INPUTFORMATS ON INPUTFORMATS.id = COMPUTE_TASK_INFO.inputFormats"
        "     JOIN STRING_IDS AS OUTPUTSHAPES ON OUTPUTSHAPES.id = COMPUTE_TASK_INFO.outputShapes"
        "     JOIN STRING_IDS AS OUTPUTDATATYPES ON OUTPUTDATATYPES.id = COMPUTE_TASK_INFO.outputDataTypes"
        "     JOIN STRING_IDS AS OUTPUTFORMATS ON OUTPUTFORMATS.id = COMPUTE_TASK_INFO.outputFormats " +
        CreatPMUTmpTableSql(pmuClos) +
        "     WHERE TASK.deviceId = ? "
        "     ORDER by duration DESC LIMIT ? ) subquery ";
    return sql;
}

std::string DbSummaryDataBase::GenerateAllQueryDetailSql(OperatorStatisticReqParams &reqParams) {
    bool isCommunication = Protocol::OperatorGroupConverter::IsCommunication(reqParams.group);
    std::string sql;
    if (isCommunication) {
        sql = GenerateQueryDetailSqlForHCCL(sql);
    } else {
        sql = GenerateQueryDetailSqlForOperator();
    }
    GenerateQueryFiltersSql<OperatorStatisticReqParams>(reqParams, sql);
    if (!StringUtil::CheckSqlValid(reqParams.orderBy)) {
        ServerLog::Error("There is an SQL injection attack on the parameter of orderBy"
                         "to generate all query detail sql.");
    } else if (!reqParams.orderBy.empty() && !reqParams.order.empty()) {
        sql += " ORDER by " + (reqParams.orderBy == "blockNum" ? blockNumColumnName : reqParams.orderBy) + " " +
            (reqParams.order == "ascend" ? "ASC" : "DESC");
    }
    return sql;
}

std::string DbSummaryDataBase::GenerateQueryDetailSql(OperatorStatisticReqParams &reqParams) {
    std::string sql = GenerateAllQueryDetailSql(reqParams);
    if (std::empty(sql)) {
        return sql;
    }
    sql += " LIMIT ? OFFSET ?";
    return sql;
}

std::string DbSummaryDataBase::GenerateQueryCategoryDurationSqlForHCCL(
    const OperatorGroupConverter::OperatorGroup &operatorGroup) {
    std::string group;
    std::string name;
    std::string duration;
    if (operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP) {
        name = " COMMUNICATION_OP.opType as name ";
        group = " GROUP by name ";
        duration = " ROUND(sum(COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs)/1000.0, 2) as duration";
    } else if (operatorGroup == OperatorGroupConverter::OperatorGroup::COMMUNICATION_NAME_GROUP) {
        name = "COMMUNICATION_OP.opName as name";
        group = "";
        duration = " ROUND((COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs)/1000.0, 2) as duration";
    }
    std::string sql = " SELECT name, duration From ("
                      " SELECT " +
        name + "," + duration + " FROM " + TABLE_COMMUNICATION_OP +
        " JOIN (SELECT DISTINCT deviceId, connectionId FROM " + TABLE_TASK +
        ") "
        " AS NTASK ON NTASK.connectionId = COMMUNICATION_OP.connectionId "
        " WHERE NTASK.deviceId = ? " +
        group +
        " ORDER BY duration DESC LIMIT ?"
        " ) subquery";
    return sql;
}

std::string &DbSummaryDataBase::GenerateQueryMoreInfoSqlForHCCL(std::string &sql) const {
    sql = " SELECT rank_id, step_id, name, type, accCore,"
          " CASE WHEN startTime == 0 THEN 'NA' ELSE  ROUND((startTime - ?) / (1000.0 * 1000.0), 2)"
          " END AS startTime, duration, waitTime, NULL AS " +
        blockNumColumnName +
        ", NULL AS inputShape,"
        " NULL AS inputType, NULL AS inputFormat, NULL AS outputShape, NULL AS outputType,"
        " NULL AS outputFormat"
        " FROM ("
        "  SELECT NULL as rank_id, NULL as step_id, NAME.value AS name,"
        "  TYPE.value as type,"
        "  NULL as accCore,COMMUNICATION_OP.startNs as startTime,"
        "  ROUND((COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs)/1000.0, 3) as duration,"
        "  ROUND(COMMUNICATION_OP.waitNs/1000.0, 3) as waitTime FROM COMMUNICATION_OP"
        "  JOIN STRING_IDS AS NAME ON NAME.id = COMMUNICATION_OP.opName"
        "  JOIN STRING_IDS AS TYPE ON TYPE.id = COMMUNICATION_OP.opType"
        "  JOIN (SELECT DISTINCT deviceId, connectionId FROM " +
        TABLE_TASK +
        ") "
        "  AS NTASK ON NTASK.connectionId = COMMUNICATION_OP.connectionId "
        "  WHERE NTASK.deviceId = ? "
        "  ORDER by duration DESC ) subquery ";
    return sql;
}

std::string &DbSummaryDataBase::GenerateQueryMoreInfoSqlForOther(std::string &sql) {
    std::set<std::string> pmuClos = FetchPmuColumnNames();
    sql = " SELECT device_id, step_id, name, type, accCore,"
          " CASE WHEN startTime == 0 THEN 'NA' ELSE ROUND((startTime - ?) / (1000.0 * 1000.0), 2)"
          " END AS startTime, duration, waitTime, " +
        blockNumColumnName +
        ","
        " inputShape, inputType, inputFormat, outputShape, outputType, outputFormat " +
        JoinExtraColName(std::vector<std::string>(pmuClos.begin(), pmuClos.end())) +
        " FROM ("
        "     SELECT " +
        blockNumColumnName +
        ", deviceId as device_id, streamId as step_id, "
        "     NAME.value AS name,  OPTYPE.value AS type,"
        "  TASKTYPE.value as accCore,startNs as startTime,ROUND((endNs - startNs)/1000.0, 3) as duration,"
        "     ROUND((waitNs)/1000.0, 3) as waitTime, INPUTSHAPES.value as inputShape, "
        "     INPUTDATATYPES.value as inputType, "
        "     INPUTFORMATS.value as inputFormat, OUTPUTSHAPES.value as outputShape, "
        "     OUTPUTDATATYPES.value as outputType, OUTPUTFORMATS.value as outputFormat " +
        GetPMUTmpTableColSql(pmuClos) + "     FROM " + TABLE_COMPUTE_TASK_INFO +
        " JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        "     JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name"
        "     JOIN STRING_IDS AS OPTYPE ON OPTYPE.id = COMPUTE_TASK_INFO.opType"
        "     JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType"
        "     JOIN STRING_IDS AS INPUTSHAPES ON INPUTSHAPES.id = COMPUTE_TASK_INFO.inputShapes"
        "     JOIN STRING_IDS AS INPUTDATATYPES ON INPUTDATATYPES.id = COMPUTE_TASK_INFO.inputDataTypes"
        "     JOIN STRING_IDS AS INPUTFORMATS ON INPUTFORMATS.id = COMPUTE_TASK_INFO.inputFormats"
        "     JOIN STRING_IDS AS OUTPUTSHAPES ON OUTPUTSHAPES.id = COMPUTE_TASK_INFO.outputShapes"
        "     JOIN STRING_IDS AS OUTPUTDATATYPES ON OUTPUTDATATYPES.id = COMPUTE_TASK_INFO.outputDataTypes"
        "     JOIN STRING_IDS AS OUTPUTFORMATS ON OUTPUTFORMATS.id = COMPUTE_TASK_INFO.outputFormats " +
        CreatPMUTmpTableSql(pmuClos) +
        "     WHERE TASK.deviceId = ? AND accCore = ?"
        "     ORDER by duration DESC ) subquery ";
    return sql;
}

std::string &DbSummaryDataBase::GenerateQueryDetailSqlForHCCL(std::string &sql) const {
    sql = " SELECT rank_id, step_id, name, type, accCore,"
          " CASE WHEN startTime == 0 THEN 'NA' ELSE  ROUND((startTime - ?) / (1000.0 * 1000.0), 2)"
          " END AS startTime, duration, waitTime, NULL AS " +
        blockNumColumnName +
        ", NULL AS inputShape,"
        " NULL AS inputType, NULL AS inputFormat, NULL AS outputShape, NULL AS outputType,"
        " NULL AS outputFormat FROM ("
        "  SELECT NULL as rank_id, NULL as step_id, NAME.value AS name, TYPE.value AS type, "
        "  NULL as accCore,COMMUNICATION_OP.startNs as startTime,"
        "  ROUND((COMMUNICATION_OP.endNs - COMMUNICATION_OP.startNs)/1000.0, 3) as duration,"
        "  ROUND(COMMUNICATION_OP.waitNs/1000.0, 3) as waitTime FROM COMMUNICATION_OP"
        "  JOIN STRING_IDS AS NAME ON NAME.id = COMMUNICATION_OP.opName "
        "  JOIN STRING_IDS AS TYPE ON TYPE.id = COMMUNICATION_OP.opType"
        "  JOIN (SELECT DISTINCT deviceId, connectionId FROM " +
        TABLE_TASK +
        ") "
        "  AS NTASK ON NTASK.connectionId = COMMUNICATION_OP.connectionId "
        "  ORDER by duration DESC LIMIT ? ) subquery ";
    return sql;
}

void DbSummaryDataBase::GenerateMoreInfoTotalNumForOther(
    std::string &sql, OperatorGroupConverter::OperatorGroup opGroup) const {
    std::string condition = (opGroup == OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP)
        ? " type = ?"
        : " name = ? AND inputShape = ?";
    sql = " SELECT COUNT(*) as nums FROM ("
          "     SELECT NAME.value AS name, INPUTSHAPES.value AS inputShape, TASKTYPE.value AS accCore, "
          "     OPTYPE.value AS type"
          "     FROM " +
        TABLE_COMPUTE_TASK_INFO +
        "     JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId"
        "     JOIN STRING_IDS AS NAME ON NAME.id = COMPUTE_TASK_INFO.name"
        "     JOIN STRING_IDS AS OPTYPE ON OPTYPE.id = COMPUTE_TASK_INFO.opType"
        "     JOIN STRING_IDS AS INPUTSHAPES ON INPUTSHAPES.id = COMPUTE_TASK_INFO.inputShapes"
        "     JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType"
        "     WHERE TASK.deviceId = ? AND accCore = ? AND" +
        condition + " ) subquery";
}

std::vector<std::pair<std::string, std::vector<std::string>>> DbSummaryDataBase::ConvertFiltersToRangeFilters(
    std::vector<std::pair<std::string, std::string>> &filters) {
    std::vector<std::pair<std::string, std::vector<std::string>>> res;
    if (filters.empty()) {
        return {};
    }
    for (const auto &filter : filters) {
        if (filter.second.empty()) {
            continue;
        }
        res.emplace_back(filter.first, DbTraceDataBase::GetIdListByFuzzNameFromCache(path, filter.second, false));
    }
    return res;
}

void DbSummaryDataBase::GenerateRangeQueryFiltersSql(
    std::vector<std::pair<std::string, std::vector<std::string>>> &rangeFilters, std::string &sql) {
    if (rangeFilters.empty()) {
        return;
    }
    std::vector<std::string> sqlList;
    for (const auto &item : rangeFilters) {
        sqlList.push_back(item.first + " IN (" + StringUtil::CreateQuestionMarkString(item.second.size()) + ")");
    }
    sql += " WHERE " + StringUtil::join(sqlList, " AND ");
}

template <typename T> void DbSummaryDataBase::GenerateQueryFiltersSql(T &reqParams, std::string &sql) {
    if (reqParams.filters.empty()) {
        return;
    }
    sql += " WHERE ";
    for (size_t index = 0; index < reqParams.filters.size(); index++) {
        std::pair<std::string, std::string> filter = reqParams.filters[index];
        if (index != 0) {
            sql += " AND ";
        }
        sql += filter.first + " LIKE ?";
    }
}

void DbSummaryDataBase::BindIdList(
    const std::vector<std::pair<std::string, std::vector<std::string>>> &rangeFilters, sqlite3_stmt *stmt, int &index) {
    if (rangeFilters.empty()) {
        return;
    }
    for (const auto &item : rangeFilters) {
        for (const auto &id : item.second) {
            sqlite3_bind_text(stmt, index++, id.c_str(), id.length(), SQLITE_TRANSIENT);
        }
    }
}

template <typename T> void DbSummaryDataBase::BindQueryFilters(T &reqParams, sqlite3_stmt *stmt, int &index) {
    if (reqParams.filters.empty()) {
        return;
    }
    for (uint64_t i = 0; i < reqParams.filters.size(); i++) {
        std::pair<std::string, std::string> filter = reqParams.filters[i];
        std::string filterParam = "%" + filter.second + "%";
        sqlite3_bind_text(stmt, index++, filterParam.c_str(), filterParam.length(), SQLITE_TRANSIENT);
    }
}

bool DbSummaryDataBase::GenerateQueryMoreInfoFilters(OperatorMoreInfoReqParams &reqParams, std::string &sql) {
    for (const auto &filter : reqParams.filters) {
        if (!StringUtil::CheckSqlValid(filter.first) || !StringUtil::CheckSqlValid(filter.second)) {
            ServerLog::Error("There is an SQL injection attack on the parameter of filter"
                             "to generate query more info filters.");
            return false;
        }
        sql += " AND " + filter.first + " LIKE '%" + filter.second + "%' ";
    }
    return true;
}
// LCOV_EXCL_BR_STOP

void DbSummaryDataBase::ParserEnd(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &msg) {
    WsSession *session = WsSessionManager::Instance().GetSession();
    if (session == nullptr) {
        ServerLog::Error("Failed to get session for summary callback.");
        return;
    }
    if (rankId.empty()) {
        auto event = std::make_unique<Protocol::ModuleResetEvent>();
        event->moduleName = MODULE_OPERATOR;
        event->result = true;
        event->reset = true;
        session->OnEvent(std::move(event));
    } else {
        auto event = std::make_unique<Protocol::OperatorParseStatusEvent>();
        event->moduleName = MODULE_OPERATOR;
        event->result = true;
        event->data.rankId = rankId;
        event->data.status = result;
        event->data.error = msg;
        event->fileId = fileId;
        event->rankList = TrackInfoManager::Instance().GetRankListByFileId(fileId, rankId);
        session->OnEvent(std::move(event));
    }
}

void DbSummaryDataBase::Reset() {
    ServerLog::Info("Summary reset. wait task completed.");
    ServerLog::Info("Summary task completed.");
    auto databaseList = Timeline::DataBaseManager::Instance().GetAllSummaryDatabase();
    for (auto &db : databaseList) {
        auto database = dynamic_cast<DbSummaryDataBase *>(db);
        if (database != nullptr) {
            database->CloseDb();
        }
    }
    Timeline::DataBaseManager::Instance().Clear(Timeline::DatabaseType::SUMMARY);
}

bool DbSummaryDataBase::QueryBandwidthContentionMatMulData(std::vector<BandwidthContentionMatMulInfo> &res) {
    std::string sql = "SELECT " + TABLE_STRING_IDS +
        ".value, ROUND(startNs / 1000.0, 3) AS startTime,"
        " ROUND((endNs - startNs) / 1000.0, 3) FROM " +
        TABLE_COMPUTE_TASK_INFO + " INNER JOIN " + TABLE_STRING_IDS + " ON " + TABLE_COMPUTE_TASK_INFO +
        ".name = " + TABLE_STRING_IDS + ".id INNER JOIN " + TABLE_TASK + " ON " + TABLE_COMPUTE_TASK_INFO +
        ".globalTaskId = " + TABLE_TASK + ".globalTaskId WHERE " + TABLE_STRING_IDS +
        ".value LIKE '%matmul%' ORDER BY startTime";
    return ExecuteQueryBandwidthContentionMatMulData(res, sql);
}

bool DbSummaryDataBase::AddCommunicationOpTableOpTypeIfNotExists() {
    // 列存在直接返回true
    if (CheckColumnExist(TABLE_COMMUNICATION_OP, "opType")) {
        return true;
    }
    const std::string opTypeColumnName = "opType";
    // 列不存在走添加列逻辑
    std::string sql = StringUtil::FormatString(
        "ALTER TABLE {} ADD COLUMN {} INTEGER DEFAULT -1;", TABLE_COMMUNICATION_OP, opTypeColumnName);
    if (!ExecSql(sql)) {
        ServerLog::Error("Failed to add column % to table %", TABLE_COMMUNICATION_OP, opTypeColumnName);
        return false;
    }
    // 依据name刷新一次opType
    sql = StringUtil::FormatString(
        "UPDATE {}  "
        "SET opType = ( "
        "  SELECT "
        "    tid  "
        "  FROM "
        "    ( "
        "      SELECT DISTINCT "
        "        Name.id AS nid, "
        "        TYPE.id AS tid  "
        "      FROM "
        "        {} "
        "        JOIN STRING_IDS AS NAME ON NAME.id = {}.opName "
        "    JOIN STRING_IDS AS TYPE ON TYPE.value = SUBSTR(NAME.value, 1, INSTR (NAME.value, '__'))) AS SUBVIEW  "
        "WHERE "
        "  SUBVIEW.nid == opName)",
        TABLE_COMMUNICATION_OP, TABLE_COMMUNICATION_OP, TABLE_COMMUNICATION_OP);
    if (!ExecSql(sql)) {
        ServerLog::Error("Failed to set opType from opName.");
        return false;
    }
    return true;
}
}
