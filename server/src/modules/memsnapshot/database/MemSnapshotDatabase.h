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
#ifndef PROFILER_SERVER_MEMSNAPSHOTDATABASE_H
#define PROFILER_SERVER_MEMSNAPSHOTDATABASE_H

#include "pch.h"
#include "Database.h"
#include "MemSnapshotResponseDTO.h"
#include "MemSnapshotProtocolRequest.h"
#include "MemSnapshotDefs.h"
#include "MemSnapshotTableColumn.h"

namespace Dic::Module::FullDb {
using namespace Dic::Protocol;
using namespace Dic::Module::MemSnapshot;

class MemSnapshotDatabase : public Database {
  public:
    explicit MemSnapshotDatabase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    bool CheckAllTableExist();
    bool OpenDbReadOnly(const std::string &dbPath);

    // API
    // 查询所有的block，用于内存块生命周期图展示
    template <typename T = Block> bool QueryAllBlocks(std::vector<T> &blocks, const std::string &deviceId);
    // 基于id查询单个block的详细信息
    std::optional<Block> QueryBlockById(int64_t blockId, const std::string &deviceId);
    // 查询指定事件ID时活跃的blocks
    bool QueryActiveBlocksByEventId(int64_t eventId, const std::string &deviceId, std::vector<Block> &blocks);
    // 查询blocks表格数据，支持分页、过滤、排序等
    int64_t QueryBlocksTable(const MemSnapshotBlockParams &queryParams, std::vector<BlockTableItemDTO> &blocks);
    bool QueryPotentialLeakStats(const MemSnapshotLeakStatsParams &queryParams, MemSnapshotLeakStatsDTO &stats);

    static void Reset();

    // 查询内存记录（allocated/reserved/active）用于内存总量曲线
    void QueryMemoryRecords(const MemSnapshotAllocationParams &queryParams, std::vector<MemoryRecord> &records);
    void QueryMemoryAllocations(const std::string &deviceId, std::vector<AllocationRecordDTO> &records);
    // 查询trace_entry表格数据，支持分页、过滤、排序等
    int64_t QueryTraceEntriesTable(
        const MemSnapshotEventParams &queryParams, std::vector<TraceEntryTableItemDTO> &entries);
    // 查询trace_entry列表数据（分片），用于事件生命周期图展示
    int64_t QueryTraceEntriesList(const PaginationParam &paginationParam, const std::string &deviceId,
        std::vector<TraceEntryListItemDTO> &entries);
    // 基于id查询单个trace_entry的详细信息
    std::optional<TraceEntry> QueryTraceEntryById(const int64_t eventId, const std::string &deviceId);
    // 查询内存块的freeRequested事件
    std::optional<TraceEntry> QueryFreeRequestedTraceEntryByBlock(const Block &block, const std::string &deviceId);

    // 查询指定事件ID之前的segment相关事件（segment_alloc, segment_free, segment_map, segment_unmap）
    bool QuerySegmentEventsUntil(const int64_t eventId, const std::string &deviceId, std::vector<TraceEntry> &events);

    // 字典表
    static std::string GetTableColumnTag(const std::string &tableName, const std::string &colName);
    std::string GetRealValueInTableDictionaryMap(const std::string &tableName, const std::string &colName, int intVal);
    int GetKeyInTableDictionaryMap(
        const std::string &tableName, const std::string &colName, const std::string &realVal);

    // 获取当前实例中的device_id列表
    bool IsDeviceIdValid(const std::string &deviceId);
    std::vector<std::string> GetDeviceIds();
    std::string GetBlockTableNameByDeviceId(const std::string &deviceId);
    std::string GetTraceEntryTableNameByDeviceId(const std::string &deviceId);
    // 查询最大事件id
    [[nodiscard]] int64_t GetDeviceMaxEntryId(const std::string &deviceId) const;
    // 懒查询指定device下的block_id范围
    void QueryBlockIdRangeByDeviceIdLazy(const std::string &deviceId, int64_t &minBlockId, int64_t &maxBlockId);

  private:
    static inline const std::string LOG_TAG = "[MemSnapshotDb] ";
    // issue #116中，为支持多device的场景，block和trace_entry表的table_name需要增加deviceId后缀
    const std::string blockTablePrefix = "block_";
    std::vector<std::string> blockTableNames;
    const std::string traceEntryTablePrefix = "trace_entry_";
    std::vector<std::string> traceEntryTableNames;
    const std::string dictionaryTable = "dictionary";
    std::map<std::string, int64_t> deviceMaxEntryIdMap;
    std::map<std::string, std::map<int, std::string>> tableDictionaryMap;
    std::map<std::string, std::pair<int64_t, int64_t>> blockIdRangeMap;
    inline static std::map<std::string_view, std::string> CALCULATED_COLUMN_MAP = {
        {TraceEntryTableColumn::SIZE, "ROUND({}/1024.0, 3)"},
        {TraceEntryTableColumn::ALLOCATED, "ROUND({}/1024.0, 3)"},
        {TraceEntryTableColumn::ACTIVE, "ROUND({}/1024.0, 3)"},
        {TraceEntryTableColumn::RESERVED, "ROUND({}/1024.0, 3)"},
        {BlockTableColumn::REQUESTED_SIZE, "ROUND({}/1024.0, 3)"},
    };
    bool InitTableDictionaryMap();
    bool InitDeviceIdsAndMaxEntryIdMap();
    bool InitContext();
    template <typename T = Block> T QueryBlockByStep(sqlite3_stmt *stmt, int startIdx = 0);
    BlockTableItemDTO QueryBlockTableItemByStep(sqlite3_stmt *stmt);
    TraceEntry QueryTraceEntryByStep(sqlite3_stmt *stmt, int startIdx = 0);
    TraceEntryTableItemDTO QueryTraceEntryTableItemByStep(sqlite3_stmt *stmt);

    std::string BuildMemSnapshotFiltersParamSql(FiltersParam &queryParams, const std::string &tableName);
    std::string BuildMemSnapshotRangeFiltersParamSql(RangeFiltersParam &queryParams);

    // Blocks table query helpers
    int64_t QueryBlocksTableCount(const MemSnapshotBlockParams &queryParams);
    static std::string GetSelectBlocksTableFullColumns();
    std::string BuildQueryBlocksTableConditionSqlByParams(
        MemSnapshotBlockParams &queryParams, bool &eventIdxRangeCondition, bool &filtersCondition);
    [[nodiscard]] sqlite3_stmt *BuildQueryBlocksTableByQueryParamsAndBindParam(
        const std::string &selectColumns, const MemSnapshotBlockParams &queryParams, bool withPagination = true);
    // TraceEntry table query helpers
    int64_t QueryTraceEntriesTableCount(const MemSnapshotEventParams &queryParams);
    static std::string GetSelectTraceEntriesTableFullColumns();
    std::string BuildQueryTraceEntriesTableConditionSqlByParams(
        MemSnapshotEventParams &queryParams, bool &eventIdxRangeCondition, bool &filtersCondition);
    [[nodiscard]] sqlite3_stmt *BuildQueryTraceEntriesTableByQueryParamsAndBindParam(
        const std::string &selectColumns, const MemSnapshotEventParams &queryParams, bool withPagination = true);
};
}

#endif //PROFILER_SERVER_MEMSNAPSHOTDATABASE_H
