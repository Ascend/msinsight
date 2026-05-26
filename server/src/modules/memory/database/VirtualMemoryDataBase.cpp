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
#include "TraceTime.h"
#include "DataBaseManager.h"
#include "VirtualMemoryDataBase.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Server;
using namespace Dic::Module::Timeline;

std::vector<std::string> VirtualMemoryDataBase::GetStreamLists(std::string deviceId, std::string deviceIdColumnName) {
    std::vector<std::string> streams = {};
    DataType type = DataBaseManager::Instance().GetDataType(path);
    std::string sql = "";
    if (type == DataType::TEXT) {
        sql += "SELECT stream FROM " + recordTable +
            " WHERE deviceId = ? AND stream <> '' "
            "Group BY stream ORDER BY timestamp ASC";
    } else if (type == DataType::DB) {
        FileType fileType = DataBaseManager::Instance().GetFileType(path);
        if (fileType == FileType::PYTORCH) {
            std::string streamPtrColumnName = "streamPtr";
            std::string timeColumnName = "timestamp";
            sql += "SELECT " + streamPtrColumnName + " FROM " + TABLE_MEMORY_RECORD + " WHERE " + deviceIdColumnName +
                " = ? AND " + streamPtrColumnName +
                " <> ''"
                " Group BY " +
                streamPtrColumnName + " ORDER BY " + timeColumnName + " ASC";
        } else {
            ServerLog::Error("Memory tab does not support msprof data.");
            return streams;
        }
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Get stream lists. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return streams;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index++, deviceId.c_str(), deviceId.length(), nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        streams.emplace_back(sqlite3_column_string(stmt, col++));
    }
    sqlite3_finalize(stmt);
    return streams;
}

bool VirtualMemoryDataBase::ExecuteMemoryType(std::vector<std::string> &graphId, std::string &type) {
    if (!Database::CheckTableContainData(TABLE_STATIC_OPERATOR)) {
        type = Module::Memory::MEMORY_TYPE_DYNAMIC;
        return true;
    }
    type = Module::Memory::MEMORY_TYPE_STATIC;
    std::string sql = StringUtil::FormatString(
        "SELECT DISTINCT {} as graphId FROM {};", StaticOpColumn::GRAPH_ID, TABLE_STATIC_OPERATOR);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query memory type. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string res = sqlite3_column_string(stmt, col++);
        graphId.emplace_back(res);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteMemoryResourceType(std::string &type, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query memory resource type. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int64_t totalNum = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        totalNum = sqlite3_column_int(stmt, resultStartIndex);
    }
    type = (totalNum > 0) ? MEMORY_RESOURCE_TYPE_MIND_SPORE : MEMORY_RESOURCE_TYPE_PYTORCH;
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteOperatorSize(
    Protocol::MemoryOperatorSizeParams &requestParams, double &min, double &max, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query operator size. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        min = sqlite3_column_double(stmt, col++);
        max = sqlite3_column_double(stmt, col++);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticOperatorSize(
    Protocol::StaticOperatorSizeParams &requestParams, double &min, double &max, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query static operator size. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    if (!requestParams.graphId.empty()) {
        sqlite3_bind_text(stmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        min = sqlite3_column_double(stmt, col++);
        max = sqlite3_column_double(stmt, col++);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteComponentTotalNum(
    Protocol::MemoryComponentParams &requestParams, int64_t &totalNum, std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query components total num. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }

    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        totalNum = sqlite3_column_int(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticOperatorListTotalNum(
    Protocol::StaticOperatorListParams &requestParams, int64_t &totalNum, std::string sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query static operators total num. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    std::string searchName = "%" + requestParams.searchName + "%";
    sqlite3_bind_text(stmt, index++, searchName.c_str(), searchName.length(), nullptr);
    if (!requestParams.graphId.empty()) {
        sqlite3_bind_text(stmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    }
    if (requestParams.minSize != std::numeric_limits<int64_t>::min()) {
        sqlite3_bind_int64(stmt, index++, requestParams.minSize);
    }
    if (requestParams.maxSize != std::numeric_limits<int64_t>::max()) {
        sqlite3_bind_int64(stmt, index++, requestParams.maxSize);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        totalNum = sqlite3_column_int(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteQueryMemoryViewExecuteSql(Protocol::MemoryViewParams &requestParams,
    std::vector<Protocol::ComponentDto> &componentDtoVec, std::vector<std::string> &streams, std::string &sql,
    std::string deviceIdColumnName) {
    sql = GetCurveSql(requestParams, sql);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query memory view. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }
    std::string peakMemory;
    bool onlyGe = true;
    componentDtoVec.reserve(2000000);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::ComponentDto componentDto{};
        FastGetString(stmt, col++, componentDto.component);
        componentDto.timesTamp = sqlite3_column_double(stmt, col++);
        componentDto.totalAllocated = sqlite3_column_double(stmt, col++);
        componentDto.totalReserved = sqlite3_column_double(stmt, col++);
        componentDto.totalActivated = sqlite3_column_double(stmt, col++);
        FastGetString(stmt, col++, componentDto.streamId);
        if (std::find(streams.begin(), streams.end(), componentDto.streamId) == streams.end()) {
            streams.emplace_back(componentDto.streamId);
        }
        if (onlyGe && componentDto.component != COMPONENT_GE) {
            onlyGe = false;
        }
        componentDtoVec.push_back(std::move(componentDto));
    }
    streams.erase(
        remove_if(streams.begin(), streams.end(), [](const std::string &s) { return s.empty(); }), streams.end());
    sqlite3_finalize(stmt);
    if (onlyGe) {
        isInference = true;
    }
    return true;
}

std::string VirtualMemoryDataBase::GetCurveSql(const MemoryViewParams &requestParams, std::string &sql) const {
    if (requestParams.type == MEMORY_OVERALL_GROUP) {
        sql += " AND component != '" + COMPONENT_PTA + "' ";
    }
    if (requestParams.type == MEMORY_STREAM_GROUP) {
        sql += " AND stream <> '' AND component = '" + COMPONENT_PTA_AND_GE + "'";
    }
    sql += " ORDER BY timestamp ASC";
    return sql;
}

bool VirtualMemoryDataBase::ExecuteQueryMemoryViewGetGraph(Protocol::MemoryViewParams &requestParams,
    std::vector<Protocol::ComponentDto> &componentDtoVec, std::vector<std::string> &streams,
    Protocol::MemoryViewData &operatorBody) {
    Protocol::MemoryPeak peak;
    if (requestParams.type == Protocol::MEMORY_OVERALL_GROUP) {
        GetOverallLines(componentDtoVec, operatorBody.tempData, operatorBody.legends, peak, streams);
        operatorBody.title = GetPeakMemory(peak, streams);
    } else if (requestParams.type == Protocol::MEMORY_STREAM_GROUP) {
        GetStreamLines(componentDtoVec, operatorBody.tempData, operatorBody.legends, peak, streams);
    } else {
        GetComponentLines(componentDtoVec, operatorBody.tempData, operatorBody.legends, peak, streams);
        operatorBody.title = GetPeakMemory(peak, streams);
    }
    return true;
}

int64_t VirtualMemoryDataBase::ExecuteOperatorDetail(
    Protocol::MemoryOperatorParams &requestParams, std::vector<Protocol::MemoryOperator> &opDetails, std::string &sql) {
    int64_t pageSize = requestParams.pageSize <= 0 ? defaultPageSize : requestParams.pageSize;
    int64_t currentPage = requestParams.currentPage - 1;
    currentPage = currentPage < 0 ? 0 : currentPage;
    if (pageSize > maxPageSize || currentPage > maxCurrentPage) {
        ServerLog::Error("Error param: pageSize or currentPage");
        return -1;
    }
    int64_t offset = currentPage * pageSize;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query operator detail. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return -1;
    }
    // 此处index不会自增溢出回绕，因为无论是filters、rangeFilters已在前序从json获取时判断了均在枚举列中，不可能超出列总数14
    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }
    // 绑定filters参数
    SqlBindQueryFilters(stmt, index, requestParams);
    // 绑定rangeFilters参数
    SqlBindQueryRangeFilters(stmt, index, requestParams);
    // 绑定分页参数
    sqlite3_bind_int64(stmt, index++, pageSize);
    sqlite3_bind_int64(stmt, index++, offset);
    return QueryOperatorDetailByStepWithCount(stmt, opDetails);
}

int64_t VirtualMemoryDataBase::QueryOperatorDetailByStepWithCount(
    sqlite3_stmt *stmt, std::vector<Protocol::MemoryOperator> &operators) {
    int64_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        count = sqlite3_column_int64(stmt, col++);
        Protocol::MemoryOperator operatorDto{};
        operatorDto.id = std::to_string(sqlite3_column_int64(stmt, col++));
        operatorDto.name = sqlite3_column_string(stmt, col++);
        operatorDto.size = sqlite3_column_double(stmt, col++);
        operatorDto.allocationTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.releaseTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.activeReleaseTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.duration = sqlite3_column_double(stmt, col++);
        operatorDto.activeDuration = sqlite3_column_double(stmt, col++);
        operatorDto.allocationAllocated = sqlite3_column_double(stmt, col++);
        operatorDto.allocationReserved = sqlite3_column_double(stmt, col++);
        operatorDto.allocationActive = sqlite3_column_double(stmt, col++);
        operatorDto.releaseAllocated = sqlite3_column_double(stmt, col++);
        operatorDto.releaseReserved = sqlite3_column_double(stmt, col++);
        operatorDto.releaseActive = sqlite3_column_double(stmt, col++);
        operatorDto.streamId = sqlite3_column_string(stmt, col++);
        operators.emplace_back(operatorDto);
    }
    sqlite3_finalize(stmt);
    return count;
}

bool VirtualMemoryDataBase::ExecuteQueryEntireOperatorTable(Protocol::MemoryOperatorParams &requestParams,
    std::vector<Protocol::MemoryOperator> &opDetails, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query entire operator table. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }

    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }

    std::vector<Protocol::MemoryOperator> &operatorDtoVec = opDetails;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::MemoryOperator operatorDto{};
        sqlite3_column_int64(stmt, col++);
        operatorDto.id = std::to_string(sqlite3_column_int64(stmt, col++));
        operatorDto.name = sqlite3_column_string(stmt, col++);
        operatorDto.size = sqlite3_column_double(stmt, col++);
        operatorDto.allocationTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.releaseTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.activeReleaseTime = ConvertTimestampStr(sqlite3_column_string(stmt, col++));
        operatorDto.duration = sqlite3_column_double(stmt, col++);
        operatorDto.activeDuration = sqlite3_column_double(stmt, col++);
        operatorDto.allocationAllocated = sqlite3_column_double(stmt, col++);
        operatorDto.allocationReserved = sqlite3_column_double(stmt, col++);
        operatorDto.allocationActive = sqlite3_column_double(stmt, col++);
        operatorDto.releaseAllocated = sqlite3_column_double(stmt, col++);
        operatorDto.releaseReserved = sqlite3_column_double(stmt, col++);
        operatorDto.releaseActive = sqlite3_column_double(stmt, col++);
        operatorDto.streamId = sqlite3_column_string(stmt, col++);
        operatorDtoVec.emplace_back(operatorDto);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteComponentDetail(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryTableColumnAttr> &columnAttr, std::vector<Protocol::MemoryComponent> &componentDetails,
    std::string &sql) {
    int64_t pageSize = requestParams.pageSize == 0 ? defaultPageSize : requestParams.pageSize;
    int64_t currentPage = requestParams.currentPage - 1;
    currentPage = currentPage < 0 ? 0 : currentPage;
    if (pageSize > maxPageSize || currentPage > maxCurrentPage) {
        ServerLog::Error("Error param: pageSize or currentPage");
        return false;
    }
    int64_t offset = currentPage * pageSize;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query component detail. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        sqlite3_bind_int64(stmt, index++, StringUtil::StringToInt(requestParams.deviceId));
    }
    sqlite3_bind_int64(stmt, index++, requestParams.pageSize);
    sqlite3_bind_int64(stmt, index++, offset);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::MemoryComponent componentElement;
        componentElement.component = sqlite3_column_string(stmt, col++);
        componentElement.totalReserved = sqlite3_column_double(stmt, col++);
        componentElement.timestamp = sqlite3_column_string(stmt, col++);
        componentDetails.emplace_back(componentElement);
    }
    sqlite3_finalize(stmt);
    for (const auto &column : componentTableColumnAttr) {
        columnAttr.emplace_back(column);
    }
    return true;
}

bool VirtualMemoryDataBase::ExecuteQueryEntireComponentTable(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryComponent> &componentDetails, std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query entire component table. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }

    int index = bindStartIndex;
    if (Timeline::DataBaseManager::Instance().GetDataType(path) == DataType::TEXT) {
        sqlite3_bind_text(stmt, index++, requestParams.deviceId.c_str(), requestParams.deviceId.length(), nullptr);
    } else {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        sqlite3_bind_int64(stmt, index++, deviceId);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::MemoryComponent componentElement;
        componentElement.component = sqlite3_column_string(stmt, col++);
        componentElement.totalReserved = sqlite3_column_double(stmt, col++);
        componentElement.timestamp = sqlite3_column_string(stmt, col++);
        componentDetails.emplace_back(componentElement);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticGraphTotalSize(
    Protocol::StaticOperatorGraphParams &requestParams, const std::string &totalSql, double &totalSize) {
    sqlite3_stmt *totalStmt = nullptr;
    int totalResult = sqlite3_prepare_v2(db, totalSql.c_str(), -1, &totalStmt, nullptr);
    if (totalResult != SQLITE_OK) {
        ServerLog::Error("Query static graph total size. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(totalStmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    if (!requestParams.modelName.empty()) {
        std::string modelName = "%" + requestParams.modelName + "%";
        sqlite3_bind_text(totalStmt, index++, modelName.c_str(), modelName.length(), nullptr);
    }
    if (sqlite3_step(totalStmt) == SQLITE_ROW) {
        totalSize = sqlite3_column_double(totalStmt, resultStartIndex);
    }
    sqlite3_finalize(totalStmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticGraphStartIndex(Protocol::StaticOperatorGraphParams &requestParams,
    const std::string &graphStartSql, std::map<int64_t, double> &graphSizeMap, int64_t &maxIndex) {
    sqlite3_stmt *startStmt = nullptr;
    int graphStartResult = sqlite3_prepare_v2(db, graphStartSql.c_str(), -1, &startStmt, nullptr);
    if (graphStartResult != SQLITE_OK) {
        ServerLog::Error("Query static graph start index. Failed to prepare sql. Error:", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(startStmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    if (!requestParams.modelName.empty()) {
        std::string modelName = "%" + requestParams.modelName + "%";
        sqlite3_bind_text(startStmt, index, modelName.c_str(), modelName.length(), nullptr);
    }
    while (sqlite3_step(startStmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t nodeIndex = sqlite3_column_int64(startStmt, col++);
        double size = sqlite3_column_double(startStmt, col++);
        if (graphSizeMap.find(nodeIndex) != graphSizeMap.end()) {
            graphSizeMap[nodeIndex] += size;
        } else {
            graphSizeMap.insert({nodeIndex, size});
        }
        if (nodeIndex >= maxUnsignedInt) {
            maxIndex = maxUnsignedInt;
        } else if (nodeIndex > maxIndex) {
            maxIndex = nodeIndex;
        }
    }
    sqlite3_finalize(startStmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticGraphEndIndex(Protocol::StaticOperatorGraphParams &requestParams,
    const std::string &graphEndSql, std::map<int64_t, double> &graphSizeMap, int64_t &maxIndex) {
    sqlite3_stmt *endStmt = nullptr;
    int graphEndResult = sqlite3_prepare_v2(db, graphEndSql.c_str(), -1, &endStmt, nullptr);
    if (graphEndResult != SQLITE_OK) {
        ServerLog::Error("Query static graph end index. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(endStmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    if (!requestParams.modelName.empty()) {
        std::string modelName = "%" + requestParams.modelName + "%";
        sqlite3_bind_text(endStmt, index, modelName.c_str(), modelName.length(), nullptr);
    }
    while (sqlite3_step(endStmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t nodeIndex = sqlite3_column_int64(endStmt, col++);
        double size = sqlite3_column_double(endStmt, col++);
        if (nodeIndex >= maxUnsignedInt) {
            maxIndex = maxUnsignedInt;
        } else if (nodeIndex > maxIndex) {
            maxIndex = nodeIndex;
        }
        if (graphSizeMap.find(nodeIndex) != graphSizeMap.end()) {
            graphSizeMap[nodeIndex] -= size;
        } else {
            graphSizeMap.insert({nodeIndex, -size});
        }
    }
    sqlite3_finalize(endStmt);
    return true;
}

bool VirtualMemoryDataBase::ExecuteStaticOperatorGraph(Protocol::StaticOperatorGraphParams &requestParams,
    Protocol::StaticOperatorGraphItem &graphItem, const std::string &totalSql, const std::string &graphStartSql,
    const std::string &graphEndSql) {
    double totalSize = staticDefaultTotalSize;
    if (!ExecuteStaticGraphTotalSize(requestParams, totalSql, totalSize)) {
        return false;
    }
    if (NumberUtil::IsDoubleEqual(totalSize, staticDefaultTotalSize)) {
        ServerLog::Error("Query static operator graph. Failed get TOTAL number. Error: ", sqlite3_errmsg(db));
        return false;
    }
    std::map<int64_t, double> graphSizeMap;
    int64_t maxIndex = 0;
    if (!ExecuteStaticGraphStartIndex(requestParams, graphStartSql, graphSizeMap, maxIndex)) {
        return false;
    }
    if (!ExecuteStaticGraphEndIndex(requestParams, graphEndSql, graphSizeMap, maxIndex)) {
        return false;
    }
    if (graphSizeMap.empty()) {
        ServerLog::Info("Query static operator graph. No data.");
        return false;
    }
    // 组装图例和图像数据
    graphItem.legends.insert(graphItem.legends.end(), staticGraphLegends.begin(), staticGraphLegends.end());
    double size = 0;
    std::string totalSizeStr =
        StringUtil::DoubleToStringWithTwoDecimalPlaces(totalSize / kbSizeDouble); // 结果保留两位有效数字
    for (auto it = graphSizeMap.begin(); it != graphSizeMap.end(); ++it) {
        std::vector<std::string> points = {};
        size += it->second; // 遍历有序map，逐点计算总需分配内存
        if (it->first < maxUnsignedInt) {
            points.emplace_back(std::to_string(it->first)); // 正常的Node Index
        } else {
            points.emplace_back(std::to_string(maxIndex + 1)); // 存储值为maxUnsignedInt时，Node Index = maxIndex + 1
        }
        points.emplace_back(
            StringUtil::DoubleToStringWithTwoDecimalPlaces(size / kbSizeDouble)); // Size，结果保留两位有效数字
        points.emplace_back(totalSizeStr); // Total Size
        graphItem.lines.emplace_back(points);
    }

    return true;
}

int64_t VirtualMemoryDataBase::ExecuteStaticOperatorDetail(Protocol::StaticOperatorListParams &requestParams,
    std::vector<Protocol::StaticOperatorItem> &opDetails, const std::string &sql) {
    int64_t pageSize = requestParams.pageSize;
    if (pageSize == 0) {
        pageSize = defaultPageSize;
    }
    int64_t currentPage = requestParams.currentPage - 1;
    if (currentPage < 0) {
        currentPage = 0;
    }
    int64_t offset = currentPage * pageSize;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query static operator detail. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return -1;
    }
    int index = bindStartIndex;
    std::string searchName = "%" + requestParams.searchName + "%";
    sqlite3_bind_text(stmt, index++, searchName.c_str(), searchName.length(), nullptr);
    if (!requestParams.graphId.empty()) {
        sqlite3_bind_text(stmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    }
    sqlite3_bind_int64(stmt, index++, pageSize);
    sqlite3_bind_int64(stmt, index++, offset);
    std::vector<Protocol::StaticOperatorItem> operatorDtoVec;
    int64_t totalNum = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::StaticOperatorItem operatorDto{};
        totalNum = sqlite3_column_int64(stmt, col++);
        operatorDto.deviceId = sqlite3_column_string(stmt, col++);
        operatorDto.opName = sqlite3_column_string(stmt, col++);
        operatorDto.nodeIndexStart = sqlite3_column_int64(stmt, col++);
        operatorDto.nodeIndexEnd = sqlite3_column_int64(stmt, col++);
        operatorDto.size = sqlite3_column_double(stmt, col++);
        operatorDtoVec.emplace_back(operatorDto);
    }
    sqlite3_finalize(stmt);
    opDetails = operatorDtoVec;
    return totalNum;
}

bool VirtualMemoryDataBase::ExecuteQueryEntireStaticOperatorTable(Protocol::StaticOperatorListParams &requestParams,
    std::vector<Protocol::StaticOperatorItem> &opDetails, const std::string &sql) {
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query entire static operator table. Failed to prepare sql. Error: ", sqlite3_errmsg(db));
        return false;
    }
    int index = bindStartIndex;
    if (!requestParams.graphId.empty()) {
        sqlite3_bind_text(stmt, index++, requestParams.graphId.c_str(), requestParams.graphId.length(), nullptr);
    }
    std::vector<Protocol::StaticOperatorItem> operatorDtoVec;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        Protocol::StaticOperatorItem operatorDto{};
        operatorDto.deviceId = sqlite3_column_string(stmt, col++);
        operatorDto.opName = sqlite3_column_string(stmt, col++);
        operatorDto.nodeIndexStart = sqlite3_column_int64(stmt, col++);
        operatorDto.nodeIndexEnd = sqlite3_column_int64(stmt, col++);
        operatorDto.size = sqlite3_column_double(stmt, col++);
        operatorDtoVec.emplace_back(operatorDto);
    }
    sqlite3_finalize(stmt);
    opDetails = operatorDtoVec;
    return true;
}

std::string VirtualMemoryDataBase::BuildQueryOperatorMemoryTimeCondition(const MemoryOperatorParams &requestParams) {
    std::string timeCondition;
    if (requestParams.startTime != -1 && requestParams.endTime != -1) {
        std::string startTimeStr = std::to_string(requestParams.startTime);
        std::string endTimeStr = std::to_string(requestParams.endTime);
        // 仅展示在区间内申请或释放的算子
        if (requestParams.isOnlyShowAllocatedOrReleasedWithinInterval) {
            timeCondition = StringUtil::FormatString(" AND (_{} BETWEEN {} AND {} OR _{} BETWEEN {} AND {}) ",
                OpMemoryColumn::ALLOCATION_TIME, startTimeStr, endTimeStr, OpMemoryColumn::RELEASE_TIME, startTimeStr,
                endTimeStr);
        } else { // 所有与区间有交集的算子; 注意判空需要用原列而不是别名列
            timeCondition = StringUtil::FormatString(" AND (({} IS NULL OR {} = 0 OR _{} >= {}) AND _{} <= {}) ",
                OpMemoryColumn::RELEASE_TIME, OpMemoryColumn::RELEASE_TIME, OpMemoryColumn::RELEASE_TIME, startTimeStr,
                OpMemoryColumn::ALLOCATION_TIME, endTimeStr);
        }
    }
    return timeCondition;
}

std::string VirtualMemoryDataBase::BuildQueryFiltersCondition(const FiltersParam &requestParams) {
    std::string filtersCondition;
    for (auto &filterPair : requestParams.filters) {
        filtersCondition.append(StringUtil::FormatString(" AND _{} LIKE ? ", filterPair.first));
    }
    return filtersCondition;
}

std::string VirtualMemoryDataBase::BuildQueryRangeFiltersCondition(const RangeFiltersParam &requestParams) {
    std::string rangeFiltersCondition;
    for (const auto &[colName, rangePair] : requestParams.rangeFilters) {
        (void)(rangePair);
        rangeFiltersCondition.append(StringUtil::FormatString(" AND (_{} BETWEEN ? AND ?) ", colName));
    }
    return rangeFiltersCondition;
}

std::string VirtualMemoryDataBase::BuildQueryOrderByCondition(const OrderByParam &orderParam) {
    if (orderParam.orderBy.empty()) {
        return "";
    }
    std::string order = orderParam.desc ? "DESC" : "ASC";
    // 释放时间戳参数, 需要基于原始列而不是带_的别名列, 为NULL或0实际含义为无限大
    if (orderParam.orderBy == OpMemoryColumn::RELEASE_TIME ||
        orderParam.orderBy == OpMemoryColumn::ACTIVE_RELEASE_TIME) {
        return StringUtil::FormatString(" ORDER BY CASE WHEN {} IS NULL OR {} = 0 THEN 1 ELSE 0 END {}, _{} {} ",
            orderParam.orderBy, orderParam.orderBy, order, orderParam.orderBy, order);
    }
    // 申请时间戳参数, 需要基于原始列而不是带_的别名列, 为NULL或0实际含义为无限小
    if (orderParam.orderBy == OpMemoryColumn::ALLOCATION_TIME) {
        return StringUtil::FormatString(" ORDER BY CASE WHEN {} IS NULL OR {} = 0 THEN 0 ELSE 1 END {}, _{} {} ",
            orderParam.orderBy, orderParam.orderBy, order, orderParam.orderBy, order);
    }
    // 其余字段正常排序
    return StringUtil::FormatString(" ORDER BY _{} {} ", orderParam.orderBy, order);
}

void VirtualMemoryDataBase::SqlBindQueryFilters(sqlite3_stmt *stmt, int &bindIndex, const FiltersParam &params) {
    if (stmt == nullptr) {
        ServerLog::Error("Failed to bind query filters param, empty stmt");
        return;
    }
    if (params.filters.size() > INT_MAX || bindIndex > static_cast<int>(INT_MAX - params.filters.size())) {
        ServerLog::Error("Failed to bind query filters param, over limit.");
        return;
    }
    for (auto &filterPair : params.filters) {
        std::string filterPattern = StringUtil::FormatString("%{}%", filterPair.second);
        sqlite3_bind_text(stmt, bindIndex++, filterPattern.c_str(), filterPattern.length(), SQLITE_TRANSIENT);
    }
}
void VirtualMemoryDataBase::SqlBindQueryRangeFilters(
    sqlite3_stmt *stmt, int &bindIndex, const RangeFiltersParam &params) {
    if (stmt == nullptr) {
        ServerLog::Error("Failed to bind query range filters param, empty stmt");
        return;
    }
    if (params.rangeFilters.size() > INT_MAX || bindIndex > static_cast<int>(INT_MAX - params.rangeFilters.size())) {
        ServerLog::Error("Failed to bind query range filters param, over limit.");
        return;
    }
    for (const auto &[colName, rangePair] : params.rangeFilters) {
        (void)(colName);
        sqlite3_bind_double(stmt, bindIndex++, rangePair.first);
        sqlite3_bind_double(stmt, bindIndex++, rangePair.second);
    }
}

void VirtualMemoryDataBase::AddOperatorSql(Protocol::MemoryOperatorParams requestParams, std::string &sql) {
    if (requestParams.type == Protocol::MEMORY_STREAM_GROUP) {
        // Stream不为空
        sql = StringUtil::StrJoin(sql, StringUtil::FormatString(" AND _{} <> '' ", OpMemoryColumn::STREAM));
    }
    std::string timeCondition = BuildQueryOperatorMemoryTimeCondition(requestParams);
    std::string filtersCondition = BuildQueryFiltersCondition(requestParams);
    std::string rangeFiltersCondition = BuildQueryRangeFiltersCondition(requestParams);
    std::string orderByCondition = BuildQueryOrderByCondition(requestParams);
    sql = StringUtil::StrJoin(
        sql, timeCondition, filtersCondition, rangeFiltersCondition, orderByCondition, " LIMIT ? OFFSET ? ");
}

void VirtualMemoryDataBase::AddStableOperatorSql(Protocol::StaticOperatorListParams requestParams, std::string &sql) {
    if (!requestParams.graphId.empty()) {
        sql += StringUtil::FormatString(" AND {} = ? ", StaticOpColumn::GRAPH_ID);
    }
    if (requestParams.startNodeIndex >= 0 && requestParams.endNodeIndex >= requestParams.startNodeIndex) {
        std::string tmpBetweenAndSql = StringUtil::FormatString("BETWEEN {} AND {}",
            std::to_string(requestParams.startNodeIndex), std::to_string(requestParams.endNodeIndex));
        sql += StringUtil::FormatString(" AND ({} {} OR {} {}) ", StaticOpColumn::NODE_INDEX_START, tmpBetweenAndSql,
            StaticOpColumn::NODE_INDEX_END, tmpBetweenAndSql);
    }

    if (requestParams.minSize != std::numeric_limits<int64_t>::min()) {
        sql += StringUtil::FormatString(" AND {} >= {} ", StaticOpColumn::SIZE, std::to_string(requestParams.minSize));
    }
    if (requestParams.maxSize != std::numeric_limits<int64_t>::max()) {
        sql += StringUtil::FormatString(" AND {} <= {} ", StaticOpColumn::SIZE, std::to_string(requestParams.maxSize));
    }
    if (!requestParams.orderBy.empty()) {
        std::string ascend = requestParams.desc ? "DESC" : "ASC";
        sql += StringUtil::FormatString(" ORDER BY {} {} ", requestParams.orderBy, ascend);
    }
    sql += " LIMIT ? offset ?";
}

void VirtualMemoryDataBase::BuildOverallLinesComponentPoints(const Protocol::ComponentDto &item,
    const std::vector<std::string> &streams, Protocol::MemoryPeak &peak, std::vector<double> &lines) {
    peak.appReserved = std::max(peak.appReserved, item.totalReserved);
    if (peak.hasPtaGe) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (!streams.empty()) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (peak.hasPtaGe) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    lines.emplace_back(item.totalReserved);
    if (peak.hasWorkspace) {
        // workspaceLegends为内部定义vector, 其size不可能超过uint8, 此处无溢出风险
        lines.insert(lines.end(), workspaceLegends.size(), std::numeric_limits<double>::quiet_NaN());
    }
}

void VirtualMemoryDataBase::BuildOverallLinesFrameworkPoints(const Protocol::ComponentDto &item,
    const std::vector<std::string> &streams, Protocol::MemoryPeak &peak, std::vector<double> &lines) {
    peak.ptaGeAllocated = std::max(peak.ptaGeAllocated, item.totalAllocated);
    peak.ptaGeReserved = std::max(peak.ptaGeReserved, item.totalReserved);
    peak.ptaGeActivated = std::max(peak.ptaGeActivated, item.totalActivated);
    lines.emplace_back(item.totalAllocated);
    if (!streams.empty()) {
        lines.emplace_back(item.totalActivated);
    }
    lines.emplace_back(item.totalReserved);
    if (peak.hasApp) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (peak.hasWorkspace) {
        lines.insert(lines.end(), workspaceLegends.size(), std::numeric_limits<double>::quiet_NaN());
    }
}

void VirtualMemoryDataBase::BuildOverallLinesWorkspacePoints(const Protocol::ComponentDto &item,
    const std::vector<std::string> &streams, Protocol::MemoryPeak &peak, std::vector<double> &lines) {
    if (peak.hasPtaGe) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (!streams.empty()) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (peak.hasPtaGe) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    if (peak.hasApp) {
        lines.emplace_back(std::numeric_limits<double>::quiet_NaN());
    }
    lines.emplace_back(item.totalAllocated);
    lines.emplace_back(item.totalReserved);
}

/*
 * 将多个单条线的数据组装成[x,y,y,y,y]的格式。
 * 各元素分别表示标签"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved" "App Reserved"。
 * 如果整组数据中某个标签的数据都不存在，不仅在标签中删除，也在[x,y,y,y,y]中删除该元素。
 * 如果只是部分时间上某些标签的数据不存在，则补NULL。
 */
void VirtualMemoryDataBase::GetOverallLines(const componentDtoVector &componentDtoVec, std::vector<double> &lines,
    std::vector<std::string> &legends, Protocol::MemoryPeak &peak, const std::vector<std::string> &streams) {
    GetOverallLinesLegends(componentDtoVec, legends, peak, streams);
    for (auto &item : componentDtoVec) {
        if (item.component == COMPONENT_APP) {
            lines.emplace_back(item.timesTamp);
            BuildOverallLinesComponentPoints(item, streams, peak, lines);
            continue;
        }
        if (item.component == COMPONENT_PTA_AND_GE || item.component == MIND_SPORE_GE ||
            (isInference && item.component == COMPONENT_GE)) {
            lines.emplace_back(item.timesTamp);
            BuildOverallLinesFrameworkPoints(item, streams, peak, lines);
            continue;
        }
        if (item.component == COMPONENT_WORKSPACE) {
            lines.emplace_back(item.timesTamp);
            BuildOverallLinesWorkspacePoints(item, streams, peak, lines);
        }
    }
}

void VirtualMemoryDataBase::GetOverallLinesLegends(const componentDtoVector &componentDtoVec,
    std::vector<std::string> &legends, Protocol::MemoryPeak &peak, const std::vector<std::string> &streams) {
    for (auto &item : componentDtoVec) {
        if (item.component == COMPONENT_WORKSPACE) {
            peak.hasWorkspace = true;
            continue;
        }
        if (item.component == COMPONENT_PTA_AND_GE || item.component == MIND_SPORE_GE ||
            (isInference && item.component == COMPONENT_GE)) {
            peak.hasPtaGe = true;
            continue;
        }
        if (item.component == COMPONENT_APP) {
            peak.hasApp = true;
        }
    }

    for (const auto &legend : baseLegends) {
        if (!peak.hasPtaGe && legend == "Operators Allocated") {
            continue;
        }
        if (streams.empty() && legend == "Operators Activated") {
            continue;
        }
        if (!peak.hasPtaGe && legend == "Operators Reserved") {
            continue;
        }
        legends.emplace_back(legend);
    }
    if (peak.hasApp) {
        legends.emplace_back(appLegend);
    }
    if (peak.hasWorkspace) {
        legends.insert(legends.end(), workspaceLegends.begin(), workspaceLegends.end());
    }
}

void VirtualMemoryDataBase::GetComponentLines(const Dic::Module::Memory::componentDtoVector &componentDtoVec,
    std::vector<double> &lines, std::vector<std::string> &legends, Protocol::MemoryPeak &peak,
    const std::vector<std::string> &streams) {
    GetComponentLinesLegends(componentDtoVec, legends, peak);
    // 3表示无PTA数据或无GE数据插入3个NULL
    const int sizeMembers = 3;
    for (auto &item : componentDtoVec) {
        if (item.component == COMPONENT_PTA) {
            peak.ptaAllocated = std::max(peak.ptaAllocated, item.totalAllocated);
            peak.ptaReserved = std::max(peak.ptaReserved, item.totalReserved);
            peak.ptaActivated = std::max(peak.ptaActivated, item.totalActivated);
            lines.emplace_back(item.timesTamp);
            InsertSize(lines, item);
            if (peak.hasGe) {
                InsertStringNull(lines, sizeMembers);
            }
            if (peak.hasApp) {
                InsertStringNull(lines, 1);
            }
        } else if (item.component == COMPONENT_GE) {
            peak.geAllocated = std::max(peak.geAllocated, item.totalAllocated);
            peak.geReserved = std::max(peak.geReserved, item.totalReserved);
            peak.geActivated = std::max(peak.geActivated, item.totalActivated);
            lines.emplace_back(item.timesTamp);
            if (peak.hasPta) {
                InsertStringNull(lines, sizeMembers);
            }
            InsertSize(lines, item);
            if (peak.hasApp) {
                InsertStringNull(lines, 1);
            }
        } else if (item.component == COMPONENT_APP) {
            peak.appReserved = std::max(peak.appReserved, item.totalReserved);
            lines.emplace_back(item.timesTamp);
            if (peak.hasPta) {
                InsertStringNull(lines, sizeMembers);
            }
            if (peak.hasGe) {
                InsertStringNull(lines, sizeMembers);
            }
            lines.emplace_back(item.totalReserved);
        }
    }
}

void VirtualMemoryDataBase::InsertSize(std::vector<double> &points, const Protocol::ComponentDto &item) {
    points.emplace_back(item.totalAllocated);
    points.emplace_back(item.totalActivated);
    points.emplace_back(item.totalReserved);
}

void VirtualMemoryDataBase::InsertStringNull(std::vector<double> &points, const int times) {
    points.insert(points.end(), times, std::numeric_limits<double>::quiet_NaN());
}

void VirtualMemoryDataBase::GetComponentLinesLegends(const Dic::Module::Memory::componentDtoVector &componentDtoVec,
    std::vector<std::string> &legends, Protocol::MemoryPeak &peak) {
    for (auto &item : componentDtoVec) {
        if (item.component == COMPONENT_PTA) {
            peak.hasPta = true;
        } else if (item.component == COMPONENT_GE) {
            peak.hasGe = true;
        } else if (item.component == COMPONENT_APP) {
            peak.hasApp = true;
        }
    }

    legends = componentTimeLegends;
    if (peak.hasPta) {
        legends.insert(legends.end(), componentPtaLegends.begin(), componentPtaLegends.end());
    }
    if (peak.hasGe) {
        legends.insert(legends.end(), componentGeLegends.begin(), componentGeLegends.end());
    }

    if (peak.hasApp) {
        legends.emplace_back(appLegend);
    }
}

void VirtualMemoryDataBase::GetStreamLines(const componentDtoVector &componentDtoVec, std::vector<double> &lines,
    std::vector<std::string> &legends, Protocol::MemoryPeak &peak, const std::vector<std::string> &streams) {
    // 组装图例
    if (componentDtoVec.empty() && streams.empty()) {
        legends.insert(legends.end(), baseLegends.begin(), baseLegends.end());
    } else {
        legends.emplace_back(baseLegends[0]);
    }
    for (const auto &stream : streams) {
        legends.emplace_back("Allocated of " + stream);
        legends.emplace_back("Activated of " + stream);
        legends.emplace_back("Reserved of " + stream);
    }
    // 组装数据点
    for (auto &item : componentDtoVec) {
        if (item.component != COMPONENT_PTA_AND_GE) {
            continue;
        }
        lines.emplace_back(item.timesTamp);
        std::string streamId = item.streamId;
        for (const auto &stream : streams) {
            if (stream == streamId) {
                lines.emplace_back(item.totalAllocated);
                lines.emplace_back(item.totalActivated);
                lines.emplace_back(item.totalReserved);
            } else {
                lines.insert(lines.end(), 3, std::numeric_limits<double>::quiet_NaN());
            }
        }
    }
}

std::string VirtualMemoryDataBase::GetPeakMemory(
    const Protocol::MemoryPeak &peak, const std::vector<std::string> &streams) {
    std::string peakMemory = "Peak Memory Usage: ";
    const size_t decimalPlacesNum = 4;
    if (peak.hasPtaGe) {
        std::string ptaGeAllo = std::to_string(peak.ptaGeAllocated);
        // double转换成string默认生成六位小数，删除后4位小数
        ptaGeAllo = ptaGeAllo.substr(0, ptaGeAllo.length() - decimalPlacesNum);
        peakMemory.append("Operator Allocated: ").append(ptaGeAllo).append("MB");
        if (!streams.empty()) {
            std::string ptaGeActive = std::to_string(peak.ptaGeActivated);
            ptaGeActive = ptaGeActive.substr(0, ptaGeActive.length() - decimalPlacesNum);
            peakMemory.append(" | Operator Activated: ").append(ptaGeActive).append("MB");
        }
        std::string ptaGeRe = std::to_string(peak.ptaGeReserved);
        ptaGeRe = ptaGeRe.substr(0, ptaGeRe.length() - decimalPlacesNum);
        peakMemory.append(" | Operator Reserved: ").append(ptaGeRe).append("MB");
    }
    if (peak.hasPta) {
        std::string ptaAllocatedText = std::to_string(peak.ptaAllocated);
        ptaAllocatedText = ptaAllocatedText.substr(0, ptaAllocatedText.length() - decimalPlacesNum);
        peakMemory.append("PTA Allocated: ").append(ptaAllocatedText).append("MB");
        std::string ptaActi = std::to_string(peak.ptaActivated);
        ptaActi = ptaActi.substr(0, ptaActi.length() - decimalPlacesNum);
        peakMemory.append(" | PTA Activated: ").append(ptaActi).append("MB");
        std::string ptaRes = std::to_string(peak.ptaReserved);
        ptaRes = ptaRes.substr(0, ptaRes.length() - decimalPlacesNum);
        peakMemory.append(" | PTA Reserved: ").append(ptaRes).append("MB");
    }
    if (peak.hasGe) {
        std::string geAllo = std::to_string(peak.geAllocated);
        geAllo = geAllo.substr(0, geAllo.length() - decimalPlacesNum);
        peakMemory.append(" | GE Allocated: ").append(geAllo).append("MB");
        std::string geActi = std::to_string(peak.geActivated);
        geActi = geActi.substr(0, geActi.length() - decimalPlacesNum);
        peakMemory.append(" | GE Activated: ").append(geActi).append("MB");
        std::string geRes = std::to_string(peak.geReserved);
        geRes = geRes.substr(0, geRes.length() - decimalPlacesNum);
        peakMemory.append(" | GE Reserved: ").append(geRes).append("MB");
    }
    if (peak.hasApp) {
        std::string appReservedText = std::to_string(peak.appReserved);
        appReservedText = appReservedText.substr(0, appReservedText.length() - decimalPlacesNum);
        peakMemory.append(" | APP Reserved: ").append(appReservedText).append("MB");
    }
    return peakMemory;
}

std::string VirtualMemoryDataBase::GetSelectOperatorMemoryFullColumnsWithCount(uint64_t baseTimestamp) {
    std::string columns = "COUNT(*) OVER() AS countOver";

    for (const auto &columnObj : OperatorMemoryTableView::FIELD_FULL_COLUMNS) {
        std::string selectColumn;
        std::string alias;
        GetSelectOperatorMemoryColumnAndAlias(columnObj.key, baseTimestamp, selectColumn, alias);
        std::string select = StringUtil::FormatString(", {} AS {}", selectColumn, alias);
        columns.append(select);
    }
    return columns;
}

std::string VirtualMemoryDataBase::ConvertTimestampStr(const std::string &timestampStr) {
    std::string result = timestampStr;
    if (timestampStr.empty() || timestampStr[0] == '-') {
        result = "N/A";
    }
    return result;
}

void VirtualMemoryDataBase::GetStaticOperatorColumns(std::vector<Protocol::MemoryTableColumnAttr> &copyTo) {
    for (const auto &col : staticOpTableColumnAttr) {
        copyTo.push_back(col);
    }
}
}
}
}
