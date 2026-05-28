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
#include "JsonUtil.h"
#include "ServerLog.h"
#include "pch.h"
#include "TableDefs.h"
#include "TraceDatabaseHelper.h"
#include "TraceDatabaseSqlConst.h"
#include "TraceTime.h"
#include "TrackInfoManager.h"
#include "FullDbEnumUtil.h"
#include "TextTraceDatabase.h"

// clang-format off
namespace {
Trace::Slice ConvertSliceDtoToTraceSlice(const SliceDto &dto) {
    Trace::Slice slice;
    slice.ts = dto.timestamp; // timestamp → ts
    slice.dur = dto.duration; // duration → dur
    slice.name = dto.name;
    slice.trackId = dto.trackId; // trackId → trackId
    slice.cat = dto.cat.empty() ? std::nullopt : std::optional(dto.cat); // 处理 optional
    slice.args = dto.args.empty() ? std::nullopt : std::optional(dto.args);
    slice.cname = ""; // 根据实际需求填充（原 InsertSQL 有 cname）
    slice.end = dto.timestamp + dto.duration; // end_time = timestamp + duration
    slice.flagId = dto.flagId;
    // 注：pid/tid/cardId/metaType 等未在 InsertSQL 中使用，按需扩展
    return slice;
}

const std::string TEXT_PYTHON_STACK_THREAD_ID_PREFIX = Dic::Protocol::PYTHON_STACK_THREAD_ID_PREFIX + "text:";
}

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;
TextTraceDatabase::TextTraceDatabase(std::recursive_mutex &sqlMutex) : VirtualTraceDatabase(sqlMutex) {
    if (sliceAnalyzerPtr == nullptr) {
        sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    }
    if (flowAnalyzerPtr == nullptr) {
        flowAnalyzerPtr = std::make_unique<FlowAnalyzer>();
    }
}

TextTraceDatabase::~TextTraceDatabase() {
    try {
        CommitData();
        ReleaseStmt();
        sliceAnalyzerPtr = nullptr;
        flowAnalyzerPtr = nullptr;
    } catch (const std::exception &) {
        // do nothing
    }
}

bool TextTraceDatabase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    if (!Database::OpenDb(dbPath, clearAllTable)) {
        return false;
    }
    return SetConfig() && CheckAndResetDatabaseOnVersionChange();
}

bool TextTraceDatabase::InitStmt() {
    if (initStmt) {
        return true;
    }
    initStmt = true;
    return InitSliceFlowCounterStmt() && InitProcessThreadStmt();
}

bool TextTraceDatabase::InitSliceFlowCounterStmt() {
    std::string sql = TextSqlConstant::GetInsertSliceSql();
    insertSliceStmt = CreatPreparedStatement(sql);
    sql = TextSqlConstant::GetInsertFlowSql();
    insertFlowStmt = CreatPreparedStatement(sql);
    sql = TextSqlConstant::GetInsertCounterSql();
    insertCounterStmt = CreatPreparedStatement(sql);
    if (insertSliceStmt == nullptr || insertFlowStmt == nullptr || insertCounterStmt == nullptr) {
        ServerLog::Error("Failed to prepare slice statement.");
        return false;
    }
    return true;
}

bool TextTraceDatabase::InitProcessThreadStmt() {
    std::string sql = UPDATE_PROCESS_NAME_SQL;
    updateProcessNameStmt = CreatPreparedStatement(sql);
    sql = UPDATE_PROCESS_LABLE_SQL;
    updateProcessLabelStmt = CreatPreparedStatement(sql);
    sql = UPDATE_PROCESS_SORTINDEX_SQL;
    updateProcessSortIndexStmt = CreatPreparedStatement(sql);
    sql = UPDATE_THREAD_NAME_SQL;
    updateThreadNameStmt = CreatPreparedStatement(sql);
    sql = UPDATE_THREAD_SORTINDEX_SQL;
    updateThreadSortIndexStmt = CreatPreparedStatement(sql);
    sql = SIMULATION_INSERT_THREAD_NAME_AND_SORT_INDEX_SQL;
    simulationInsertThreadNameStmt = CreatPreparedStatement(sql);
    sql = SIMULATION_INSERT_PROCESS_NAME_SQL;
    simulationInsertProcessNameStmt = CreatPreparedStatement(sql);
    if (updateProcessNameStmt == nullptr || updateProcessLabelStmt == nullptr ||
        updateProcessSortIndexStmt == nullptr || updateThreadNameStmt == nullptr ||
        updateThreadSortIndexStmt == nullptr || simulationInsertThreadNameStmt == nullptr ||
        simulationInsertProcessNameStmt == nullptr) {
        ServerLog::Error("Failed to prepare process and thread statement.");
        return false;
    }
    return true;
}

void TextTraceDatabase::ReleaseStmt() {
    if (!initStmt) {
        return;
    }
    initStmt = false;
    // stmt对象需要在关闭数据库之前释放
    insertSliceStmt = nullptr;
    updateProcessNameStmt = nullptr;
    updateProcessLabelStmt = nullptr;
    updateProcessSortIndexStmt = nullptr;
    updateThreadNameStmt = nullptr;
    updateThreadSortIndexStmt = nullptr;
    insertFlowStmt = nullptr;
    insertCounterStmt = nullptr;
    simulationInsertThreadNameStmt = nullptr;
    simulationInsertProcessNameStmt = nullptr;
    insertFtraceStatStmt = nullptr;
}

bool TextTraceDatabase::SetConfig() {
    // PRAGMA case_sensitive_like=1; 设置数据库大小写敏感。 不会写入数据库的配置，不会造成死锁
    return Database::SetConfig() && ExecSql("PRAGMA case_sensitive_like=1;");
}

bool TextTraceDatabase::CreateTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to set config. Database is not open.");
        return false;
    }
    std::string sql = CREATE_TABLE_SQL;
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool TextTraceDatabase::DropTable() const {
    std::vector<std::string> tables = {sliceTable, threadTable, processTable, flowTable, counterTable};
    std::unique_lock<std::recursive_mutex> lock(mutex);
    return DropSomeTables(tables);
}

bool TextTraceDatabase::CreateIndex() {
    auto start = std::chrono::system_clock::now();
    if (!isOpen) {
        ServerLog::Error("Failed to creat index. Database is not open.");
        return false;
    }
    std::string sql = CREATE_INDEX_SQL;
    std::unique_lock<std::recursive_mutex> lock(mutex);
    ExecSql(sql);
    auto dur = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - start);
    ServerLog::Info("Creating index end. time:", dur.count());
    return true;
}

bool TextTraceDatabase::InsertSlice(const Trace::Slice &event) {
    sliceCache.emplace_back(event);
    if (sliceCache.size() == CACHE_SIZE) {
        InsertSliceList(sliceCache);
        sliceCache.clear();
    }
    return true;
}

bool TextTraceDatabase::InsertSliceList(const std::vector<Trace::Slice> &eventList) {
    std::unique_ptr<SqlitePreparedStatement> stmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> &refStmt = (eventList.size() == CACHE_SIZE) ? insertSliceStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetSliceStmt(eventList.size()); // 数据长度不是预设的长度则新建一个对象
    } else {
        refStmt->Reset(); // 数据长度是预设长度，需要Reset后再使用
    }
    if (refStmt == nullptr) {
        ServerLog::Error("Failed to get slice stmt.");
        return false;
    }
    for (const auto &event : eventList) {
        // 这里需要极限性能，所以直接调用原生接口
        sqlite3_bind_int64(refStmt->stmt, refStmt->bindIndex++, event.ts); // 第1个参数
        sqlite3_bind_int64(refStmt->stmt, refStmt->bindIndex++, event.dur); // 第2个参数
        sqlite3_bind_text(
            refStmt->stmt, refStmt->bindIndex++, event.name.c_str(), event.name.size(), SQLITE_STATIC); // 第3个参数
        sqlite3_bind_int64(refStmt->stmt, refStmt->bindIndex++, static_cast<int64_t>(event.trackId)); // 第4个参数
        if (event.cat.has_value()) {
            sqlite3_bind_text(refStmt->stmt, refStmt->bindIndex++, event.cat.value().c_str(), event.cat.value().size(),
                SQLITE_STATIC); // 第5个参数
        } else {
            sqlite3_bind_null(refStmt->stmt, refStmt->bindIndex++); // 第5个参数
        }
        if (event.args.has_value()) {
            sqlite3_bind_text(refStmt->stmt, refStmt->bindIndex++, event.args.value().c_str(),
                event.args.value().size(), SQLITE_STATIC); // 第6个参数
        } else {
            sqlite3_bind_null(refStmt->stmt, refStmt->bindIndex++); // 第6个参数
        }
        sqlite3_bind_text(
            refStmt->stmt, refStmt->bindIndex++, event.cname.c_str(), event.cname.size(), SQLITE_STATIC); // 第7个参数
        sqlite3_bind_int64(refStmt->stmt, refStmt->bindIndex++, event.end); // 第8个参数
        sqlite3_bind_text(
            refStmt->stmt, refStmt->bindIndex++, event.flagId.c_str(), event.flagId.size(), SQLITE_STATIC); // 第9个参数
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert slice data fail. ", refStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::ReplaceAllSlices(const std::vector<SliceDto> &slices) {
    // ===== 1. 预转换（锁外执行，减少锁持有时间）=====
    std::vector<Trace::Slice> traceSlices;
    traceSlices.reserve(slices.size());
    for (const auto &dto : slices) {
        traceSlices.emplace_back(ConvertSliceDtoToTraceSlice(dto)); // 见下方转换函数
    }

    // ===== 2. 全程加锁 + 事务 =====
    std::unique_lock<std::recursive_mutex> lock(mutex); // 与 InsertSliceList 递归锁兼容

    if (sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
        ServerLog::Error("Begin transaction failed: ", sqlite3_errmsg(db));
        return false;
    }

    // ===== 3. 清空（单文件场景：安全无 WHERE）=====
    if (sqlite3_exec(db, "DELETE FROM slice", nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        ServerLog::Error("Delete slices failed: ", sqlite3_errmsg(db));
        return false;
    }

    // ===== 4. 分批次复用 InsertSliceList（无缝对接现有逻辑）=====
    bool allSuccess = true;
    const size_t total = traceSlices.size();
    for (size_t i = 0; i < total && allSuccess; i += CACHE_SIZE) {
        size_t batchEnd = std::min(i + CACHE_SIZE, total);
        std::vector<Trace::Slice> batch(traceSlices.begin() + i, traceSlices.begin() + batchEnd);
        if (!InsertSliceList(batch)) { // 直接复用现有高性能插入
            allSuccess = false;
        }
    }

    // ===== 5. 提交/回滚 =====
    if (allSuccess && sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK) {
        ServerLog::Debug("Replaced ", total, " slices atomically");
        return true;
    }

    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    ServerLog::Error("Slice replacement failed, rolled back");
    return false;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetSliceStmt(uint64_t paramLen) {
    std::string sql = "INSERT INTO " + sliceTable +
        " (timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id) VALUES (?,?,?,?,?,?,?,?,?)";
    for (uint64_t i = 0; i < paramLen - 1; ++i) {
        sql.append(",(?,?,?,?,?,?,?,?,?)");
    }
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::UpdateProcessName(const Trace::MetaData &event) {
    if (updateProcessNameStmt == nullptr) {
        ServerLog::Error("Update process name fail. ");
        return false;
    }
    updateProcessNameStmt->Reset();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!updateProcessNameStmt->Execute(event.pid, event.args.name)) {
        ServerLog::Error("Update process name fail. ", updateProcessNameStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::UpdateProcessLabel(const Trace::MetaData &event) {
    if (updateProcessLabelStmt == nullptr) {
        ServerLog::Error("Update process label fail. ");
        return false;
    }
    updateProcessLabelStmt->Reset();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!updateProcessLabelStmt->Execute(event.pid, event.args.labels)) {
        ServerLog::Error("Update process label fail. ", updateProcessLabelStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::UpdateProcessSortIndex(const Trace::MetaData &event) {
    if (updateProcessSortIndexStmt == nullptr) {
        ServerLog::Error("Update process sort index fail. ");
        return false;
    }
    updateProcessSortIndexStmt->Reset();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!updateProcessSortIndexStmt->Execute(event.pid, event.args.sortIndex)) {
        ServerLog::Error("Update process sort index fail. ", updateProcessSortIndexStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::AddSimulationThreadCache(Trace::ThreadEvent &&event) {
    simulationThreadInfoCache.insert(std::move(event));
    return true;
}

bool TextTraceDatabase::AddSimulationProcessCache(Trace::ProcessEvent &&event) {
    simulationProcessInfoCache.insert(std::move(event));
    return true;
}

bool TextTraceDatabase::InsertSimulationThreadList() {
    if (simulationInsertThreadNameStmt == nullptr) {
        ServerLog::Error("Insert thread info fail. ");
        return false;
    }
    for (const auto &item : simulationThreadInfoCache) {
        simulationInsertThreadNameStmt->Reset();
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if (!simulationInsertThreadNameStmt->Execute(
                item.trackId, item.tid, item.pid, item.threadName, item.threadSortIndex)) {
            ServerLog::Error("Insert thread info fail. ", simulationInsertThreadNameStmt->GetErrorMessage());
            return false;
        }
    }
    return true;
}

bool TextTraceDatabase::InsertSimulationProcessList() {
    if (simulationInsertProcessNameStmt == nullptr) {
        ServerLog::Error("Update process info fail. ");
        return false;
    }
    for (const auto &item : simulationProcessInfoCache) {
        simulationInsertProcessNameStmt->Reset();
        std::unique_lock<std::recursive_mutex> lock(mutex);
        if (!simulationInsertProcessNameStmt->Execute(item.pid, item.processName)) {
            ServerLog::Error("Update process info fail. ", simulationInsertProcessNameStmt->GetErrorMessage());
            return false;
        }
    }
    return true;
}

bool TextTraceDatabase::UpdateThreadName(const Trace::MetaData &event) {
    if (updateThreadNameStmt == nullptr) {
        ServerLog::Error("Update thread name fail. ");
        return false;
    }
    updateThreadNameStmt->Reset();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!updateThreadNameStmt->Execute(event.trackId, event.tid, event.pid, event.args.name)) {
        ServerLog::Error("Update thread name fail. ", updateThreadNameStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::UpdateThreadSortIndex(const Trace::MetaData &event) {
    if (updateThreadSortIndexStmt == nullptr) {
        ServerLog::Error("Update thread sort index fail. ");
        return false;
    }
    updateThreadSortIndexStmt->Reset();
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!updateThreadSortIndexStmt->Execute(event.trackId, event.args.sortIndex)) {
        ServerLog::Error("Update thread sort index fail. ", updateThreadSortIndexStmt->GetErrorMessage());
        return false;
    }
    return true;
}

bool TextTraceDatabase::InsertFlow(const Trace::Flow &event) {
    flowCache.emplace_back(event);
    if (flowCache.size() == CACHE_SIZE) {
        InsertFlowList(flowCache);
        flowCache.clear();
    }
    return true;
}

bool TextTraceDatabase::InsertFlowList(const std::vector<Trace::Flow> &eventList) {
    std::unique_ptr<SqlitePreparedStatement> stmt;
    std::unique_ptr<SqlitePreparedStatement> &refStmt = (eventList.size() == CACHE_SIZE) ? insertFlowStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetFlowStmt(eventList.size()); // 数据长度不是预设的长度则新建一个对象
    } else {
        refStmt->Reset(); // 数据长度是预设长度，需要Reset后再使用
    }
    if (refStmt == nullptr) {
        ServerLog::Error("Failed to get flow stmt.");
        return false;
    }
    for (const auto &event : eventList) {
        refStmt->BindParams(event.flowId, event.name, event.trackId, event.ts, event.cat, event.type);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert flow fail. ", refStmt->GetErrorMessage());
        return false;
    }
    return true;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetFlowStmt(uint64_t paramLen) {
    std::string sql =
        "INSERT INTO " + flowTable + " (flow_id, name, track_id, timestamp, cat, type)" + " VALUES (?,?,?,?,?,?)";
    for (uint64_t i = 0; i < paramLen - 1; ++i) {
        sql.append(",(?,?,?,?,?,?)");
    }
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::InsertCounter(const Trace::Counter &event) {
    counterCache.emplace_back(event);
    if (counterCache.size() == CACHE_SIZE) {
        InsertCounterList(counterCache);
        counterCache.clear();
    }
    return true;
}

bool TextTraceDatabase::InsertCounterList(const std::vector<Trace::Counter> &eventList) {
    std::unique_ptr<SqlitePreparedStatement> stmt;
    std::unique_ptr<SqlitePreparedStatement> &refStmt = (eventList.size() == CACHE_SIZE) ? insertCounterStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetCounterStmt(eventList.size()); // 数据长度不是预设的长度则新建一个对象
    } else {
        refStmt->Reset(); // 数据长度是预设长度，需要Reset后再使用
    }
    if (refStmt == nullptr) {
        ServerLog::Error("Failed to get counter stmt.");
        return false;
    }
    for (const auto &event : eventList) {
        refStmt->BindParams(event.name, event.pid, event.ts, event.cat, event.args);
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert counter data fail. ", sqlite3_errmsg(db));
        return false;
    }
    return true;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetCounterStmt(uint64_t paramLen) {
    std::string sql = "INSERT INTO " + counterTable + " (name, pid, timestamp, cat, args)" + " VALUES (?,?,?,?,?)";
    for (uint64_t i = 0; i < paramLen - 1; ++i) {
        sql.append(",(?,?,?,?,?)");
    }
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::QueryThreadTracesSummary(const Protocol::UnitThreadTracesSummaryParams &requestParams,
    Protocol::UnitThreadTracesSummaryBody &responseBody, uint64_t minTimestamp) {
    if (IsHardwareMetricsUnit(requestParams.processId, requestParams.metaType)) {
        return true;
    }
    const int64_t maxDataCount = 30000;
    uint64_t unitTime =
        (requestParams.endTime - requestParams.startTime) / maxDataCount; // 校验过保证 endTime > startTime
    unitTime = unitTime == 0 ? 1 : unitTime;
    std::vector<uint64_t> trackIds = QueryAllTrackIdsByPid(requestParams.processId);
    std::string sql = TextSqlConstant::GetSummarySliceSql(trackIds.size());
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query thread traces summary failed to prepare sql.", GetLastError());
        return false;
    }
    for (const auto &item : trackIds) {
        stmt->BindParams(item);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query thread traces summary failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    TraceDatabaseHelper::QueryAllSliceInRangeByTrackIdHelper(resultSet, unitTime, minTimestamp, responseBody);
    return true;
}

bool TextTraceDatabase::QueryThreads(const Protocol::UnitThreadsParams &requestParams,
    Protocol::UnitThreadsBody &responseBody, uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) {
    std::vector<CompeteSliceDomain> competeSliceVec;
    std::map<std::string, uint64_t> selfTimeKeyValue;
    SliceQuery sliceQuery;
    sliceQuery.rankId = requestParams.rankId;
    sliceQuery.minTimestamp = minTimestamp;
    sliceQuery.startTime = requestParams.startTime;
    sliceQuery.endTime = requestParams.endTime;
    sliceQuery.startDepth = requestParams.startDepth;
    sliceQuery.endDepth = requestParams.endDepth;
    sliceQuery.metaType = PROCESS_TYPE::TEXT;
    /*
     遍历metaDataList,这里不在一个sql里查询出来是为了以后预留pid.tid删选
    */
    for (size_t i = 0; i < requestParams.metadataList.size(); i++) {
        const Dic::Protocol::Metadata &metadata = requestParams.metadataList.at(i);
        sliceQuery.tid = metadata.tid;
        sliceQuery.pid = metadata.pid;
        sliceQuery.trackId = trackIdList[i];
        std::string error;
        if (!sliceQuery.QueryThreadsCheck(error)) {
            ServerLog::Error(error);
            continue;
        }
        size_t sliceStartIndex = competeSliceVec.size();
        sliceAnalyzerPtr->ComputeSliceDomainVecAndSelfTimeByTimeRange(sliceQuery, competeSliceVec, selfTimeKeyValue,
                                                                      metadata.isPythonStack);
        if (metadata.isPythonStack) {
            std::string pythonStackMetaType = ENUM_TO_STR(PROCESS_TYPE::PYTHON_STACK).value_or("");
            for (size_t index = sliceStartIndex; index < competeSliceVec.size(); ++index) {
                competeSliceVec[index].metaType = pythonStackMetaType;
            }
        }
    }

    if (competeSliceVec.empty()) {
        responseBody.emptyFlag = true;
        return true;
    }
    TraceDatabaseHelper::ReduceThread(competeSliceVec, selfTimeKeyValue, responseBody);
    return true;
}

std::vector<FlowDetailDto> TextTraceDatabase::QuerySingleFlowDetail(const std::string &flowId) {
    std::vector<FlowDetailDto> flowDetailVec;
    auto stmt = CreatPreparedStatement(QUERY_FLOW_BY_FLOWID_SQL);
    if (stmt == nullptr) {
        ServerLog::Error("Query single flow detail failed to prepare sql.");
        return flowDetailVec;
    }
    auto resultSet = stmt->ExecuteQuery(flowId);
    if (resultSet == nullptr) {
        ServerLog::Error("Query single flow detail failed to get result set.", stmt->GetErrorMessage());
        return flowDetailVec;
    }
    while (resultSet->Next()) {
        FlowDetailDto flowDetailDto{};
        flowDetailDto.name = resultSet->GetString("name");
        flowDetailDto.cat = resultSet->GetString("cat");
        flowDetailDto.flowId = resultSet->GetString("flowId");
        flowDetailDto.flowTimestamp = resultSet->GetUint64("timestamp");
        flowDetailDto.type = resultSet->GetString("type");
        flowDetailDto.trackId = resultSet->GetUint64("trackId");
        flowDetailVec.emplace_back(flowDetailDto);
    }
    return flowDetailVec;
}

std::map<uint64_t, std::pair<std::string, std::string>> TextTraceDatabase::QueryAllThreadMap() {
    auto threadStmt = CreatPreparedStatement(QUERY_ALL_THREAD_SQL);
    std::map<uint64_t, std::pair<std::string, std::string>> threadMap;
    if (threadStmt == nullptr) {
        ServerLog::Error("Query all thread failed to prepare sql.");
        return threadMap;
    }
    auto threadSet = threadStmt->ExecuteQuery();
    if (threadSet == nullptr) {
        ServerLog::Error("Query all thread failed to get result set.", threadStmt->GetErrorMessage());
        return threadMap;
    }

    while (threadSet->Next()) {
        uint64_t trackId = threadSet->GetUint64("trackId");
        std::string tid = threadSet->GetString("tid");
        std::string pid = threadSet->GetString("pid");
        threadMap[trackId] = std::make_pair(tid, pid);
    }
    return threadMap;
}

bool TextTraceDatabase::QueryUnitFlows(const Protocol::UnitFlowsParams &requestParams,
    Protocol::UnitFlowsBody &responseBody, uint64_t minTimestamp, uint64_t trackId) {
    if (requestParams.isSimulation) {
        QuerySimulationUintFlows(requestParams, responseBody, minTimestamp);
        return true;
    }
    FlowQuery flowQuery;
    flowQuery.startTime = requestParams.startTime;
    flowQuery.minTimestamp = minTimestamp;
    flowQuery.trackId = trackId;
    flowQuery.endTime = requestParams.endTime;
    flowQuery.fileId = requestParams.rankId;
    std::vector<FlowPoint> flowPointVec = flowAnalyzerPtr->ComputeAllFlowPointBySliceId(flowQuery, requestParams.id);
    std::unordered_map<std::string, std::vector<FlowPoint>> flowPointMap;
    ThreadQuery threadQuery;
    threadQuery.fileId = requestParams.rankId;
    std::unordered_map<uint64_t, std::pair<std::string, std::string>> threadInfo;
    sliceAnalyzerPtr->ComputeAllThreadInfo(threadQuery, threadInfo);
    for (auto &item : flowPointVec) {
        std::vector<SliceDomain> sliceVec;
        SliceQuery sliceQuery;
        sliceQuery.rankId = requestParams.rankId;
        sliceQuery.trackId = item.trackId;
        sliceAnalyzerPtr->ComputeSliceDomainVecByTrackId(sliceQuery, sliceVec);
        auto it = flowAnalyzerPtr->ComputeSliceByFlowPoint(item, sliceVec);
        if (it != sliceVec.end()) {
            item.depth = it->depth;
            item.id = it->id;
            item.duration = it->endTime - it->timestamp;
        }
        item.pid = threadInfo[item.trackId].first;
        item.tid = threadInfo[item.trackId].second;
        flowPointMap[item.flowId].emplace_back(item);
    }
    AssembleUnitFlowsBody(responseBody, minTimestamp, flowPointMap);
    return true;
}

bool TextTraceDatabase::SetCardAlias(
    const Protocol::SetCardAliasParams &requestParams, Protocol::SetCardAliasBody &responseBody) {
    if (!CreateMetaDataTableForText()) {
        ServerLog::Error("Failed to create meta data table for text.");
        return false;
    }
    return UpdateMetaDataTable(cardAliasName, requestParams.cardAlias);
}

std::string TextTraceDatabase::QueryCardAlias() {
    std::string cardAlias = GetValueFromMetaDataTable(cardAliasName);
    if (cardAlias.empty()) {
        return "";
    }
    return cardAlias;
}

void TextTraceDatabase::QuerySimulationUintFlows(
    const UnitFlowsParams &requestParams, UnitFlowsBody &responseBody, uint64_t minTimestamp) {
    SliceDto sliceDto;
    std::set<std::string> flowIdSet;
    QuerySliceDtoById(requestParams.id, sliceDto);
    flowIdSet.emplace(sliceDto.flagId);
    std::map<std::string, std::vector<UnitSingleFlow>> flowMap;
    std::vector<UnitCatFlows> unitAllFlow;
    for (const auto &flowId : flowIdSet) {
        std::vector<FlowDetailDto> flowDetailVec = QuerySingleFlowDetail(flowId);
        std::map<uint64_t, std::pair<std::string, std::string>> threadMap = QueryAllThreadMap();
        for (auto &item : flowDetailVec) {
            std::vector<SimpleSlice> simpliceVec = QuerySimpleSliceByFlagAndTrackId(item.flowId, item.trackId);
            if (std::empty(simpliceVec)) {
                continue;
            }
            item.tid = threadMap[item.trackId].first;
            item.pid = threadMap[item.trackId].second;
            item.depth = simpliceVec.front().depth;
            item.timestamp = item.flowTimestamp;
        }
        flowAnalyzerPtr->ComputeCategoryAndFlowMap(flowDetailVec, flowMap, minTimestamp);
    }
    for (const auto &item : flowMap) {
        UnitCatFlows unitCatFlows;
        unitCatFlows.cat = item.first;
        unitCatFlows.flows = item.second;
        unitAllFlow.emplace_back(unitCatFlows);
    }
    responseBody.unitAllFlows = unitAllFlow;
}

bool TextTraceDatabase::QuerySliceDtoById(const std::string &sliceId, SliceDto &sliceDto) {
    std::string sliceSql = QUERY_SLICE_BY_ID_SQL;
    auto sliceStmt = CreatPreparedStatement(sliceSql);
    if (sliceStmt == nullptr) {
        ServerLog::Error("Query slice by id failed to prepare sql.");
        return false;
    }
    auto sliceSet = sliceStmt->ExecuteQuery(sliceId);
    if (sliceSet == nullptr) {
        ServerLog::Error("Query slice by id failed to get result set.", sliceStmt->GetErrorMessage());
        return false;
    }
    while (sliceSet->Next()) {
        int col = resultStartIndex;
        sliceDto.trackId = sliceSet->GetUint64(col++);
        sliceDto.flagId = sliceSet->GetString(col++);
    }
    return true;
}

bool TextTraceDatabase::QuerySliceDtoList(std::vector<SliceDto> &sliceDtoList) {
    const std::string sliceSql = "select id, timestamp, duration, depth, track_id,"
                                 "name, args, cat, flag_id from slice";
    const auto sliceStmt = CreatPreparedStatement(sliceSql);
    if (sliceStmt == nullptr) {
        ServerLog::Error("Query slice list failed to prepare sql.");
        return false;
    }
    const auto sliceSet = sliceStmt->ExecuteQuery();
    if (sliceSet == nullptr) {
        ServerLog::Error("Query slice list failed to get result set.", sliceStmt->GetErrorMessage());
        return false;
    }
    while (sliceSet->Next()) {
        int col = resultStartIndex;
        sliceDtoList.push_back(SliceDto{
            .id = sliceSet->GetUint64(col++),
            .timestamp = sliceSet->GetUint64(col++),
            .duration = sliceSet->GetUint64(col++),
            .depth = sliceSet->GetUint32(col++),
            .trackId = sliceSet->GetUint64(col++),
            .name = sliceSet->GetString(col++),
            .args = sliceSet->GetString(col++),
            .cat = sliceSet->GetString(col++),
            .flagId = sliceSet->GetString(col++),
        });
    }
    return true;
}

std::vector<SimpleSlice> TextTraceDatabase::QuerySimpleSliceByFlagAndTrackId(
    const std::string &flagId, uint64_t trackId) {
    std::string sliceSql = QUERY_SLICE_BY_FLAG_ID_SQL;
    auto sliceStmt = CreatPreparedStatement(sliceSql);
    std::vector<SimpleSlice> simpleSliceVec;
    if (sliceStmt == nullptr) {
        ServerLog::Error("Query simple slice by flag and trackId failed to prepare sql.");
        return simpleSliceVec;
    }
    auto sliceSet = sliceStmt->ExecuteQuery(flagId, trackId);
    if (sliceSet == nullptr) {
        ServerLog::Error(
            "Query simple slice by flag and trackId failed to get result set.", sliceStmt->GetErrorMessage());
        return simpleSliceVec;
    }
    SliceQuery sliceQuery;
    sliceQuery.trackId = trackId;
    std::unordered_map<uint64_t, uint32_t> depthCache;
    sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
    while (sliceSet->Next()) {
        SimpleSlice simpleSlice;
        uint64_t id = sliceSet->GetUint64("id");
        simpleSlice.id = id;
        simpleSlice.depth = depthCache[id];
        simpleSliceVec.emplace_back(simpleSlice);
    }
    ServerLog::Info("simple slice array size is: ", simpleSliceVec.size());
    return simpleSliceVec;
}

bool TextTraceDatabase::QueryUnitsMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    std::map<std::string, std::set<std::string>> pythonThreadMap;
    std::string pythonPidSql = "SELECT DISTINCT t.pid, t.tid FROM slice s JOIN thread t ON s.track_id = t.track_id "
                               "WHERE s.cat = 'python_function'";
    auto pythonStmt = CreatPreparedStatement(pythonPidSql);
    if (pythonStmt != nullptr) {
        auto pythonRs = pythonStmt->ExecuteQuery();
        while (pythonRs != nullptr && pythonRs->Next()) {
            pythonThreadMap[pythonRs->GetString("pid")].emplace(pythonRs->GetString("tid"));
        }
    }
    std::string pythonStackMetaType = ENUM_TO_STR(PROCESS_TYPE::PYTHON_STACK).value_or("");

    std::vector<Process> processes = QueryAllProcess();
    std::map<std::string, std::vector<Thread>> threads = QueryAllThreadInfo();
    std::map<std::pair<std::string, std::string>, std::string> counters = QueryAllCounterInfo();
    std::vector<std::unique_ptr<Protocol::UnitTrack>> tempMetaData;
    for (const auto &item : processes) {
        std::unique_ptr<Protocol::UnitTrack> process = std::make_unique<Protocol::UnitTrack>();
        process->type = "process";
        process->metaData.processName = item.name;
        process->metaData.label = item.label;
        process->metaData.cardId = fileId;
        process->metaData.processId = item.pid;
        process->metaData.parentProcessId = item.parentPid;
        std::vector<Thread> pthreads = threads[item.pid];
        for (const auto &tThread : pthreads) {
            AddThreadTrack(fileId, counters, process, tThread);
        }
        auto pythonThreadIt = pythonThreadMap.find(item.pid);
        if (pythonThreadIt != pythonThreadMap.end()) {
            for (auto &child : process->children) {
                if (child->type != "thread" ||
                    pythonThreadIt->second.count(child->metaData.threadId) == 0) {
                    continue;
                }
                auto pythonStack = std::make_unique<UnitTrack>();
                pythonStack->type = "thread";
                pythonStack->metaData.metaType = pythonStackMetaType;
                pythonStack->metaData.cardId = fileId;
                pythonStack->metaData.processId = item.pid;
                pythonStack->metaData.threadId = TEXT_PYTHON_STACK_THREAD_ID_PREFIX + child->metaData.threadId;
                pythonStack->metaData.threadName = "Python Stack " + child->metaData.threadId;
                pythonStack->metaData.maxDepth = 1;
                child->children.emplace_back(std::move(pythonStack));
            }
        }
        tempMetaData.emplace_back(std::move(process));
    }
    TraceDatabaseHelper::ComputeTree(metaData, processes, tempMetaData);
    GroupCounterMetadata(fileId, metaData);
    return true;
}

bool TextTraceDatabase::IsHardwareMetricsUnit(const std::string &processId, const std::string &metaType) {
    return processId == HARDWARE_METRICS_PROCESS_ID || metaType == HARDWARE_METRICS_META_TYPE;
}

std::unique_ptr<Protocol::UnitTrack> TextTraceDatabase::GenerateHardwareMetricsUnitTrack(const std::string &fileId) {
    std::unique_ptr<UnitTrack> hardwareMetrics = std::make_unique<UnitTrack>();
    hardwareMetrics->type = "label";
    hardwareMetrics->metaData.cardId = fileId;
    hardwareMetrics->metaData.processId = HARDWARE_METRICS_PROCESS_ID;
    hardwareMetrics->metaData.processName = HARDWARE_METRICS_PROCESS_NAME;
    hardwareMetrics->metaData.metaType = HARDWARE_METRICS_META_TYPE;
    return hardwareMetrics;
}

bool TextTraceDatabase::ExtractCounterMetadata(std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData,
    Protocol::UnitTrack &hardwareMetrics) {
    bool hasCounter = false;
    for (auto unitIt = metaData.begin(); unitIt != metaData.end();) {
        if (*unitIt == nullptr) {
            unitIt = metaData.erase(unitIt);
            continue;
        }

        bool hasChildCounter = ExtractCounterMetadata((*unitIt)->children, hardwareMetrics);
        std::vector<std::unique_ptr<UnitTrack>> counterChildren;
        for (auto childIt = (*unitIt)->children.begin(); childIt != (*unitIt)->children.end();) {
            if (*childIt != nullptr && (*childIt)->type == "counter") {
                counterChildren.emplace_back(std::move(*childIt));
                childIt = (*unitIt)->children.erase(childIt);
            } else {
                ++childIt;
            }
        }

        bool hasDirectCounter = !counterChildren.empty();
        if (hasDirectCounter) {
            std::unique_ptr<UnitTrack> counterGroup = std::make_unique<UnitTrack>();
            counterGroup->type = "label";
            counterGroup->metaData = (*unitIt)->metaData;
            counterGroup->children = std::move(counterChildren);
            hardwareMetrics.children.emplace_back(std::move(counterGroup));
        }

        bool hasCounterInUnit = hasChildCounter || hasDirectCounter;
        hasCounter = hasCounter || hasCounterInUnit;
        if (hasCounterInUnit && (*unitIt)->children.empty()) {
            unitIt = metaData.erase(unitIt);
            continue;
        }
        ++unitIt;
    }
    return hasCounter;
}

void TextTraceDatabase::GroupCounterMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    auto hardwareMetrics = GenerateHardwareMetricsUnitTrack(fileId);
    ExtractCounterMetadata(metaData, *hardwareMetrics);
    if (!hardwareMetrics->children.empty()) {
        metaData.emplace_back(std::move(hardwareMetrics));
    }
}

void TextTraceDatabase::AddThreadTrack(const std::string &fileId,
    std::map<std::pair<std::string, std::string>, std::string> &counters, std::unique_ptr<Protocol::UnitTrack> &process,
    const Thread &tThread) {
    std::unique_ptr<UnitTrack> thread = std::make_unique<UnitTrack>();
    thread->metaData.metaType = "TEXT";
    auto it = counters.find({tThread.pid, tThread.threadName});
    if (it == counters.end()) { // thread
        thread->type = "thread";
        thread->metaData.cardId = fileId;
        thread->metaData.processId = tThread.pid;
        thread->metaData.threadId = tThread.tid;
        thread->metaData.threadName = tThread.threadName;
        // 解开 threadName = "Group {groupNameValue} Communication" 的形式，获取 {groupNameValue}
        if (StringUtil::StartWith(tThread.threadName, "Group") &&
            StringUtil::EndWith(tThread.threadName, "Communication")) {
            const std::string groupNameValue = ExtractGroupNameValue(tThread.threadName);
            if (TraceDatabaseHelper::IsValidHCCLGroupNameValue(groupNameValue)) {
                thread->metaData.groupNameValue = groupNameValue;
            }
        }
    } else { // counter
        thread->type = "counter";
        thread->metaData.cardId = fileId;
        thread->metaData.processId = tThread.pid;
        thread->metaData.threadName = tThread.threadName;
        thread->metaData.dataType = GetCounterDataType(it->second);
    }
    process->children.emplace_back(std::move(thread));
}

std::vector<Process> TextTraceDatabase::QueryAllProcess() {
    std::vector<Process> res;
    std::string sql = "SELECT pid, process_name, label, process_sort_index, parentPid FROM process ORDER BY "
                      "process_sort_index ASC, process_name ASC";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query all process failed!.");
        return res;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query all process failed to get result set.", stmt->GetErrorMessage());
        return res;
    }
    while (resultSet->Next()) {
        Process process;
        process.pid = resultSet->GetString("pid");
        if (TraceTime::Instance().GetIsSimulation()) {
            process.name = resultSet->GetString("process_name");
        } else {
            std::string processName = resultSet->GetString("process_name");
            if (processName != process.pid) {
                process.name = resultSet->GetString("process_name") + " (" + process.pid + ")";
            } else {
                process.name = process.pid;
            }
        }
        process.label = resultSet->GetString("label");
        process.sortIndex = resultSet->GetUint32("process_sort_index");
        process.parentPid = resultSet->GetString("parentPid");
        res.emplace_back(process);
    }
    return res;
}

std::map<std::string, std::vector<Thread>> TextTraceDatabase::QueryAllThreadInfo() {
    std::map<std::string, std::vector<Thread>> res;
    std::string sql = "select track_id,tid, pid, thread_name, thread_sort_index FROM thread where pid is not null "
                      "ORDER BY pid ASC, thread_sort_index ASC, thread_name ASC";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query all thread failed!.");
        return res;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query all thread failed to get result set.", stmt->GetErrorMessage());
        return res;
    }
    while (resultSet->Next()) {
        Thread thread;
        thread.trackId = resultSet->GetUint64("track_id");
        thread.tid = resultSet->GetString("tid");
        thread.pid = resultSet->GetString("pid");
        thread.threadName = resultSet->GetString("thread_name");
        thread.sortIndex = resultSet->GetUint32("thread_sort_index");
        res[thread.pid].emplace_back(thread);
    }
    return res;
}

std::map<std::string, std::string> TextTraceDatabase::QueryAllModelIdOfAscendHardwareThreads() {
    std::map<std::string, std::string> res;
    // INSTR: 比 like 语句更快的子字符串搜索方法
    const std::string sql = "select thread.tid, thread.track_id, slice.args from slice join thread on "
                            "slice.track_id = thread.track_id where INSTR(LOWER(slice.args), 'model id') > 0 "
                            "and thread.thread_name like 'Stream%' group by thread.track_id;";
    const auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query all model id of ascend hardware thread failed!.");
        return res;
    }
    const auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error(
            "Query all model id of ascend hardware thread failed to get result set.", stmt->GetErrorMessage());
        return res;
    }
    while (resultSet->Next()) {
        const auto tid = resultSet->GetString("tid");
        auto args = resultSet->GetString("args");
        std::string error;
        auto json = JsonUtil::TryParse(args, error);
        if (!json.has_value() || !error.empty()) {
            continue;
        }
        std::string modelId;
        if (JsonUtil::IsJsonKeyValid(json.value(), "Model Id")) {
            modelId = JsonUtil::GetString(json.value(), "Model Id");
        }
        if (JsonUtil::IsJsonKeyValid(json.value(), "model id")) {
            modelId = JsonUtil::GetString(json.value(), "model id");
        }
        res[tid] = modelId;
    }
    return res;
}

std::map<std::pair<std::string, std::string>, std::string> TextTraceDatabase::QueryAllCounterInfo() {
    std::map<std::pair<std::string, std::string>, std::string> res;
    std::string sql = "SELECT pid,name, args FROM counter group by pid, name";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query all counter info failed!.");
        return res;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query all counter info failed to get result set.", stmt->GetErrorMessage());
        return res;
    }
    while (resultSet->Next()) {
        std::string pid = resultSet->GetString("pid");
        std::string name = resultSet->GetString("name");
        std::string args = resultSet->GetString("args");
        res[{pid, name}] = args;
    }
    return res;
}

std::vector<std::string> TextTraceDatabase::GetCounterDataType(const std::string &args) {
    std::vector<std::string> type{};
    if (args.empty()) {
        return type;
    }
    rapidjson::Document document;
    document.Parse(args.c_str(), args.length()); // 解析，Parse() 不会抛异常
    if (document.HasParseError()) { // 通过HasParseError()来判断解析是否成功
        ServerLog::Error("Get counter data type. Parse failed: ", document.GetParseError(), " at offset ",
            document.GetErrorOffset());
        return type;
    }
    if (!document.IsObject()) {
        ServerLog::Error("Get counter data type. Parse json is not object: %", args);
        return type;
    }
    for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it) {
        if (it->name.IsString()) {
            type.emplace_back(it->name.GetString());
        } else {
            ServerLog::Warn("Counter data type is not string. args:", args, ". nameType: ", it->name.GetType());
        }
    }
    std::sort(type.begin(), type.end()); // 与metadata数据顺序一致，可能是因为使用json开源软件不一致
    return type;
}

bool TextTraceDatabase::QueryExtremumTimestamp(uint64_t &min, uint64_t &max) {
    std::string sql = QUERY_EXETREME_TIME_SQL;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query extremum timestamp failed!.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query extremum timestamp failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        min = resultSet->GetUint64("totalMinTimestamp");
        max = resultSet->GetUint64("totalMaxTimestamp");
    }
    return true;
}

void TextTraceDatabase::CommitData() {
    if (!sliceCache.empty()) {
        InsertSliceList(sliceCache);
        sliceCache.clear();
    }
    if (!flowCache.empty()) {
        InsertFlowList(flowCache);
        flowCache.clear();
    }
    if (!counterCache.empty()) {
        InsertCounterList(counterCache);
        counterCache.clear();
    }
    if (!simulationThreadInfoCache.empty()) {
        InsertSimulationThreadList();
        simulationThreadInfoCache.clear();
    }
    if (!simulationProcessInfoCache.empty()) {
        InsertSimulationProcessList();
        simulationProcessInfoCache.clear();
    }
    if (!ftraceStatCache.empty()) {
        InsertFtraceStatList(ftraceStatCache);
        ftraceStatCache.clear();
    }
}

uint32_t TextTraceDatabase::SearchSliceNameCount(const Protocol::SearchCountParams &params) {
    uint32_t result = 0;
    std::string sql = TextSqlConstant::GetSearchSliceNameCountSql(params.isMatchExact, params.isMatchCase);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name count failed!.");
        return 0;
    }
    auto resultSet = stmt->ExecuteQuery(params.searchContent);
    if (resultSet == nullptr) {
        ServerLog::Error("Query slice name count. Failed to get result set.", stmt->GetErrorMessage());
        return 0;
    }
    if (resultSet->Next()) {
        result = resultSet->GetUint32("count");
    }
    return result;
}

uint32_t TextTraceDatabase::SearchSliceNameCount(
    const Protocol::SearchCountParams &params, const std::vector<TrackQuery> &trackQuery) {
    uint32_t result = 0;
    if (trackQuery.empty() && !params.metadataList.empty()) {
        return result;
    }
    if (trackQuery.empty()) {
        return SearchSliceNameCount(params);
    }
    std::string sql;
    // 根据是否有二级筛选，生成不同的SQL
    if (!params.nameFilter.empty()) {
        sql = TextSqlConstant::GetSearchSliceCountWithLockRangeAndFilterSql(params.isMatchExact, params.isMatchCase);
    } else {
        sql = TextSqlConstant::GetSearchSliceNameCountSql(params.isMatchExact, params.isMatchCase);
        sql += " AND track_id = ? AND timestamp >= ? AND end_time <= ? ";
    }
    std::vector<std::string> sqls(trackQuery.size(), sql);
    sql = StringUtil::join(sqls, " UNION ALL ");
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name count failed!.");
        return 0;
    }
    // 根据是否有二级筛选，绑定不同的参数
    if (!params.nameFilter.empty()) {
        for (const auto &item : trackQuery) {
            stmt->BindParams(params.searchContent, params.nameFilter, item.trackId, item.startTime, item.endTime);
        }
    } else {
        for (const auto &item : trackQuery) {
            stmt->BindParams(params.searchContent, item.trackId, item.startTime, item.endTime);
        }
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query slice name count. Failed to get result set.", stmt->GetErrorMessage());
        return 0;
    }
    while (resultSet->Next()) {
        const uint32_t count = resultSet->GetUint32("count");
        if (result > UINT32_MAX - count) {
            ServerLog::Warn("Sum of searching slice name count is overflow.");
            break;
        }
        result += count;
    }
    return result;
}

bool TextTraceDatabase::SearchSliceNameWithOutLock(const Protocol::SearchSliceParams &params, int index,
    uint64_t minTimestamp, Protocol::SearchSliceBody &responseBody) {
    std::string sql = TextSqlConstant::GetSearchSliceNameSql(params.isMatchExact, params.isMatchCase);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(minTimestamp, params.searchContent, index);
    if (resultSet == nullptr) {
        ServerLog::Error("Query slice name. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    if (!resultSet->Next()) {
        return false;
    }
    uint64_t id = resultSet->GetUint64("id");
    responseBody.id = std::to_string(id);
    responseBody.pid = resultSet->GetString("pid");
    responseBody.tid = resultSet->GetString("tid");
    responseBody.startTime = resultSet->GetUint64("startTime");
    responseBody.duration = resultSet->GetUint64("duration");
    uint64_t trackId = resultSet->GetUint64("trackId");
    SliceQuery sliceQuery;
    sliceQuery.trackId = trackId;
    sliceQuery.rankId = params.rankId;
    std::unordered_map<uint64_t, uint32_t> depthCache;
    sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
    responseBody.depth = depthCache[id];
    return true;
}

bool TextTraceDatabase::SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
    Protocol::SearchSliceBody &responseBody, const std::vector<TrackQuery> &trackQuery) {
    if (std::empty(trackQuery)) {
        return SearchSliceNameWithOutLock(params, index, minTimestamp, responseBody);
    }
    std::string nameMatch = TextSqlConstant::GetSearchNameSqlSuffix(params.isMatchExact, params.isMatchCase);
    nameMatch += " AND track_id = ? AND timestamp >= ? AND end_time <= ?";
    std::string sql = "SELECT id, pid, tid, timestamp - ? as startTime, duration, track_id AS trackId"
                      " FROM " +
        SLICE_TABLE + " JOIN " + THREAD_TABLE + " USING (track_id) WHERE " + nameMatch;
    std::vector<std::string> sqls(trackQuery.size(), sql);
    sql = StringUtil::join(sqls, " UNION ALL ");
    sql += " ORDER BY startTime ASC, track_id ASC, id ASC LIMIT 1 OFFSET ?";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }
    for (const auto &item : trackQuery) {
        stmt->BindParams(minTimestamp, params.searchContent, item.trackId, item.startTime, item.endTime);
    }
    stmt->BindParams(index);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query slice name. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    if (!resultSet->Next()) {
        return false;
    }
    uint64_t id = resultSet->GetUint64("id");
    responseBody.id = std::to_string(id);
    responseBody.pid = resultSet->GetString("pid");
    responseBody.tid = resultSet->GetString("tid");
    responseBody.startTime = resultSet->GetUint64("startTime");
    responseBody.duration = resultSet->GetUint64("duration");
    uint64_t trackId = resultSet->GetUint64("trackId");
    SliceQuery sliceQuery;
    sliceQuery.trackId = trackId;
    sliceQuery.rankId = params.rankId;
    std::unordered_map<uint64_t, uint32_t> depthCache;
    sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
    responseBody.depth = depthCache[id];
    return true;
}

bool TextTraceDatabase::QueryHostSlicesByName(const std::string &sliceName, const std::string &metaType,
    std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    return true;
}

bool TextTraceDatabase::QueryDeviceSlicesByName(const std::string &rankId, const std::string &sliceName,
    const std::string &metaType, std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    return true;
}

bool TextTraceDatabase::QueryTextSlicesByName(const std::string &sliceName, const std::string &metaType,
    std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    if (metaType != "TEXT") {
        return true;
    }

    std::string sql = "SELECT slice.id, thread.pid, slice.timestamp, slice.duration FROM " + sliceTable +
        " slice JOIN " + threadTable + " thread ON slice.track_id = thread.track_id WHERE slice.name = ?";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query text slices by name failed to prepare sql.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(sliceName);
    if (resultSet == nullptr) {
        ServerLog::Error("Query text slices by name failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::SimpleSlice slice;
        slice.name = sliceName;
        slice.pid = resultSet->GetString("pid");
        slice.metaType = metaType;
        slice.timestamp = resultSet->GetUint64("timestamp");
        slice.duration = resultSet->GetUint64("duration");
        slice.id = resultSet->GetUint64("id");
        result.emplace_back(slice);
        processIds.insert(slice.pid);
    }
    return true;
}

bool TextTraceDatabase::QueryFlowCategoryList(std::vector<std::string> &categories, const std::string &rankId) {
    std::string sql = "SELECT cat FROM flow GROUP BY cat";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query flow category list failed!.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query flow category list. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        categories.emplace_back(resultSet->GetString(resultStartIndex));
    }
    return true;
}

std::vector<uint64_t> TextTraceDatabase::QueryAllTrackIdsByPid(std::string pid) {
    std::vector<uint64_t> trackIds;
    std::string sql = "Select track_id AS trackId from " + threadTable + " where pid = ? ;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query all track ids by pid failed!.");
        return trackIds;
    }
    auto resultSet = stmt->ExecuteQuery(pid);
    if (resultSet == nullptr) {
        ServerLog::Error("Query all track ids by pid. Failed to get result set.", stmt->GetErrorMessage());
        return trackIds;
    }
    while (resultSet->Next()) {
        int col = resultStartIndex;
        uint64_t trackId = resultSet->GetUint64(col++);
        trackIds.emplace_back(trackId);
    }
    return trackIds;
}

bool TextTraceDatabase::QueryUnitCounter(
    Protocol::UnitCounterParams &params, uint64_t minTimestamp, std::vector<Protocol::UnitCounterData> &dataList) {
    std::string sql = QUERY_UNIT_COUNTER_SQL;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query unit counter failed!.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(minTimestamp, params.pid, params.threadName, params.startTime, params.endTime);
    if (resultSet == nullptr) {
        ServerLog::Error("Query unit counter. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    std::string curArgs;
    UnitCounterData unitCounterData;
    while (resultSet->Next()) {
        unitCounterData.timestamp = resultSet->GetUint64("startTime");
        unitCounterData.valueJsonStr = resultSet->GetString("args");
        if (unitCounterData.valueJsonStr != curArgs) {
            dataList.emplace_back(unitCounterData);
            curArgs = unitCounterData.valueJsonStr;
        }
    }
    if (!dataList.empty()) {
        dataList = DownSampleUnitCounterData(dataList, counterSampleSize);
        dataList.emplace_back(unitCounterData);
    }
    return true;
}

bool TextTraceDatabase::QueryComputeStatisticsData(
    const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) {
    std::string sql = TextSqlConstant::GetComputeStatisticsSQL(requestParams.stepId);
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query compute statistics data failed!. ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    if (!requestParams.stepId.empty() && requestParams.stepId != "ALL" && requestParams.stepId.length() <= INT_MAX) {
        sqlite3_bind_text(stmt, index, requestParams.stepId.c_str(), requestParams.stepId.length(), SQLITE_TRANSIENT);
    }
    double totalDuration = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Protocol::SummaryStatisticsItem item;
        int col = resultStartIndex;
        item.duration = static_cast<double>(sqlite3_column_int64(stmt, col++));
        item.acceleratorCore = sqlite3_column_string(stmt, col++);
        totalDuration += item.duration;
        responseBody.summaryStatisticsItemList.push_back(item);
    }
    for (auto &item : responseBody.summaryStatisticsItemList) {
        item.utilization = totalDuration > 0 ? item.duration / totalDuration : 0;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool TextTraceDatabase::QueryCommunicationStatisticsData(
    const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) {
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    uint64_t min;
    uint64_t max;
    if (!requestParams.stepId.empty()) {
        QueryStepDuration(requestParams.stepId, min, max);
    }
    std::string sql = TextSqlConstant::GetCommunicationStatisticsSql(requestParams.stepId);
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query communication statistics data failed!. ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    if (!requestParams.stepId.empty()) {
        sqlite3_bind_int64(stmt, index++, NumberUtil::CeilingClamp(min, (uint64_t)INT64_MAX));
        sqlite3_bind_int64(stmt, index, NumberUtil::CeilingClamp(max, (uint64_t)INT64_MAX));
    }
    double communicationTime = 0;
    double notOverlapTime = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        auto duration = static_cast<double>(sqlite3_column_int64(stmt, col++));
        std::string overType = sqlite3_column_string(stmt, col++);
        std::strcmp(overType.c_str(), "Communication") == 0 ? communicationTime = duration : notOverlapTime = duration;
    }
    Protocol::SummaryStatisticsItem overlapItem;
    overlapItem.duration = communicationTime - notOverlapTime;
    overlapItem.overlapType = "Communication(Overlapped)";
    responseBody.summaryStatisticsItemList.push_back(overlapItem);
    Protocol::SummaryStatisticsItem notOverlapItem;
    notOverlapItem.duration = notOverlapTime;
    notOverlapItem.overlapType = "Communication(Not Overlapped)";
    responseBody.summaryStatisticsItemList.push_back(notOverlapItem);
    sqlite3_finalize(stmt);
    return true;
}

bool TextTraceDatabase::QueryStepDuration(const std::string &stepId, uint64_t &min, uint64_t &max) {
    std::string profileName = "ProfilerStep#" + stepId;
    std::string sql = "select timestamp, duration from " + sliceTable + " where name=?";
    sqlite3_stmt *stmt = nullptr;
    int index = bindStartIndex;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Query step duration failed!. ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_bind_text(stmt, index++, profileName.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t tempMin = sqlite3_column_int64(stmt, col++);
        int64_t tempDur = sqlite3_column_int64(stmt, col++);
        if (tempMin >= 0 && tempDur >= 0) {
            min = static_cast<uint64_t>(tempMin);
            max = static_cast<uint64_t>(min) + static_cast<uint64_t>(tempDur);
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

bool TextTraceDatabase::QueryEventsViewData(
    const EventsViewParams &params, EventsViewBody &body, uint64_t minTimestamp) {
    auto stmt = CreatPreparedStatement();
    if (stmt == nullptr) {
        return false;
    }
    return TraceDatabaseHelper::QueryEventsViewData4Text(stmt, params, body, minTimestamp);
}

bool TextTraceDatabase::QuerySystemViewData(const Protocol::SystemViewParams &requestParams,
    Protocol::SystemViewBody &responseBody, const uint64_t &minTimestamp) {
    std::string searchName = "%" + requestParams.searchName + "%";
    const std::string &timeCondSql =
        TextSqlConstant::AppendTextTimeRangeConditionSql(requestParams.startTime, requestParams.endTime);
    const LayerStatData &data = QueryLayerData(requestParams, searchName, minTimestamp, timeCondSql);
    double layerOperatorTime = data.allOperatorTime;
    if (!StringUtil::CheckSqlValid(requestParams.orderBy)) {
        ServerLog::Error("Query system view data an SQL injection attack.");
        return false;
    }
    const std::vector<std::string> layers = (requestParams.layer == "HCCL" || requestParams.layer == "COMMUNICATION")
        ? std::vector<std::string>{"hccl", "communication"}
        : std::vector{StringUtil::ToLower(requestParams.layer)};
    std::string sql =
        TextSqlConstant::GetQueryPythonViewDataSql(requestParams.order, requestParams.orderBy, layers, timeCondSql);
    uint64_t offset = (requestParams.current - 1) * requestParams.pageSize;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query system view data, fail to prepare sql.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (std::find(layers.begin(), layers.end(), "python") == layers.end() &&
        std::find(layers.begin(), layers.end(), "cann") == layers.end()) {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        if (requestParams.startTime == requestParams.endTime) {
            resultSet = stmt->ExecuteQuery(layerOperatorTime, searchName, deviceId, requestParams.pageSize, offset);
        } else {
            resultSet =
                stmt->ExecuteQuery(layerOperatorTime, searchName, deviceId, requestParams.startTime + minTimestamp,
                    requestParams.endTime + minTimestamp, requestParams.pageSize, offset);
        }
    } else {
        if (requestParams.startTime == requestParams.endTime) {
            resultSet = stmt->ExecuteQuery(layerOperatorTime, searchName, requestParams.pageSize, offset);
        } else {
            resultSet = stmt->ExecuteQuery(layerOperatorTime, searchName, requestParams.startTime + minTimestamp,
                requestParams.endTime + minTimestamp, requestParams.pageSize, offset);
        }
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Query system view data. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    TraceDatabaseHelper::SetSystemViewHelpler(std::move(resultSet), data, requestParams, responseBody);
    return true;
}

bool TextTraceDatabase::QueryExpAnaAICoreFreqData(const Protocol::SystemViewAICoreFreqParams &requestParams,
    Protocol::ExpAnaAICoreFreqBody &responseBody, std::vector<std::pair<uint64_t, uint64_t>> &freqs, uint64_t &maxFreq,
    uint64_t &minFreq) {
    std::string sql = TextSqlConstant::GetAICoreViewDataSql();
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query system view AI core freq data, fail to prepare sql.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(StringUtil::StringToInt(requestParams.deviceId));
    if (resultSet == nullptr) {
        ServerLog::Error("Query system view AI core freq data. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        std::pair<uint64_t, uint64_t> detail;
        int col = resultStartIndex;
        detail.first = resultSet->GetUint64(col++);
        rapidjson::Document jsonArgs;
        if (jsonArgs.Parse(resultSet->GetString(col++).c_str()).HasParseError()) {
            ServerLog::Error("Parse AI core freq data Failed.");
            break;
        }
        // 确保 "MHz" 键存在且类型正确
        if (jsonArgs.HasMember("MHz") && jsonArgs["MHz"].IsString()) {
            int temp = NumberUtil::StringToInt(jsonArgs["MHz"].GetString());
            detail.second = temp < 0 ? 0 : temp;
        } else {
            ServerLog::Error("Invalid AI core freq data structure detected.");
            break;
        }
        responseBody.pid = resultSet->GetString(col++);
        responseBody.tid = resultSet->GetString(col++);
        maxFreq = std::max(maxFreq, detail.second);
        minFreq = std::min(minFreq, detail.second);
        freqs.emplace_back(detail);
    }
    return true;
}

LayerStatData TextTraceDatabase::QueryLayerData(const Protocol::SystemViewParams &requestParams,
    const std::string &name, const uint64_t &minTimestamp, const std::string &timeCondSql) {
    LayerStatData layerStatData;
    std::vector<std::string> layerList = (requestParams.layer == "HCCL" || requestParams.layer == "COMMUNICATION")
        ? std::vector<std::string>{"hccl", "communication"}
        : std::vector<std::string>{StringUtil::ToLower(requestParams.layer)};
    std::string sql = TextSqlConstant::GetQueryLayerDataSql(layerList, timeCondSql);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query layer operator time, fail to prepare sql.");
        return layerStatData;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (std::find(layerList.begin(), layerList.end(), "python") == layerList.end() &&
        std::find(layerList.begin(), layerList.end(), "cann") == layerList.end()) {
        int deviceId = StringUtil::StringToInt(requestParams.deviceId);
        if (requestParams.startTime == requestParams.endTime) {
            resultSet = stmt->ExecuteQuery(name, deviceId);
        } else {
            resultSet = stmt->ExecuteQuery(
                name, deviceId, requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
        }
    } else {
        if (requestParams.startTime == requestParams.endTime) {
            resultSet = stmt->ExecuteQuery(name);
        } else {
            resultSet =
                stmt->ExecuteQuery(name, requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
        }
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Query layer operator time. Failed to get result set.", stmt->GetErrorMessage());
        return layerStatData;
    }
    if (resultSet->Next()) {
        layerStatData.allOperatorTime = resultSet->GetDouble("totalTime");
        layerStatData.total = resultSet->GetUint64("count(distinct name)");
    }
    return layerStatData;
}

std::vector<std::string> TextTraceDatabase::QueryCoreType() {
    std::vector<std::string> acceleratorCoreList;
    std::string sql = QUERY_QUERY_TYPE_SQL;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query core type, fail to prepare sql.");
        return acceleratorCoreList;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query core type. Failed to get result set.", stmt->GetErrorMessage());
        return acceleratorCoreList;
    }
    while (resultSet->Next()) {
        std::string res = resultSet->GetString("accelerator_core");
        acceleratorCoreList.emplace_back(res);
    }
    return acceleratorCoreList;
}

uint64_t TextTraceDatabase::QueryTotalKernel(
    const Protocol::KernelDetailsParams &requestParams, uint64_t minTimestamp) {
    uint64_t total = 0;
    std::string sql =
        "SELECT count(*) FROM ("
        "    SELECT name, op_type AS type, accelerator_core AS acceleratorCore, "
        "    input_shapes AS inputShapes, input_data_types AS inputDataTypes, input_formats AS inputFormats, "
        "    output_shapes AS outputShapes, output_data_types AS outputDataTypes, "
        "    output_formats AS outputFormats FROM kernel_detail WHERE deviceId = ? ";
    if (requestParams.startTime != requestParams.endTime) {
        sql += " AND (start_time + duration*1000) >= ? AND start_time <= ? ";
    }
    sql += ") subquery ";
    for (const auto &filter : requestParams.filters) {
        if (!StringUtil::CheckSqlValid(filter.first)) {
            Server::ServerLog::Error("There is an SQL injection attack on this parameter. param: filter");
            return total;
        }
        sql += " WHERE lower(" + filter.first + ") LIKE lower(?) ";
    }
    if (!requestParams.coreType.empty()) {
        sql += " AND accelerator_core = ? ";
    }
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query total kernel, fail to prepare sql.");
        return total;
    }
    stmt->BindParams(requestParams.deviceId);
    if (requestParams.startTime != requestParams.endTime) {
        stmt->BindParams(requestParams.startTime, requestParams.endTime);
    }
    if (!requestParams.coreType.empty()) {
        stmt->BindParams(requestParams.coreType);
    }
    for (const auto &filter : requestParams.filters) {
        std::string bindFilter = "%" + filter.second + "%";
        stmt->BindParams(bindFilter);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query total kernel. Failed to get result set.", stmt->GetErrorMessage());
        return total;
    }
    if (resultSet->Next()) {
        total = resultSet->GetUint64("count(*)");
    }
    return total;
}

bool TextTraceDatabase::QueryKernelDetailData(const Protocol::KernelDetailsParams &requestParams,
    Protocol::KernelDetailsBody &responseBody, uint64_t minTimestamp) {
    if (!StringUtil::CheckSqlValid(requestParams.orderBy)) {
        ServerLog::Error("Query kernel detail data is an SQL injection attack");
        return false;
    }
    std::string sql = TextSqlConstant::GetKernelDetailSql(requestParams);
    uint64_t offset = (requestParams.current - 1) * requestParams.pageSize;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query kernel detail data, fail to prepare sql.");
        return false;
    }
    stmt->BindParams(requestParams.deviceId);
    if (requestParams.startTime != requestParams.endTime) {
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    if (!requestParams.coreType.empty()) {
        stmt->BindParams(requestParams.coreType);
    }
    for (const auto &filter : requestParams.filters) {
        std::string bindFilter = "%" + filter.second + "%";
        stmt->BindParams(bindFilter);
    }
    auto resultSet = stmt->ExecuteQuery(requestParams.pageSize, offset);
    if (resultSet == nullptr) {
        ServerLog::Error("Query kernel detail data. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    TraceDatabaseHelper::SetKernelDetailHelpler(std::move(resultSet), minTimestamp, responseBody);
    responseBody.pageSize = requestParams.pageSize;
    responseBody.currentPage = requestParams.current;
    const std::vector<std::string> cores = QueryCoreType();
    responseBody.acceleratorCoreList = cores;
    responseBody.count = QueryTotalKernel(requestParams, minTimestamp);
    return true;
}

bool TextTraceDatabase::QueryCommunicationKernelInfo(
    const std::string &name, const std::string &rankId, CommunicationKernelBody &body) {
    std::string sql = "SELECT id, track_id, timestamp FROM " + sliceTable + " WHERE name = ?";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to query communication kernel info, prepare sql fail.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    resultSet = stmt->ExecuteQuery(name);
    if (resultSet == nullptr) {
        ServerLog::Error("Fail to query communication kernel info, get result set fail.", stmt->GetErrorMessage());
        return false;
    }
    uint64_t trackId = 0;
    if (resultSet->Next()) {
        uint64_t id = resultSet->GetUint64("id");
        trackId = resultSet->GetUint64("track_id");
        uint64_t startTime = resultSet->GetUint64("timestamp");
        SliceQuery sliceQuery;
        sliceQuery.rankId = rankId;
        sliceQuery.trackId = trackId;
        body.id = std::to_string(id);
        body.startTime = startTime > Timeline::TraceTime::Instance().GetStartTime()
            ? startTime - Timeline::TraceTime::Instance().GetStartTime()
            : startTime;
    }
    const OneKernelData &data = QueryKernelTid(trackId);
    body.threadId = data.threadId;
    body.pid = data.pid;
    body.rankId = rankId;
    return true;
}

bool TextTraceDatabase::QueryKernelDepthAndThread(
    const Protocol::KernelParams &params, Protocol::OneKernelBody &responseBody, uint64_t minTimestamp) {
    std::string sql =
        "SELECT id, duration, track_id FROM " + sliceTable + " WHERE name = ? AND timestamp > ? AND timestamp < ?";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query kernel depth and thread, fail to prepare sql.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    uint64_t timestamp = params.timestamp + minTimestamp;
    if (timestamp <= tolerance) {
        resultSet = stmt->ExecuteQuery(params.name, 0, timestamp);
    } else if (UINT64_MAX - timestamp > tolerance) {
        resultSet = stmt->ExecuteQuery(params.name, timestamp - tolerance, timestamp + tolerance);
    } else {
        ServerLog::Error("Query kernel depth and thread, The minTimestamp is out of the valid range.");
        return false;
    }

    if (resultSet == nullptr) {
        ServerLog::Error("Query kernel depth and thread. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    uint64_t trackId = 0;
    if (resultSet->Next()) {
        uint64_t id = resultSet->GetUint64("id");
        trackId = resultSet->GetUint64("track_id");
        SliceQuery sliceQuery;
        sliceQuery.rankId = params.rankId;
        sliceQuery.trackId = trackId;
        std::unordered_map<uint64_t, uint32_t> depthCache;
        sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
        responseBody.id = std::to_string(id);
        responseBody.depth = depthCache[id];
        responseBody.duration = resultSet->GetUint64("duration");
    }
    const OneKernelData &data = QueryKernelTid(trackId);
    responseBody.threadId = data.threadId;
    responseBody.pid = data.pid;
    responseBody.rankId = params.rankId;
    return true;
}

OneKernelData TextTraceDatabase::QueryKernelTid(const uint64_t trackId) {
    std::string sql = "SELECT tid, pid FROM " + threadTable + " WHERE track_id = ? ";
    auto stmt = CreatPreparedStatement(sql);
    OneKernelData oneKernel;
    if (stmt == nullptr) {
        ServerLog::Error("Query kernel tid, fail to prepare sql.");
        return oneKernel;
    }
    auto resultSet = stmt->ExecuteQuery(trackId);
    if (resultSet == nullptr) {
        ServerLog::Error("Query kernel tid. Failed to get result set.", stmt->GetErrorMessage());
        return oneKernel;
    }
    if (resultSet->Next()) {
        oneKernel.threadId = resultSet->GetString("tid");
        oneKernel.pid = resultSet->GetString("pid");
    }
    return oneKernel;
}

bool TextTraceDatabase::QueryThreadSameOperatorsDetails(const Protocol::UnitThreadsOperatorsParams &requestParams,
    Protocol::UnitThreadsOperatorsBody &responseBody, uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) {
    uint64_t startTime = requestParams.startTime + minTimestamp;
    uint64_t endTime = requestParams.endTime + minTimestamp;
    std::string sql =
        TextSqlConstant::GetThreadSameOperatorsDetailsSql(requestParams.order, requestParams.orderBy, trackIdList);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query thread same operators details. Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(requestParams.name, endTime, startTime);
    if (resultSet == nullptr) {
        ServerLog::Error("Query thread same operators details. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    ExecuteQueryThreadSameOperatorsDetails(resultSet, minTimestamp, requestParams, responseBody);
    responseBody.currentPage = requestParams.current;
    responseBody.pageSize = requestParams.pageSize;
    responseBody.count = SameOperatorsCount(requestParams.name, trackIdList, startTime, endTime);
    return true;
}

void TextTraceDatabase::ExecuteQueryThreadSameOperatorsDetails(const std::unique_ptr<SqliteResultSet> &resultSet,
    uint64_t minTimestamp, const Protocol::UnitThreadsOperatorsParams &requestParams,
    Protocol::UnitThreadsOperatorsBody &responseBody) {
    uint64_t offset = (requestParams.current - 1) > UINT64_MAX / requestParams.pageSize
        ? 0
        : (requestParams.current - 1) * requestParams.pageSize;
    uint64_t count = 0;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> trackIdDepthCache;
    while (resultSet->Next()) {
        int col = resultStartIndex;
        Protocol::SameOperatorsDetails sameOperatorsDetail{};
        uint64_t tempStartTime = resultSet->GetUint64(col++);
        if (tempStartTime < minTimestamp) {
            continue;
        }
        sameOperatorsDetail.timestamp = tempStartTime - minTimestamp;
        sameOperatorsDetail.duration = resultSet->GetUint64(col++);
        sameOperatorsDetail.id = resultSet->GetString(col++);
        uint64_t trackId = resultSet->GetUint64("track_id");
        auto item = trackIdDepthCache.find(trackId);
        if (item != trackIdDepthCache.end()) {
            sameOperatorsDetail.depth = item->second[NumberUtil::StringToLongLong(sameOperatorsDetail.id)];
        } else {
            std::unordered_map<uint64_t, uint32_t> depthCache;
            SliceQuery sliceQuery;
            sliceQuery.rankId = requestParams.rankId;
            sliceQuery.trackId = trackId;
            sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
            trackIdDepthCache[trackId] = depthCache;
            sameOperatorsDetail.depth = depthCache[NumberUtil::StringToLongLong(sameOperatorsDetail.id)];
        }
        TrackInfo trackInfo;
        TrackInfoManager::Instance().GetTrackInfo(trackId, trackInfo, requestParams.rankId);
        sameOperatorsDetail.tid = trackInfo.threadId;
        sameOperatorsDetail.pid = trackInfo.processId;
        if (!requestParams.startDepth.empty() && !requestParams.endDepth.empty() &&
            !(sameOperatorsDetail.depth >= NumberUtil::StringToUint32(requestParams.startDepth) &&
                sameOperatorsDetail.depth <= NumberUtil::StringToUint32(requestParams.endDepth))) {
            continue;
        }
        if (++count <= offset) {
            continue;
        }
        if (count > offset + requestParams.pageSize) {
            break;
        }
        responseBody.sameOperatorsDetails.emplace_back(sameOperatorsDetail);
    }
}

uint64_t TextTraceDatabase::SameOperatorsCount(
    const std::string &name, const std::vector<uint64_t> &trackIdList, uint64_t &startTime, uint64_t &endTime) {
    uint64_t total = 0;
    std::string trackIdPlaceholders = StringUtil::join(trackIdList, ", ");
    std::string sql = "SELECT count(*) FROM " + sliceTable +
        " WHERE name = ? AND "
        " track_id in (" +
        trackIdPlaceholders + ") AND timestamp <= ? AND timestamp + duration >= ?;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for same operators count.", sqlite3_errmsg(db));
        return total;
    }
    auto resultSet = stmt->ExecuteQuery(name, endTime, startTime);
    if (resultSet == nullptr) {
        ServerLog::Error("same operators count. Failed to get result set.", stmt->GetErrorMessage());
        return total;
    }
    if (resultSet->Next()) {
        total = resultSet->GetUint64("count(*)");
    }
    return total;
}

bool TextTraceDatabase::QueryAICpuOpCanBeOptimized(const Protocol::KernelDetailsParams &params,
    const std::vector<std::string> &replace, const std::map<std::string, Timeline::AICpuCheckDataType> &dataType,
    std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) {
    if (!CheckTableExist(sliceTable) || !CheckTableExist(TABLE_KERNEL)) {
        return false;
    }
    std::string sql = TextSqlConstant::GenerateAICpuQueryTextSql(replace, params, dataType);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for AI cpu op exceed threshold.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.deviceId, AICPU_OP_DURATION_THRESHOLD / THOUSAND);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.deviceId, params.startTime + minTimestamp,
            params.endTime + minTimestamp, AICPU_OP_DURATION_THRESHOLD / THOUSAND);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for AI cpu op exceed threshold.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.type = resultSet->GetString("type");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        one.inputType = resultSet->GetString("input");
        one.outputType = resultSet->GetString("output");
        data.emplace_back(one);
    }
    return true;
}

bool TextTraceDatabase::UpdateParseStatus(const std::string &status) {
    return UpdateValueIntoStatusInfoTable(timelineParseStatus, status);
}

bool TextTraceDatabase::HasFinishedParseLastTime(const std::string &statuInfo) {
    return CheckValueFromStatusInfoTable(timelineParseStatus, statuInfo);
}

bool TextTraceDatabase::SearchAllSlicesDetails(const Protocol::SearchAllSliceParams &params,
    Protocol::SearchAllSlicesBody &body, uint64_t minTimestamp, const std::vector<TrackQuery> &trackQueryVec) {
    if (!params.metadataList.empty() && trackQueryVec.empty()) {
        body.currentPage = params.current;
        body.pageSize = params.pageSize;
        Protocol::SearchCountParams searchCountParams;
        searchCountParams.searchContent = params.searchContent;
        searchCountParams.isMatchCase = params.isMatchCase;
        searchCountParams.isMatchExact = params.isMatchExact;
        searchCountParams.rankId = params.rankId;
        body.count = 0;
        return true;
    }
    if (trackQueryVec.empty()) {
        return SearchAllSlicesDetails(params, body, minTimestamp);
    }
    std::string sql = GetSearchAllSliceWithLockRangeSql(params, trackQueryVec);
    uint64_t offset = (params.current - 1) * params.pageSize;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Search all slices details failed!.");
        return false;
    }
    // 根据是否有二级筛选，绑定不同的参数
    if (!params.nameFilter.empty()) {
        for (const auto &item: trackQueryVec) {
            stmt->BindParams(params.searchContent, params.nameFilter, item.trackId, item.startTime, item.endTime);
        }
    } else {
        for (const auto &item: trackQueryVec) {
            stmt->BindParams(params.searchContent, item.trackId, item.startTime, item.endTime);
        }
    }
    stmt->BindParams(params.pageSize, offset);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Search all slices details. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    GetSearchAllSliceData(params, body, minTimestamp, resultSet);
    body.currentPage = params.current;
    body.pageSize = params.pageSize;
    Protocol::SearchCountParams searchCountParams;
    searchCountParams.searchContent = params.searchContent;
    searchCountParams.isMatchCase = params.isMatchCase;
    searchCountParams.isMatchExact = params.isMatchExact;
    searchCountParams.rankId = params.rankId;
    searchCountParams.nameFilter = params.nameFilter;
    body.count = SearchSliceNameCount(searchCountParams, trackQueryVec);
    return true;
}

void TextTraceDatabase::GetSearchAllSliceData(const SearchAllSliceParams &params, SearchAllSlicesBody &body,
    uint64_t minTimestamp, std::unique_ptr<SqliteResultSet> &resultSet) const {
    while (resultSet->Next()) {
        int col = resultStartIndex;
        SearchAllSlices searchAllSlice{};
        searchAllSlice.name = resultSet->GetString(col++);
        uint64_t tempStartTime = resultSet->GetUint64(col++);
        if (tempStartTime < minTimestamp) {
            continue;
        }
        searchAllSlice.timestamp = tempStartTime - minTimestamp;
        searchAllSlice.duration = resultSet->GetUint64(col++);
        searchAllSlice.id = resultSet->GetString(col++);
        searchAllSlice.tid = resultSet->GetString(col++);
        searchAllSlice.pid = resultSet->GetString(col++);
        searchAllSlice.rankId = params.rankId;
        searchAllSlice.deviceId = params.rankId;
        searchAllSlice.fileId = params.fileId;
        body.searchAllSlices.emplace_back(searchAllSlice);
    }
}

std::string TextTraceDatabase::GetSearchAllSliceWithLockRangeSql(
    const SearchAllSliceParams &params, const std::vector<TrackQuery> &trackQueryVec) const {
    std::string orderBy;
    if (params.order == "descend") {
        orderBy = " ORDER BY " + params.orderBy + " DESC";
    } else {
        orderBy = " ORDER BY " + params.orderBy + " ASC";
    }
    // 生成带别名s.的name匹配条件
    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "s.name like ?";
    } else if (params.isMatchExact) {
        nameMatch = "lower(s.name) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "s.name like '%'||?||'%'";
    } else {
        nameMatch = "lower(s.name) like lower('%'||?||'%')";
    }
    std::string sql;
    // 根据是否有二级筛选，生成不同的SQL
    if (!params.nameFilter.empty()) {
        std::string nameFilterMatch = "lower(s.name) LIKE lower('%'||?||'%')";
        sql = "SELECT s.name as name, s.timestamp as timestamp, s.duration as duration,"
            " s.id as id, t.tid as tid, t.pid as pid"
            " FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE +
            " t on s.track_id = t.track_id "
            "WHERE " + nameMatch + " AND " + nameFilterMatch + " AND s.track_id = ? AND s.timestamp >= ? AND s.end_time <= ?";
    } else {
        sql = "SELECT s.name as name, s.timestamp as timestamp, s.duration as duration,"
            " s.id as id, t.tid as tid, t.pid as pid"
            " FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE +
            " t on s.track_id = t.track_id "
            "WHERE " + nameMatch + " AND s.track_id = ? AND s.timestamp >= ? AND s.end_time <= ?";
    }
    std::vector<std::string> sqls(trackQueryVec.size(), sql);
    sql = StringUtil::join(sqls, " UNION ALL ");
    sql += orderBy + " limit ? offset ?";
    return sql;
}

bool TextTraceDatabase::SearchAllSlicesDetails(const Protocol::SearchAllSliceParams &params,
    Protocol::SearchAllSlicesBody &body, uint64_t minTimestamp)
{
    std::string sql;
    if (!params.nameFilter.empty()) {
        sql = TextSqlConstant::GetSearchSliceDetailWithFilterSql(params.isMatchExact, params.isMatchCase,
            params.order, params.orderBy);
    } else {
        sql = TextSqlConstant::GetSearchSliceDetailSql(params.isMatchExact, params.isMatchCase, params.order, params.orderBy);
    }

    uint64_t offset = (params.current - 1) * params.pageSize;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Search all slices details failed!.");
        return false;
    }

    std::unique_ptr<SqliteResultSet> resultSet;
    if (!params.nameFilter.empty()) {
        resultSet = stmt->ExecuteQuery(params.searchContent, params.nameFilter, params.pageSize, offset);
    } else {
        resultSet = stmt->ExecuteQuery(params.searchContent, params.pageSize, offset);
    }

    if (resultSet == nullptr) {
        ServerLog::Error("Search all slices details. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        int col = resultStartIndex;
        Protocol::SearchAllSlices searchAllSlice{};
        searchAllSlice.name = resultSet->GetString(col++);
        uint64_t tempStartTime = resultSet->GetUint64(col++);
        if (tempStartTime < minTimestamp) {
            continue;
        }
        searchAllSlice.timestamp = tempStartTime - minTimestamp;
        searchAllSlice.duration = resultSet->GetUint64(col++);
        searchAllSlice.id = resultSet->GetString(col++);
        searchAllSlice.tid = resultSet->GetString(col++);
        searchAllSlice.pid = resultSet->GetString(col++);
        searchAllSlice.rankId = params.rankId;
        searchAllSlice.deviceId = params.rankId;
        searchAllSlice.fileId = params.fileId;
        body.searchAllSlices.emplace_back(searchAllSlice);
    }
    body.currentPage = params.current;
    body.pageSize = params.pageSize;

    // 计算总数
    if (!params.nameFilter.empty()) {
        auto countStmt = CreatPreparedStatement(
            TextSqlConstant::GetSearchSliceCountWithFilterSql(params.isMatchExact, params.isMatchCase));
        if (countStmt != nullptr) {
            auto countResult = countStmt->ExecuteQuery(params.searchContent, params.nameFilter);
            if (countResult != nullptr && countResult->Next()) {
                body.count = countResult->GetUint64("count");
            }
        }
    } else {
        Protocol::SearchCountParams searchCountParams;
        searchCountParams.searchContent = params.searchContent;
        searchCountParams.isMatchCase = params.isMatchCase;
        searchCountParams.isMatchExact = params.isMatchExact;
        searchCountParams.rankId = params.rankId;
        body.count = SearchSliceNameCount(searchCountParams, {});
    }
    return true;
}

bool TextTraceDatabase::QueryAclnnOpCountExceedThreshold(const KernelDetailsParams &params, uint64_t threshold,
    std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp) {
    auto stmt = CreatPreparedStatement(TextSqlConstant::GenerateAclnnQueryTextSql(params));
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for Aclnn Op Exceed Threshold.");
        return false;
    }
    int deviceId = StringUtil::StringToInt(params.deviceId);
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp, deviceId, threshold);
    } else {
        resultSet = stmt->ExecuteQuery(
            minTimestamp, deviceId, params.startTime + minTimestamp, params.endTime + minTimestamp, threshold);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for Aclnn Op Exceed Threshold.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        data.emplace_back(one);
    }
    return true;
}

bool TextTraceDatabase::QueryOperatorDispatchData(const Protocol::KernelDetailsParams &params,
    std::vector<Protocol::KernelBaseInfo> &data, uint64_t minTimestamp, uint64_t threshold) {
    auto stmt = CreatPreparedStatement(TextSqlConstant::GenerateOperatorDispatchQueryTextSql(params));
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for Operator Dispatch data.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for Operator Dispatch data.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        data.emplace_back(one);
    }
    if (data.size() > 0 && data.size() < threshold) {
        ServerLog::Error(
            "Failed to get Operator Dispatch data because the total count should greater than or equal to " +
            std::to_string(threshold) + " .");
        data.clear();
    }
    return true;
}

std::string TextTraceDatabase::QueryHostInfo() {
    std::string host;
    return host;
}

bool TextTraceDatabase::QueryFwdBwdDataByFlow(const std::string &rankId, uint64_t offset,
    const Protocol::ExtremumTimestamp &range, std::vector<Protocol::ThreadTraces> &fwdBwdData) {
    std::vector<std::string> tableList = {SLICE_TABLE, FLOW_TABLE};
    if (!CheckTablesExist(tableList)) {
        ServerLog::Error("Failed to check dependent table for query fwdbwd data in the TEXT scenario.");
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!ExecSql(CREATE_TEMP_FWDBWD_FLOW_TABLE_TEXT_SQL)) {
        ServerLog::Error("Failed to create temp fwdbwd table in the TEXT scenario.");
        return false;
    }
    auto stmt = CreatPreparedStatement(QUERY_FWDBWD_FLOW_DATA_SQL);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query fwd/bwd data by flow in the TEXT scenario.");
        return false;
    }
    return TraceDatabaseHelper::ExecuteQueryFwdBwdDataByFlow(std::move(stmt), rankId, offset, range, fwdBwdData);
}

bool TextTraceDatabase::QueryP2PCommunicationOpData(const std::string &rankId, uint64_t offset,
    const ExtremumTimestamp &range, std::vector<Protocol::ThreadTraces> &p2pOpData) {
    auto stmt = CreatPreparedStatement(QUERY_P2P_COMMUNICATION_OP_TEXT_SQL);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query p2p communication op data in the TEXT scenario.");
        return false;
    }
    return TraceDatabaseHelper::ExecuteQueryP2POpData(std::move(stmt), rankId, offset, range, p2pOpData);
}

bool TextTraceDatabase::QueryByteAlignmentAnalyzerData(std::vector<CommunicationLargeOperatorInfo> &data) {
    std::vector<std::pair<std::string, std::string>> rawData;
    if (!QueryByteAlignmentAnalyzerRawData(rawData)) {
        return false;
    }
    ProcessByteAlignmentAnalyzerDataForText(data, rawData);
    return true;
}

bool TextTraceDatabase::QueryByteAlignmentAnalyzerRawData(std::vector<std::pair<std::string, std::string>> &rawData) {
    std::string sql = QUERY_BYTE_ALIGNMENT_ANALYZER_DATA_SQL;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query byte alignment analyzer data. Error: ", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::pair<std::string, std::string> item;
        item.first = sqlite3_column_string(stmt, col++);
        item.second = sqlite3_column_string(stmt, col++);
        rawData.emplace_back(item);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool TextTraceDatabase::DeleteEmptyThread() {
    if (!isOpen) {
        ServerLog::Error("Failed to delete empty thread. Database is not open.");
        return false;
    }
    std::string sql = "DELETE FROM thread "
                      " WHERE NOT EXISTS ("
                      "    SELECT 1 FROM slice WHERE slice.track_id = thread.track_id"
                      ") AND NOT EXISTS (SELECT 1 FROM counter WHERE counter.name = thread.tid);";
    std::unique_lock<std::recursive_mutex> lock(mutex);
    ExecSql(sql);
    return true;
}

bool TextTraceDatabase::DeleteEmptyFlow() {
    if (!isOpen) {
        ServerLog::Error("Failed to delete empty flow. Database is not open.");
        return false;
    }
    std::string sql = "DELETE FROM flow "
                      " WHERE NOT EXISTS ("
                      "    SELECT 1 FROM slice WHERE slice.track_id = flow.track_id"
                      ");";
    std::unique_lock<std::recursive_mutex> lock(mutex);
    ExecSql(sql);
    return true;
}

std::vector<std::pair<std::string, std::string>> TextTraceDatabase::QueryTableDataNameList() {
    std::string sql = "Select name, view_name from data_table Order BY id;";
    auto stmt = CreatPreparedStatement(sql);
    if (!TryOpt(stmt, "Query table data name failed!")) {
        return {};
    }
    auto resultSet = stmt->ExecuteQuery();
    if (!TryOpt(resultSet, "Query table data name failed to get result!")) {
        return {};
    }
    std::vector<std::pair<std::string, std::string>> res;
    while (resultSet->Next()) {
        std::string name = resultSet->GetString("name");
        std::string viewName = resultSet->GetString("view_name");
        std::pair<std::string, std::string> pair = std::make_pair(viewName, name);
        res.emplace_back(pair);
    }
    return res;
}

bool TextTraceDatabase::LoadSliceCache(LightSliceCache& cache,
    const Protocol::SearchAllSliceParams& params, uint64_t minTimestamp)
{
    cache.clear();
    cache.rankId = params.rankId;
    cache.primarySearchContent = params.searchContent;

    // TextTraceDatabase 的 slice 表直接存储名称，字段是 timestamp 和 duration
    // 时间戳需要减去 minTimestamp 转换为相对时间
    std::string nameMatch = TextSqlConstant::GetSearchNameSqlSuffix(params.isMatchExact, params.isMatchCase);
    std::string sql = "SELECT ROWID as rowId, name, timestamp - " + std::to_string(minTimestamp) +
                      " as startTime, duration FROM " + sliceTable + " WHERE " + nameMatch;

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("LoadSliceCache: Failed to prepare sql.");
        return false;
    }

    auto result = stmt->ExecuteQuery(params.searchContent);
    if (result == nullptr) {
        ServerLog::Error("LoadSliceCache: Failed to execute query.");
        return false;
    }

    int32_t stringIdCounter = 0;
    while (result->Next()) {
        std::string name = result->GetString("name");
        int32_t stringId = stringIdCounter++;

        cache.dictMap[stringId] = name;
        cache.tableTypes.push_back(static_cast<int8_t>(SliceTableType::TASK));
        cache.rowIds.push_back(result->GetUint64("rowId"));
        cache.stringIds.push_back(stringId);
        cache.timestamps.push_back(result->GetUint64("startTime"));
        cache.durations.push_back(result->GetUint64("duration"));
    }

    SearchSliceCacheManager::InitializeSortedIndices(cache);
    return cache.size() > 0;
}

bool TextTraceDatabase::FetchSliceDetails(const LightSliceCache& cache,
    const std::vector<TargetRow>& rows,
    const Protocol::SearchAllSliceParams& params,
    Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp)
{
    if (rows.empty()) return true;

    // 构建查询 SQL
    // 时间戳需要减去 minTimestamp 转换为相对时间
    std::string idList;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) idList += ",";
        idList += std::to_string(rows[i].rowId);
    }

    std::string sql = "SELECT s.ROWID as rowId, s.name, s.timestamp - " + std::to_string(minTimestamp) +
                      " as startTime, s.duration, t.tid, t.pid FROM " + sliceTable + " s" +
                      " JOIN " + threadTable + " t ON s.track_id = t.track_id" +
                      " WHERE s.ROWID IN (" + idList + ")";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) return false;

    auto result = stmt->ExecuteQuery();
    if (result == nullptr) return false;

    while (result->Next()) {
        Protocol::SearchAllSlices slice{};
        slice.fileId = params.fileId;
        slice.name = result->GetString("name");
        slice.timestamp = result->GetUint64("startTime");
        slice.duration = result->GetUint64("duration");
        slice.id = std::to_string(result->GetUint64("rowId"));
        slice.tid = result->GetString("tid");
        slice.pid = result->GetString("pid");
        slice.depth = 0;  // TextTraceDatabase 通常没有 depth
        slice.rankId = params.rankId;
        slice.deviceId = params.rankId;

        body.searchAllSlices.push_back(slice);
    }

    return true;
}

} // end of namespace Dic::Module::Timeline
// clang-format on
