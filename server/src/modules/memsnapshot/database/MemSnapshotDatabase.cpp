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

#include "MemSnapshotDatabase.h"
#include "DataBaseManager.h"
#include "ServerLog.h"

namespace Dic::Module::FullDb {
bool MemSnapshotDatabase::CheckAllTableExist() {
    blockTableNames = QueryTableNamesByPrefix(blockTablePrefix);
    if (blockTableNames.empty()) {
        Server::ServerLog::Error(LOG_TAG + "Check table exist: The snapshot database does not have block table.");
        return false;
    }
    traceEntryTableNames = QueryTableNamesByPrefix(traceEntryTablePrefix);
    if (traceEntryTableNames.empty()) {
        Server::ServerLog::Error(LOG_TAG + "Check table exist: The snapshot database does not have trace entry table.");
        return false;
    }
    // 解析时block表与trace_entry表根据deviceId配对生成，因此数量必须一致
    if (blockTableNames.size() != traceEntryTableNames.size()) {
        Server::ServerLog::Error(LOG_TAG +
            "Check table exist: The snapshot database block table number "
            "is not equal to trace entry table number.");
        return false;
    }
    return Database::CheckTableExist({dictionaryTable});
}

bool MemSnapshotDatabase::OpenDbReadOnly(const std::string &dbPath) {
    if (isOpen) {
        Server::ServerLog::Warn(LOG_TAG + "The database connection has been opened. Nothing to do.");
        return true;
    }
    if (!Database::OpenDb(dbPath, false)) {
        Server::ServerLog::Error(LOG_TAG + "Failed to open snapshot db with path: %.", dbPath);
        return false;
    }
    // 检查必要表
    if (!CheckAllTableExist()) {
        Server::ServerLog::Error(LOG_TAG + "Failed to check all table exists.");
        return false;
    }
    // 初始化db实例上下文
    if (!InitContext()) {
        Server::ServerLog::Error(LOG_TAG + "Failed to initialize snapshot database context.");
        return false;
    }
    return true;
}

std::string MemSnapshotDatabase::GetRealValueInTableDictionaryMap(
    const std::string &tableName, const std::string &colName, int intVal) {
    std::string colTag = GetTableColumnTag(tableName, colName);
    if (tableDictionaryMap.find(colTag) == tableDictionaryMap.end()) {
        return std::to_string(intVal);
    }
    if (tableDictionaryMap[colTag].find(intVal) == tableDictionaryMap[colTag].end()) {
        return std::to_string(intVal);
    }
    return tableDictionaryMap[colTag][intVal];
}

int MemSnapshotDatabase::GetKeyInTableDictionaryMap(
    const std::string &tableName, const std::string &colName, const std::string &realVal) {
    std::string colTag = GetTableColumnTag(tableName, colName);
    if (tableDictionaryMap.find(colTag) == tableDictionaryMap.end()) {
        Server::ServerLog::Warn(LOG_TAG + "Failed to get key in table dictionary map, table: %s, col: %s",
            tableName.c_str(), colName.c_str());
        return -1;
    }
    for (auto &[key, val] : tableDictionaryMap[colTag]) {
        if (val == realVal) {
            return key;
        }
    }
    Server::ServerLog::Warn(LOG_TAG + "Failed to get key in table dictionary map, table: %s, col: %s, val: %s",
        tableName.c_str(), colName.c_str(), realVal.c_str());
    return -1;
}

bool MemSnapshotDatabase::IsDeviceIdValid(const std::string &deviceId) {
    return deviceMaxEntryIdMap.find(deviceId) != deviceMaxEntryIdMap.end();
}

std::vector<std::string> MemSnapshotDatabase::GetDeviceIds() {
    std::vector<std::string> deviceIds;
    deviceIds.reserve(deviceMaxEntryIdMap.size());
    for (auto &[deviceId, maxEntryId] : deviceMaxEntryIdMap) {
        deviceIds.push_back(deviceId);
    }
    return deviceIds;
}

std::string MemSnapshotDatabase::GetBlockTableNameByDeviceId(const std::string &deviceId) {
    return blockTablePrefix + deviceId;
}

std::string MemSnapshotDatabase::GetTraceEntryTableNameByDeviceId(const std::string &deviceId) {
    return traceEntryTablePrefix + deviceId;
}

template <typename T> bool MemSnapshotDatabase::QueryAllBlocks(std::vector<T> &blocks, const std::string &deviceId) {
    std::string querySql = "SELECT * FROM {} ORDER BY {}";
    querySql =
        StringUtil::FormatString(querySql, GetBlockTableNameByDeviceId(deviceId), BlockTableColumn::ALLOC_EVENT_ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepared query block sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        blocks.push_back(QueryBlockByStep<T>(stmt));
    };
    sqlite3_finalize(stmt);
    return true;
}

/**
 * 该方法用于分片请求全量事件场景，该场景只要查询成功则固定返回全量事件数量
 * @param paginationParam 分片参数
 * @param deviceId 设备id
 * @param entries 事件列表
 * @return 事件数量
 */
int64_t MemSnapshotDatabase::QueryTraceEntriesList(
    const PaginationParam &paginationParam, const std::string &deviceId, std::vector<TraceEntryListItemDTO> &entries) {
    if (!IsDeviceIdValid(deviceId)) {
        ServerLog::Error(LOG_TAG + "Failed to query trace entries list by pagination: invalid device.");
        return -1;
    }
    std::string querySql = "SELECT {}, {}, {}, {}, {} FROM {} WHERE {} >= 0 LIMIT ? OFFSET ?;";
    querySql = StringUtil::FormatString(querySql, TraceEntryTableColumn::ID, TraceEntryTableColumn::ACTION,
        TraceEntryTableColumn::ADDRESS, TraceEntryTableColumn::SIZE, TraceEntryTableColumn::STREAM,
        GetTraceEntryTableNameByDeviceId(deviceId), TraceEntryTableColumn::ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepared query trace entry sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    int bindIdx = bindStartIndex;
    CommonBindPaginationParams(paginationParam.pageSize, paginationParam.currentPage, stmt, bindIdx);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TraceEntryListItemDTO entry;
        int col = resultStartIndex;
        entry.id = sqlite3_column_int64(stmt, col++);
        entry.action = GetRealValueInTableDictionaryMap(
            traceEntryTablePrefix, std::string(TraceEntryTableColumn::ACTION), sqlite3_column_int(stmt, col++));
        entry.address = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        entry.size = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        entry.stream = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        entries.emplace_back(entry);
    };
    sqlite3_finalize(stmt);
    // 解析事件id是根据事件发生顺序编制而成(从0开始)。因此使用最大id+1作为事件总数
    return GetDeviceMaxEntryId(deviceId) + 1;
}

std::optional<Block> MemSnapshotDatabase::QueryBlockById(const int64_t blockId, const std::string &deviceId) {
    std::string querySql = "SELECT * FROM {} WHERE {} = ?;";
    querySql = StringUtil::FormatString(querySql, GetBlockTableNameByDeviceId(deviceId), BlockTableColumn::ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed prepared query block sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, bindStartIndex, blockId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Block block = QueryBlockByStep(stmt);
        sqlite3_finalize(stmt);
        return std::make_optional(block);
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

int64_t MemSnapshotDatabase::GetDeviceMaxEntryId(const std::string &deviceId) const {
    if (deviceMaxEntryIdMap.find(deviceId) != deviceMaxEntryIdMap.end()) {
        return deviceMaxEntryIdMap.at(deviceId);
    }
    Server::ServerLog::Warn(LOG_TAG + "Cannot get max entry id for device: %", deviceId);
    return -1;
}

void MemSnapshotDatabase::QueryBlockIdRangeByDeviceIdLazy(
    const std::string &deviceId, int64_t &minBlockId, int64_t &maxBlockId) {
    // 如果缓存中找到了则直接返回
    if (blockIdRangeMap.find(deviceId) != blockIdRangeMap.end()) {
        minBlockId = blockIdRangeMap.at(deviceId).first;
        maxBlockId = blockIdRangeMap.at(deviceId).second;
        return;
    }
    // 未找到则需要重新初始化
    const auto querySql = StringUtil::FormatString("SELECT MIN({}), MAX({}) FROM {};", BlockTableColumn::ID,
        BlockTableColumn::ID, GetBlockTableNameByDeviceId(deviceId));
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed prepared query block id range sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        minBlockId = sqlite3_column_int64(stmt, resultStartIndex);
        maxBlockId = sqlite3_column_int64(stmt, resultStartIndex + 1);
        blockIdRangeMap[deviceId] = std::make_pair(minBlockId, maxBlockId);
    }
    sqlite3_finalize(stmt);
}

void MemSnapshotDatabase::Reset() {
    Server::ServerLog::Info(LOG_TAG + "MemSnapshot db reset.");
    auto databaseList = Timeline::DataBaseManager::Instance().GetAllMemSnapshotDatabase();
    for (auto &db : databaseList) {
        if (db != nullptr && db->IsOpen()) {
            db->CloseDb();
        }
    }
    Timeline::DataBaseManager::Instance().Clear(Timeline::DatabaseType::MEM_SNAPSHOT);
}

bool MemSnapshotDatabase::InitTableDictionaryMap() {
    if (!isOpen) {
        Server::ServerLog::Error(LOG_TAG +
            "Failed to initialize table dictionary map; "
            "database connection has not been established.");
        return false;
    }
    const std::string queryDictionaryTableSql = "SELECT * FROM dictionary;";
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, queryDictionaryTableSql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG +
                "Failed to initialize table dictionary map; "
                "failed to prepare sql. Error: ",
            sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string table = sqlite3_column_string(stmt, col++);
        // 在支持多device特性中，不同device后缀的表会使用相同的dictionary，因此使用同一套tableColumnTag即可
        if (StringUtil::StartWith(table, blockTablePrefix)) {
            table = blockTablePrefix;
        } else if (StringUtil::StartWith(table, traceEntryTablePrefix)) {
            table = traceEntryTablePrefix;
        }
        std::string column = sqlite3_column_string(stmt, col++);
        int colKey = sqlite3_column_int(stmt, col++);
        std::string colValue = sqlite3_column_string(stmt, col++);
        tableDictionaryMap[GetTableColumnTag(table, column)][colKey] = colValue;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool MemSnapshotDatabase::InitDeviceIdsAndMaxEntryIdMap() {
    for (const auto &tableName : traceEntryTableNames) {
        if (!StringUtil::StartWith(tableName, traceEntryTablePrefix)) {
            Server::ServerLog::Error(LOG_TAG +
                    "Failed initialize device max entry id map, "
                    "trace entry table name is not valid: %",
                tableName);
            return false;
        }
        const std::string deviceId = tableName.substr(traceEntryTablePrefix.size());
        const std::string querySql =
            StringUtil::FormatString("SELECT MAX({}) FROM {};", TraceEntryTableColumn::ID, tableName);
        sqlite3_stmt *stmt = nullptr;
        int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            Server::ServerLog::Error(LOG_TAG + "Failed prepared query max entry id sql, error: ", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return false;
        }
        int64_t maxId = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            maxId = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
        deviceMaxEntryIdMap[deviceId] = maxId;
    }
    return true;
}

bool MemSnapshotDatabase::InitContext() {
    // 初始化dictionary
    if (!InitTableDictionaryMap()) {
        Server::ServerLog::Error(LOG_TAG +
            "Failed to initialize mem snapshot database context; "
            "failed to initialize table dictionary map.");
        return false;
    }
    // 初始化maxEntryId及device
    if (!InitDeviceIdsAndMaxEntryIdMap()) {
        Server::ServerLog::Error(LOG_TAG + "Failed to initialize device ids and max entry id map.");
        return false;
    }
    return true;
}

template <typename T> T MemSnapshotDatabase::QueryBlockByStep(sqlite3_stmt *stmt, int startIdx) {
    int col = startIdx;
    T block;
    block.id = sqlite3_column_int64(stmt, col++);
    block.address = static_cast<uint64_t>(sqlite3_column_int64(stmt, col++));
    block.size = static_cast<uint64_t>(sqlite3_column_int64(stmt, col++));
    block.requestedSize = static_cast<uint64_t>(sqlite3_column_int64(stmt, col++));
    block.state = GetRealValueInTableDictionaryMap(
        blockTablePrefix, std::string(BlockTableColumn::STATE), sqlite3_column_int(stmt, col++));
    block.allocEventId = sqlite3_column_int64(stmt, col++);
    block.freeEventId = sqlite3_column_int64(stmt, col++);
    return block;
}

BlockTableItemDTO MemSnapshotDatabase::QueryBlockTableItemByStep(sqlite3_stmt *stmt) {
    BlockTableItemDTO block;
    int col = resultStartIndex;
    block.id = sqlite3_column_int64(stmt, col++);
    block.address = static_cast<uint64_t>(sqlite3_column_int64(stmt, col++));
    block.size = sqlite3_column_double(stmt, col++);
    block.requestedSize = sqlite3_column_double(stmt, col++);
    block.state = GetRealValueInTableDictionaryMap(
        blockTablePrefix, std::string(BlockTableColumn::STATE), sqlite3_column_int(stmt, col++));
    block.allocEventId = sqlite3_column_int64(stmt, col++);
    block.freeEventId = sqlite3_column_int64(stmt, col++);
    return block;
}

TraceEntry MemSnapshotDatabase::QueryTraceEntryByStep(sqlite3_stmt *stmt, const int startIdx) {
    int col = startIdx;
    TraceEntry entry;
    entry.id = sqlite3_column_int64(stmt, col++);
    entry.action = GetRealValueInTableDictionaryMap(
        traceEntryTablePrefix, std::string(TraceEntryTableColumn::ACTION), sqlite3_column_int(stmt, col++));
    entry.address = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.size = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.stream = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.allocated = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.active = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.reserved = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.callstack = sqlite3_column_string(stmt, col++);
    return entry;
}

TraceEntryTableItemDTO MemSnapshotDatabase::QueryTraceEntryTableItemByStep(sqlite3_stmt *stmt) {
    TraceEntryTableItemDTO entry;
    int col = resultStartIndex;
    entry.id = sqlite3_column_int64(stmt, col++);
    entry.action = GetRealValueInTableDictionaryMap(
        traceEntryTablePrefix, std::string(TraceEntryTableColumn::ACTION), sqlite3_column_int(stmt, col++));
    entry.address = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.size = sqlite3_column_double(stmt, col++);
    entry.stream = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
    entry.allocated = sqlite3_column_double(stmt, col++);
    entry.active = sqlite3_column_double(stmt, col++);
    entry.reserved = sqlite3_column_double(stmt, col++);
    entry.callstack = sqlite3_column_string(stmt, col++);
    return entry;
}

std::string MemSnapshotDatabase::GetTableColumnTag(const std::string &tableName, const std::string &colName) {
    return StringUtil::FormatString("{}.{}", tableName, colName);
}

/**
 * 用于为MemSnapshot数据库中的表进行查询时，根据查询参数构建WHERE子句的SQL语句
 * 由于MemSnapshot表中存在类enum结构（即部分列的真实值是通过dictionary表中的映射关系获取的），因此Database基类中的公共方法此处并不完全适用，需要自定义实现
 * @param queryParams 查询参数, 注意：此参数会被修改, 删除非常规filter, 便于后续bind
 * @param tableName 表名
 * @return 构建好的WHERE子句SQL语句
 */
std::string MemSnapshotDatabase::BuildMemSnapshotFiltersParamSql(
    FiltersParam &queryParams, const std::string &tableName) {
    SearchMap normalFilters; // 普通字段过滤设置，可使用Database中的公用方法
    std::string filtersSql;
    for (const auto &[colKey, targetStr] : queryParams.filters) {
        std::string colTag = GetTableColumnTag(tableName, colKey);
        // 不在字典映射表缓存中，则认为是普通字段
        if (tableDictionaryMap.find(colTag) == tableDictionaryMap.end()) {
            normalFilters[colKey] = targetStr;
            continue;
        }
        // 需要额外处理的场景，此处替换为"{colKey} in ({containTargetKeys})"的形式
        const auto &valueMap = tableDictionaryMap[colTag];
        std::vector<int> containTargetKeys;
        for (const auto &[key, value] : valueMap) {
            if (StringUtil::ContainsIgnoreCase(value, targetStr)) {
                containTargetKeys.push_back(key);
            }
        }
        // 由于containTargetKeys均为int类型且为内部定义，不存在sql注入问题，此处不使用参数化查询而直接拼入sql中
        const std::string rangeInSql = StringUtil::join(containTargetKeys, ",");
        filtersSql = StringUtil::StrJoin(filtersSql, StringUtil::FormatString(" AND {} IN ({}) ", colKey, rangeInSql));
    }
    filtersSql = StringUtil::StrJoin(filtersSql, Database::BuildQueryFiltersConditionSql(normalFilters));
    queryParams.filters = normalFilters;
    return filtersSql;
}

/**
 * 用于为MemSnapshot数据库中的表进行基于字段的范围查询时，根据查询参数构建WHERE子句的SQL语句
 * 由于部分列查询时是计算列，因此不能直接使用Database中的BuildQueryRangeFiltersConditionSql方法，需要根据计算列进行范围查询，如各类size
 * @param queryParams 范围查询参数，注意！！！此处原列会被替换为计算列，需要使用copy
 * @return 构建好的WHERE子句SQL语句
 */
std::string MemSnapshotDatabase::BuildMemSnapshotRangeFiltersParamSql(RangeFiltersParam &queryParams) {
    RangeMap withCalculatedColRangeFilters;
    std::string sql;
    for (const auto &[colKey, bounds] : queryParams.rangeFilters) {
        // 不在计算列定义中
        if (CALCULATED_COLUMN_MAP.find(colKey) == CALCULATED_COLUMN_MAP.end()) {
            withCalculatedColRangeFilters[colKey] = bounds;
            continue;
        }
        // 需要替换key为计算列
        auto calculatedCol = StringUtil::FormatString(CALCULATED_COLUMN_MAP[colKey], colKey);
        withCalculatedColRangeFilters[calculatedCol] = bounds;
    }
    queryParams.rangeFilters = withCalculatedColRangeFilters;
    return BuildQueryRangeFiltersConditionSql(queryParams.rangeFilters);
}

int64_t MemSnapshotDatabase::QueryBlocksTable(
    const MemSnapshotBlockParams &queryParams, std::vector<BlockTableItemDTO> &blocks) {
    const int64_t count = QueryBlocksTableCount(queryParams);
    if (count <= 0) {
        return count;
    }
    const std::string queryColumns = GetSelectBlocksTableFullColumns();
    sqlite3_stmt *stmt = BuildQueryBlocksTableByQueryParamsAndBindParam(queryColumns, queryParams);
    if (stmt == nullptr) {
        Server::ServerLog::Error(LOG_TAG + "Failed to query blocks table: failed to prepare sql.");
        return -1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        blocks.push_back(QueryBlockTableItemByStep(stmt));
    }
    sqlite3_finalize(stmt);
    return count;
}

int64_t MemSnapshotDatabase::QueryBlocksTableCount(const MemSnapshotBlockParams &queryParams) {
    int64_t count = 0;
    const std::string queryColumns = " COUNT(*) ";
    sqlite3_stmt *stmt = BuildQueryBlocksTableByQueryParamsAndBindParam(queryColumns, queryParams, false);
    if (stmt == nullptr) {
        Server::ServerLog::Error(LOG_TAG + "Failed to count blocks table with params: failed to prepare sql.");
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return count;
}

std::string MemSnapshotDatabase::GetSelectBlocksTableFullColumns() {
    std::string columns;
    for (auto &columnObj : BlockTableColumn::FIELD_FULL_COLUMNS) {
        if (!columns.empty()) {
            columns.append(", ");
        }
        auto col = std::string(columnObj.key);
        if (CALCULATED_COLUMN_MAP.find(columnObj.key) != CALCULATED_COLUMN_MAP.end()) {
            col = StringUtil::FormatString(CALCULATED_COLUMN_MAP[col], col);
        }
        columns.append(col);
    }
    return columns;
}

std::string MemSnapshotDatabase::BuildQueryBlocksTableConditionSqlByParams(
    MemSnapshotBlockParams &params, bool &eventIdxRangeCondition, bool &filtersCondition) {
    std::string conditionSql = " where 1=1 ";
    // 构造Size参数
    if (params.maxSize > 0) {
        conditionSql.append(StringUtil::FormatString(" AND ({} BETWEEN {} AND {}) ", BlockTableColumn::SIZE,
            std::to_string(params.minSize), std::to_string(params.maxSize)));
    }
    // 构造事件索引范围参数
    if (params.endEventIdx >= params.startEventIdx && params.endEventIdx > 0) {
        eventIdxRangeCondition = true;
        // allocEventId < 0 时视为无限小，freeEventId < 0 时视为无限大
        conditionSql.append(StringUtil::FormatString(" AND (({} < 0 OR {} >= ?) AND ({} < 0 OR {} <= ?)) ",
            BlockTableColumn::FREE_EVENT_ID, BlockTableColumn::FREE_EVENT_ID, BlockTableColumn::ALLOC_EVENT_ID,
            BlockTableColumn::ALLOC_EVENT_ID));
    }
    // 构造过滤参数
    if (!params.filters.empty() || !params.rangeFilters.empty()) {
        filtersCondition = true;
        // 此处可能会修改`params.filters`中对于dictionary字段过滤列
        conditionSql.append(BuildMemSnapshotFiltersParamSql(params, blockTablePrefix));
        // 此处可能会修改`params.rangeFilters`中的计算值范围过滤列，将其替换为计算列
        conditionSql.append(BuildMemSnapshotRangeFiltersParamSql(params));
    }
    return conditionSql;
}

sqlite3_stmt *MemSnapshotDatabase::BuildQueryBlocksTableByQueryParamsAndBindParam(
    const std::string &selectColumns, const MemSnapshotBlockParams &queryParams, const bool withPagination) {
    bool eventIdxRangeCondition = false;
    bool filtersCondition = false;
    auto paramsCopy = queryParams;
    std::string conditionSql =
        BuildQueryBlocksTableConditionSqlByParams(paramsCopy, eventIdxRangeCondition, filtersCondition);
    std::string sql = StringUtil::FormatString(
        "select {} from {} {} ", selectColumns, GetBlockTableNameByDeviceId(queryParams.deviceId), conditionSql);
    // 构造排序参数
    if (!paramsCopy.orderBy.empty()) {
        sql.append(Database::BuildQueryOrderSql(paramsCopy.orderBy, paramsCopy.desc));
    }
    if (withPagination) {
        sql.append(" LIMIT ? OFFSET ? ");
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepare query blocks: failed to prepare sql.");
        return nullptr;
    }
    int bindIdx = bindStartIndex;
    // 绑定事件索引范围参数
    if (eventIdxRangeCondition) {
        sqlite3_bind_int64(stmt, bindIdx++, paramsCopy.startEventIdx);
        sqlite3_bind_int64(stmt, bindIdx++, paramsCopy.endEventIdx);
    }
    // 绑定filters参数
    if (filtersCondition) {
        Database::CommonBindFiltersParams(paramsCopy.filters, stmt, bindIdx);
        Database::CommonBindRangeFiltersParams(paramsCopy.rangeFilters, stmt, bindIdx);
    }
    if (withPagination) {
        // 绑定分页参数
        Database::CommonBindPaginationParams(paramsCopy.pageSize, paramsCopy.currentPage, stmt, bindIdx);
    }
    return stmt;
}

void MemSnapshotDatabase::QueryMemoryRecords(
    const MemSnapshotAllocationParams &queryParams, std::vector<MemoryRecord> &records) {
    std::string querySql = "SELECT {}, {}, {}, {} FROM {} WHERE {} >= 0 ORDER BY {} ASC";
    querySql = StringUtil::FormatString(querySql, TraceEntryTableColumn::ID, TraceEntryTableColumn::ALLOCATED,
        TraceEntryTableColumn::RESERVED, TraceEntryTableColumn::ACTIVE,
        GetTraceEntryTableNameByDeviceId(queryParams.deviceId), TraceEntryTableColumn::ID, TraceEntryTableColumn::ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepared query memory records sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        MemoryRecord record;
        record.id = sqlite3_column_int64(stmt, col++);
        record.allocated = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        record.reserved = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        record.active = NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, col++));
        records.push_back(record);
    }
    sqlite3_finalize(stmt);
}

void MemSnapshotDatabase::QueryMemoryAllocations(
    const std::string &deviceId, std::vector<AllocationRecordDTO> &records) {
    std::string querySql = "SELECT {}, {} FROM {} WHERE {} >= 0";
    querySql = StringUtil::FormatString(querySql, TraceEntryTableColumn::ID, TraceEntryTableColumn::ALLOCATED,
        GetTraceEntryTableNameByDeviceId(deviceId), TraceEntryTableColumn::ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepared query memory records sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        records.emplace_back(sqlite3_column_int64(stmt, resultStartIndex),
            NumberUtil::Int64ToUint64(sqlite3_column_int64(stmt, resultStartIndex + 1)));
    }
    sqlite3_finalize(stmt);
}

int64_t MemSnapshotDatabase::QueryTraceEntriesTable(
    const MemSnapshotEventParams &queryParams, std::vector<TraceEntryTableItemDTO> &entries) {
    const int64_t count = QueryTraceEntriesTableCount(queryParams);
    if (count <= 0) {
        return count;
    }
    const std::string queryColumns = GetSelectTraceEntriesTableFullColumns();
    sqlite3_stmt *stmt = BuildQueryTraceEntriesTableByQueryParamsAndBindParam(queryColumns, queryParams);
    if (stmt == nullptr) {
        Server::ServerLog::Error(LOG_TAG + "Failed to query trace entries table: failed to prepare sql.");
        return -1;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        entries.push_back(QueryTraceEntryTableItemByStep(stmt));
    }
    sqlite3_finalize(stmt);
    return count;
}

int64_t MemSnapshotDatabase::QueryTraceEntriesTableCount(const MemSnapshotEventParams &queryParams) {
    int64_t count = 0;
    std::string queryColumns = " COUNT(*) ";
    sqlite3_stmt *stmt = BuildQueryTraceEntriesTableByQueryParamsAndBindParam(queryColumns, queryParams, false);
    if (stmt == nullptr) {
        Server::ServerLog::Error(LOG_TAG + "Failed to query trace entries table: failed to prepare sql.");
        return -1;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return count;
}

std::string MemSnapshotDatabase::GetSelectTraceEntriesTableFullColumns() {
    std::string columns;
    for (auto &columnObj : TraceEntryTableColumn::FIELD_FULL_COLUMNS) {
        if (!columns.empty()) {
            columns.append(", ");
        }
        auto col = std::string(columnObj.key);
        if (CALCULATED_COLUMN_MAP.find(columnObj.key) != CALCULATED_COLUMN_MAP.end()) {
            col = StringUtil::FormatString(CALCULATED_COLUMN_MAP[col], col);
        }
        columns.append(col);
    }
    return columns;
}

std::string MemSnapshotDatabase::BuildQueryTraceEntriesTableConditionSqlByParams(
    MemSnapshotEventParams &queryParams, bool &eventIdxRangeCondition, bool &filtersCondition) {
    std::string conditionSql = StringUtil::FormatString(" WHERE  {} >= 0 ", TraceEntryTableColumn::ID);
    // 构造事件索引范围参数
    if (queryParams.endEventIdx >= queryParams.startEventIdx && queryParams.endEventIdx > 0) {
        eventIdxRangeCondition = true;
        conditionSql.append(StringUtil::FormatString(" AND ({} BETWEEN ? AND ?) ", TraceEntryTableColumn::ID));
    }
    // 构造过滤参数
    if (!queryParams.filters.empty() || !queryParams.rangeFilters.empty()) {
        filtersCondition = true;
        // 涉及到使用dictionary表进行映射，使用prefix即可
        conditionSql.append(BuildMemSnapshotFiltersParamSql(queryParams, traceEntryTablePrefix));
        // 此处可能会修改`params.rangeFilters`中的计算值范围过滤列，将其替换为计算列
        conditionSql.append(BuildMemSnapshotRangeFiltersParamSql(queryParams));
    }
    return conditionSql;
}

sqlite3_stmt *MemSnapshotDatabase::BuildQueryTraceEntriesTableByQueryParamsAndBindParam(
    const std::string &selectColumns, const MemSnapshotEventParams &queryParams, const bool withPagination) {
    bool eventIdxRangeCondition = false;
    bool filtersCondition = false;
    auto paramsCopy = queryParams;
    std::string conditionSql =
        BuildQueryTraceEntriesTableConditionSqlByParams(paramsCopy, eventIdxRangeCondition, filtersCondition);
    std::string sql = StringUtil::FormatString(
        "select {} from {} {} ", selectColumns, GetTraceEntryTableNameByDeviceId(queryParams.deviceId), conditionSql);
    // 构造排序参数
    if (!paramsCopy.orderBy.empty()) {
        sql.append(Database::BuildQueryOrderSql(paramsCopy.orderBy, paramsCopy.desc));
    }
    if (withPagination) {
        sql.append(" LIMIT ? OFFSET ? ");
    }
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepare query trace entries: failed to prepare sql.");
        return nullptr;
    }
    int bindIdx = bindStartIndex;
    // 绑定事件索引范围参数
    if (eventIdxRangeCondition) {
        sqlite3_bind_int64(stmt, bindIdx++, paramsCopy.startEventIdx);
        sqlite3_bind_int64(stmt, bindIdx++, paramsCopy.endEventIdx);
    }
    // 绑定filters参数
    if (filtersCondition) {
        Database::CommonBindFiltersParams(paramsCopy.filters, stmt, bindIdx);
        Database::CommonBindRangeFiltersParams(paramsCopy.rangeFilters, stmt, bindIdx);
    }
    if (withPagination) {
        // 绑定分页参数
        Database::CommonBindPaginationParams(paramsCopy.pageSize, paramsCopy.currentPage, stmt, bindIdx);
    }
    return stmt;
}

std::optional<TraceEntry> MemSnapshotDatabase::QueryTraceEntryById(const int64_t eventId, const std::string &deviceId) {
    std::string querySql = "SELECT * FROM {} WHERE {} = ?;";
    querySql =
        StringUtil::FormatString(querySql, GetTraceEntryTableNameByDeviceId(deviceId), TraceEntryTableColumn::ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed prepared query trace entry sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, bindStartIndex, eventId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        TraceEntry entry = QueryTraceEntryByStep(stmt);
        sqlite3_finalize(stmt);
        return std::make_optional(entry);
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<TraceEntry> MemSnapshotDatabase::QueryFreeRequestedTraceEntryByBlock(
    const Block &block, const std::string &deviceId) {
    std::string querySql = StringUtil::FormatString("SELECT * FROM {} WHERE {} > ? AND {} = ? AND {} = ?;",
        GetTraceEntryTableNameByDeviceId(deviceId), TraceEntryTableColumn::ID, TraceEntryTableColumn::ADDRESS,
        TraceEntryTableColumn::ACTION);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed prepared query trace entry sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    int bindIdx = bindStartIndex;
    sqlite3_bind_int64(stmt, bindIdx++, block.allocEventId);
    sqlite3_bind_int64(stmt, bindIdx++, block.address);
    // 涉及到使用dictionary，则使用prefix
    int freeRequestedActionKey = GetKeyInTableDictionaryMap(
        traceEntryTablePrefix, std::string(TraceEntryTableColumn::ACTION), TRACE_ENTRY_ACTION_FREE_REQUESTED);
    if (freeRequestedActionKey < 0) {
        Server::ServerLog::Error(LOG_TAG + "Failed to get free requested action key.");
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, bindIdx++, freeRequestedActionKey);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        TraceEntry entry = QueryTraceEntryByStep(stmt);
        sqlite3_finalize(stmt);
        return std::make_optional(entry);
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool MemSnapshotDatabase::QuerySegmentEventsUntil(
    const int64_t eventId, const std::string &deviceId, std::vector<TraceEntry> &events) {
    std::string querySql = "SELECT * FROM {} WHERE {} <= ? AND {} BETWEEN 0 AND 3;";
    querySql = StringUtil::FormatString(
        querySql, GetTraceEntryTableNameByDeviceId(deviceId), TraceEntryTableColumn::ID, TraceEntryTableColumn::ACTION);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(LOG_TAG + "Failed to prepare query segment events sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_int64(stmt, bindStartIndex, eventId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        events.push_back(QueryTraceEntryByStep(stmt));
    }
    sqlite3_finalize(stmt);
    return true;
}

bool MemSnapshotDatabase::QueryActiveBlocksByEventId(
    const int64_t eventId, const std::string &deviceId, std::vector<Block> &blocks) {
    std::string querySql = "SELECT * FROM {} WHERE {} <= ? AND ({} > ? OR {} < 0);";
    querySql = StringUtil::FormatString(querySql, GetBlockTableNameByDeviceId(deviceId), BlockTableColumn::ID,
        BlockTableColumn::FREE_EVENT_ID, BlockTableColumn::FREE_EVENT_ID);
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, querySql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error(
            LOG_TAG + "Failed to prepare query blocks by event id sql, error: ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    int bindIdx = bindStartIndex;
    sqlite3_bind_int64(stmt, bindIdx++, eventId);
    sqlite3_bind_int64(stmt, bindIdx++, eventId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        blocks.push_back(QueryBlockByStep(stmt));
    }
    sqlite3_finalize(stmt);
    return true;
}

template bool MemSnapshotDatabase::QueryAllBlocks<Block>(std::vector<Block> &, const std::string &);
template bool MemSnapshotDatabase::QueryAllBlocks<BlockViewItemDTO>(
    std::vector<BlockViewItemDTO> &, const std::string &);
} // namespace Dic::Module::FullDb
