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
#include <map>
#include <ConstantDefs.h>
#include <algorithm>
#include "pch.h"
#include "NumDefs.h"
#include "CollectionUtil.h"
#include "TraceTime.h"
#include "VirtualClusterDatabase.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;

VirtualClusterDatabase::~VirtualClusterDatabase() { ReleaseStmt(); }

bool VirtualClusterDatabase::HasColumn(const std::string &tableName, const std::string &columnName) {
    if (!Database::CheckTableExist(tableName)) {
        ServerLog::Error("Failed to check table: ", tableName);
        return false;
    }
    std::string sql = "PRAGMA table_info(" + tableName + ")";
    sqlite3_stmt *stmt;
    bool result = false;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare statement. error:", sqlite3_errmsg(db));
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        if (columnName == reinterpret_cast<const char *>(name)) {
            result = true;
            break;
        }
    }
    sqlite3_finalize(stmt);

    return result;
}

bool VirtualClusterDatabase::ExecuteQueryBaseInfo(Protocol::SummaryBaseInfo &baseInfo, std::string sql) {
    sqlite3_stmt *stmtBaseInfo = nullptr;
    int baseInfoResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmtBaseInfo, nullptr);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Failed to prepare Query base info statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::map<std::string, std::string> info;
    while (sqlite3_step(stmtBaseInfo) == SQLITE_ROW) {
        int coll = resultStartIndex;
        std::string key = sqlite3_column_string(stmtBaseInfo, coll++);
        std::string value = sqlite3_column_string(stmtBaseInfo, coll++);
        info.insert({key, value});
    }
    std::string valueRanks = CollectionUtil::FindValueByKey(info, "ranks", CollectionUtil::EMPTY_STRING);
    std::string valueSteps = CollectionUtil::FindValueByKey(info, "steps", CollectionUtil::EMPTY_STRING);
    if (!valueRanks.empty()) {
        baseInfo.rankList = JsonUtil::JsonToVector(valueRanks);
    }
    if (!valueSteps.empty()) {
        baseInfo.stepList = JsonUtil::JsonToVector(valueSteps);
    }
    baseInfo.stepNum = NumberUtil::CeilingClamp(baseInfo.stepList.size(), static_cast<size_t>(UINT_MAX));
    baseInfo.rankCount = NumberUtil::CeilingClamp(baseInfo.rankList.size(), static_cast<size_t>(UINT_MAX));
    std::string startTime = CollectionUtil::FindValueByKey(info, "collect_start_time", CollectionUtil::EMPTY_STRING);
    std::string duration = CollectionUtil::FindValueByKey(info, "collect_duration", CollectionUtil::EMPTY_STRING);
    if (!startTime.empty()) {
        baseInfo.collectStartTime = NumberUtil::StringToLongLong(startTime);
    }
    if (!duration.empty()) {
        baseInfo.collectDuration = NumberUtil::StringToDouble(duration);
    }
    sqlite3_finalize(stmtBaseInfo);
    return true;
}

std::map<std::string, std::string> VirtualClusterDatabase::ExecuteQueryBaseInfoByKeys(
    const std::vector<std::string> &keys, const std::string &tableName) {
    if (keys.empty()) {
        return {};
    }
    std::string sql = "SELECT key, value FROM " + tableName + " WHERE key IN (" +
        StringUtil::CreateQuestionMarkString(keys.size()) + ");";
    sqlite3_stmt *stmtBaseInfo = nullptr;
    int baseInfoResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmtBaseInfo, nullptr);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Failed to prepare Query base info by keys statement. error:", sqlite3_errmsg(db));
        return {};
    }
    int index = bindStartIndex;
    for (const auto &item : keys) {
        sqlite3_bind_text(stmtBaseInfo, index++, item.c_str(), item.length(), SQLITE_TRANSIENT);
    }
    std::map<std::string, std::string> info;
    while (sqlite3_step(stmtBaseInfo) == SQLITE_ROW) {
        int coll = resultStartIndex;
        std::string key = sqlite3_column_string(stmtBaseInfo, coll++);
        std::string value = sqlite3_column_string(stmtBaseInfo, coll++);
        info.insert({key, value});
    }
    sqlite3_finalize(stmtBaseInfo);
    return info;
}

bool VirtualClusterDatabase::ExecuteInsertDuplicateUpdateBaseInfo(
    const std::map<std::string, std::string> &baseInfoMap, const std::string &tableName) {
    if (baseInfoMap.empty()) {
        return false;
    }
    std::string sql = "INSERT INTO " + tableName + " (key, value) VALUES (?,?)";
    for (size_t i = 0; i < baseInfoMap.size() - 1; ++i) {
        sql += ",(?,?)";
    }
    sql += " ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt *stmt = nullptr;
    int baseInfoResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (baseInfoResult != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert duplicate update base info statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    for (const auto &item : baseInfoMap) {
        sqlite3_bind_text(stmt, index++, item.first.c_str(), item.first.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, index++, item.second.c_str(), item.second.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

bool VirtualClusterDatabase::ExecuteGetGroups(std::vector<GroupInfoDo> &groupList, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare get groups statement. error:", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string rankSet = sqlite3_column_string(stmt, col++);
        if (rankSet == "p2p") {
            continue;
        }
        // rank set标准化处理：1.根据rank set数字从小到大排序；2.输出标准化格式“(1, 2, 3)”，统一“,”后面有空格
        std::vector<std::string> rankList = StringUtil::SplitStringWithParenthesesByComma(rankSet);
        rankSet = StringUtil::JoinNumberStrWithParenthesesByOrder(rankList);
        std::string groupIdHash = sqlite3_column_string(stmt, col++);
        std::string pgName = sqlite3_column_string(stmt, col++);
        groupList.push_back({rankSet, groupIdHash, pgName});
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryMatrixList(
    Protocol::MatrixBandwidthParam &param, std::vector<MatrixInfoDo> &matrixInfoDoList, const std::string &sql) {
    int index = bindStartIndex;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query matrix list statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), param.groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), param.iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, param.operatorName.c_str(), param.operatorName.length(), SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        MatrixInfoDo matrixInfoDo;
        matrixInfoDo.srcRank = sqlite3_column_int(stmt, col++);
        matrixInfoDo.dstRank = sqlite3_column_int(stmt, col++);
        matrixInfoDo.transportType = sqlite3_column_string(stmt, col++);
        matrixInfoDo.transitSize = sqlite3_column_double(stmt, col++);
        matrixInfoDo.transitTime = sqlite3_column_double(stmt, col++);
        matrixInfoDo.bandwidth = sqlite3_column_double(stmt, col++);
        matrixInfoDo.opName = sqlite3_column_string(stmt, col++);
        matrixInfoDoList.emplace_back(matrixInfoDo);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryExtremumTimestamp(std::string &sql, uint64_t &min, uint64_t &max) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query extremum timestamp statement. error:", sqlite3_errmsg(db));
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t tempMin = sqlite3_column_int64(stmt, col++);
        if (tempMin <= 0) {
            min = 0;
        } else {
            min = static_cast<uint64_t>(tempMin);
        }
        int64_t tempMax = sqlite3_column_int64(stmt, col++);
        if (tempMax <= 0) {
            max = 0;
        } else {
            max = static_cast<uint64_t>(tempMax);
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryIterationAndCommunicationGroup(std::string &sql, std::string &opName,
    const std::string &rankId, std::string &iteration, std::string &communicationGroup) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare query iteration and communication group statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, opName.c_str(), opName.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, rankId.c_str(), rankId.length(), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        iteration = sqlite3_column_type(stmt, col) == SQLITE_NULL ? "" : sqlite3_column_string(stmt, col++);
        communicationGroup = sqlite3_column_type(stmt, col) == SQLITE_NULL ? "" : sqlite3_column_string(stmt, col++);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryAllOperators(Protocol::OperatorDetailsParam &param,
    Protocol::OperatorDetailsResBody &resBody, std::string sql, uint64_t startTime) {
    sqlite3_stmt *stmt = nullptr;
    std::vector<std::string> orderByFlagVector = {"operatorName", "startTime", "elapseTime", "synchronizationTime",
        "waitTime", "idleTime", "transitTime", "sdmaBw", "rdmaBw", "synchronizationTimeRatio", "waitTimeRatio"};
    if (param.orderBy.empty() ||
        std::find(orderByFlagVector.begin(), orderByFlagVector.end(), param.orderBy) == orderByFlagVector.end()) {
        param.orderBy = "elapseTime";
    }
    std::string orderBy = " order by " + param.orderBy;
    std::string order = !param.order.empty() && std::strcmp(param.order.c_str(), "ascend") == 0 ? "ASC" : "DESC";
    sql += orderBy + " " + order + " LIMIT ?, ?";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query all operators statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, static_cast<uint64_t>(INT64_MAX)));
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), param.iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.rankId.c_str(), param.rankId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), param.groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), param.iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.rankId.c_str(), param.rankId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), param.groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, index++, (param.currentPage - 1) * param.pageSize);
    sqlite3_bind_int(stmt, index, param.pageSize);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::OperatorItem operatorItem;
        operatorItem.operatorName = sqlite3_column_string(stmt, col++);
        operatorItem.startTime = sqlite3_column_double(stmt, col++);
        operatorItem.elapseTime = sqlite3_column_double(stmt, col++);
        operatorItem.transitTime = sqlite3_column_double(stmt, col++);
        operatorItem.synchronizationTime = sqlite3_column_double(stmt, col++);
        operatorItem.waitTime = sqlite3_column_double(stmt, col++);
        operatorItem.idleTime = sqlite3_column_double(stmt, col++);
        operatorItem.synchronizationTimeRatio = sqlite3_column_double(stmt, col++);
        operatorItem.waitTimeRatio = sqlite3_column_double(stmt, col++);
        operatorItem.sdmaBw = sqlite3_column_double(stmt, col++);
        operatorItem.rdmaBw = sqlite3_column_double(stmt, col++);
        resBody.allOperators.emplace_back(operatorItem);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryOperatorsCount(
    Protocol::OperatorDetailsParam &param, Protocol::OperatorDetailsResBody &resBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    if (!param.iterationId.empty()) {
        sql.append("and iteration_id = ? ");
    }
    if (!param.rankId.empty()) {
        sql.append(" AND rank_id = ? ");
    }
    if (!param.groupIdHash.empty()) {
        sql.append(" AND op_suffix = ? ");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query operators count statement. error:", sqlite3_errmsg(db));
        return false;
    }
    if (!param.iterationId.empty()) {
        sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!param.rankId.empty()) {
        sqlite3_bind_text(stmt, index++, param.rankId.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!param.groupIdHash.empty()) {
        sqlite3_bind_text(stmt, index, param.groupIdHash.c_str(), -1, SQLITE_TRANSIENT);
    }
    bool res = (sqlite3_step(stmt) == SQLITE_ROW);
    if (res) {
        resBody.count = sqlite3_column_int(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return res;
}

bool VirtualClusterDatabase::ExecuteQueryBandwidthData(
    Protocol::BandwidthDataParam &param, Protocol::BandwidthDataResBody &resBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query bandwidth data statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.rankId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, param.operatorName.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::BandwidthDataItem bandwidth;
        bandwidth.transportType = sqlite3_column_string(stmt, col++);
        bandwidth.transitSize = sqlite3_column_double(stmt, col++);
        bandwidth.transitTime = sqlite3_column_double(stmt, col++);
        bandwidth.bandwidth = sqlite3_column_double(stmt, col++);
        bandwidth.largePacketRatio = sqlite3_column_double(stmt, col++);
        resBody.items.emplace_back(bandwidth);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryDistributionData(
    Protocol::DistributionDataParam &param, Protocol::DistributionResBody &resBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare distribution data statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, param.iterationId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.rankId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.groupIdHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index++, param.operatorName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, index, param.transportType.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        resBody.distributionData = sqlite3_column_string(stmt, col);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryRanksHandler(
    std::vector<Protocol::IterationsOrRanksObject> &responseBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query ranks statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::map<std::string, std::string> info;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string key = sqlite3_column_string(stmt, col++);
        std::string value = sqlite3_column_string(stmt, col++);
        info.insert({key, value});
    }
    std::string valueRanks = CollectionUtil::FindValueByKey(info, "ranks", CollectionUtil::EMPTY_STRING);
    GetStepsOrRanksObject(valueRanks, responseBody);
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryOperatorNames(Protocol::OperatorNamesParams &requestParams,
    std::vector<Protocol::OperatorNamesObject> &responseBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    std::string iterationId = requestParams.iterationId;
    std::string groupIdHash = requestParams.groupIdHash;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query operator names statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, iterationId.c_str(), iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, groupIdHash.c_str(), groupIdHash.length(), SQLITE_TRANSIENT);
    bool flag = false;
    Protocol::OperatorNamesObject totalOpInfo;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::OperatorNamesObject object;
        object.operatorName = sqlite3_column_string(stmt, col++);
        if (object.operatorName == "Total Op Info" && !flag) { // summary opName default select Total Op Info
            totalOpInfo = object;
            flag = true;
            continue;
        }
        responseBody.emplace_back(object);
    }
    if (!totalOpInfo.operatorName.empty()) {
        responseBody.insert(responseBody.begin(), totalOpInfo);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryIterations(
    std::vector<Protocol::IterationsOrRanksObject> &responseBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query iterations statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::map<std::string, std::string> info;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string key = sqlite3_column_string(stmt, col++);
        std::string value = sqlite3_column_string(stmt, col++);
        info.insert({key, value});
    }
    std::string valueSteps = CollectionUtil::FindValueByKey(info, "steps", CollectionUtil::EMPTY_STRING);
    GetStepsOrRanksObject(valueSteps, responseBody);
    if (responseBody.empty()) {
        ServerLog::Warn("Failed to obtain the number of iteration ids. At least one id must be contained. "
                        "Check whether communication data files exist in the directory.");
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryDurationList(Protocol::DurationListParams &requestParams,
    std::vector<DurationDo> &durationDoList, std::string sql, uint64_t startTime) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    std::string iterationId = requestParams.iterationId;
    std::string groupIdHash = requestParams.groupIdHash;
    std::string operatorName = requestParams.operatorName;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare Query Duration List statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, static_cast<uint64_t>(INT64_MAX)));
    sqlite3_bind_text(stmt, index++, iterationId.c_str(), iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, groupIdHash.c_str(), groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, operatorName.c_str(), operatorName.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, iterationId.c_str(), iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, groupIdHash.c_str(), groupIdHash.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, operatorName.c_str(), operatorName.length(), SQLITE_TRANSIENT);
    std::vector<Protocol::BandwidthStatistic> bwStat = {{"SDMA", 0, 0, DBL_MAX, 0, 0}, {"RDMA", 0, 0, DBL_MAX, 0, 0}};
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        DurationDo object;
        object.rankId = sqlite3_column_string(stmt, col++);
        object.startTime = sqlite3_column_double(stmt, col++);
        object.elapseTime = sqlite3_column_double(stmt, col++);
        object.transitTime = sqlite3_column_double(stmt, col++);
        object.synchronizationTime = sqlite3_column_double(stmt, col++);
        object.waitTime = sqlite3_column_double(stmt, col++);
        object.idleTime = sqlite3_column_double(stmt, col++);
        object.synchronizationTimeRatio = sqlite3_column_double(stmt, col++);
        object.waitTimeRatio = sqlite3_column_double(stmt, col++);
        object.sdmaBw = sqlite3_column_double(stmt, col++);
        object.rdmaBw = sqlite3_column_double(stmt, col++);
        object.sdmaTime = sqlite3_column_double(stmt, col++);
        object.rdmaTime = sqlite3_column_double(stmt, col++);
        durationDoList.emplace_back(object);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryOperatorList(Protocol::DurationListParams &requestParams,
    std::vector<OperatorTimeDo> &operatorTimeDoList, const std::string &sql, uint64_t startTime) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    std::string iterationId = requestParams.iterationId;
    std::string groupIdHash = requestParams.groupIdHash;
    std::string operatorName = requestParams.operatorName;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare Query Operator List statement. error:", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(startTime, static_cast<uint64_t>(INT64_MAX)));
    sqlite3_bind_text(stmt, index++, iterationId.c_str(), iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, groupIdHash.c_str(), groupIdHash.length(), SQLITE_TRANSIENT);
    if (requestParams.operatorName != totalOpInfo) {
        sqlite3_bind_text(stmt, index, operatorName.c_str(), operatorName.length(), SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        OperatorTimeDo object{};
        object.rankId = sqlite3_column_string(stmt, col++);
        object.operatorName = sqlite3_column_string(stmt, col++);
        int64_t tempStartTime = sqlite3_column_int64(stmt, col++);
        if (tempStartTime <= 0) {
            object.startTime = 0;
        } else {
            object.startTime = static_cast<uint64_t>(tempStartTime);
        }
        int64_t tempElapseTime = sqlite3_column_int64(stmt, col++);
        if (tempElapseTime <= 0) {
            object.elapseTime = 0;
        } else {
            object.elapseTime = static_cast<uint64_t>(tempElapseTime);
        }
        operatorTimeDoList.push_back(object);
    }
    sqlite3_finalize(stmt);
    return true;
}

void VirtualClusterDatabase::GetStepsOrRanksObject(
    const std::string &jsonStr, std::vector<Protocol::IterationsOrRanksObject> &responseBody) {
    std::string err;
    auto json = JsonUtil::TryParse(jsonStr, err);
    if (!json || !err.empty()) {
        ServerLog::Error("Failed to parse json on get steps or ranks: %.", err);
        return;
    }
    if (!json->IsArray()) {
        return;
    }
    for (auto &item : json->GetArray()) {
        Protocol::IterationsOrRanksObject object;
        object.iterationOrRankId = item.IsString() ? item.GetString() : "";
        responseBody.emplace_back(object);
    }
}

bool VirtualClusterDatabase::ExecuteQueryMatrixSortOpNames(Protocol::OperatorNamesParams &requestParams,
    std::vector<Protocol::OperatorNamesObject> &responseBody, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    std::string iterationId = requestParams.iterationId;
    std::string groupIdHash = requestParams.groupIdHash;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare Query Matrix Sort OpNames statement. error: ", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_bind_text(stmt, index++, iterationId.c_str(), iterationId.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index, groupIdHash.c_str(), groupIdHash.length(), SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::OperatorNamesObject object;
        object.operatorName = sqlite3_column_string(stmt, col++);
        responseBody.emplace_back(object);
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string VirtualClusterDatabase::GetRanksSql(const std::vector<std::string> &rankList) {
    std::string ranks = "(";
    if (rankList.empty()) {
        return "";
    } else {
        for (size_t i = 0; i < rankList.size(); i++) {
            if (!StringUtil::CheckSqlValid(rankList[i])) {
                ServerLog::Error("There is an SQL injection attack on this parameter. error param: ", rankList[i]);
                return "";
            }

            if (i == rankList.size() - 1) {
                ranks += rankList[i];
            } else {
                ranks += rankList[i];
                ranks += ", ";
            }
        }
    }
    ranks += ")";
    return ranks;
}

bool VirtualClusterDatabase::ExecuteQueryParallelStrategyConfig(
    std::string &sql, ParallelStrategyConfig &config, std::string &level) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare query parallel strategy config statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::map<std::string, std::string> info;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string key = sqlite3_column_string(stmt, col++);
        std::string value = sqlite3_column_string(stmt, col++);
        info.insert({key, value});
    }
    std::string valueAlgorithm = CollectionUtil::FindValueByKey(info, "algorithm", CollectionUtil::EMPTY_STRING);
    std::string valueDpSize = CollectionUtil::FindValueByKey(info, "dp_size", CollectionUtil::EMPTY_STRING);
    std::string valuePpSize = CollectionUtil::FindValueByKey(info, "pp_size", CollectionUtil::EMPTY_STRING);
    std::string valueTpSize = CollectionUtil::FindValueByKey(info, "tp_size", CollectionUtil::EMPTY_STRING);
    std::string valueCpSize = CollectionUtil::FindValueByKey(info, "cp_size", CollectionUtil::EMPTY_STRING);
    std::string valueEpSize = CollectionUtil::FindValueByKey(info, "ep_size", CollectionUtil::EMPTY_STRING);
    std::string valueMoeTpSize = CollectionUtil::FindValueByKey(info, "moe_tp_size", CollectionUtil::EMPTY_STRING);
    std::string valueLevel = CollectionUtil::FindValueByKey(info, "level", CollectionUtil::EMPTY_STRING);
    config.algorithm = valueAlgorithm;
    config.dpSize = NumberUtil::StringToUint32(valueDpSize);
    config.ppSize = NumberUtil::StringToUint32(valuePpSize);
    config.tpSize = NumberUtil::StringToUint32(valueTpSize);
    config.cpSize = NumberUtil::StringToUint32(valueCpSize);
    config.epSize = NumberUtil::StringToUint32(valueEpSize);
    config.moeTpSize = NumberUtil::StringToUint32(valueMoeTpSize);
    level = valueLevel;
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteSetParallelStrategyConfig(
    std::string &sql, const ParallelStrategyConfig &config, std::string &level) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare set parallel strategy config statement. error:", sqlite3_errmsg(db));
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    int index = bindStartIndex;
    std::string stringDpSize = std::to_string(config.dpSize);
    std::string stringPpSize = std::to_string(config.ppSize);
    std::string stringTpSize = std::to_string(config.tpSize);
    std::string stringCpSize = std::to_string(config.cpSize);
    std::string stringEpSize = std::to_string(config.epSize);
    std::string stringMoeTpSize = std::to_string(config.moeTpSize);
    sqlite3_bind_text(stmt, index++, config.algorithm.c_str(), config.algorithm.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringDpSize.c_str(), stringDpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringPpSize.c_str(), stringPpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringTpSize.c_str(), stringTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringCpSize.c_str(), stringCpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringEpSize.c_str(), stringEpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, stringMoeTpSize.c_str(), stringMoeTpSize.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, level.c_str(), level.length(), SQLITE_TRANSIENT);
    auto result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Fail to update parallel strategy config. ", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteGetParallelConfigFromStepTrace(
    std::string &sql, ParallelStrategyConfig &config, std::string &level) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare get parallel config statement. error:", sqlite3_errmsg(db));
        return false;
    }
    bool flag = false;
    uint32_t prePpIndex{};
    uint32_t preDpIndex{};
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int rawDpSize = sqlite3_column_int(stmt, col++);
        int rawPpSize = sqlite3_column_int(stmt, col++);
        int rawTpSize = sqlite3_column_int(stmt, col++);
        if (rawDpSize < 0 || rawPpSize < 0 || rawTpSize < 0) {
            return false;
        }
        auto dpSize = static_cast<uint32_t>(rawDpSize);
        auto ppSize = static_cast<uint32_t>(rawPpSize);
        config.tpSize = std::max(config.tpSize, static_cast<uint32_t>(rawTpSize));
        config.dpSize = std::max(config.dpSize, dpSize);
        config.ppSize = std::max(config.ppSize, ppSize);
        // 通过判断dp和pp哪个先增加，来判断tp-dp-pp还是tp-pp-dp
        if (!flag) {
            if (dpSize > preDpIndex) {
                config.algorithm = MEGATRON_LM_TP_CP_EP_DP_PP_ALG;
                flag = true;
            } else if (ppSize > prePpIndex) {
                config.algorithm = MEGATRON_LM_TP_CP_PP_EP_DP_ALG;
                flag = true;
            }
            preDpIndex = dpSize;
            prePpIndex = ppSize;
        }
    }
    if (config.dpSize == 0 && config.ppSize == 0 && config.tpSize == 0) {
        level = PARALLEL_CONFIG_LEVEL_UNDEFINED;
    } else {
        level = PARALLEL_CONFIG_LEVEL_COLLECTED;
    }
    // 索引从0开始，转化为实际大小时需要+1
    config.dpSize++;
    config.ppSize++;
    config.tpSize++;
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::HasFinishedParseLastTime() {
    return CheckValueFromStatusInfoTable(clusterParseStatus, FINISH_STATUS);
}

bool VirtualClusterDatabase::UpdatesClusterParseStatus(const std::string &status) {
    return UpdateValueIntoStatusInfoTable(clusterParseStatus, status);
}

bool VirtualClusterDatabase::ExecuteQueryAllPerformanceDataByStep(
    const std::string &sql, const std::string &step, std::unordered_map<std::uint32_t, StepStatistic> &data) {
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query all performance data by step.");
        return false;
    }
    if (!step.empty() && step != "All") {
        stmt->BindParams(step);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query all performance data by step.");
        return false;
    }
    while (resultSet->Next()) {
        StepStatistic one{};
        one.rankId = resultSet->GetString("rank");
        one.computingTime = resultSet->GetDouble("compute");
        one.pureCommunicationTime = resultSet->GetDouble("not_overlap");
        one.overlapCommunicationTime = resultSet->GetDouble("overlap");
        one.communicationTime = resultSet->GetDouble("communication");
        one.freeTime = resultSet->GetDouble("free");
        one.prepareTime = resultSet->GetDouble("preparing");
        one.pureCommunicationExcludeReceiveTime = resultSet->GetDouble("exclude_receive");
        one.npuTotalTime = one.computingTime + one.pureCommunicationTime + one.freeTime;
        one.npuTotalTime += std::max(0.0, one.prepareTime);
        uint32_t rankIdNum = StringUtil::StringToUint32(one.rankId);
        if (rankIdNum != UINT32_MAX) {
            data.emplace(rankIdNum, one);
        } else {
            ServerLog::Warn(
                "RankId % could not be converted to a valid uint32_t. This entry will be skipped.", one.rankId.c_str());
        }
    }
    return true;
}

std::vector<std::string> VirtualClusterDatabase::ExecuteGetAllRankFromStepStatisticInfo(std::string &sql) {
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query all rank from step statistic info.");
        return {};
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query all rank from step statistic info.");
        return {};
    }
    std::vector<std::string> res;
    while (resultSet->Next()) {
        res.push_back(resultSet->GetString("rankId"));
    }
    return res;
}

bool VirtualClusterDatabase::ExecuteQuerySlowOpByCommDuration(const std::string &sql,
    const Protocol::DurationListParams &params, const std::string &fastestRankId,
    Protocol::RankDetailsForSlowRank &slowRank) {
    uint64_t startTime = Module::Timeline::TraceTime::Instance().GetStartTime();
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query slow operator by communication duration.");
        return false;
    }
    stmt->BindParams(startTime, fastestRankId, params.iterationId, params.groupIdHash, startTime, slowRank.rankId,
        params.iterationId, params.groupIdHash);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result for query slow operator by communication duration.");
        return false;
    }
    int slowOpCnt = 0;
    while (resultSet->Next()) {
        slowOpCnt++;
        Protocol::OpDetailsForSlowRank opDetail;
        opDetail.diffTime =
            NumberUtil::DoubleReservedNDigits(resultSet->GetDouble("diffElapseTime"), doubleReservedNum);
        if (opDetail.diffTime <= 0 || slowOpCnt > slowRankOpCount) {
            return true;
        }
        opDetail.name = resultSet->GetString("opName");
        opDetail.startTime =
            NumberUtil::DoubleReservedNDigits(resultSet->GetDouble("startTimeOfSlow"), doubleReservedNum);
        opDetail.elapseTime =
            NumberUtil::DoubleReservedNDigits(resultSet->GetDouble("elapseTimeOfSlow"), doubleReservedNum);
        opDetail.maxElapseTime =
            NumberUtil::DoubleReservedNDigits(resultSet->GetDouble("elapseTimeOfFast"), doubleReservedNum);
        opDetail.maxStartTime =
            NumberUtil::DoubleReservedNDigits(resultSet->GetDouble("startTimeOfFast"), doubleReservedNum);
        slowRank.opDetails.push_back(opDetail);
    }
    return true;
}

std::vector<CommInfoUnderRank> VirtualClusterDatabase::ExecuteGetCommTimeForRankDim(
    std::string &sql, const std::string &step) {
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query communication time for rank dimension.");
        return {};
    }
    if (step != "" && step != "All") {
        stmt->BindParams(step);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result for query communication time for rank dimension.");
        return {};
    }
    std::vector<CommInfoUnderRank> res;
    while (resultSet->Next()) {
        CommInfoUnderRank info{};
        info.rankId = resultSet->GetString("rankId");
        info.commTime = resultSet->GetDouble("commTime");
        info.rankSet = resultSet->GetString("rankSet");
        info.groupIdHash = resultSet->GetString("groupIdHash");
        info.pgName = resultSet->GetString("pgName");
        res.push_back(info);
    }
    return res;
}

sqlite3_stmt *VirtualClusterDatabase::InitExpertHotspotInsertStmt(uint64_t paramLen) {
    if (paramLen == 0) {
        return nullptr;
    }
    std::string sql = "INSERT INTO " + TABLE_EXPERT_HOTSPOT_INTO +
        " (localExpertId, modelStage, rankId, visits, layer,"
        " version) VALUES (?,?,?,?,?,?)";
    for (size_t i = 0; i < paramLen - 1; ++i) {
        sql.append(",(?,?,?,?,?,?)");
    }
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare insert expert hotspot statement. error:", sqlite3_errmsg(db));
        return nullptr;
    }
    return stmt;
}

sqlite3_stmt *VirtualClusterDatabase::InitExpertDeploymentInsertStmt(uint64_t paramLen) {
    if (paramLen == 0) {
        return nullptr;
    }
    std::string sql = "INSERT INTO " + TABLE_EXPERT_DEPLOYMENT_INFO +
        " (modelStage, rankId, layer, expertList, "
        " version) VALUES (?,?,?,?,?)";
    for (size_t i = 0; i < paramLen - 1; ++i) {
        sql.append(",(?,?,?,?,?)");
    }
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare insert expert deployment statement. error:", sqlite3_errmsg(db));
        return nullptr;
    }
    return stmt;
}

sqlite3_stmt *VirtualClusterDatabase::GetExpertHotspotInsertStmt(uint64_t paramLen) {
    if (paramLen == CACHE_SIZE) {
        if (insertHotspotStmt == nullptr) {
            // 初始化
            insertHotspotStmt = InitExpertHotspotInsertStmt(paramLen);
        } else {
            sqlite3_reset(insertHotspotStmt);
        }
        return insertHotspotStmt;
    } else {
        sqlite3_stmt *stmt = InitExpertHotspotInsertStmt(paramLen);
        return stmt;
    }
}

sqlite3_stmt *VirtualClusterDatabase::GetExpertDeploymentInsertStmt(uint64_t paramLen) {
    if (paramLen == CACHE_SIZE) {
        if (insertDeploymentStmt == nullptr) {
            // 初始化
            insertDeploymentStmt = InitExpertDeploymentInsertStmt(paramLen);
        } else {
            sqlite3_reset(insertDeploymentStmt);
        }
        return insertDeploymentStmt;
    } else {
        sqlite3_stmt *stmt = InitExpertDeploymentInsertStmt(paramLen);
        return stmt;
    }
}

void VirtualClusterDatabase::InsertExpertHotspotDataForCache(const ExpertHotspotStruct &info) {
    expertHotspotCache.emplace_back(info);
    if (expertHotspotCache.size() == CACHE_SIZE) {
        BatchInsertExpertHotspotData(expertHotspotCache);
        expertHotspotCache.clear();
    }
}

void VirtualClusterDatabase::InsertExpertDeploymentForCache(const ExpertDeploymentStruct &info) {
    expertDeploymentCache.emplace_back(info);
    if (expertDeploymentCache.size() == CACHE_SIZE) {
        BatchInsertExpertDeployment(expertDeploymentCache);
        expertDeploymentCache.clear();
    }
}

void VirtualClusterDatabase::SaveExpertDeployment() {
    if (expertDeploymentCache.size() > 0) {
        BatchInsertExpertDeployment(expertDeploymentCache);
        expertDeploymentCache.clear();
    }
}

void VirtualClusterDatabase::SaveExpertHotspot() {
    if (expertHotspotCache.size() > 0) {
        BatchInsertExpertHotspotData(expertHotspotCache);
        expertHotspotCache.clear();
    }
}

bool VirtualClusterDatabase::BatchInsertExpertHotspotData(const std::vector<ExpertHotspotStruct> &expertHotspotInfos) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    sqlite3_stmt *stmt = GetExpertHotspotInsertStmt(expertHotspotInfos.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get insert expert hotspot statement.");
        return false;
    }
    int idx = bindStartIndex;
    for (const auto &info : expertHotspotInfos) {
        sqlite3_bind_int(stmt, idx++, info.localExpertId);
        sqlite3_bind_text(stmt, idx++, info.modelStage.c_str(), info.modelStage.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, idx++, info.rankId);
        sqlite3_bind_int64(stmt, idx++, info.visits);
        sqlite3_bind_int(stmt, idx++, info.layer);
        sqlite3_bind_text(stmt, idx++, info.version.c_str(), info.version.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    if (expertHotspotInfos.size() != CACHE_SIZE) {
        sqlite3_finalize(stmt);
    }
    return result == SQLITE_DONE;
}

bool VirtualClusterDatabase::BatchInsertExpertDeployment(
    const std::vector<ExpertDeploymentStruct> &expertDeploymentInfos) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    sqlite3_stmt *stmt = GetExpertDeploymentInsertStmt(expertDeploymentInfos.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get insert expert deployment statement.");
        return false;
    }
    int idx = bindStartIndex;
    for (const auto &info : expertDeploymentInfos) {
        sqlite3_bind_text(stmt, idx++, info.modelStage.c_str(), info.modelStage.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, idx++, info.deviceId);
        sqlite3_bind_int(stmt, idx++, info.layer);
        std::string expertListStr = StringUtil::join(info.expertList, ",");
        sqlite3_bind_text(stmt, idx++, expertListStr.c_str(), expertListStr.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, info.version.c_str(), info.version.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    if (expertDeploymentInfos.size() != CACHE_SIZE) {
        sqlite3_finalize(stmt);
    }
    return result == SQLITE_DONE;
}

bool VirtualClusterDatabase::DeleteExpertHotspot(const std::string &modelStage, const std::string &version) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string sql = "DELETE FROM " + TABLE_EXPERT_HOTSPOT_INTO + " WHERE 1 = 1";
    if (!modelStage.empty()) {
        sql += " AND modelStage = ? ";
    }
    if (!version.empty()) {
        sql += " AND version = ? ";
    }
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare delete expert hotspot statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int idx = bindStartIndex;
    if (!modelStage.empty()) {
        sqlite3_bind_text(stmt, idx++, modelStage.c_str(), modelStage.length(), SQLITE_TRANSIENT);
    }
    if (!version.empty()) {
        sqlite3_bind_text(stmt, idx++, version.c_str(), version.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

bool VirtualClusterDatabase::DeleteDeployment(const std::string &modelStage, const std::string &version) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    std::string sql = "DELETE FROM " + TABLE_EXPERT_DEPLOYMENT_INFO + " WHERE 1 = 1";
    if (!modelStage.empty()) {
        sql += " AND modelStage = ? ";
    }
    if (!version.empty()) {
        sql += " AND version = ? ";
    }
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare delete expert hotspot statement. error:", sqlite3_errmsg(db));
        return false;
    }
    int idx = bindStartIndex;
    if (!modelStage.empty()) {
        sqlite3_bind_text(stmt, idx++, modelStage.c_str(), modelStage.length(), SQLITE_TRANSIENT);
    }
    if (!version.empty()) {
        sqlite3_bind_text(stmt, idx++, version.c_str(), version.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return result == SQLITE_DONE;
}

std::vector<ExpertHotspotStruct> VirtualClusterDatabase::QueryExpertHotspotData(
    const std::string &modelStage, const std::string &version) {
    std::string sql = "SELECT localExpertId, modelStage, rankId, visits, version, layer FROM " +
        TABLE_EXPERT_HOTSPOT_INTO + " WHERE modelStage = ? and version = ?";
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare query expert hotspot statement. error:", sqlite3_errmsg(db));
        return {};
    }
    int idx = bindStartIndex;
    sqlite3_bind_text(stmt, idx++, modelStage.c_str(), modelStage.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, version.c_str(), version.length(), SQLITE_TRANSIENT);
    std::vector<ExpertHotspotStruct> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        ExpertHotspotStruct info{};
        info.localExpertId = sqlite3_column_int(stmt, col++);
        info.modelStage = sqlite3_column_string(stmt, col++);
        info.rankId = sqlite3_column_int(stmt, col++);
        int64_t visits = sqlite3_column_int64(stmt, col++);
        info.visits = static_cast<uint64_t>(visits > 0 ? visits : 0);
        info.version = sqlite3_column_string(stmt, col++);
        info.layer = sqlite3_column_int(stmt, col++);
        res.emplace_back(info);
    }
    sqlite3_finalize(stmt);
    return res;
}

std::vector<ExpertDeploymentStruct> VirtualClusterDatabase::QueryExpertDeployment(
    const std::string &modelStage, const std::string &version) {
    std::string sql = "SELECT modelStage, rankId, layer, expertList, version FROM " + TABLE_EXPERT_DEPLOYMENT_INFO +
        " WHERE modelStage = ? and version = ?";
    sqlite3_stmt *stmt = nullptr;
    int stmtResult = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (stmtResult != SQLITE_OK || stmt == nullptr) {
        ServerLog::Error("Failed to prepare query expert hotspot statement. error:", sqlite3_errmsg(db));
        return {};
    }
    int idx = bindStartIndex;
    sqlite3_bind_text(stmt, idx++, modelStage.c_str(), modelStage.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, version.c_str(), version.length(), SQLITE_TRANSIENT);
    std::vector<ExpertDeploymentStruct> res;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        ExpertDeploymentStruct info{};
        info.modelStage = sqlite3_column_string(stmt, col++);
        info.deviceId = sqlite3_column_int(stmt, col++);
        info.layer = sqlite3_column_int(stmt, col++);
        std::string expertListStr = sqlite3_column_string(stmt, col++);
        if (!expertListStr.empty()) {
            for (const auto &item : StringUtil::Split(expertListStr, ",")) {
                info.expertList.push_back(StringUtil::StringToInt(item));
            }
        }
        info.version = sqlite3_column_string(stmt, col++);
        res.emplace_back(info);
    }
    sqlite3_finalize(stmt);
    return res;
}

void VirtualClusterDatabase::ReleaseStmt() {
    if (insertHotspotStmt != nullptr) {
        sqlite3_finalize(insertHotspotStmt);
        insertHotspotStmt = nullptr;
    }
}

bool VirtualClusterDatabase::ExecuteQueryPacketAnalyzerData(
    std::vector<PacketAnalyzerData> &data, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query packet analyzer. Error:", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        PacketAnalyzerData singleOperator;
        singleOperator.type = sqlite3_column_string(stmt, col++);
        singleOperator.transitSize = sqlite3_column_double(stmt, col++);
        singleOperator.transitTime = sqlite3_column_double(stmt, col++);
        data.emplace_back(singleOperator);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryBandwidthContentionAnalyzerData(
    std::vector<BandwidthContentionSDMAInfo> &res, const std::string &rankId, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query bandwidth contention analyzer."
                         " Error:",
            sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, rankId.c_str(), rankId.length(), SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        BandwidthContentionSDMAInfo info;
        info.name = sqlite3_column_string(stmt, col++);
        info.startTime = sqlite3_column_double(stmt, col++);
        info.duration = sqlite3_column_double(stmt, col++);
        info.bandwidth = sqlite3_column_double(stmt, col++);
        res.emplace_back(info);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteQueryRetransmissionAnalyzerData(
    std::vector<RetransmissionClassificationInfo> &data, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare sql for query retransmission analyzer detail data. Error:", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RetransmissionClassificationInfo info;
        int col = resultStartIndex;
        info.iterationId = sqlite3_column_string(stmt, col++);
        info.groupId = sqlite3_column_string(stmt, col++);
        info.opName = sqlite3_column_string(stmt, col++);
        info.minElapseTime = sqlite3_column_double(stmt, col++);
        info.maxRDMATransitTime = sqlite3_column_double(stmt, col++);
        data.emplace_back(info);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualClusterDatabase::ExecuteUpdateCollectTimeInfo(
    const Protocol::SummaryBaseInfo &baseInfo, const std::string &sql) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare sql for update collect time for cluster base info. Error:", sqlite3_errmsg(db));
        return false;
    }
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get stmt for update collect time for cluster base info.");
        return false;
    }
    std::string valueCollectStartTime = std::to_string(baseInfo.collectStartTime);
    std::string valueCollectDuration = std::to_string(baseInfo.collectDuration);
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, valueCollectStartTime.c_str(), valueCollectStartTime.length(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, index++, valueCollectDuration.c_str(), valueCollectDuration.length(), SQLITE_TRANSIENT);
    auto result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        ServerLog::Error("Fail to update collect time for cluster base info. error: ", sqlite3_errmsg(db));
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

std::string VirtualClusterDatabase::GenerateReplaceSql(
    const std::string &columnName, const std::vector<std::string> &replaceList) {
    if (replaceList.empty()) {
        return columnName;
    }
    std::string res = columnName;
    for (const auto &item : replaceList) {
        res = StringUtil::FormatString("REPLACE({}, '{}', '')", res, item);
    }
    return res;
}

std::string VirtualClusterDatabase::NormalizeOpName(const std::string &rawName) const {
    if (rawName.empty()) {
        return "";
    }

    constexpr int ASCII_SIZE = 256;
    // 静态查找表（只初始化一次）
    static const std::array<bool, ASCII_SIZE> shouldRemove = [this]() {
        std::array<bool, ASCII_SIZE> table = {false};
        for (const auto &str : replaceCharList) {
            if (!str.empty()) {
                const auto c = static_cast<unsigned char>(str[0]);
                table[c] = true;
            }
        }
        return table;
    }();

    std::string result;
    result.reserve(rawName.length());

    for (const char c : rawName) {
        if (!shouldRemove[static_cast<unsigned char>(c)]) {
            result.push_back(c);
        }
    }

    return result;
}

std::vector<OpTypeStatistics> VirtualClusterDatabase::ExecuteGetOpStatByStepId(
    const std::string &stepId, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for get op stat by step id. Error:", sqlite3_errmsg(db));
        return {};
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, stepId.c_str(), stepId.length(), SQLITE_TRANSIENT);
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> aggMap;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        auto opType = NormalizeOpName(sqlite3_column_string(stmt, col++));
        auto groupIdHash = sqlite3_column_string(stmt, col++);
        aggMap[opType][groupIdHash]++;
    }
    sqlite3_finalize(stmt);

    std::vector<OpTypeStatistics> res;
    res.reserve(aggMap.size() * 2);

    for (auto &[opType, groupMap] : aggMap) {
        for (auto &[groupName, count] : groupMap) {
            res.push_back(OpTypeStatistics{count, opType, groupName});
        }
    }
    std::sort(res.begin(), res.end(),
        [](const OpTypeStatistics &a, const OpTypeStatistics &b) { return a.opType < b.opType; });
    return res;
}
}
}
