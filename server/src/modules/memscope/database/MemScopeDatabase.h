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

#ifndef PROFILER_SERVER_MEM_SCOPE_DB_H
#define PROFILER_SERVER_MEM_SCOPE_DB_H

#include "pch.h"
#include "WsSender.h"
#include "Database.h"
#include "TimelineProtocolEvent.h"
#include "ServerLog.h"
#include "TraceTime.h"
#include "ParserStatusManager.h"
#include "MemScopeProtocolRequest.h"
#include "MemScopeEntities.h"
#include "MemScopeDefs.h"
#include "MemScopeTableColumn.h"

namespace Dic {
namespace Module {
namespace FullDb {
using namespace Dic::Protocol;
using namespace Dic::Module::MemScope;
class MemScopeDatabase : public Database {
  public:
    explicit MemScopeDatabase(std::recursive_mutex &sqlMutex) : Database(sqlMutex) {};
    ~MemScopeDatabase() override;
    bool CheckAllTableExist();
    static void Reset();
    bool OpenDb(const std::string &dbPath, bool clearAllTable) override;
    bool HasFinishedParseLastTime();
    bool UpdateParseStatus(const std::string &status);
    bool InitStmt();
    void ReleaseStmt();
    sqlite3_stmt *GetInsertAllocationsStmt(uint64_t allocationsLen);
    sqlite3_stmt *GetInsertBlocksStmt(uint64_t blocksLen);
    sqlite3_stmt *GetUpdatePythonTraceSliceStmt(uint64_t processId);
    bool CreateMemoryAllocationAndBlockTable();
    bool AppendDepthColumnForPythonTraceTables();
    bool DropMemoryAllocationAndBlockTable();
    bool QueryEntireEventsTable(std::vector<MemScopeEvent> &eventDetails);
    int64_t QueryEventsByRequestParams(const MemScopeEventParams &queryParams, std::vector<MemScopeEvent> &events);
    void QueryDeviceIds(std::set<std::string> &deviceIdSet);
    void QueryThreadIdsByProcessId(uint64_t processId, std::vector<uint64_t> &threadIds);
    void QueryMallocOrFreeEventTypeWithDeviceId(std::unordered_map<std::string, std::vector<std::string>> &resultMap);
    void QueryThreadIds(std::vector<uint64_t> &threadIds);
    int64_t QueryMemoryBlocks(
        const MemScopeMemoryBlockParams &queryParams, const bool isTable, std::vector<MemoryBlock> &blocks);
    void QueryMemoryBlocksOwnersReleasedAfterTimestamp(
        const std::string &deviceId, const std::string &eventType, uint64_t timestamp, std::set<std::string> &owners);
    void QueryPythonTrace(const MemScopeThreadPythonTraceParams &queryParams, MemScopePythonTrace &trace);
    void QueryPythonTracesUsingTableName(const std::string &traceTableName,
        const MemScopeThreadPythonTraceParams &queryParams, MemScopePythonTrace &trace);
    uint64_t QueryTotalSizeUntilTimestampUsingOwner(
        const std::string &deviceId, uint64_t timestamp, const std::string &owner);
    void QueryMemoryAllocations(
        const MemScopeMemoryAllocationParams &queryParams, std::vector<MemoryAllocation> &allocations);
    std::optional<MemoryAllocation> QueryLatestAllocationWithinTimestamp(
        const std::string &deviceId, const std::string &eventType, uint64_t timestamp);
    std::optional<MemoryAllocation> QueryNextAllocationAfterTimestamp(
        const std::string &deviceId, const std::string &eventType, uint64_t timestamp);
    void QueryAllDeviceExtremumTimestamp(std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> &extreTsMap);
    void QueryEventsByGroupId(
        const uint64_t groupId, const std::string &deviceId, bool relativeTime, std::vector<MemScopeEvent> &events);
    std::vector<std::string> GetPythonTraceTables();
    void UpdatePythonTraceSlice(const PythonTraceSlice &slice);
    void UpdatePythonTraceSliceList(const std::vector<PythonTraceSlice> &slices, uint64_t processId);
    void InsertMemoryAllocationList(const std::vector<MemoryAllocation> &allocList);
    void InsertMemoryAllocation(const MemoryAllocation &alloc);
    void InsertMemoryBlockList(const std::vector<MemoryBlock> &blocklist);
    void InsertMemoryBlock(const MemoryBlock &block);
    void FlushMemoryBlocksCache();
    void FlushMemoryAllocationsCache();
    void FlushPythonTraceCache();
    uint64_t GetGlobalMinTimestamp() const;
    uint64_t GetGlobalMaxTimestamp() const;

    bool withCallStackC = false;
    bool withCallStackPython = false;

  private:
    int64_t QueryMemoryEventsByStep(
        sqlite3_stmt *stmt, std::vector<MemScopeEvent> &events, const bool withExtraCountCol);
    int64_t QueryMemoryBlocksByStep(sqlite3_stmt *stmt, std::vector<MemoryBlock> &blocks, const bool withExtraCountCol);
    bool QueryMemoryAllocationsByStep(sqlite3_stmt *stmt, std::vector<MemoryAllocation> &allocations);
    bool QueryMemoryPythonTracesByStep(sqlite3_stmt *stmt, MemScopePythonTrace &trace);
    std::string BuildQueryEventsConditionSqlByParams(
        const MemScopeEventParams &queryParams, bool &timeCondition, bool &filtersCondition);
    std::string AppendInefficientBlockColumnSql(
        const std::string &selectColumn, const MemScopeMemoryBlockParams &queryParams);
    std::string BuildQueryBlocksConditionSqlByParams(const MemScopeMemoryBlockParams &queryParams,
        bool onlyAllocOrFreeInTimeRange, bool &timeCondition, bool &filtersCondition);
    sqlite3_stmt *BuildQueryEventsByQueryParamsAndBindParam(
        std::string &selectColumns, const MemScopeEventParams &queryParams);
    sqlite3_stmt *BuildQueryBlocksByQueryParamsAndBindParam(
        const std::string &selectColumns, const MemScopeMemoryBlockParams &queryParams, const bool isTable);

    bool QueryAndSetGlobalExtremumTimestamp();
    bool CheckGlobalExtremumTimestampValid() const;
    bool ExecuteQueryAndSetGlobalExtremumTimestamp(const std::string &sql);
    uint64_t GetProcessIdByPythonTraceTableName(const std::string &tableName);
    std::string GetCreateMemoryAllocationTableSql();
    std::string GetCreateMemoryBlockTableSql();
    std::string GetSelectEventsFullColumns(const bool relativeTime);
    std::string GetSelectBlocksFullColumns(const bool relativeTime);
    std::vector<std::string> GetAlterPythonTraceTablesAddDepthColumnSql();
    bool SetCallStackExistsFlagByCheckColumn();

    // memscope dump内存表，可能为leaks_dump(旧数据)或memscope_dump(新数据)
    std::string memScopeDumpTable;
    // 内存折线图数据库中存储表名为memory_alloc, 包含优化前、后
    const std::string memoryAllocationTable = "memory_allocation";
    // 内存块图数据
    const std::string memoryBlockTable = "memory_block";
    // 火焰图数据
    const std::string pythonTraceTablePrefix = "python_trace_";
    // 调用栈列前缀
    const std::string callStackPrefix = "Call Stack";

    // 低效显存列别名标识
    const std::string lazyUsedCol = "_lazyUsed";
    const std::string delayedFreeCol = "_delayedFree";
    const std::string longIdleCol = "_longIdle";

    const std::string allocationColumnPattern =
        StringUtil::GenerateColumnString(MemoryAllocationTableColumn::FULL_COLUMNS_WITHOUT_ID);
    const std::string allocationValuePattern =
        StringUtil::CreateQuestionMarkString(std::size(MemoryAllocationTableColumn::FULL_COLUMNS_WITHOUT_ID));
    const std::string blockColumnPattern =
        StringUtil::GenerateColumnString(MemoryBlockTableColumn::FULL_COLUMNS_WITHOUT_ID);
    const std::string blockValuePattern =
        StringUtil::CreateQuestionMarkString(std::size(MemoryBlockTableColumn::FULL_COLUMNS_WITHOUT_ID));

    // Global Extremum Timestamp
    uint64_t globalMinTimestamp = INT64_MAX;
    uint64_t globalMaxTimestamp = 0;

    // Parse status info
    const std::string memScopeParseStatus = "MEM_SCOPE_PARSE_STATUS";
    const uint64_t cacheSize = 100;

    bool hasInitStmt = false;

    sqlite3_stmt *insertAllocationStmt = nullptr;
    sqlite3_stmt *insertBlockStmt = nullptr;
    std::unordered_map<uint64_t, sqlite3_stmt *> updatePythonTraceDepthStmtPidMap;
    std::mutex cacheMutex;
    std::vector<MemoryAllocation> allocationCache;
    std::vector<MemoryBlock> blockCache;
    std::unordered_map<uint64_t, std::vector<PythonTraceSlice>> slicePidCache;
};

} // namespace FullDb
} // namespace Module
} // namespace Dic

#endif // PROFILER_SERVER_MEM_SCOPE_DB_H
