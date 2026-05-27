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
#include "DbTraceDataBase.h"
#include "TrackInfoManager.h"
#include "pch.h"
#include "TraceTime.h"
#include "TableDefs.h"
#include "TraceDatabaseHelper.h"
#include "TraceDatabaseSqlConst.h"
#include "CommonDefs.h"
#include "DataBaseManager.h"
#include "CollectionTimeService.h"
#include "MetaDataParser.h"
#include "MetaDataCacheManager.h"
#include "CounterEventHelper.h"

// clang-format off
namespace Dic::Module::FullDb {
using namespace Server;
std::map<std::string, std::map<std::string, std::string>> DbTraceDataBase::stringsCache = {};

DbTraceDataBase::DbTraceDataBase(std::recursive_mutex &sqlMutex) : VirtualTraceDatabase(sqlMutex) {
    if (sliceAnalyzerPtr == nullptr) {
        sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    }
    if (flowAnalyzerPtr == nullptr) {
        flowAnalyzerPtr = std::make_unique<FlowAnalyzer>();
    }
}

DbTraceDataBase::~DbTraceDataBase() {
    updateCannApiDepthStmt = nullptr;
    insertOverlapStmt = nullptr;
    updateApiDepthStmt = nullptr;
    updateTaskDepthStmt = nullptr;
    sliceAnalyzerPtr = nullptr;
    flowAnalyzerPtr = nullptr;
}

bool DbTraceDataBase::QueryThreads(const Protocol::UnitThreadsParams &requestParams,
    Protocol::UnitThreadsBody &responseBody, uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) {
    uint64_t startTime = requestParams.startTime + minTimestamp;
    uint64_t endTime = requestParams.endTime + minTimestamp;
    // 遍历metadataList,这里不用union all,方便后续返回中拼接pid和tid等信息用于前端过滤
    std::vector<SimpleSlice> simpleSliceVec;
    std::map<std::string, uint64_t> selfTimeKeyValue;
    for (auto &&metadata : requestParams.metadataList) {
        std::vector<Protocol::SimpleSlice> completeSlice =
            QueryThreadByPid(metadata, startTime, endTime, requestParams.rankId, selfTimeKeyValue);
        simpleSliceVec.insert(simpleSliceVec.end(), completeSlice.begin(), completeSlice.end());
    }
    // process data
    if (simpleSliceVec.empty()) {
        responseBody.emptyFlag = true;
        return true;
    }

    std::vector<Protocol::SimpleSlice> nRows =
        TraceDatabaseHelper::ThreadsInfoFilter(requestParams, simpleSliceVec, startTime, endTime);
    TraceDatabaseHelper::ReduceThread(nRows, selfTimeKeyValue, responseBody);
    return true;
}

std::map<std::string, std::string> DbTraceDataBase::QueryAllModelIdOfAscendHardwareThreads() {
    std::map<std::string, std::string> res;
    // Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
    // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
    // 非MSTX事件查询时必须使用connectionId NOT IN显式排除MSTX事件，否则会将MSTX事件同时查询
    // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
    // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
    const std::string sql = "select main.streamId as tid, main.modelId as modelId from " + TABLE_TASK +
        " main "
        " where main.connectionId not in (select connectionId from " +
        TABLE_MSTX_EVENTS +
        ")"
        " group by main.streamId;";
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
        const auto modelId = resultSet->GetString("modelId");
        res[tid] = modelId;
    }
    return res;
}

bool DbTraceDataBase::QueryUnitsMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    QueryHostMetadata(fileId, metaData);
    bool existOverlapAnalysis = false;
    if (CheckTableExist(TABLE_TASK)) {
        QueryAscendHardwareOperatorMetadata(fileId, metaData);
        existOverlapAnalysis = true;
    }
    // 通信大算子支持独立展示，不依赖COMMUNICATION_TASK_INFO表和TASK表
    if (CheckTableExist(TABLE_COMMUNICATION_OP)) {
        QueryHCCLOperatorMetadata(fileId, metaData);
        existOverlapAnalysis = true;
    }
    // 只要TASK表或者COMMUNICATION_OP表存在，展示覆盖分析，TASK表反映计算信息，COMMUNICATION_OP表反映通信信息
    if (existOverlapAnalysis) {
        GenerateOverlapAnalysisMetadata(fileId, metaData);
    }
    QueryCounterMetadata(fileId, metaData);
    ProcessHostCounterEventsMetadata(fileId, metaData);
    return false;
}

bool DbTraceDataBase::GenerateOverlapAnalysisMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    auto metaType = ENUM_TO_STR(PROCESS_TYPE::OVERLAP_ANALYSIS).value_or("");
    auto overlap_analysis = GenerateBaseUnitTrack("process", fileId, metaType, "Overlap Analysis", metaType);
    if (stringsCache.find(path) == stringsCache.end()) {
        stringsCache[path] = {}; // 初始化空的映射
    }
    for (size_t index = 0; index < OVERLAP_TYPES.size(); index++) {
        auto thread = GenerateBaseUnitTrack("thread", fileId, overlap_analysis->metaData.processId, "", metaType);
        thread->metaData.threadId = std::to_string(index);
        thread->metaData.threadName = OVERLAP_TYPES[index];
        thread->metaData.maxDepth = 1;
        overlap_analysis->children.emplace_back(std::move(thread));
        stringsCache.at(path).emplace(metaType + std::to_string(index), OVERLAP_TYPES[index]);
    }
    metaData.emplace_back(std::move(overlap_analysis));
    return true;
}

bool DbTraceDataBase::QueryExtremumTimestamp(uint64_t &min, uint64_t &max) { return true; }

bool DbTraceDataBase::SetCardAlias(
    const Protocol::SetCardAliasParams &requestParams, Protocol::SetCardAliasBody &responseBody) {
    if (!CheckTableExist(metaDataTable)) {
        ServerLog::Error("Failed to set card alias because table is not exist.");
        return false;
    }
    return UpdateMetaDataTableWithNoPrimaryKey(cardAliasName, requestParams.cardAlias);
}

std::string DbTraceDataBase::QueryCardAlias() {
    std::string cardAlias = GetValueFromMetaDataTable(cardAliasName);
    if (cardAlias.empty()) {
        return "";
    }
    return cardAlias;
}

uint32_t DbTraceDataBase::SearchSliceNameCount(const Protocol::SearchCountParams &params) {
    uint32_t result = 0;
    SearchSliceSqlParams sqlParams;
    sqlParams.isMatchExact = params.isMatchExact;
    sqlParams.isMatchCase = params.isMatchCase;
    sqlParams.rankId = params.rankId;

    const std::string &sql = GetSearchSliceNameCountSql(sqlParams);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name count failed!.");
        return 0;
    }
    auto resultSet = stmt->ExecuteQuery(params.searchContent, GetDeviceId(params.rankId));
    if (resultSet == nullptr) {
        ServerLog::Error("Query_slice_name_count. Failed to get result set.", stmt->GetErrorMessage());
        return 0;
    }
    if (resultSet->Next()) {
        result = resultSet->GetUint32("count");
    }
    return result;
}

uint32_t DbTraceDataBase::SearchSliceNameCount(
    const Protocol::SearchCountParams &params, const std::vector<TrackQuery> &trackQuery) {
    if (trackQuery.empty() && !params.metadataList.empty()) {
        return 0;
    }
    if (trackQuery.empty()) {
        return SearchSliceNameCount(params);
    }
    std::string sql = GetSearchCountWithLockSql(params, trackQuery);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name count failed!.");
        return 0;
    }
    TraceDatabaseHelper::SearchCountWithLockRangeBindStmt(params, trackQuery, stmt, GetDeviceId(params.rankId));
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Query slice name count. Failed to get result set.", stmt->GetErrorMessage());
        return 0;
    }
    uint32_t result = 0;
    while (resultSet->Next()) {
        const uint32_t count = resultSet->GetUint32("count");
        if (result > UINT32_MAX - count) {
            ServerLog::Warn("Sum of searching slice name count is overflow.");
            break;
        }
        result = result + count;
    }
    return result;
}

bool DbTraceDataBase::QueryFlowCategoryList(std::vector<std::string> &categories, const std::string &rankId) {
    auto stmt = CreatPreparedStatement("select cat from connectionCats group by cat");
    if (stmt == nullptr) {
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        return false;
    }
    while (resultSet->Next()) {
        categories.emplace_back(resultSet->GetString("cat"));
    }
    return true;
}

bool DbTraceDataBase::SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
    Protocol::SearchSliceBody &responseBody) {
    std::string sql = GetSearchSliceNameSql(params.isMatchExact, params.isMatchCase, responseBody.rankId, path);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(params.searchContent, minTimestamp, GetDeviceId(responseBody.rankId), index);
    if (resultSet == nullptr || !resultSet->Next()) {
        ServerLog::Error("Query_slice_name. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    responseBody.pid = resultSet->GetString("pid");
    responseBody.tid = resultSet->GetString("tid");
    responseBody.startTime = resultSet->GetUint64("startTime");
    responseBody.duration = resultSet->GetUint64("duration");
    responseBody.id = resultSet->GetString("id");
    std::string metaType = resultSet->GetString("metaType");
    SliceQuery sliceQuery = CreateSliceQueryWithTimeRange({responseBody.rankId, responseBody.pid, responseBody.tid,
        metaType, responseBody.startTime, responseBody.duration});
    responseBody.depth = GetSliceDepthForJump(sliceQuery, NumberUtil::StringToUnsignedLongLong(responseBody.id));
    return true;
}

bool DbTraceDataBase::SearchSliceName(const Protocol::SearchSliceParams &params, int index, uint64_t minTimestamp,
    Protocol::SearchSliceBody &responseBody, const std::vector<TrackQuery> &trackQuery) {
    if (trackQuery.empty() && !params.metadataList.empty()) {
        return true;
    }
    if (trackQuery.empty()) {
        return SearchSliceName(params, index, minTimestamp, responseBody);
    }
    std::string sql = TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(params, trackQuery, path);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }
    TraceDatabaseHelper::SearchSliceNameWithLockRangeBindStmt(
        params, trackQuery, stmt, path, GetDeviceId(params.rankId));
    auto resultSet = stmt->ExecuteQuery(index);
    if (resultSet == nullptr || !resultSet->Next()) {
        ServerLog::Error("Query_slice_name. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    responseBody.pid = resultSet->GetString("pid");
    responseBody.tid = resultSet->GetString("tid");
    responseBody.startTime = resultSet->GetUint64("timestamp");
    uint64_t endTime = resultSet->GetUint64("endTime");
    responseBody.duration = endTime >= responseBody.startTime ? endTime - responseBody.startTime : 0;
    responseBody.startTime -= minTimestamp; // 业务上 minTimestamp 是最小的时间，一定有 item.timestamp > minTimestamp
    responseBody.id = resultSet->GetString("id");
    std::string metaType = resultSet->GetString("metaType");
    SliceQuery sliceQuery = CreateSliceQueryWithTimeRange({responseBody.rankId, responseBody.pid, responseBody.tid,
        metaType, responseBody.startTime, responseBody.duration});
    responseBody.depth = GetSliceDepthForJump(sliceQuery, NumberUtil::StringToUnsignedLongLong(responseBody.id));
    return true;
}

bool DbTraceDataBase::QueryHostSlicesByName(const std::string &sliceName, const std::string &metaType,
    std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    if (!CheckTableExist(TABLE_STRING_IDS)) {
        return true;
    }

    std::string sql;
    std::string processSql;
    if (metaType == TABLE_CANN_API && CheckTableExist(TABLE_CANN_API)) {
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, cann.globalTid as pid, 'CANN_API' as metaType, "
              "cann.startNs as startTime, cann.endNs - cann.startNs as duration, "
              "cann.ROWID as id from " +
            TABLE_CANN_API + " cann join ids on ids.id = cann.name";
        processSql = "SELECT DISTINCT globalTid AS pid FROM " + TABLE_CANN_API;
    } else if (metaType == TABLE_API && CheckTableExist(TABLE_API)) {
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, python.globalTid as pid, 'PYTORCH_API' as metaType, "
              "python.startNs as startTime, python.endNs - python.startNs as duration, "
              "python.ROWID as id from " +
            TABLE_API + " python join ids on ids.id = python.name";
        processSql = "SELECT DISTINCT globalTid AS pid FROM " + TABLE_API;
    } else if (metaType == TABLE_MSTX_EVENTS && CheckTableExist(TABLE_MSTX_EVENTS)) {
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, mstx.globalTid as pid, 'MSTX_EVENTS' as metaType, "
              "mstx.startNs as startTime, mstx.endNs - mstx.startNs as duration, "
              "mstx.ROWID as id from " +
            TABLE_MSTX_EVENTS + " mstx join ids on ids.id = mstx.message";
        processSql = "SELECT DISTINCT globalTid AS pid FROM " + TABLE_MSTX_EVENTS;
    } else if (metaType == TABLE_OSRT_API && CheckTableExist(TABLE_OSRT_API)) {
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, osrt.globalTid as pid, 'OSRT_API' as metaType, "
              "osrt.startNs as startTime, osrt.endNs - osrt.startNs as duration, "
              "osrt.ROWID as id from " +
            TABLE_OSRT_API + " osrt join ids on ids.id = osrt.name";
        processSql = "SELECT DISTINCT globalTid AS pid FROM " + TABLE_OSRT_API;
    }

    if (!sql.empty()) {
        auto stmt = CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Error("Query host slices by name failed to prepare sql.");
            return false;
        }
        auto resultSet = stmt->ExecuteQuery(sliceName);
        if (resultSet == nullptr) {
            ServerLog::Error("Query host slices by name failed to get result set.", stmt->GetErrorMessage());
            return false;
        }
        while (resultSet->Next()) {
            Protocol::SimpleSlice slice;
            slice.name = resultSet->GetString("name");
            slice.pid = resultSet->GetString("pid");
            slice.metaType = resultSet->GetString("metaType");
            slice.timestamp = resultSet->GetUint64("startTime");
            slice.duration = resultSet->GetUint64("duration");
            slice.id = resultSet->GetUint64("id");
            result.emplace_back(slice);
        }
    }

    if (processSql.empty()) {
        return true;
    }
    auto processStmt = CreatPreparedStatement(processSql);
    if (processStmt == nullptr) {
        ServerLog::Error("Query host rank offset process ids failed to prepare sql.");
        return false;
    }
    auto processResultSet = processStmt->ExecuteQuery();
    if (processResultSet == nullptr) {
        ServerLog::Error(
            "Query host rank offset process ids failed to get result set.", processStmt->GetErrorMessage());
        return false;
    }
    while (processResultSet->Next()) {
        processIds.insert(processResultSet->GetString("pid"));
    }
    return true;
}

bool DbTraceDataBase::QueryDeviceSlicesByName(const std::string &rankId, const std::string &sliceName,
    const std::string &metaType, std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    if (!CheckTableExist(TABLE_STRING_IDS)) {
        return true;
    }

    std::string sql;
    std::string processSql;
    std::string deviceId = GetDeviceId(rankId);
    bool bindDeviceIdForSlice = false;
    bool bindDeviceIdForProcess = false;
    if (metaType == "Ascend Hardware" && CheckTableExist(TABLE_TASK)) {
        bool hasCompute = CheckTableExist(TABLE_COMPUTE_TASK_INFO);
        bool hasSchedule = CheckTableExist(TABLE_COMMUNICATION_SCHEDULE_TASK);
        std::string taskNameExpr = "main.taskType";
        std::string taskJoinSql;
        if (hasCompute) {
            taskJoinSql += " left join COMPUTE_TASK_INFO compute on compute.globalTaskId = main.globalTaskId ";
            taskNameExpr = "coalesce(compute.name, " + taskNameExpr + ")";
        }
        if (hasSchedule) {
            taskJoinSql += " left join COMMUNICATION_SCHEDULE_TASK_INFO schedule "
                           "on main.globalTaskId = schedule.globalTaskId ";
            taskNameExpr = "coalesce(schedule.name, " + taskNameExpr + ")";
        }
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, 'Ascend Hardware' as pid, 'Ascend Hardware' as metaType, "
              "main.startNs as startTime, main.endNs - main.startNs as duration, "
              "main.ROWID as id from TASK main" +
            taskJoinSql + " join ids on ids.id = " + taskNameExpr + " where main.deviceId = ?";
        processSql = "SELECT DISTINCT 'Ascend Hardware' AS pid FROM TASK WHERE deviceId = ?";
        bindDeviceIdForSlice = true;
        bindDeviceIdForProcess = true;
    } else if (metaType == "HCCL" && CheckTableExist(TABLE_COMMUNICATION_OP)) {
        std::string associationTaskSql;
        if (CheckTableExist(TABLE_TASK) && !TraceDatabaseHelper::IsDeviceIdUnique(path)) {
            associationTaskSql = " join TASK associatedTask on op.connectionId = associatedTask.connectionId "
                                 "and associatedTask.deviceId = ? ";
            bindDeviceIdForSlice = true;
        }
        sql = "with ids as (select id, value from STRING_IDS where value = ?) "
              "select ids.value as name, 'HCCL' as pid, 'HCCL' as metaType, "
              "op.startNs as startTime, op.endNs - op.startNs as duration, op.ROWID as id "
              "from COMMUNICATION_OP op" +
            associationTaskSql + " join ids on ids.id = op.opName group by op.opId";
        processSql = "SELECT DISTINCT 'HCCL' AS pid FROM COMMUNICATION_OP";
    }

    if (!sql.empty()) {
        auto stmt = CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Error("Query device slices by name failed to prepare sql.");
            return false;
        }
        std::unique_ptr<SqliteResultSet> resultSet =
            bindDeviceIdForSlice ? stmt->ExecuteQuery(sliceName, deviceId) : stmt->ExecuteQuery(sliceName);
        if (resultSet == nullptr) {
            ServerLog::Error("Query device slices by name failed to get result set.", stmt->GetErrorMessage());
            return false;
        }
        while (resultSet->Next()) {
            Protocol::SimpleSlice slice;
            slice.name = resultSet->GetString("name");
            slice.pid = resultSet->GetString("pid");
            slice.metaType = resultSet->GetString("metaType");
            slice.timestamp = resultSet->GetUint64("startTime");
            slice.duration = resultSet->GetUint64("duration");
            slice.id = resultSet->GetUint64("id");
            result.emplace_back(slice);
        }
    }

    if (processSql.empty()) {
        return true;
    }
    auto processStmt = CreatPreparedStatement(processSql);
    if (processStmt == nullptr) {
        ServerLog::Error("Query device rank offset process ids failed to prepare sql.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> processResultSet =
        bindDeviceIdForProcess ? processStmt->ExecuteQuery(deviceId) : processStmt->ExecuteQuery();
    if (processResultSet == nullptr) {
        ServerLog::Error(
            "Query device rank offset process ids failed to get result set.", processStmt->GetErrorMessage());
        return false;
    }
    while (processResultSet->Next()) {
        processIds.insert(processResultSet->GetString("pid"));
    }
    return true;
}

bool DbTraceDataBase::QueryTextSlicesByName(const std::string &sliceName, const std::string &metaType,
    std::vector<Protocol::SimpleSlice> &result, std::set<std::string> &processIds) {
    return true;
}

bool DbTraceDataBase::QueryComputeStatisticsData(
    const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) {
    sqlite3_stmt *stmt = nullptr;
    std::string sql =
        "SELECT round(sum(endNs - startNs) / 1000.0, 2) as duration, TASKTYPE.value as acceleratorCore "
        "FROM COMPUTE_TASK_INFO JOIN TASK ON COMPUTE_TASK_INFO.globalTaskId = TASK.globalTaskId "
        "JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType "
        "WHERE acceleratorCore in ('AI_CPU','AI_CORE', 'AI_VECTOR_CORE', 'MIX_AIC', 'MIX_AIV', 'FFTS_PLUS') "
        "GROUP BY acceleratorCore";
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Query compute statistics data failed!. ", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return false;
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

bool DbTraceDataBase::QueryCommunicationStatisticsData(
    const Protocol::SummaryStatisticParams &requestParams, Protocol::SummaryStatisticsBody &responseBody) {
    return false;
}

bool DbTraceDataBase::QueryStepDuration(const std::string &stepId, uint64_t &min, uint64_t &max) { return false; }

bool DbTraceDataBase::QuerySystemViewData(const Protocol::SystemViewParams &requestParams,
    Protocol::SystemViewBody &responseBody, const uint64_t &minTimestamp) {
    auto stmt = CreatPreparedStatement(); // 这里不需要判断空指针，TraceDatabaseHelper里面统一进行了判空操作
    std::unique_ptr<SqliteResultSet> resultSet;
    try {
        const std::string &timeCondSql =
            TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(requestParams.startTime, requestParams.endTime);
        resultSet = TraceDatabaseHelper::QuerySystemViewData(
            stmt, requestParams, GetDeviceId(requestParams.rankId), minTimestamp, timeCondSql);
    } catch (DatabaseException &e) {
        ServerLog::Error("Query system view data failed, ", e.What());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::SystemViewDetail systemViewDetail;
        int col = resultStartIndex;
        systemViewDetail.name = resultSet->GetString(col++);
        systemViewDetail.time = resultSet->GetDouble(col++);
        systemViewDetail.totalTime = resultSet->GetDouble(col++);
        systemViewDetail.numberCalls = resultSet->GetUint64(col++);
        systemViewDetail.avg = resultSet->GetDouble(col++);
        systemViewDetail.min = resultSet->GetDouble(col++);
        systemViewDetail.max = resultSet->GetDouble(col++);
        if (responseBody.total == 0) {
            responseBody.total = resultSet->GetUint64(col++);
        }
        responseBody.systemViewDetail.emplace_back(systemViewDetail);
    }
    responseBody.pageSize = requestParams.pageSize;
    responseBody.currentPage = requestParams.current;
    return true;
}

bool DbTraceDataBase::QueryExpAnaAICoreFreqData(const Protocol::SystemViewAICoreFreqParams &requestParams,
    Protocol::ExpAnaAICoreFreqBody &responseBody, std::vector<std::pair<uint64_t, uint64_t>> &freqs, uint64_t &maxFreq,
    uint64_t &minFreq) {
    std::unique_ptr<SqliteResultSet> resultSet;
    std::string sql = "SELECT timestampNs, freq FROM AICORE_FREQ "
                      "WHERE deviceId = ? ORDER BY timestampNs ASC;";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query system view AI core freq data failed!.");
        return false;
    }
    int deviceId = StringUtil::StringToInt(requestParams.deviceId);
    resultSet = stmt->ExecuteQuery(deviceId);
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query AI core freq.", stmt->GetErrorMessage());
        return 0;
    }
    while (resultSet->Next()) {
        std::pair<uint64_t, uint64_t> detail;
        int col = resultStartIndex;
        detail.first = resultSet->GetUint64(col++);
        detail.second = resultSet->GetUint64(col++);
        responseBody.pid = "AI Core Freq";
        maxFreq = std::max(maxFreq, detail.second);
        minFreq = std::min(minFreq, detail.second);
        freqs.emplace_back(detail);
    }
    return true;
}

bool DbTraceDataBase::QueryKernelDetailData(const Protocol::KernelDetailsParams &requestParams,
    Protocol::KernelDetailsBody &responseBody, uint64_t minTimestamp) {
    const std::string blockNumColumnName =
        (CheckColumnExist(TABLE_COMPUTE_TASK_INFO, "blockNum") ? "blockNum" : "blockDim");
    std::string sql = GetKernelDetailSql(requestParams, blockNumColumnName);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        Server::ServerLog::Error("Fail to prepare sql to query kernel detail data.");
        return false;
    }
    for (const auto &filter : requestParams.filters) { // 第一次绑定filter
        std::string bindFilter = "%" + filter.second + "%";
        stmt->BindParams(bindFilter);
    }
    stmt->BindParams(StringUtil::StringToInt(requestParams.deviceId));
    for (const auto &filter : requestParams.filters) { // 第二次绑定filter
        std::string bindFilter = "%" + filter.second + "%";
        stmt->BindParams(bindFilter);
    }
    if (requestParams.startTime != requestParams.endTime) {
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    if (!ExcecuteQueryKernelDetailData(stmt, requestParams, responseBody, minTimestamp, blockNumColumnName)) {
        return false;
    }
    responseBody.pageSize = requestParams.pageSize;
    responseBody.currentPage = requestParams.current;
    responseBody.acceleratorCoreList = QueryCoreType();
    return true;
}

bool DbTraceDataBase::ExcecuteQueryKernelDetailData(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::KernelDetailsParams &requestParams, Protocol::KernelDetailsBody &responseBody,
    uint64_t minTimestamp, const std::string &blockNumColumnName) {
    uint64_t offset = (requestParams.current - 1) > UINT64_MAX / requestParams.pageSize
        ? 0
        : (requestParams.current - 1) * requestParams.pageSize;
    auto resultSet = stmt->ExecuteQuery(requestParams.pageSize, offset);
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query kernel detail data.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::KernelDetail detail;
        detail.id = resultSet->GetString("id");
        detail.name = resultSet->GetString("name");
        detail.type = GetStringCacheValue(path, resultSet->GetString("type"));
        detail.acceleratorCore = GetStringCacheValue(path, resultSet->GetString("acceleratorCore"));
        uint64_t tempStartTime = resultSet->GetUint64("startTime");
        if (tempStartTime < minTimestamp) {
            continue;
        }
        detail.startTime = tempStartTime - minTimestamp;
        detail.duration = resultSet->GetDouble("duration");
        detail.waitTime = resultSet->GetDouble("waitTime");
        detail.blockNum = resultSet->GetUint64(blockNumColumnName);
        detail.inputShapes = GetStringCacheValue(path, resultSet->GetString("inputShapes"));
        detail.inputDataTypes = GetStringCacheValue(path, resultSet->GetString("inputDataTypes"));
        detail.inputFormats = GetStringCacheValue(path, resultSet->GetString("inputFormats"));
        detail.outputShapes = GetStringCacheValue(path, resultSet->GetString("outputShapes"));
        detail.outputDataTypes = GetStringCacheValue(path, resultSet->GetString("outputDataTypes"));
        detail.outputFormats = GetStringCacheValue(path, resultSet->GetString("outputFormats"));
        if (responseBody.count == 0) {
            responseBody.count = resultSet->GetUint64("num");
        }
        detail.taskId = std::to_string(resultSet->GetUint64("taskId"));
        responseBody.kernelDetails.emplace_back(detail);
    }
    return true;
}

uint64_t DbTraceDataBase::QueryTotalKernel(const Protocol::KernelDetailsParams &requestParams, uint64_t minTimestamp) {
    std::string sql = "SELECT count(1) as num FROM " + TABLE_COMPUTE_TASK_INFO;
    if (!requestParams.coreType.empty()) {
        sql += " AND accelerator_core = ? ";
    }
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        Server::ServerLog::Error("Fail to prepare sql to query total kernel.");
        return 0;
    }
    if (!requestParams.coreType.empty()) {
        stmt->BindParams(requestParams.coreType);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query total kernel.", stmt->GetErrorMessage());
        return 0;
    }
    uint64_t total = 0;
    if (resultSet->Next()) {
        total = resultSet->GetUint64("num");
    }
    return total;
}

bool DbTraceDataBase::QueryCommunicationKernelInfo(
    const std::string &name, const std::string &rankId, CommunicationKernelBody &body) {
    std::string sql = "SELECT info.ROWID as id, info.groupName||'group' as tid, info.opName as name, 'HCCL' as pid, "
                      "info.startNs from COMMUNICATION_OP info ";
    auto getDeviceStmt = CreatPreparedStatement();
    bool isDeviceIdUnique = TraceDatabaseHelper::IsDeviceIdUnique(rankId);
    if (!isDeviceIdUnique) {
        sql += "LEFT JOIN COMMUNICATION_TASK_INFO taskInfo ON info.opId = taskInfo.opId "
               "LEFT JOIN TASK ON TASK.globalTaskId = taskInfo.globalTaskId "
               " where opName = (select id from STRING_IDS where value = ?) and TASK.deviceId = ?";
    } else {
        sql += "where opName = (select id from STRING_IDS where value = ?)";
    }
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql to query kernel depth and thread.");
        return false;
    }
    std::string deviceId = GetDeviceId(rankId);
    std::unique_ptr<SqliteResultSet> resultSet =
        isDeviceIdUnique ? stmt->ExecuteQuery(name) : stmt->ExecuteQuery(name, deviceId);
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query kernel depth and thread.", stmt->GetErrorMessage());
        return false;
    }
    if (resultSet->Next()) {
        // 通信大算子depth恒定是0
        body.id = resultSet->GetString("id");
        body.threadId = resultSet->GetString("tid");
        body.pid = resultSet->GetString("pid");
        body.rankId = QueryHostInfo() + rankId;
        body.startTime = resultSet->GetUint64("startNs");
        body.startTime = body.startTime > Timeline::TraceTime::Instance().GetStartTime()
            ? body.startTime - Timeline::TraceTime::Instance().GetStartTime()
            : body.startTime;
    }
    return true;
}

bool DbTraceDataBase::QueryKernelDepthAndThread(
    const Protocol::KernelParams &params, Protocol::OneKernelBody &responseBody, uint64_t minTimestamp) {
    // 精度缺失，设置500的浮动区间
    std::string sql = QUERY_KERNEL_SQL;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql to query kernel depth and thread.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    uint64_t timestamp = params.timestamp + minTimestamp;
    constexpr uint8_t QUERY_KERNEL_SQL_UNION_TABLE_NUM = 8; // 这个值需要等于 QUERY_KERNEL_SQL 联合的表数
    for (uint8_t i = 0; i < QUERY_KERNEL_SQL_UNION_TABLE_NUM; ++i) {
        stmt->BindParams(params.name, timestamp);
    }
    resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query kernel depth and thread.", stmt->GetErrorMessage());
        return false;
    }
    if (resultSet->Next()) {
        responseBody.id = resultSet->GetString("id");
        responseBody.threadId = resultSet->GetString("tid");
        responseBody.pid = resultSet->GetString("pid");
        responseBody.rankId = params.rankId;
        std::string metaType = resultSet->GetString("metaType");
        SliceQuery sliceQuery = CreateSliceQueryWithTimeRange({responseBody.rankId, responseBody.pid,
            responseBody.threadId, metaType, params.timestamp, params.duration});
        responseBody.depth = GetSliceDepthForJump(sliceQuery, NumberUtil::StringToUnsignedLongLong(responseBody.id));
    }
    return true;
}

LayerStatData DbTraceDataBase::QueryLayerData(const Protocol::SystemViewParams &requestParams, const std::string &name,
    const uint64_t &minTimestamp, const std::string &timeRangeConditionSql) {
    return LayerStatData();
}

std::vector<std::string> DbTraceDataBase::QueryCoreType() {
    std::vector<std::string> acceleratorCoreList;
    std::string sql = "SELECT DISTINCT TASKTYPE.value as accelerator_core FROM " + TABLE_COMPUTE_TASK_INFO +
        " JOIN STRING_IDS AS TASKTYPE ON TASKTYPE.id = COMPUTE_TASK_INFO.taskType ORDER BY accelerator_core";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql to query core type.");
        return acceleratorCoreList;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query core type.", stmt->GetErrorMessage());
        return acceleratorCoreList;
    }
    while (resultSet->Next()) {
        std::string res = resultSet->GetString("accelerator_core");
        acceleratorCoreList.emplace_back(res);
    }
    return acceleratorCoreList;
}

OneKernelData DbTraceDataBase::QueryKernelTid(uint64_t trackId) { return OneKernelData(); }

bool DbTraceDataBase::QueryThreadTracesSummary(const Protocol::UnitThreadTracesSummaryParams &requestParams,
    Protocol::UnitThreadTracesSummaryBody &responseBody, uint64_t minTimestamp) {
    auto stmt = CreatPreparedStatement();
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql to query thread traces summary. ", sqlite3_errmsg(db));
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    try {
        resultSet = TraceDatabaseHelper::QueryThreadTracesSummary(
            GetDeviceId(requestParams.cardId), minTimestamp, stmt, requestParams);
    } catch (DatabaseException &e) {
        ServerLog::Error("Query thread traces summary failed, ", e.What());
        return false;
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set to query thread traces summary.", stmt->GetErrorMessage());
        return false;
    }
    const uint64_t maxDataCount = 30000;
    uint64_t unitTime = (requestParams.endTime - requestParams.startTime) / maxDataCount;
    unitTime = unitTime == 0 ? 1 : unitTime;
    TraceDatabaseHelper::ComputeSummarySlice(resultSet, unitTime, responseBody);
    return true;
}
void DbTraceDataBase::UpdateStartTime(const std::string &fileId) {
    sqlite3_stmt *stmt = nullptr;
    std::string sql;
    if (CheckTableExist(TABLE_API) && !CheckTableExist(TABLE_SESSION_TIME_INFO)) {
        sql = "SELECT min(startNs) as startTimeNs, max(endNs) as endTimeNs FROM " + TABLE_API;
    } else {
        sql = "SELECT startTimeNs, endTimeNs FROM " + TABLE_SESSION_TIME_INFO;
    }
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Failed to Update Start Time. Msg: ", sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(stmt);
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        int64_t startTime = sqlite3_column_int64(stmt, col++);
        int64_t endTime = sqlite3_column_int64(stmt, col++);
        if (startTime < 0 || endTime < 0) {
            continue;
        }
        TraceTime::Instance().UpdateTime(startTime, endTime);
        TraceTime::Instance().UpdateCardMinTimestamp(fileId, startTime);
        TraceTime::Instance().UpdateCardTimeDuration(fileId, startTime, endTime);
        TraceTime::Instance().UpdateCardTimeDuration(QueryHostInfo() + "Host", startTime, endTime);
    }
    Server::ServerLog::Info("Update start and end time. ");
    sqlite3_finalize(stmt);
}

std::vector<OVERLAP_INFO> DbTraceDataBase::BuildOverlapInfoList(
    const std::vector<OVERLAP_INFO> &timeInfoList, const std::string &deviceId) {
    std::vector<OVERLAP_INFO> overlapInfoList;

    // 客户要求Free的跨度从TASK表的最早开始时间到TASK表的最晚结束时间，这样TASK表开始和结束时非计算或通信的算子也显示为Free时间
    std::pair<int64_t, int64_t> taskEarliestTimeAndLatestTime;
    bool isQuerySuccess = QueryTaskEarliestAndLatestTimeExcludingCertainEvent(taskEarliestTimeAndLatestTime, deviceId);
    if (isQuerySuccess && taskEarliestTimeAndLatestTime.first < timeInfoList.begin()->startNs) {
        overlapInfoList.emplace_back(taskEarliestTimeAndLatestTime.first, timeInfoList.begin()->startNs, 3); // Free = 3
    }
    // 记录当前最大截结束时间对应的覆盖区块
    // 此处为了添加第一块数据
    OVERLAP_INFO curBlock = OVERLAP_INFO(timeInfoList.begin()->startNs, timeInfoList.begin()->startNs, -1);
    for (const auto &timeInfo : timeInfoList) {
        if (curBlock.type == 1) { // Communication = 1
            overlapInfoList.emplace_back(curBlock.startNs, // Communication(Not Overlapped) = 2
                timeInfo.startNs > curBlock.endNs ? curBlock.endNs : timeInfo.startNs, 2);
        }
        if (timeInfo.startNs > curBlock.endNs) {
            overlapInfoList.emplace_back(curBlock.endNs, timeInfo.startNs, 3); // Free = 3
            curBlock.endNs = timeInfo.endNs;
            curBlock.type = timeInfo.type;
            curBlock.startNs = timeInfo.startNs;
        } else {
            curBlock.type = timeInfo.endNs > curBlock.endNs ? timeInfo.type : curBlock.type;
            curBlock.startNs = timeInfo.endNs > curBlock.endNs ? curBlock.endNs : timeInfo.endNs;
            curBlock.endNs = timeInfo.endNs > curBlock.endNs ? timeInfo.endNs : curBlock.endNs;
        }
    }
    // 此处为了添加最后一块数据
    if (curBlock.type == 1) { // Communication = 1
        overlapInfoList.emplace_back(curBlock.startNs, // Communication(Not Overlapped) = 2
            curBlock.endNs, 2);
    }

    if (isQuerySuccess && curBlock.endNs < taskEarliestTimeAndLatestTime.second) {
        overlapInfoList.emplace_back(curBlock.endNs, taskEarliestTimeAndLatestTime.second, 3); // Free = 3
    }
    return overlapInfoList;
}

bool DbTraceDataBase::QueryTaskEarliestAndLatestTimeExcludingCertainEvent(
    std::pair<int64_t, int64_t> &time, const std::string &deviceId) {
    // 查询TASK表的最早开始时间和最晚结束时间会排除特定类型事件，text和db保持一致，text由profiling保证
    sqlite3_stmt *stmt = nullptr;
    std::string sql = "SELECT min(main.startNs), max(main.endNs) FROM " + TABLE_TASK + " AS main INNER JOIN " +
        TABLE_STRING_IDS +
        " AS si ON main.taskType = si.id "
        "WHERE si.value NOT IN ('PROFILING_ENABLE', 'PROFILING_DISABLE', "
        "'FFTS_PLUS', 'KERNEL_AICORE', 'KERNEL_AIVEC', 'KERNEL_MIX_AIC', 'KERNEL_MIX_AIV') AND main.deviceId = ?";
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Failed to query task earliest and latest time excluding certain event. Msg: ",
            sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(stmt);
        return false;
    }
    int index = bindStartIndex;
    sqlite3_bind_text(stmt, index, deviceId.c_str(), deviceId.length(), SQLITE_TRANSIENT);
    int64_t earliestTime = 0;
    int64_t latestTime = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        earliestTime = sqlite3_column_int64(stmt, col++);
        latestTime = sqlite3_column_int64(stmt, col++);
        if (earliestTime < 0 || latestTime < 0) {
            Server::ServerLog::Error(
                "Failed to query task earliest and latest time excluding certain event due to invalid time.");
            sqlite3_finalize(stmt);
            return false;
        }
    }
    time.first = earliestTime;
    time.second = latestTime;
    sqlite3_finalize(stmt);
    return true;
}

bool DbTraceDataBase::GenerateOverlapAnalysis() {
    {
        std::unique_lock<std::recursive_mutex> lockGuard(mutex);
        ExecSql("delete from OVERLAP_ANALYSIS where 1 = 1");
    }
    QueryRankId();
    // 考虑到PyTorch多Device场景，这里需要遍历所有deviceId
    // NPU_INFO表除了纯Host数据没有，其它场景保证有，它的id列保存了所有的deviceId
    std::vector<uint64_t> deviceIdList = TraceDatabaseHelper::GetDeviceIdList(path);
    for (const auto deviceIdNum : deviceIdList) {
        std::string deviceId = std::to_string(deviceIdNum);
        std::vector<OVERLAP_INFO> timeInfoList; // 包含computing,Communication 覆盖数据
        QueryTaskTimeInfo(true, timeInfoList, deviceId);
        QueryTaskTimeInfo(false, timeInfoList, deviceId);
        if (timeInfoList.empty()) {
            continue;
        }
        std::sort(timeInfoList.begin(), timeInfoList.end(), std::less<OVERLAP_INFO>());
        const std::vector<OVERLAP_INFO> overlapInfoList = BuildOverlapInfoList(timeInfoList, deviceId);
        if (!InsertOverlapAnalysisInfo(timeInfoList, deviceId) ||
            !InsertOverlapAnalysisInfo(overlapInfoList, deviceId)) {
            Server::ServerLog::Error("Failed to generate overlap analysis.");
            return false;
        }
        Server::ServerLog::Info("Successfully generated overlap analysis for device: ", deviceId);
    }
    return true;
}

bool DbTraceDataBase::InsertOverlapAnalysisInfo(
    const std::vector<OVERLAP_INFO> &overlapInfoList, const std::string &rankId) {
    std::lock_guard<std::recursive_mutex> lockGuard(mutex);
    if (!insertOverlapStmt) {
        ServerLog::Error("Failed to InsertOverlap due to invalid pointer.");
        return false;
    }
    size_t size = overlapInfoList.size();
    size_t count = size / cacheSize;
    bool result = true;
    for (size_t index = 0; index <= count; ++index) {
        size_t start = index * cacheSize;
        size_t length = cacheSize;
        if (size - start < cacheSize) {
            length = size - start;
        }
        if (!StartTransaction()) {
            ServerLog::Error("Failed to start Transaction.");
            return false;
        }
        for (size_t tmpIndex = start; tmpIndex < start + length; tmpIndex++) {
            insertOverlapStmt->Reset();
            insertOverlapStmt->BindParams(rankId, overlapInfoList[tmpIndex].startNs, overlapInfoList[tmpIndex].endNs,
                overlapInfoList[tmpIndex].type);
            if (!insertOverlapStmt->Execute()) {
                ServerLog::Error("Failed to InsertOverlap");
                result = false;
                break;
            }
        }
        if (!EndTransaction()) {
            ServerLog::Error("Failed to end Transaction.");
            return false;
        }
    }
    return result;
}

void DbTraceDataBase::QueryTaskTimeInfo(
    bool isComputing, std::vector<OVERLAP_INFO> &timeInfoList, const std::string &deviceId) {
    std::string sql;
    bool isUniqueDevice = TraceDatabaseHelper::IsDeviceIdUnique(path);
    if (isComputing) {
        sql = "select startNs, endNs from TASK main join COMPUTE_TASK_INFO info "
              " on info.globalTaskId = main.globalTaskId where deviceId=? and startNs != endNs order by startNs, endNs";
    } else {
        sql = "select op.startNs, op.endNs from COMMUNICATION_OP op ";
        if (!isUniqueDevice) {
            sql += " join TASK task on task.connectionId = op.connectionId where task.deviceId=? ";
        }
        sql += " group by opId  order by op.startNs, op.endNs";
    }
    auto stmt = CreatPreparedStatement();
    try {
        auto resultSet = (!isComputing && isUniqueDevice) ? TraceDatabaseHelper::ExecuteQuery(stmt, sql)
                                                          : TraceDatabaseHelper::ExecuteQuery(stmt, sql, deviceId);
        OVERLAP_INFO curInfo{};
        bool hasCurInfo = false;
        while (resultSet->Next()) { // Computing = 0, Communication = 1
            auto info = OVERLAP_INFO(resultSet->GetInt64("startNs"), resultSet->GetInt64("endNs"), isComputing ? 0 : 1);
            if (!hasCurInfo) {
                curInfo = info;
                hasCurInfo = true;
            } else if (info.startNs <= curInfo.endNs) {
                curInfo.endNs = std::max(info.endNs, curInfo.endNs);
            } else {
                timeInfoList.emplace_back(curInfo);
                curInfo = info;
            }
        }
        if (hasCurInfo) {
            timeInfoList.emplace_back(curInfo);
        }
    } catch (DatabaseException &e) {
        ServerLog::Error("Query task time info fail, ", e.What());
        return;
    }
}

void DbTraceDataBase::UpdateWaitTime() {
    auto stmt = CreatPreparedStatement(FULL_DB_UPDATE_TIME); // 查询数据
    auto updateComputeStmt = CreatPreparedStatement("UPDATE COMPUTE_TASK_INFO SET waitNs = ? WHERE ROWID = ?;");
    auto updateCommunicationStmt = CreatPreparedStatement("UPDATE COMMUNICATION_OP SET waitNs = ? WHERE ROWID = ?;");
    if (stmt == nullptr || updateComputeStmt == nullptr || updateCommunicationStmt == nullptr) {
        ServerLog::Error("Update wait time, fail to prepare sql.");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Update wait time. failed to get result set.", stmt->GetErrorMessage());
        return;
    }
    std::map<int64_t, int64_t> prevTime;
    std::lock_guard<std::recursive_mutex> lockGuard(mutex);
    while (resultSet->Next()) {
        std::string type = resultSet->GetString("type");
        int64_t startNs = resultSet->GetInt64("startNs");
        int64_t endNs = resultSet->GetInt64("endNs");
        int64_t deviceId = resultSet->GetInt32("deviceId");
        if (prevTime.find(deviceId) == prevTime.end()) {
            prevTime[deviceId] = startNs;
        }
        int64_t waitNs = startNs > prevTime[deviceId] ? startNs - prevTime[deviceId] : 0;
        prevTime[deviceId] = endNs;
        WAIT_TIME task;
        task.id = resultSet->GetInt64("id");
        task.waitTime = waitNs;
        task.type = type;
        taskWaitTimeCache.push_back(task);
        if (taskWaitTimeCache.size() == cacheSize &&
            !UpdateTaskInfoWaitTime(updateComputeStmt, updateCommunicationStmt)) {
            ServerLog::Error("Update wait time. Failed to update data.");
            return;
        }
    }
    if (!UpdateTaskInfoWaitTime(updateComputeStmt, updateCommunicationStmt)) {
        ServerLog::Error("Update wait time. Failed to update last data.");
        return;
    }
}

bool DbTraceDataBase::UpdateTaskInfoWaitTime(std::unique_ptr<SqlitePreparedStatement> &updateComputeStmt,
    std::unique_ptr<SqlitePreparedStatement> &updateCommunicationStmt) {
    std::lock_guard<std::recursive_mutex> lockGuard(mutex);
    if (!StartTransaction()) {
        ServerLog::Error("Failed to start Transaction.");
        return false;
    }
    auto result = true;
    for (const auto &item : taskWaitTimeCache) {
        std::unique_ptr<SqlitePreparedStatement> &refStmt =
            item.type == "compute" ? updateComputeStmt : updateCommunicationStmt;
        refStmt->Reset();
        refStmt->BindParams(item.waitTime, item.id);
        if (!refStmt->Execute()) {
            ServerLog::Error("Failed to update task info wait time");
            result = false;
            break;
        }
    }
    taskWaitTimeCache.clear();
    if (!EndTransaction()) {
        ServerLog::Error("Failed to end update task info wait time.");
        return false;
    }
    return result;
}

std::unordered_map<std::string, std::string> DbTraceDataBase::QueryRankIdAndDeviceMap() {
    std::unordered_map<std::string, std::string> rankAndDeviceMap;
    sqlite3_stmt *stmt = nullptr;
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (type == FileType::MS_PROF || !CheckTableDataInvalid(TABLE_PYTORCH_INFO)) {
        return rankAndDeviceMap;
    } else if (type == FileType::PYTORCH) {
        sql = "SELECT DISTINCT deviceId, rankId FROM " + TABLE_PYTORCH_INFO;
    }
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Failed to query rank id device map. Msg: ", sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(stmt);
        return rankAndDeviceMap;
    }

    // PyTorch多Device场景，map实际保存deviceId到自身的映射，其它场景保存rankId到deviceId的映射
    // NPU_INFO表除了纯Host数据没有，其它场景保证有，它的id列保存了所有的deviceId
    bool isMultiDevice = TraceDatabaseHelper::GetDeviceIdList(path).size() > 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string deviceId = sqlite3_column_string(stmt, resultStartIndex);
        std::string rankId = sqlite3_column_string(stmt, resultStartIndex + 1);
        // rankId存在多个映射值时取较大值，为-1时丢弃
        if (deviceId == "-1") {
            continue;
        }
        if (isMultiDevice) {
            rankId = deviceId;
        }
        rankAndDeviceMap[rankId] = StringUtil::StrNumMax(deviceId, rankAndDeviceMap[rankId]);
    }
    sqlite3_finalize(stmt);
    return rankAndDeviceMap;
}

std::string DbTraceDataBase::GetDeviceId(const std::string &rankIdWithHost) {
    auto hostStr = QueryHostInfo();
    auto rankAndDeviceMap = QueryRankIdAndDeviceMap();
    std::string realRankId = rankIdWithHost;
    if (!hostStr.empty() && StringUtil::StartWith(rankIdWithHost, hostStr)) {
        realRankId = rankIdWithHost.substr(hostStr.length());
    }
    if (rankAndDeviceMap.count(realRankId) > 0) {
        return rankAndDeviceMap[realRankId];
    }

    return realRankId;
}

std::string DbTraceDataBase::QueryHostInfo() { return DbTraceDataBase::QueryHostInfoWithHostPath(hostPath); }

std::string DbTraceDataBase::QueryHostInfoWithHostPath(const std::string &path) {
    if (!host.empty() || !CheckTableDataInvalid(TABLE_HOST_INFO)) {
        return host;
    }
    std::string sql = "select hostName||hostUid||'' as host from " + TABLE_HOST_INFO;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return host;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        host = sqlite3_column_string(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);

    sqlite3_stmt *timeStmt = nullptr;
    std::string timeSql = "SELECT startTimeNs, endTimeNs FROM " + TABLE_SESSION_TIME_INFO;
    int timeResult = sqlite3_prepare_v2(db, timeSql.c_str(), -1, &timeStmt, nullptr);
    if (timeResult != SQLITE_OK) {
        Server::ServerLog::Error(" Msg: ", sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(timeStmt);
        host = host + " ";
        return host;
    }
    int64_t startTime = 0;
    int64_t endTime = 0;
    while (sqlite3_step(timeStmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        startTime = sqlite3_column_int64(timeStmt, col++);
        endTime = sqlite3_column_int64(timeStmt, col++);
    }
    sqlite3_finalize(timeStmt);
    host = CollectionTimeService::Instance().ComputeMarkHost(host, path, startTime, endTime);
    return host;
}

std::vector<std::string> DbTraceDataBase::QueryRankId() {
    if (!rankIds.empty()) {
        return rankIds;
    }
    sqlite3_stmt *stmt = nullptr;
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (type == FileType::MS_PROF) {
        sql = "SELECT id FROM " + TABLE_NPU_INFO;
    } else if (type == FileType::PYTORCH) {
        if (CheckTableDataInvalid(TABLE_PYTORCH_INFO)) {
            sql = "SELECT DISTINCT rankId FROM " + TABLE_PYTORCH_INFO;
        } else {
            sql = "SELECT DISTINCT id FROM " + TABLE_NPU_INFO;
        }
    }
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Failed to get Statistic Num. Msg: ", sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(stmt);
        rankIds.emplace_back(path);
        return rankIds;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = sqlite3_column_string(stmt, resultStartIndex);
        rankIds.emplace_back(id);
    }
    sqlite3_finalize(stmt);
    if (rankIds.empty()) {
        rankIds.emplace_back(path);
    }
    return rankIds;
}

bool DbTraceDataBase::CheckTableDataInvalid(std::string tableName) {
    if (!CheckTableExist(tableName)) {
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    std::string sql = "  SELECT COUNT(*) FROM " + tableName;
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        Server::ServerLog::Error("Failed to get Memory Data. Msg: ", sqlite3_errmsg(db), " ", result);
        sqlite3_finalize(stmt);
        return false;
    }
    int64_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, resultStartIndex);
    }
    sqlite3_finalize(stmt);
    return count != 0;
}

bool DbTraceDataBase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    this->hostPath = DbTraceDataBase::GetHostPath(dbPath);
    return Database::OpenDb(dbPath, clearAllTable) && QueryMetaVersion() && SetConfig();
}

std::string DbTraceDataBase::GetHostPath(const std::string &filePath) {
    if (std::empty(filePath)) {
        return "";
    }
    std::string leavePath = filePath;
    std::vector<std::string> pathList = FileUtil::SplitFilePath(leavePath);

    std::string result;
    // 查找第一个出现的以 "PROF_" 开头或以 "_ascend_pt" 或 "_ascend_ms" 结尾的部分
    for (const auto &fileStr : pathList) {
        if (StringUtil::StartWith(fileStr, "PROF_") || StringUtil::EndWith(fileStr, "_ascend_pt") ||
            StringUtil::EndWith(fileStr, "_ascend_ms")) {
            return result;
        }
        result += fileStr + FILE_SEPARATOR;
    }
    // 不存在以 "PROF_" 开头或以 "_ascend_pt" 或 "_ascend_ms" 结尾的部分，则认为没找到 hostPath，返回空字符串
    return "";
}

bool DbTraceDataBase::InitConnectionCats() { return ExecSql(DbSqlDefs::GetConnectionCatSql()); }

std::string DbTraceDataBase::GetStringCacheValue(const std::string &path, const std::string &key) {
    if (stringsCache.count(path) == 0 || stringsCache.at(path).count(key) == 0) {
        return key;
    }
    return stringsCache.at(path)[key];
}

void DbTraceDataBase::ClearStringsCache() { stringsCache = {}; }

void DbTraceDataBase::InitStringsCache() {
    if (!stringsCache[path].empty()) {
        return;
    }
    auto sql = "select id, value from STRING_IDS";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Init strings cache. Failed to prepare sql.", sqlite3_errmsg(db));
        return;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Error("Init strings cache. Failed to get result set.", stmt->GetErrorMessage());
        return;
    }
    while (result->Next()) {
        stringsCache[path].emplace(result->GetString("id"), result->GetString("value"));
    }
}

void DbTraceDataBase::InitMetaDataInfo() {
    if (CheckTableExist(TABLE_META_DATA)) {
        std::string groupInfoStr = QueryValueFromMetaDataByName("parallel_group_info");
        auto groupInfoList = MetaDataParser::ParserParallelGroupInfoByText(groupInfoStr);
        MetaDataCacheManager::Instance().AddParallelGroupInfo(groupInfoList);
    }
}

void DbTraceDataBase::CreateTemporaryTable() {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // 初始化所有全量查询功能需要的表，空表不影响展示，方便sql扩展
    for (const auto &item : FULL_DB_TABLE_MAP) {
        if (!CheckTableExist(item.first)) {
            ExecSql(item.second);
        }
    }
}

void DbTraceDataBase::AddHelperColumnsAndSetStatus() {
    auto isVersionChange = IsDatabaseVersionChange();
    if (!isVersionChange) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (isExistTask) {
        if (!CheckColumnExist(TABLE_TASK, std::string(PytorchApiColumn::DEPTH))) {
            ExecSql("alter table " + TABLE_TASK + " add depth integer;");
        } else {
            ExecSql("update " + TABLE_TASK + " set depth = NULL;");
        }
    }
    // 只要TASK表或者COMMUNICATION_OP表存在，展示覆盖分析，TASK表反映计算信息，COMMUNICATION_OP表反映通信信息
    if (isExistTask || isExistCommOp) {
        ExecSql(" create table if not exists OVERLAP_ANALYSIS (id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " deviceId integer, startNs integer, endNs integer, type integer);");
    }
    if (isExistMstx) {
        if (!CheckColumnExist(TABLE_MSTX_EVENTS, std::string(PytorchApiColumn::DEPTH))) {
            ExecSql("alter table " + TABLE_MSTX_EVENTS + " add depth integer;");
        } else {
            ExecSql("update " + TABLE_MSTX_EVENTS + " set depth = null");
        }
    }
    if (isExistPytorch) {
        if (!CheckColumnExist(TABLE_API, std::string(PytorchApiColumn::DEPTH))) {
            ExecSql("alter table " + TABLE_API + " add depth integer;");
        } else {
            ExecSql("update " + TABLE_API + " set depth = NULL");
        }
    }
    AddColumns2Table(isExistPytorch, TABLE_API, std::string(PytorchApiColumn::DEPTH), "integer");
    AddColumns2Table(isExistCann, TABLE_CANN_API, std::string(PytorchApiColumn::DEPTH), "integer");
    AddColumns2Table(isExistComputeTask, TABLE_COMPUTE_TASK_INFO, "waitNs", "INTEGER");
    AddColumns2Table(isExistCommOp, TABLE_COMMUNICATION_OP, "waitNs", "integer");
    AddColumns2Table(isExistCommOp, TABLE_COMMUNICATION_OP, "opConnectionId", "TEXT");
    for (const auto &status : DB_STATUS_LIST) {
        UpdateValueIntoStatusInfoTable(status, NOT_FINISH_STATUS);
    }
}

bool DbTraceDataBase::InitStmt() {
    if (initStmt) {
        return true;
    }
    std::string sql;
    if (CheckTableExist(TABLE_TASK)) {
        sql = "UPDATE " + TABLE_TASK + " set depth = ? where ROWID = ?";
        updateTaskDepthStmt = CreatPreparedStatement(sql);
        if (updateTaskDepthStmt == nullptr) {
            ServerLog::Error("Failed to prepare update task depth statement.");
            return false;
        }
    }
    // 只要TASK表或者COMMUNICATION_OP表存在，展示覆盖分析，TASK表反映计算信息，COMMUNICATION_OP表反映通信信息
    if (CheckTableExist(TABLE_TASK) || CheckTableExist(TABLE_COMMUNICATION_OP)) {
        sql = "INSERT INTO " + TABLE_OVERLAP_ANALYSIS + " (deviceId, startNs, endNs, type) VALUES (?,?,?,?)";
        insertOverlapStmt = CreatPreparedStatement(sql);
        if (insertOverlapStmt == nullptr) {
            ServerLog::Error("Failed to prepare insert overlap statement.");
            return false;
        }
    }
    if (CheckTableExist(TABLE_API)) {
        sql = "UPDATE " + TABLE_API + " set depth = ? where ROWID = ?";
        updateApiDepthStmt = CreatPreparedStatement(sql);
        if (updateApiDepthStmt == nullptr) {
            ServerLog::Error("Failed to prepare update api depth statement.");
            return false;
        }
    }
    if (CheckTableExist(TABLE_CANN_API)) {
        sql = "UPDATE " + TABLE_CANN_API + " set depth = ? where ROWID = ?";
        updateCannApiDepthStmt = CreatPreparedStatement(sql);
        if (updateCannApiDepthStmt == nullptr) {
            ServerLog::Error("Failed to prepare update cann api depth statement.");
            return false;
        }
    }
    initStmt = true;
    return true;
}

bool DbTraceDataBase::SetConfig() {
    if (!Database::SetConfig()) {
        return false;
    }
    isExistPytorch = CheckTableExist(TABLE_API);
    isExistCann = CheckTableExist(TABLE_CANN_API);
    isExistMstx = CheckTableExist(TABLE_MSTX_EVENTS);
    isExistCommOp = CheckTableExist(TABLE_COMMUNICATION_OP);
    isExistTask = CheckTableExist(TABLE_TASK);
    isExistComputeTask = CheckTableExist(TABLE_COMPUTE_TASK_INFO);
    // 临时表只在该数据库连接的生命周期内有效，连接断开后自动销毁
    CreateTemporaryTable();
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return ExecSql("PRAGMA case_sensitive_like=1;");
}

void DbTraceDataBase::AddColumns2Table(
    const bool isExist, const std::string &tableName, const std::string &columnName, const std::string &columnType) {
    if (isExist && !CheckColumnExist(tableName, columnName)) {
        ExecSql("alter table " + tableName + " add column " + columnName + " " + columnType + ";");
    }
}

bool DbTraceDataBase::QueryHostMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    PROCESS_TYPE types[] = {PROCESS_TYPE::CANN_API, PROCESS_TYPE::API, PROCESS_TYPE::OSRT_API, PROCESS_TYPE::MS_TX,
        PROCESS_TYPE::PYTHON_GC};
    std::map<std::string, std::vector<MetaDataDto>> threadMap;
    for (const auto &type : types) {
        auto typeName = ENUM_TO_STR(type).value_or("");
        std::string sql;
        switch (type) {
        case PROCESS_TYPE::CANN_API:
            sql = QUERY_HOST_METADATA_CANN_SQL;
            break;
        case PROCESS_TYPE::API:
            sql = QUERY_HOST_METADATA_PYTORCH_SQL;
            break;
        case PROCESS_TYPE::OSRT_API:
            sql = QUERY_HOST_METADATA_OSRT_SQL;
            break;
        case PROCESS_TYPE::MS_TX:
            sql = QUERY_HOST_METADATA_MSTX_SQL;
            break;
        case PROCESS_TYPE::PYTHON_GC:
            sql = QUERY_HOST_METADATA_PYTHONGC_SQL;
            break;
        default:
            return false;
        }
        auto stmt = CreatPreparedStatement();
        try {
            auto resultSet = TraceDatabaseHelper::ExecuteQuery(stmt, sql);
            while (resultSet->Next()) {
                MetaDataDto metadata;
                metadata.pid = resultSet->GetString("globalTid");
                metadata.metaType = typeName;
                metadata.threadId = resultSet->GetString("type");
                metadata.threadName = resultSet->GetString("name");
                metadata.maxDepth = resultSet->GetInt32("maxDepth") + 1;
                threadMap[metadata.pid].emplace_back(metadata);
            }
        } catch (DatabaseException &e) {
            ServerLog::Error("Failed to query host metadata, MetaType: ", typeName, " reason: ", e.What());
        }
    }
    DealHostMetadata(fileId, metaData, threadMap);
    AddPythonStackMetadata(fileId, metaData);
    return true;
}

void DbTraceDataBase::AddPythonStackMetadata(const std::string &fileId,
    std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData)
{
    if (!isExistPytorch && !CheckTableExist(TABLE_API)) {
        return;
    }
    std::string sql = "SELECT DISTINCT globalTid FROM PYTORCH_API WHERE type = 50003";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        return;
    }
    std::set<std::string> globalTidsWithPythonFunc;
    while (resultSet->Next()) {
        globalTidsWithPythonFunc.insert(resultSet->GetString("globalTid"));
    }
    if (globalTidsWithPythonFunc.empty()) {
        return;
    }
    auto pythonStackMetaType = ENUM_TO_STR(PROCESS_TYPE::PYTHON_STACK).value_or("");
    for (auto &process : metaData) {
        for (auto &child : process->children) {
            if (globalTidsWithPythonFunc.count(child->metaData.processId)) {
                auto pythonStack = GenerateBaseUnitTrack("thread", fileId, child->metaData.processId,
                    "", pythonStackMetaType);
                pythonStack->metaData.threadId = Protocol::PYTHON_STACK_THREAD_ID_PREFIX + child->metaData.processId;
                pythonStack->metaData.threadName = "Python Stack " + child->metaData.threadId;
                pythonStack->metaData.maxDepth = 1;
                child->children.emplace_back(std::move(pythonStack));
            }
        }
    }
}

void DbTraceDataBase::DealHostMetadata(const std::string &fileId,
    std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData,
    std::map<std::string, std::vector<MetaDataDto>> &threadMap) {
    uint64_t curPid = 0;
    std::unique_ptr<UnitTrack> process;
    for (auto &thread : threadMap) {
        uint64_t globalTid = NumberUtil::StringToUnsignedLongLong(thread.first);
        auto pid = globalTid >> 32;
        auto tid = globalTid & 0XFFFFFFFF;
        if (curPid != pid) {
            if (process.operator bool()) {
                metaData.emplace_back(std::move(process));
            }
            process = GenerateBaseUnitTrack("process", fileId, thread.first, "Process " + std::to_string(pid),
                ENUM_TO_STR(PROCESS_TYPE::PROCESS).value());
            curPid = pid;
        }
        auto threadUnit = GenerateBaseUnitTrack("process", fileId, thread.first, "Thread " + std::to_string(tid),
            ENUM_TO_STR(PROCESS_TYPE::CANN_API).value());
        threadUnit->metaData.threadId = std::to_string(tid);
        auto cannApiUnit =
            GenerateBaseUnitTrack("label", fileId, thread.first, "CANN", ENUM_TO_STR(PROCESS_TYPE::CANN_API).value());
        auto mstxUnit =
            GenerateBaseUnitTrack("process", fileId, thread.first, "MSTX", ENUM_TO_STR(PROCESS_TYPE::MS_TX).value());
        mstxUnit->metaData.threadId = std::to_string(tid);
        for (const auto &item : thread.second) {
            auto level = GenerateBaseUnitTrack("thread", fileId, thread.first, "", item.metaType);
            level->metaData.threadId = item.threadId;
            level->metaData.threadName = item.threadName;
            level->metaData.maxDepth = item.maxDepth;
            if (item.metaType == ENUM_TO_STR(PROCESS_TYPE::MS_TX)) {
                mstxUnit->children.emplace_back(std::move(level));
            } else if (std::find(CANN_APIS.begin(), CANN_APIS.end(), item.threadName) != CANN_APIS.end()) {
                cannApiUnit->children.emplace_back(std::move(level));
            } else {
                threadUnit->children.emplace_back(std::move(level));
            }
        }
        if (!cannApiUnit->children.empty()) {
            threadUnit->children.emplace_back(std::move(cannApiUnit));
        }
        if (!mstxUnit->children.empty()) {
            threadUnit->children.emplace_back(std::move(mstxUnit));
        }
        if (process.operator bool()) {
            process->children.emplace_back(std::move(threadUnit));
        }
    }
    if (process.operator bool()) {
        metaData.emplace_back(std::move(process));
    }
}

std::unique_ptr<Protocol::UnitTrack> DbTraceDataBase::GenerateBaseUnitTrack(const std::string &type,
    const std::string &cardId, const std::string &processId, const std::string &processName,
    const std::string &metaType) {
    std::unique_ptr<Protocol::UnitTrack> unitTrack = std::make_unique<Protocol::UnitTrack>();
    unitTrack->type = type;
    unitTrack->metaData.cardId = cardId;
    unitTrack->metaData.processId = processId;
    unitTrack->metaData.processName = processName;
    unitTrack->metaData.metaType = metaType;
    return unitTrack;
}

std::string DbTraceDataBase::GetHcclOperatorMetaData(const std::string &fileId) {
    // 这里COMMUNICATION_TASK_INFO和COMMUNICATION_OP表关联后，按照planeId和groupName分组，
    // 理论上每一组都会有很多globalTaskId，但是我们只需要任意一个globalTaskId即可，就能够根据这个信息去Task表关联判断deviceId的信息
    // 这里仍然使用关联TASK表去判断deviceId是为了兼容老数据，后续可以完全使用COMMUNICATION_OP表进行判断
    // 届时可以去掉这里globalTaskId的逻辑
    std::string sql = "with main as (SELECT groupTemp.planeId, groupTemp.groupName ,sids.value AS groupNameValue FROM "
                      " (SELECT info.planeId, op.groupName, info.globalTaskId FROM COMMUNICATION_TASK_INFO info "
                      " JOIN COMMUNICATION_OP op ON op.opId = info.opId GROUP BY info.planeId,op.groupName ) groupTemp "
                      " LEFT JOIN TASK task ON task.globalTaskId = groupTemp.globalTaskId "
                      " LEFT JOIN STRING_IDS sids ON groupTemp.groupName = sids.id "
                      " WHERE task.deviceId = ?) "
                      " select 'Plane ' || planeId as name, groupName || '_' || planeId as tid, 0 as maxDepth, "
                      " groupName, groupNameValue, planeId from main group by planeId, groupName union ";
    if (!TraceDatabaseHelper::IsDeviceIdUnique(fileId)) {
        // deviceId不唯一，走老逻辑
        sql += "select 'Group ' || ((row_number() over ()) -1) || ' Communication' as name, "
               " groupName || 'group' as tid, 0 as maxDepth, groupName, groupNameValue, -1 as planeId from main "
               " group by groupName order by groupName ASC, planeId ASC";
    } else {
        // deviceId唯一，走新逻辑
        sql += "SELECT 'Group ' || ( ( row_number() OVER () ) - 1 ) || ' Communication' AS name, "
               "groupName || 'group' AS tid, 0 AS maxDepth, groupName, sids.value as groupNameValue, - 1 AS planeId "
               "FROM " +
            TABLE_COMMUNICATION_OP +
            " op left join STRING_IDS sids on op.groupName = sids.id GROUP BY groupName "
            "order by groupName ASC, planeId ASC";
    }
    return sql;
}

bool DbTraceDataBase::QueryAscendHardwareOperatorMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    // 这个函数会将Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
    // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
    // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
    // 因为Device侧存在非MSTX事件，所以和MSTX_EVENTS连接必须左连接
    // 和STRING_IDS表必须用左连接，不能内连接，因为只有新数据的domainId才会在STRING_IDS中有对应，老数据没有对应，如果内连接会导致SQL查不出老数据
    // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
    PROCESS_TYPE type = PROCESS_TYPE::ASCEND_HARDWARE;
    std::string sql = "SELECT table1.streamId AS tid, table2.domainId AS did, "
                      "table3.value AS dname, MAX(table1.depth) AS maxDepth "
                      "FROM " +
        TABLE_TASK +
        " AS table1 "
        "LEFT JOIN " +
        TABLE_MSTX_EVENTS +
        " AS table2 "
        "ON table1.connectionId = table2.connectionId "
        "LEFT JOIN " +
        TABLE_STRING_IDS +
        " AS table3 "
        "ON table2.domainId = table3.id "
        "WHERE table1.deviceId = ? "
        "GROUP BY table1.streamId, table2.domainId, table3.value "
        "ORDER BY table1.streamId ASC, table2.domainId ASC NULLS LAST;";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query Ascend Hardware operator metadata.");
        return false;
    }
    std::string metaType = ENUM_TO_STR(type).value_or("");
    std::string processName = metaType;
    auto process = GenerateBaseUnitTrack("process", fileId, metaType, processName, metaType);
    auto resultSet = stmt->ExecuteQuery(GetDeviceId(fileId));
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query Ascend Hardware operator metadata.");
        return false;
    }

    while (resultSet->Next()) {
        std::string threadId = resultSet->GetString("tid");
        auto thread = GenerateBaseUnitTrack("thread", fileId, process->metaData.processId, "", metaType);
        std::string domainId = resultSet->GetString("did");
        if (domainId.empty()) {
            thread->metaData.threadId = threadId;
            thread->metaData.threadName = "Stream " + threadId;
        } else {
            std::string domainName = resultSet->GetString("dname");
            if (domainName.empty()) {
                thread->metaData.threadId = threadId + "_" + domainId;
                thread->metaData.threadName = "Stream " + threadId + " MSTX";
            } else {
                thread->metaData.threadId = threadId + "_" + domainId;
                thread->metaData.threadName = "Stream " + threadId + " MSTX domain " + domainName;
            }
        }
        thread->metaData.maxDepth = resultSet->GetInt32("maxDepth") + 1;
        process->children.emplace_back(std::move(thread));
    }

    if (!process->children.empty()) {
        metaData.emplace_back(std::move(process));
    }
    return true;
}

bool DbTraceDataBase::QueryHCCLOperatorMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    PROCESS_TYPE type = PROCESS_TYPE::HCCL;
    std::string sql = GetHcclOperatorMetaData(fileId);
    auto stmt = CreatPreparedStatement();
    auto metaType = ENUM_TO_STR(type).value_or("");
    // 临时增加对 HCCL 的特殊处理，之后将 PROCESS_TYPE::HCCL 换成 Communication
    auto processName = "Communication";
    auto process = GenerateBaseUnitTrack("process", fileId, metaType, processName, metaType);
    try {
        auto resultSet = TraceDatabaseHelper::ExecuteQuery(stmt, sql, GetDeviceId(fileId));
        while (resultSet->Next()) {
            auto thread = GenerateBaseUnitTrack("thread", fileId, process->metaData.processId, "", metaType);
            std::string threadId = resultSet->GetString("tid");
            ProcessThreadUnit(process, resultSet, thread, threadId, type);
        }
    } catch (DatabaseException &e) {
        ServerLog::Error("Failed to query HCCL operator metadata, reason: ", e.What());
        return false;
    }
    UpdataCommucationThreadName(type, process);
    if (!process->children.empty()) {
        metaData.emplace_back(std::move(process));
    }
    return true;
}

// 这是一个补丁，后续需要修改掉
void DbTraceDataBase::UpdataCommucationThreadName(
    const PROCESS_TYPE &type, std::unique_ptr<Protocol::UnitTrack> &process) const {
    const std::string suffix = "group";
    if (!std::empty(metaVersion) && !StringUtil::StartWith(metaVersion, "1.0") && type == PROCESS_TYPE::HCCL) {
        for (auto &item : process->children) {
            if (StringUtil::StartWith(item->metaData.threadName, "Group") &&
                StringUtil::EndWith(item->metaData.threadName, "Communication")) {
                std::string threadId = item->metaData.threadId;
                // 此时 threadId 是 groupName + "group", 形如 "409group"
                std::string groupName = threadId.substr(0, threadId.size() - suffix.size());
                item->metaData.threadName = "Group " + stringsCache.at(path)[groupName] + " Communication";
            }
        }
    }
}

void DbTraceDataBase::ProcessThreadUnit(std::unique_ptr<Protocol::UnitTrack> &process,
    std::unique_ptr<SqliteResultSet> &resultSet, std::unique_ptr<Protocol::UnitTrack> &thread,
    const std::string &threadId, const PROCESS_TYPE &type) const {
    const static std::string WRONG_THREAD_ID = std::to_string(UINT32_MAX);
    // hccl的plane泳道约定一个异常数据不做展示
    if (threadId.find(WRONG_THREAD_ID) != std::string::npos) {
        return;
    }
    // 在 metaVersion 版本高于 '1.0' 的情况下，type == PROCESS_TYPE::HCCL 时
    if (!std::empty(metaVersion) && !StringUtil::StartWith(metaVersion, "1.0") && type == PROCESS_TYPE::HCCL) {
        const std::string groupNameValue = resultSet->GetString("groupNameValue");
        const std::string threadName = resultSet->GetString("name");
        if (!StringUtil::StartWith(threadName, "Plane") &&
            TraceDatabaseHelper::IsValidHCCLGroupNameValue(groupNameValue)) {
            thread->metaData.groupNameValue = groupNameValue;
        }
    }
    thread->metaData.threadId = threadId;
    thread->metaData.threadName = resultSet->GetString("name");
    thread->metaData.maxDepth = resultSet->GetInt32("maxDepth") + 1;
    process->children.emplace_back(std::move(thread));
}

bool DbTraceDataBase::QueryCounterMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    PROCESS_TYPE types[] = {PROCESS_TYPE::HBM, PROCESS_TYPE::LLC, PROCESS_TYPE::SAMPLE_PMU, PROCESS_TYPE::QOS,
        PROCESS_TYPE::NIC, PROCESS_TYPE::PCIE, PROCESS_TYPE::HCCS, PROCESS_TYPE::AI_CORE, PROCESS_TYPE::ACC_PMU,
        PROCESS_TYPE::DDR, PROCESS_TYPE::STARS_SOC, PROCESS_TYPE::NPU_MEM};
    for (const auto &type : types) {
        std::string processName;
        std::string metaType;
        std::string sql;
        if (!QueryCounterMetadataGenerateInfo(type, processName, metaType, sql)) {
            continue;
        }
        auto counter = GenerateBaseUnitTrack("label", fileId, processName, processName, metaType);
        auto stmt = CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Error("Query counter metadata failed!.");
            return false;
        }
        uint64_t countOfQuestionMark = 0;
        auto tempCount = std::count(sql.begin(), sql.end(), '?');
        if (tempCount > 0) {
            countOfQuestionMark = static_cast<uint64_t>(tempCount);
        }
        for (uint64_t i = 0; i < countOfQuestionMark; ++i) {
            stmt->BindParams(GetDeviceId(fileId));
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Error("Query counter metadata. Failed to get result set.", stmt->GetErrorMessage());
            return false;
        }
        while (resultSet->Next()) {
            auto thread = GenerateBaseUnitTrack("counter", fileId, processName, "", metaType);
            thread->metaData.threadId = resultSet->GetString("name");
            thread->metaData.threadName = thread->metaData.threadId;
            thread->metaData.dataType = StringUtil::Split(resultSet->GetString("types"), ",");
            counter->children.emplace_back(std::move(thread));
        }
        metaData.emplace_back(std::move(counter));
    }
    return true;
}

bool DbTraceDataBase::QueryCounterMetadataGenerateInfo(const Dic::Module::Timeline::PROCESS_TYPE &type,
    std::string &processName, std::string &metaType, std::string &sql) {
    std::string tableName;
    CounterEventHelper helper;
    helper.RegisterDeviceMap();

    metaType = ENUM_TO_STR(type).value_or("");
    processName = helper.GetDeviceProcessName(type);
    tableName = helper.GetDeviceTableName(type);
    if (metaType.empty() || processName.empty() || tableName.empty()) {
        ServerLog::Error("Counter event type % is not supported.", static_cast<int>(type));
        return false;
    }
    if (!CheckTableExist(tableName)) {
        ServerLog::Warn("Query counter metadata failed, table ", tableName, " Not Exist.");
        return false;
    }
    sql = helper.GenerateDeviceMetadataSQL(type);
    return true;
}

bool DbTraceDataBase::SearchAllSlicesDetails(
    const Protocol::SearchAllSliceParams &params, Protocol::SearchAllSlicesBody &body, uint64_t minTimestamp) {
    uint64_t count = 0;
    uint64_t offset = (params.current - 1) * params.pageSize;

    SearchSliceSqlParams sqlParams;
    sqlParams.isMatchExact = params.isMatchExact;
    sqlParams.isMatchCase = params.isMatchCase;
    sqlParams.order = params.order;
    sqlParams.orderByField = params.orderBy;
    sqlParams.rankId = params.rankId;
    sqlParams.nameFilter = params.nameFilter;

    std::string sql = GetSearchAllSlicesDetailsSql(sqlParams);

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }

    std::unique_ptr<SqliteResultSet> resultSet;
    if (!params.nameFilter.empty()) {
        resultSet = stmt->ExecuteQuery(params.searchContent, params.nameFilter, minTimestamp,
            GetDeviceId(params.rankId), params.pageSize, offset);
    } else {
        resultSet = stmt->ExecuteQuery(params.searchContent, minTimestamp, GetDeviceId(params.rankId),
            params.pageSize, offset);
    }

    if (resultSet == nullptr) {
        ServerLog::Error("search All slices details. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::SearchAllSlices searchAllSlice{};
        searchAllSlice.fileId = params.fileId;
        searchAllSlice.name = resultSet->GetString("value");
        searchAllSlice.timestamp = resultSet->GetUint64("startTime");
        searchAllSlice.duration = resultSet->GetUint64("duration");
        searchAllSlice.id = resultSet->GetString("id");
        searchAllSlice.tid = resultSet->GetString("tid");
        searchAllSlice.pid = resultSet->GetString("pid");
        searchAllSlice.depth = resultSet->GetUint64("depth");
        auto deviceId = resultSet->GetString("deviceId");
        searchAllSlice.rankId = params.rankId;
        searchAllSlice.deviceId = deviceId.empty() ? params.rankId : QueryHostInfo() + deviceId;
        body.searchAllSlices.emplace_back(searchAllSlice);
    }
    body.currentPage = params.current;
    body.pageSize = params.pageSize;

    // 计算总数
    auto countStmt = CreatPreparedStatement(GetSearchSliceNameCountSql(sqlParams));
    if (countStmt != nullptr) {
        std::unique_ptr<SqliteResultSet> countResult;
        if (!params.nameFilter.empty()) {
            countResult = countStmt->ExecuteQuery(params.searchContent, params.nameFilter, GetDeviceId(params.rankId));
        } else {
            countResult = countStmt->ExecuteQuery(params.searchContent, GetDeviceId(params.rankId));
        }
        if (countResult != nullptr && countResult->Next()) {
            count = countResult->GetUint64("count");
        }
    }
    body.count = count;
    return true;
}

bool DbTraceDataBase::SearchAllSlicesDetails(const Protocol::SearchAllSliceParams &params,
    Protocol::SearchAllSlicesBody &body, uint64_t minTimestamp, const std::vector<TrackQuery> &trackQueryVec) {
    if (trackQueryVec.empty() && !params.metadataList.empty()) {
        return true;
    }
    if (trackQueryVec.empty()) {
        return SearchAllSlicesDetails(params, body, minTimestamp);
    }
    auto stmt = CreatPreparedStatement(TraceDatabaseHelper::GetLockRangeSql(params, trackQueryVec));
    if (stmt == nullptr) {
        ServerLog::Error("Query slice name failed!.");
        return false;
    }
    TraceDatabaseHelper::SearchAllSliceWithLockRangeBindStmt(params, trackQueryVec, stmt, GetDeviceId(params.rankId));
    uint64_t count = 0;
    uint64_t offset = (params.current - 1) * params.pageSize;
    stmt->BindParams(params.pageSize, offset);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("search All slices details. Failed to get result set.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::SearchAllSlices searchAllSlice{};
        searchAllSlice.fileId = params.fileId;
        searchAllSlice.name = resultSet->GetString("value");
        searchAllSlice.timestamp = resultSet->GetUint64("timestamp");
        searchAllSlice.duration =
            resultSet->GetUint64("endTime") - searchAllSlice.timestamp; // 保证 endTime > timestamp
        searchAllSlice.timestamp -= minTimestamp; // 业务上 minTimestamp 是最小时间，保证 timestamp > minTimestamp
        searchAllSlice.id = resultSet->GetString("id");
        searchAllSlice.tid = resultSet->GetString("tid");
        searchAllSlice.pid = resultSet->GetString("pid");
        searchAllSlice.depth = resultSet->GetUint64("depth");
        auto deviceId = resultSet->GetString("deviceId");
        searchAllSlice.rankId = params.rankId;
        searchAllSlice.deviceId = deviceId.empty() ? params.rankId : QueryHostInfo() + deviceId;
        body.searchAllSlices.emplace_back(searchAllSlice);
    }
    body.currentPage = params.current;
    body.pageSize = params.pageSize;
    Protocol::SearchCountParams searchCountParams;
    searchCountParams.searchContent = params.searchContent;
    searchCountParams.isMatchCase = params.isMatchCase;
    searchCountParams.isMatchExact = params.isMatchExact;
    searchCountParams.rankId = params.rankId;
    searchCountParams.nameFilter = params.nameFilter;
    count += SearchSliceNameCount(searchCountParams, trackQueryVec);
    body.count = count;
    return true;
}

// LCOV_EXCL_BR_STOP
bool DbTraceDataBase::QueryEventsViewData(
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp) {
    auto stmt = CreatPreparedStatement();
    if (stmt == nullptr) {
        return false;
    }
    return TraceDatabaseHelper::QueryEventsViewData4Db(stmt, params, body, minTimestamp, GetDeviceId(params.rankId));
}
std::vector<Protocol::SimpleSlice> DbTraceDataBase::QueryThreadByPid(const Metadata &metaData, uint64_t startTime,
    uint64_t endTime, const std::string &rankId, std::map<std::string, uint64_t> &selfTimeKeyValue) {
    auto stmt = CreatPreparedStatement();
    if (stmt == nullptr) {
        ServerLog::Error("Query_threads. Failed to prepare sql.", sqlite3_errmsg(db));
        return {};
    }
    std::string deviceId = GetDeviceId(rankId);
    std::vector<Protocol::SimpleSlice> completeSlice;
    try {
        auto resultSet = TraceDatabaseHelper::QueryThreadsByPid(stmt, startTime, endTime, metaData, deviceId);
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> trackIdDepthCache;
        while (resultSet->Next()) {
            int col = resultStartIndex;
            Protocol::SimpleSlice simpleSlice{};
            uint64_t id = resultSet->GetUint64(col++);
            simpleSlice.timestamp = resultSet->GetUint64(col++);
            simpleSlice.duration = resultSet->GetUint64(col++);
            simpleSlice.endTime = resultSet->GetUint64(col++);
            simpleSlice.name = stringsCache.at(path)[resultSet->GetString(col++)];
            simpleSlice.depth = resultSet->GetUint32(col++);
            simpleSlice.tid = metaData.tid;
            simpleSlice.pid = metaData.pid;
            simpleSlice.metaType = metaData.isPythonStack ?
                ENUM_TO_STR(PROCESS_TYPE::PYTHON_STACK).value_or("") : metaData.metaType;
            uint64_t trackId = TrackInfoManager::Instance().GetTrackId(rankId, metaData.pid, metaData.tid);
            SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
            auto item = trackIdDepthCache.find(trackId);
            if (item != trackIdDepthCache.end()) {
                simpleSlice.depth = item->second[id];
            } else {
                std::unordered_map<uint64_t, uint32_t> depthCache;
                sliceCacheManager.QueryDepthInfoWithoutTimeRange(std::to_string(trackId), rankId, depthCache);
                trackIdDepthCache[trackId] = depthCache;
                simpleSlice.depth = depthCache[id];
            }
            completeSlice.emplace_back(simpleSlice);
        }
    } catch (DatabaseException &e) {
        ServerLog::Error("Query threads failed, ", e.What());
        return {};
    }
    if (completeSlice.empty()) {
        return completeSlice;
    }
    TraceDatabaseHelper::CalculateSelfTime(completeSlice, selfTimeKeyValue, startTime, endTime);
    return completeSlice;
}

void DbTraceDataBase::Reset() { stringsCache.clear(); }

bool DbTraceDataBase::FillDictMap(LightSliceCache& cache, const Protocol::SearchAllSliceParams& params, std::unordered_set<int32_t>& matchedIds)
{
    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "value like '%'||?||'%'";
    } else {
        nameMatch = "lower(value) like lower('%'||?||'%')";
    }
    std::string dictSql = "SELECT id, value FROM STRING_IDS WHERE " + nameMatch;

    auto dictStmt = CreatPreparedStatement(dictSql);
    if (dictStmt == nullptr) {
        ServerLog::Error("LoadSliceCache: Failed to prepare dict sql.");
        return false;
    }

    auto dictResult = dictStmt->ExecuteQuery(params.searchContent);
    if (dictResult == nullptr) {
        ServerLog::Error("LoadSliceCache: Failed to execute dict query.");
        return false;
    }

    while (dictResult->Next()) {
        int32_t id = dictResult->GetInt32("id");
        std::string name = dictResult->GetString("value");
        matchedIds.insert(id);
        cache.dictMap[id] = name;
    }
    return !matchedIds.empty();
}

void DbTraceDataBase::LoadTableData(LightSliceCache& cache, const std::unordered_set<int32_t>& matchedIds, bool isExist, const std::string& sql, SliceTableType tableType)
{
    if (!isExist) return;
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) return;
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) return;

    while (result->Next()) {
        int32_t stringId = result->GetInt32("nameId");
        if (matchedIds.count(stringId) == 0) continue;

        cache.tableTypes.push_back(static_cast<int8_t>(tableType));
        cache.rowIds.push_back(result->GetUint64("rowId"));
        cache.stringIds.push_back(stringId);
        cache.timestamps.push_back(result->GetUint64("startTime"));
        cache.durations.push_back(result->GetUint64("duration"));
    }
}

bool DbTraceDataBase::LoadSliceCache(LightSliceCache& cache,
    const Protocol::SearchAllSliceParams& params, uint64_t minTimestamp)
{
    cache.clear();
    cache.rankId = params.rankId;
    cache.primarySearchContent = params.searchContent;

    // Step 1: 查询 STRING_IDS 获取匹配的 id 和名称
    std::unordered_set<int32_t> matchedIds;
    if (!FillDictMap(cache, params, matchedIds)) {
        return true;  // 无匹配结果，缓存为空
    }

    // Step 2: 查询各业务表获取轻量数据
    // 注意：时间戳需要减去 minTimestamp 转换为相对时间
    std::string deviceId = GetDeviceId(params.rankId);

    // 2.1 加载 TASK 表数据
    std::string taskSql = "SELECT main.ROWID as rowId, "
        "coalesce(compute.name, schedule.name, main.taskType) as nameId, "
        "main.startNs - " + std::to_string(minTimestamp) + " as startTime, "
        "main.endNs - main.startNs as duration "
        "FROM TASK main "
        "LEFT JOIN COMPUTE_TASK_INFO compute ON compute.globalTaskId = main.globalTaskId "
        "LEFT JOIN COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId";
    if (!deviceId.empty()) {
        taskSql += " WHERE main.deviceId = '" + deviceId + "'";
    }
    LoadTableData(cache, matchedIds, isExistTask, taskSql, SliceTableType::TASK);

    // 2.2 加载 CANN_API 表数据
    std::string cannSql = "SELECT ROWID as rowId, name as nameId, startNs - " + std::to_string(minTimestamp) +
                          " as startTime, endNs - startNs as duration FROM CANN_API";
    LoadTableData(cache, matchedIds, isExistCann, cannSql, SliceTableType::CANN_API);

    // 2.3 加载 PYTORCH_API 表数据
    std::string pytorchSql = "SELECT ROWID as rowId, name as nameId, startNs - " + std::to_string(minTimestamp) +
                             " as startTime, endNs - startNs as duration FROM PYTORCH_API";
    LoadTableData(cache, matchedIds, isExistPytorch, pytorchSql, SliceTableType::PYTORCH_API);

    // 2.4 加载 COMMUNICATION_OP 表数据
    std::string commSql = "SELECT ROWID as rowId, opName as nameId, startNs - " + std::to_string(minTimestamp) +
                          " as startTime, endNs - startNs as duration FROM COMMUNICATION_OP";
    LoadTableData(cache, matchedIds, isExistCommOp, commSql, SliceTableType::COMMUNICATION_OP);

    // 2.5 加载 MSTX_EVENTS 表数据
    std::string mstxSql = "SELECT ROWID as rowId, message as nameId, startNs - " + std::to_string(minTimestamp) +
                          " as startTime, endNs - startNs as duration FROM MSTX_EVENTS";
    LoadTableData(cache, matchedIds, isExistMstx, mstxSql, SliceTableType::MSTX);

    // Step 3: 初始化排序索引
    SearchSliceCacheManager::InitializeSortedIndices(cache);

    return cache.size() > 0;
}

std::string DbTraceDataBase::BuildIdList(const std::vector<uint64_t>& ids)
{
    std::string result;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) result += ",";
        result += std::to_string(ids[i]);
    }
    return result;
}

std::string DbTraceDataBase::GetSliceDetailSql(SliceTableType type, uint64_t minTimestamp, const std::string& idList)
{
    std::string minTimeStr = std::to_string(minTimestamp);
    switch (type) {
        case SliceTableType::TASK:
            return "SELECT main.ROWID as rowId, coalesce(compute.name, schedule.name, main.taskType) as nameId, "
                   "main.startNs - " + minTimeStr + " as startTime, main.endNs - main.startNs as duration, "
                   "main.streamId as tid, 'Ascend Hardware' as pid, main.depth, main.deviceId "
                   "FROM TASK main "
                   "LEFT JOIN COMPUTE_TASK_INFO compute ON compute.globalTaskId = main.globalTaskId "
                   "LEFT JOIN COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId "
                   "WHERE main.ROWID IN (" + idList + ")";
        case SliceTableType::CANN_API:
            return "SELECT ROWID as rowId, name as nameId, startNs - " + minTimeStr + " as startTime, endNs - startNs as duration, "
                   "type as tid, globalTid as pid, depth, '' as deviceId "
                   "FROM CANN_API WHERE ROWID IN (" + idList + ")";
        case SliceTableType::PYTORCH_API:
            return "SELECT ROWID as rowId, name as nameId, startNs - " + minTimeStr + " as startTime, endNs - startNs as duration, "
                   "'pytorch' as tid, globalTid as pid, depth, '' as deviceId "
                   "FROM PYTORCH_API WHERE ROWID IN (" + idList + ")";
        case SliceTableType::COMMUNICATION_OP:
            return "SELECT ROWID as rowId, opName as nameId, startNs - " + minTimeStr + " as startTime, endNs - startNs as duration, "
                   "groupName||'group' as tid, 'HCCL' as pid, 0 as depth, '' as deviceId "
                   "FROM COMMUNICATION_OP WHERE ROWID IN (" + idList + ")";
        case SliceTableType::MSTX:
            return "SELECT ROWID as rowId, message as nameId, startNs - " + minTimeStr + " as startTime, endNs - startNs as duration, "
                   "domainId as tid, globalTid as pid, depth, '' as deviceId "
                   "FROM MSTX_EVENTS WHERE ROWID IN (" + idList + ")";
        default:
            return "";
    }
}

void DbTraceDataBase::FillSearchAllSlices(const LightSliceCache& cache, const Protocol::SearchAllSliceParams& params,
    SqliteResultSet* result, Protocol::SearchAllSlicesBody& body)
{
    while (result->Next()) {
        Protocol::SearchAllSlices slice{};
        slice.fileId = params.fileId;

        int32_t nameId = result->GetInt32("nameId");
        auto it = cache.dictMap.find(nameId);
        if (it != cache.dictMap.end()) {
            slice.name = it->second;
        }

        slice.timestamp = result->GetUint64("startTime");
        slice.duration = result->GetUint64("duration");
        slice.id = std::to_string(result->GetUint64("rowId"));
        slice.tid = result->GetString("tid");
        slice.pid = result->GetString("pid");
        slice.depth = result->GetUint64("depth");
        slice.rankId = params.rankId;
        auto deviceId = result->GetString("deviceId");
        slice.deviceId = deviceId.empty() ? params.rankId : QueryHostInfo() + deviceId;

        body.searchAllSlices.push_back(slice);
    }
}

bool DbTraceDataBase::FetchSliceDetails(const LightSliceCache& cache,
    const std::vector<TargetRow>& rows,
    const Protocol::SearchAllSliceParams& params,
    Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp)
{
    if (rows.empty()) return true;

    // 按表类型分组
    std::unordered_map<SliceTableType, std::vector<uint64_t>> groupedRows;
    for (const auto& row : rows) {
        groupedRows[row.tableType].push_back(row.rowId);
    }

    // 分别查询各表
    for (const auto& [tableType, rowIds] : groupedRows) {
        std::string idList = BuildIdList(rowIds);
        std::string sql = GetSliceDetailSql(tableType, minTimestamp, idList);
        if (sql.empty()) continue;

        auto stmt = CreatPreparedStatement(sql);
        if (stmt == nullptr) continue;

        auto result = stmt->ExecuteQuery();
        if (result == nullptr) continue;

        FillSearchAllSlices(cache, params, result.get(), body);
    }

    return true;
}

}
// clang-format on
