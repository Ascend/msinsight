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
#include "VirtualMemoryDataBase.h"
#include "ConstantDefs.h"
#include "DataBaseManager.h"
#include "TextMemoryDataBase.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Server;
TextMemoryDataBase::TextMemoryDataBase(std::recursive_mutex &sqlMutex) : VirtualMemoryDataBase(sqlMutex) {}

TextMemoryDataBase::~TextMemoryDataBase() {
    if (hasInitStmt) {
        ReleaseStmt();
        hasInitStmt = false;
    }
    CloseDb();
}
bool TextMemoryDataBase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    if (!Database::OpenDb(dbPath, clearAllTable)) {
        return false;
    }
    return SetConfig() && CheckAndResetDatabaseOnVersionChange();
}
bool TextMemoryDataBase::SetConfig() { return Database::SetConfig(); }

bool TextMemoryDataBase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Database is not open.");
        return false;
    }
    std::string sql = GetCreateOperatorMemoryTableSql() + "CREATE TABLE " + recordTable +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, component TEXT, " +
        "total_allocated INTEGER, total_reserve INTEGER, total_active INTEGER, "
        "deviceId TEXT, stream TEXT, timestamp INTEGER);" +
        GetCreateStaticOpTableSql() + "CREATE TABLE " + componentTable +
        " (id INTEGER PRIMARY KEY AUTOINCREMENT, component TEXT, " +
        "timestamp INTEGER, total_reserved INTEGER, deviceId TEXT);";
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool TextMemoryDataBase::DropTable() {
    std::vector<std::string> tables = {operatorTable, recordTable, staticOpTable, componentTable};
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return DropSomeTables(tables);
}

bool TextMemoryDataBase::InitStmt() {
    if (hasInitStmt) {
        return true;
    }
    std::string sql = StringUtil::FormatString("INSERT INTO {} ({}) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        operatorTable, StringUtil::GenerateColumnString(OpMemoryColumn::FULL_COLUMNS_WITHOUT_ID));
    for (size_t i = 0; i < cacheSize - 1; ++i) {
        sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &insertOperatorStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert Operator statement. Error: ", sqlite3_errmsg(db));
        return false;
    }

    sql = "INSERT INTO " + recordTable +
        " (component, total_allocated, total_reserve, total_active, deviceId, stream, timestamp)" +
        " VALUES (?,?,?,?,?,?,?)";
    for (size_t i = 0; i < cacheSize - 1; ++i) {
        sql.append(",(?,?,?,?,?,?,?)");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &insertRecordStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert Record statement. Error: ", sqlite3_errmsg(db));
        return false;
    }
    sql = StringUtil::FormatString("INSERT INTO {} ({}) VALUES (?,?,?,?,?,?,?)", staticOpTable,
        StringUtil::GenerateColumnString(StaticOpColumn::FULL_COLUMNS_WITHOUT_ID));
    for (size_t i = 0; i < cacheSize - 1; ++i) {
        sql.append(",(?,?,?,?,?,?,?)");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &insertStaticOpStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert Static Op statement. Error: ", sqlite3_errmsg(db));
        return false;
    }

    sql = "INSERT INTO " + componentTable + " (component, timestamp, total_reserved, deviceId)" + " VALUES (?,?,?,?)";
    for (size_t i = 0; i < cacheSize - 1; ++i) {
        sql.append(",(?,?,?,?)");
    }
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &insertComponentStmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare insert Component statement. Error: ", sqlite3_errmsg(db));
        return false;
    }

    hasInitStmt = true;
    return true;
}

void TextMemoryDataBase::ReleaseStmt() {
    if (insertOperatorStmt != nullptr) {
        sqlite3_finalize(insertOperatorStmt);
        insertOperatorStmt = nullptr;
    }
    if (insertRecordStmt != nullptr) {
        sqlite3_finalize(insertRecordStmt);
        insertRecordStmt = nullptr;
    }
    if (insertStaticOpStmt != nullptr) {
        sqlite3_finalize(insertStaticOpStmt);
        insertStaticOpStmt = nullptr;
    }
    if (insertComponentStmt != nullptr) {
        sqlite3_finalize(insertComponentStmt);
        insertComponentStmt = nullptr;
    }
}

void TextMemoryDataBase::InsertOperatorDetailList(const std::vector<Operator> &eventList) {
    sqlite3_stmt *stmt = GetOperatorStmt(eventList.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get operator stmt.");
        return;
    }
    int idx = bindStartIndex;
    std::unique_lock<std::recursive_mutex> lock(mutex);
    for (const auto &event : eventList) {
        sqlite3_bind_text(stmt, idx++, event.name.c_str(), event.name.length(), SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, idx++, event.size);
        sqlite3_bind_int64(stmt, idx++, event.allocationTime);
        sqlite3_bind_int64(stmt, idx++, event.releaseTime);
        sqlite3_bind_int64(stmt, idx++, event.activeReleaseTime);
        sqlite3_bind_double(stmt, idx++, event.duration);
        sqlite3_bind_double(stmt, idx++, event.activeDuration);
        sqlite3_bind_double(stmt, idx++, event.allocationAllocated);
        sqlite3_bind_double(stmt, idx++, event.allocationReserved);
        sqlite3_bind_double(stmt, idx++, event.allocationActive);
        sqlite3_bind_double(stmt, idx++, event.releaseAllocated);
        sqlite3_bind_double(stmt, idx++, event.releaseReserved);
        sqlite3_bind_double(stmt, idx++, event.releaseActive);
        sqlite3_bind_text(stmt, idx++, event.streamId.c_str(), event.streamId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.deviceType.c_str(), event.deviceType.length(), SQLITE_TRANSIENT);
    }
    auto result = sqlite3_step(stmt);
    if (eventList.size() != cacheSize) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert Operator fail. Error: ", sqlite3_errmsg(db));
    }
}

void TextMemoryDataBase::InsertOperatorDetail(const Operator &event) {
    std::lock_guard lock(mutex);
    operatorCache.emplace_back(event);
    if (operatorCache.size() == cacheSize) {
        InsertOperatorDetailList(operatorCache);
        operatorCache.clear();
    }
}

void TextMemoryDataBase::InsertRecordDetailList(const std::vector<Record> &eventList) {
    sqlite3_stmt *stmt = GetRecordStmt(eventList.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get Record stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &event : eventList) {
        sqlite3_bind_text(stmt, idx++, event.component.c_str(), event.component.length(), SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, idx++, event.totalAllocated);
        sqlite3_bind_double(stmt, idx++, event.totalReserved);
        sqlite3_bind_double(stmt, idx++, event.totalActivated);
        sqlite3_bind_text(stmt, idx++, event.deviceType.c_str(), event.deviceType.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.streamId.c_str(), event.streamId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, idx++, event.timesTamp);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto result = sqlite3_step(stmt);
    if (eventList.size() != cacheSize) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert Record fail. Error: ", sqlite3_errmsg(db));
    }
}

void TextMemoryDataBase::InsertStaticOpDetailList(const std::vector<StaticOp> &eventList) {
    sqlite3_stmt *stmt = GetStaticOpStmt(eventList.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get Record stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &event : eventList) {
        sqlite3_bind_text(stmt, idx++, event.deviceId.c_str(), event.deviceId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.opName.c_str(), event.opName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.modelName.c_str(), event.modelName.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, event.graphId.c_str(), event.graphId.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, idx++, event.nodeIndexStart);
        sqlite3_bind_int64(stmt, idx++, event.nodeIndexEnd);
        sqlite3_bind_double(stmt, idx++, event.size);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto result = sqlite3_step(stmt);
    if (eventList.size() != cacheSize) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Insert StaticOp fail. Error: ", sqlite3_errmsg(db));
    }
}

void TextMemoryDataBase::InsertRecordDetail(const Record &event) {
    std::lock_guard lock(mutex);
    recordCache.emplace_back(event);
    if (recordCache.size() == cacheSize) {
        InsertRecordDetailList(recordCache);
        recordCache.clear();
    }
}

void TextMemoryDataBase::InsertStaticOpDetail(const StaticOp &event) {
    std::lock_guard lock(mutex);
    staticOpCache.emplace_back(event);
    if (staticOpCache.size() == cacheSize) {
        InsertStaticOpDetailList(staticOpCache);
        staticOpCache.clear();
    }
}

void TextMemoryDataBase::InsertComponentDetailList(const std::vector<Component> &eventList) {
    sqlite3_stmt *stmt = GetComponentStmt(eventList.size());
    if (stmt == nullptr) {
        ServerLog::Error("Failed to get Component Stmt.");
        return;
    }
    int idx = bindStartIndex;
    for (const auto &event : eventList) {
        sqlite3_bind_text(stmt, idx++, event.component.c_str(), event.component.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, idx++, event.timestamp);
        sqlite3_bind_double(stmt, idx++, event.totalReserved);
        sqlite3_bind_text(stmt, idx++, event.device.c_str(), event.device.length(), SQLITE_TRANSIENT);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto result = sqlite3_step(stmt);
    if (eventList.size() != cacheSize) {
        sqlite3_finalize(stmt);
    }
    if (result != SQLITE_DONE) {
        ServerLog::Error("Failed to insert component. Error: ", sqlite3_errmsg(db));
    }
}

void TextMemoryDataBase::InsertComponentDetail(const Component &event) {
    std::lock_guard lock(mutex);
    componentCache.emplace_back(event);
    if (componentCache.size() == cacheSize) {
        InsertComponentDetailList(componentCache);
        componentCache.clear();
    }
}

bool TextMemoryDataBase::UpdateParseStatus(const std::string &status) {
    return UpdateValueIntoStatusInfoTable(memoryParseStatus, status);
}

bool TextMemoryDataBase::HasFinishedParseLastTime() {
    return CheckValueFromStatusInfoTable(memoryParseStatus, FINISH_STATUS);
}

uint64_t TextMemoryDataBase::QueryMinOperatorAllocationTime() {
    std::string sql = StringUtil::FormatString("SELECT MIN({}) FROM {} WHERE {} != 0", OpMemoryColumn::ALLOCATION_TIME,
        operatorTable, OpMemoryColumn::ALLOCATION_TIME);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query min operator allocation time. Error: ", sqlite3_errmsg(db));
        return 0;
    }
    uint64_t min = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t result = sqlite3_column_int64(stmt, col++);
        if (result < 0) {
            min = 0;
        } else {
            min = static_cast<uint64_t>(result);
        }
    }
    sqlite3_finalize(stmt);
    return min;
}

uint64_t TextMemoryDataBase::QueryMinRecordTimestamp() {
    std::string sql = "Select MIN(timestamp) FROM " + recordTable + " WHERE timestamp != 0";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query min record timestamp. Error: ", sqlite3_errmsg(db));
        return 0;
    }
    uint64_t min = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t result = sqlite3_column_int64(stmt, col++);
        if (result < 0) {
            min = 0;
        } else {
            min = static_cast<uint64_t>(result);
        }
    }
    sqlite3_finalize(stmt);
    return min;
}

uint64_t TextMemoryDataBase::QueryMinComponentTimestamp() {
    std::string sql = "Select MIN(timestamp) FROM " + componentTable + " WHERE timestamp != 0";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query min component timestamp. Error: ", sqlite3_errmsg(db));
        return 0;
    }
    uint64_t min = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t result = sqlite3_column_int64(stmt, col++);
        if (result < 0) {
            min = 0;
        } else {
            min = static_cast<uint64_t>(result);
        }
    }
    sqlite3_finalize(stmt);
    return min;
}

std::string TextMemoryDataBase::GetOperatorSql(Protocol::MemoryOperatorParams &requestParams) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(requestParams.rankId);
    // 溢出防护
    if (startTime > std::numeric_limits<uint64_t>::max() - offsetTime) {
        ServerLog::Error("Failed to calculate relative to the reference time due to integer overflow.");
        return "";
    }
    std::string selectColumns = GetSelectOperatorMemoryFullColumnsWithCount(startTime + offsetTime);
    // 在 text 情况下 allocation_time release_time 不可能为 null，不用再判断
    std::string sql = StringUtil::FormatString(
        " SELECT {} FROM {} WHERE {} = ? ", selectColumns, operatorTable, OpMemoryColumn::DEVICE_ID);
    AddOperatorSql(requestParams, sql);
    return sql;
}

std::string TextMemoryDataBase::GetStaticOperatorSql(Protocol::StaticOperatorListParams &requestParams) {
    std::string sql = StringUtil::FormatString("SELECT COUNT(*) OVER(), {}, {}, {}, {}, {} FROM {} WHERE {} LIKE ? AND "
                                               "{} <> 'TOTAL' ",
        StaticOpColumn::DEVICE_ID, StaticOpColumn::OP_NAME, StaticOpColumn::NODE_INDEX_START,
        StaticOpColumn::NODE_INDEX_END,
        StringUtil::FormatString("ROUND({}/1024.0, 2) AS {}", StaticOpColumn::SIZE, StaticOpColumn::SIZE),
        staticOpTable, StaticOpColumn::OP_NAME, StaticOpColumn::OP_NAME);
    AddStableOperatorSql(requestParams, sql);
    return sql;
}

void TextMemoryDataBase::GenerateGetStaticGraphNodeIndexSql(
    Protocol::StaticOperatorGraphParams &requestParams, std::string &startSql, std::string &endSql) {
    std::string pattern = "SELECT {}, {} FROM {} WHERE {} <> 'TOTAL' AND {} <> 0 AND {} = ?";
    startSql = StringUtil::FormatString(pattern, StaticOpColumn::NODE_INDEX_START, StaticOpColumn::SIZE, staticOpTable,
        StaticOpColumn::OP_NAME, StaticOpColumn::SIZE, StaticOpColumn::GRAPH_ID);
    endSql = StringUtil::FormatString(pattern, StaticOpColumn::NODE_INDEX_END, StaticOpColumn::SIZE, staticOpTable,
        StaticOpColumn::OP_NAME, StaticOpColumn::SIZE, StaticOpColumn::GRAPH_ID);
    if (!requestParams.modelName.empty()) {
        const std::string condition = StringUtil::FormatString(" AND {} = ? ", StaticOpColumn::MODEL_NAME);
        startSql += condition;
        endSql += condition;
    }
    startSql += StringUtil::FormatString(" ORDER BY {} ASC ", StaticOpColumn::NODE_INDEX_START);
    endSql += StringUtil::FormatString(" ORDER BY {} ASC ", StaticOpColumn::NODE_INDEX_END);
}

bool TextMemoryDataBase::QueryMemoryType(std::string &type, std::vector<std::string> &graphId) {
    return ExecuteMemoryType(graphId, type);
}

bool TextMemoryDataBase::QueryMemoryResourceType(std::string &type) {
    std::string sql = "SELECT count(*) as nums FROM " + recordTable + " WHERE component = 'MindSpore'";
    return ExecuteMemoryResourceType(type, sql);
}

int64_t TextMemoryDataBase::QueryOperatorDetail(
    Protocol::MemoryOperatorParams &requestParams, std::vector<Protocol::MemoryOperator> &opDetails) {
    std::string sql = GetOperatorSql(requestParams);
    return ExecuteOperatorDetail(requestParams, opDetails, sql);
}

bool TextMemoryDataBase::QueryEntireOperatorTable(Protocol::MemoryOperatorParams &requestParams,
    std::vector<Protocol::MemoryOperator> &opDetails, uint64_t offsetTime) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    // 溢出防护
    if (startTime > std::numeric_limits<uint64_t>::max() - offsetTime) {
        ServerLog::Error("Failed to calculate relative to the reference time due to integer overflow.");
        return false;
    }
    // 在 text 情况下 allocation_time release_time 不可能为 null，不用再判断
    std::string sql = StringUtil::FormatString("SELECT {} FROM {} WHERE {} = ? ",
        GetSelectOperatorMemoryFullColumnsWithCount(startTime + offsetTime), operatorTable, OpMemoryColumn::DEVICE_ID);
    return ExecuteQueryEntireOperatorTable(requestParams, opDetails, sql);
}

bool TextMemoryDataBase::QueryComponentDetail(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryTableColumnAttr> &columnAttr,
    std::vector<Protocol::MemoryComponent> &componentDetails) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(requestParams.rankId);
    // 内层SQL根据组件名分组取出内存占用峰值大于100M的那些组件，外层SQL和内层SQL的查询结果根据组件名和内存占用值做一次连接
    // 外层再做一次分组的原因是可能有多个时刻内存占用为峰值，只需要时刻最小的那个
    std::string sql = "SELECT t1.component AS componentColumn, ROUND(t1.total_reserved, 2) as totalReservedColumn,"
                      " MIN(ROUND((t1.timestamp - " +
        std::to_string(startTime + offsetTime) + ") / (1000.0 * 1000.0), 3)) AS timestampColumn FROM " +
        componentTable + " AS t1 JOIN " + "(SELECT component, MAX(total_reserved) AS max_total_reserved FROM " +
        componentTable + " GROUP BY component HAVING max_total_reserved >= " + std::to_string(componentThresholdMb) +
        ") AS t2 ON t1.component = t2.component AND t1.total_reserved = t2.max_total_reserved "
        "WHERE t1.deviceId = ? "
        "GROUP BY t1.component, t1.total_reserved";
    if (!requestParams.order.empty() && !requestParams.orderBy.empty()) {
        sql += " ORDER BY " + requestParams.orderBy + "Column";
        if (requestParams.order == "ascend") {
            sql += " ASC ";
        } else {
            sql += " DESC ";
        }
    }
    sql += " LIMIT ? OFFSET ? ";
    return ExecuteComponentDetail(requestParams, columnAttr, componentDetails, sql);
}

bool TextMemoryDataBase::QueryEntireComponentTable(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryComponent> &componentDetails, uint64_t offsetTime) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::string sql = "SELECT t1.component, t1.total_reserved, MIN(ROUND((t1.timestamp - " +
        std::to_string(startTime + offsetTime) + ") / (1000.0 * 1000.0), 3)) AS timestamp_maxsize FROM " +
        componentTable + " AS t1 JOIN " + "(SELECT component, MAX(total_reserved) AS max_total_reserved FROM " +
        componentTable + " GROUP BY component HAVING max_total_reserved >= " + std::to_string(componentThresholdMb) +
        ") AS t2 ON t1.component = t2.component AND t1.total_reserved = t2.max_total_reserved "
        "WHERE t1.deviceId = ? "
        "GROUP BY t1.component, t1.total_reserved";
    return ExecuteQueryEntireComponentTable(requestParams, componentDetails, sql);
}

bool TextMemoryDataBase::QueryMemoryView(
    Protocol::MemoryViewParams &requestParams, Protocol::MemoryViewData &operatorBody, uint64_t offsetTime) {
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::string sql = "SELECT component, ROUND((timestamp - " + std::to_string(startTime) + " - " +
        std::to_string(offsetTime) +
        ") / (1000.0 * 1000.0), 3) as timestamp, "
        "ROUND(total_allocated, 2) as total_allocated, ROUND(total_reserve, 2) as total_reserve, "
        "ROUND(total_active, 2) as total_active, stream FROM " +
        recordTable + " WHERE deviceId = ? ";
    std::vector<Protocol::ComponentDto> componentDtoVec;
    std::vector<std::string> streams;
    if (!ExecuteQueryMemoryViewExecuteSql(requestParams, componentDtoVec, streams, sql, "deviceId")) {
        return false;
    }
    return ExecuteQueryMemoryViewGetGraph(requestParams, componentDtoVec, streams, operatorBody);
}

int64_t TextMemoryDataBase::QueryStaticOperatorList(
    Protocol::StaticOperatorListParams &requestParams, std::vector<Protocol::StaticOperatorItem> &opDetails) {
    std::string sql = GetStaticOperatorSql(requestParams);
    return ExecuteStaticOperatorDetail(requestParams, opDetails, sql);
}

bool TextMemoryDataBase::QueryEntireStaticOperatorTable(
    Protocol::StaticOperatorListParams &requestParams, std::vector<Protocol::StaticOperatorItem> &opDetails) {
    const std::string pattern = "SELECT {},{},{},{},{} FROM {} WHERE {} <> 'TOTAL' ";
    std::string sql = StringUtil::FormatString(pattern, StaticOpColumn::DEVICE_ID, StaticOpColumn::OP_NAME,
        StaticOpColumn::NODE_INDEX_START, StaticOpColumn::NODE_INDEX_END,
        StringUtil::FormatString("ROUND({}/1024.0, 2) as {} ", StaticOpColumn::SIZE, StaticOpColumn::SIZE),
        staticOpTable, StaticOpColumn::OP_NAME);
    if (!requestParams.graphId.empty()) {
        sql += StringUtil::FormatString(" AND {} = ? ", StaticOpColumn::GRAPH_ID);
    }
    return ExecuteQueryEntireStaticOperatorTable(requestParams, opDetails, sql);
}

bool TextMemoryDataBase::QueryStaticOperatorGraph(
    Protocol::StaticOperatorGraphParams &requestParams, Protocol::StaticOperatorGraphItem &graphItem) {
    std::string totalSql = StringUtil::FormatString("SELECT {} FROM {} WHERE {} = 'TOTAL' AND {} = ? ",
        StaticOpColumn::SIZE, staticOpTable, StaticOpColumn::OP_NAME, StaticOpColumn::GRAPH_ID);
    if (!requestParams.modelName.empty()) {
        totalSql += StringUtil::FormatString(" AND {} = ? ", StaticOpColumn::MODEL_NAME);
    }
    std::string graphStartSql;
    std::string graphEndSql;
    GenerateGetStaticGraphNodeIndexSql(requestParams, graphStartSql, graphEndSql);
    return ExecuteStaticOperatorGraph(requestParams, graphItem, totalSql, graphStartSql, graphEndSql);
}

void TextMemoryDataBase::SaveOperatorDetail() {
    if (operatorCache.size() > 0) {
        InsertOperatorDetailList(operatorCache);
        operatorCache.clear();
    }
}

void TextMemoryDataBase::SaveRecordDetail() {
    if (recordCache.size() > 0) {
        InsertRecordDetailList(recordCache);
        recordCache.clear();
    }
}

void TextMemoryDataBase::SaveStaticOpDetail() {
    if (staticOpCache.size() > 0) {
        InsertStaticOpDetailList(staticOpCache);
        staticOpCache.clear();
    }
}

void TextMemoryDataBase::SaveComponentDetail() {
    if (componentCache.size() > 0) {
        InsertComponentDetailList(componentCache);
        componentCache.clear();
    }
}

sqlite3_stmt *TextMemoryDataBase::GetOperatorStmt(uint64_t paramLen) {
    sqlite3_stmt *stmt = nullptr;
    if (paramLen == 0) {
        return stmt;
    } else if (paramLen == cacheSize) {
        if (!hasInitStmt) {
            InitStmt();
        }
        stmt = insertOperatorStmt;
        sqlite3_reset(stmt);
    } else {
        std::string sql = StringUtil::FormatString("INSERT INTO {} ({}) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            operatorTable, StringUtil::GenerateColumnString(OpMemoryColumn::FULL_COLUMNS_WITHOUT_ID));
        for (uint64_t i = 0; i < paramLen - 1; ++i) {
            sql.append(",(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        }
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare insertOperator stat. Error: ", sqlite3_errmsg(db));
            return nullptr;
        }
    }
    return stmt;
}

sqlite3_stmt *TextMemoryDataBase::GetRecordStmt(uint64_t paramLen) {
    sqlite3_stmt *stmt = nullptr;
    if (paramLen == 0) {
        return stmt;
    } else if (paramLen == cacheSize) {
        stmt = insertRecordStmt;
        sqlite3_reset(stmt);
    } else {
        std::string sql = "INSERT INTO " + recordTable +
            " (component, total_allocated, total_reserve, total_active, deviceId, stream, timestamp)"
            " VALUES (?,?,?,?,?,?,?)";
        for (uint64_t i = 0; i < paramLen - 1; ++i) {
            sql.append(",(?,?,?,?,?,?,?)");
        }
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare insertRecord stat. Error: ", sqlite3_errmsg(db));
            return nullptr;
        }
    }
    return stmt;
}

sqlite3_stmt *TextMemoryDataBase::GetStaticOpStmt(uint64_t paramLen) {
    sqlite3_stmt *stmt = nullptr;
    if (paramLen == 0) {
        return stmt;
    } else if (paramLen == cacheSize) {
        stmt = insertStaticOpStmt;
        sqlite3_reset(stmt);
    } else {
        std::string sql = StringUtil::FormatString("INSERT INTO {} ({}) VALUES (?,?,?,?,?,?,?)", staticOpTable,
            StringUtil::GenerateColumnString(StaticOpColumn::FULL_COLUMNS_WITHOUT_ID));
        for (uint64_t i = 0; i < paramLen - 1; ++i) {
            sql.append(",(?,?,?,?,?,?,?)");
        }
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare insertStaticOp stat. Error: ", sqlite3_errmsg(db));
            return nullptr;
        }
    }
    return stmt;
}

sqlite3_stmt *TextMemoryDataBase::GetComponentStmt(uint64_t paramLen) {
    sqlite3_stmt *stmt = nullptr;
    if (paramLen == 0) {
        return stmt;
    }
    if (paramLen == cacheSize) {
        stmt = insertComponentStmt;
        sqlite3_reset(stmt);
    } else {
        std::string sql =
            "INSERT INTO " + componentTable + " (component, timestamp, total_reserved, deviceId)" + " VALUES (?,?,?,?)";
        for (uint64_t i = 0; i < paramLen - 1; ++i) {
            sql.append(",(?,?,?,?)");
        }
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare insertComponent stat. Error: ", sqlite3_errmsg(db));
            return nullptr;
        }
    }
    return stmt;
}

bool TextMemoryDataBase::QueryComponentsTotalNum(Protocol::MemoryComponentParams &requestParams, int64_t &totalNum) {
    std::string sql = "SELECT count(*) FROM (SELECT component FROM " + componentTable +
        " WHERE deviceId = ? "
        " GROUP BY component HAVING MAX(total_reserved) >= " +
        std::to_string(componentThresholdMb) + ") AS t3";
    return ExecuteComponentTotalNum(requestParams, totalNum, sql);
}

bool TextMemoryDataBase::QueryOperatorSize(
    Protocol::MemoryOperatorSizeParams &requestParams, double &min, double &max) {
    std::string sql = StringUtil::FormatString("SELECT min({}), max({}) FROM {} WHERE {} = ?", OpMemoryColumn::SIZE,
        OpMemoryColumn::SIZE, operatorTable, OpMemoryColumn::DEVICE_ID);
    return ExecuteOperatorSize(requestParams, min, max, sql);
}

bool TextMemoryDataBase::QueryStaticOperatorSize(
    Protocol::StaticOperatorSizeParams &requestParams, double &min, double &max) {
    std::string sql = StringUtil::FormatString("SELECT min({}) as minSize, max({}) as maxSize FROM {} "
                                               "WHERE {} <> 'TOTAL'",
        StaticOpColumn::SIZE, StaticOpColumn::SIZE, staticOpTable, StaticOpColumn::OP_NAME);
    if (!requestParams.graphId.empty()) {
        sql += StringUtil::FormatString(" AND {} = ? ", StaticOpColumn::GRAPH_ID);
    }
    return ExecuteStaticOperatorSize(requestParams, min, max, sql);
}

void TextMemoryDataBase::GetSelectOperatorMemoryColumnAndAlias(
    std::string_view columnKey, uint64_t baseTimestamp, std::string &column, std::string &alias) {
    // id列，从db中的rowid查出并别名为id
    if (columnKey == "id") {
        column = StringUtil::FormatString("{}.{}", operatorTable, OpMemoryColumn::ID);
        alias = columnKey;
        return;
    }
    // 注意此处会将所有列别名前缀_, 用于避免计算列where的判断条件时使用原值而不是计算值
    alias = StringUtil::FormatString("_{}", columnKey);
    // 保留两位小数的列
    if (OPERATOR_MEMORY_ARA_SIZE_COLUMNS.find(columnKey) != OPERATOR_MEMORY_ARA_SIZE_COLUMNS.end()) {
        column = StringUtil::FormatString("ROUND({}/1.0, 2)", columnKey);
        return;
    }
    std::string baseTimestampStr = std::to_string(baseTimestamp);
    // ns转换为ms的列
    if (columnKey == OpMemoryColumn::RELEASE_TIME || columnKey == OpMemoryColumn::ALLOCATION_TIME ||
        columnKey == OpMemoryColumn::ACTIVE_RELEASE_TIME) {
        column = StringUtil::FormatString("ROUND(({} - {})/(1000.0*1000.0), 3)", columnKey, baseTimestampStr);
        return;
    }
    // us转换为ms的列
    if (columnKey == OpMemoryColumn::DURATION || columnKey == OpMemoryColumn::ACTIVE_DURATION) {
        column = StringUtil::FormatString("ROUND({}/(1000.0), 3)", columnKey);
        return;
    }
    // 缺省不计算
    column = std::string(columnKey);
}

std::string TextMemoryDataBase::GetCreateOperatorMemoryTableSql() {
    return StringUtil::FormatString("CREATE TABLE {} ("
                                    "{} TEXT,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} TEXT,"
                                    "{} TEXT"
                                    ");",
        operatorTable, OpMemoryColumn::NAME, OpMemoryColumn::SIZE, OpMemoryColumn::ALLOCATION_TIME,
        OpMemoryColumn::RELEASE_TIME, OpMemoryColumn::ACTIVE_RELEASE_TIME, OpMemoryColumn::DURATION,
        OpMemoryColumn::ACTIVE_DURATION, OpMemoryColumn::ALLOCATION_ALLOCATED, OpMemoryColumn::ALLOCATION_RESERVE,
        OpMemoryColumn::ALLOCATION_ACTIVE, OpMemoryColumn::RELEASE_ALLOCATED, OpMemoryColumn::RELEASE_RESERVE,
        OpMemoryColumn::RELEASE_ACTIVE, OpMemoryColumn::STREAM, OpMemoryColumn::DEVICE_ID);
}

std::string TextMemoryDataBase::GetCreateStaticOpTableSql() const {
    return StringUtil::FormatString("CREATE TABLE {} ("
                                    "{} INTEGER PRIMARY KEY AUTOINCREMENT,"
                                    "{} TEXT,"
                                    "{} TEXT,"
                                    "{} TEXT,"
                                    "{} TEXT,"
                                    "{} INTEGER,"
                                    "{} INTEGER,"
                                    "{} INTEGER"
                                    ");",
        staticOpTable, StaticOpColumn::ID, StaticOpColumn::DEVICE_ID, StaticOpColumn::OP_NAME,
        StaticOpColumn::MODEL_NAME, StaticOpColumn::GRAPH_ID, StaticOpColumn::NODE_INDEX_START,
        StaticOpColumn::NODE_INDEX_END, StaticOpColumn::SIZE);
}

MemoryDataBaseContext TextMemoryDataBase::GetMemoryDbContext() {
    if (!initContextFlag) {
        memDbContext.withMemoryRecord = CheckTableExist(recordTable);
        memDbContext.withOperatorMemory = CheckTableExist(operatorTable);
        memDbContext.withNpuModuleMem = CheckTableExist(componentTable);
    }
    initContextFlag = true;
    return memDbContext;
}

} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
