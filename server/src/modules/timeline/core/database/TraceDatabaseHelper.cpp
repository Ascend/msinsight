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
#include "TraceDatabaseHelper.h"
#include <sstream>
#include <algorithm>
#include "CounterEventHelper.h"
#include "NpuInfoRepo.h"
#include "Database.h"
#include "OverlapAnsRepo.h"
#include "FullDbEnumUtil.h"
#include "TraceDatabaseSqlConst.h"

// clang-format off
namespace Dic::Module::Timeline {
std::map<std::string, PROCESS_TYPE> metaTypeMap = {
    {"Python", PROCESS_TYPE::API},
    {"python3", PROCESS_TYPE::API},
    {"MindSpore", PROCESS_TYPE::API},
    {"CANN", PROCESS_TYPE::CANN_API},
    {"Ascend Hardware", PROCESS_TYPE::ASCEND_HARDWARE},
    {"HCCL", PROCESS_TYPE::HCCL},
    {"Communication", PROCESS_TYPE::HCCL},
    {"Overlap Analysis", PROCESS_TYPE::OVERLAP_ANALYSIS},
    {"Python GC", PROCESS_TYPE::PYTHON_GC},
};

const Protocol::EventsViewColumnAttr columnName = {"Name", "string", "name"};
const Protocol::EventsViewColumnAttr columnStart = {"Start", "number", "start"};
const Protocol::EventsViewColumnAttr columnDuration = {"Duration(ns)", "number", "duration"};
const Protocol::EventsViewColumnAttr columnTid = {"TID", "string", "tid"};
const Protocol::EventsViewColumnAttr columnPid = {"PID", "string", "pid"};
const Protocol::EventsViewColumnAttr columnRankId = {"Rank ID", "string", "rankId"};
const Protocol::EventsViewColumnAttr columnStreamName = {"Stream Name", "string", "threadName"};
const Protocol::EventsViewColumnAttr columnGroupName = {"Group Name", "string", "threadName"};
const Protocol::EventsViewColumnAttr columnAnalysisType = {"Analysis Type", "string", "threadName"};

std::map<std::string, std::string> analysisType = {
    {"0", "Computing"},
    {"1", "Communication"},
    {"2", "Communication(Not Overlapped)"},
    {"3", "Free"},
};

std::map<PROCESS_TYPE, std::vector<Protocol::EventsViewColumnAttr>> eventsViewColumnsMap = {
    {PROCESS_TYPE::PROCESS, {columnName, columnStart, columnDuration, columnTid, columnPid}},
    {PROCESS_TYPE::MS_TX, {columnName, columnStart, columnDuration, columnTid, columnPid}},
    {PROCESS_TYPE::API, {columnName, columnStart, columnDuration, columnTid, columnPid}},
    {PROCESS_TYPE::CANN_API, {columnName, columnStart, columnDuration, columnTid, columnPid}},
    {PROCESS_TYPE::OSRT_API, {columnName, columnStart, columnDuration, columnTid, columnPid}},
    {PROCESS_TYPE::ASCEND_HARDWARE,
        {columnName, columnStart, columnDuration, columnStreamName, columnRankId}},
    {PROCESS_TYPE::HCCL, {columnName, columnStart, columnDuration, columnGroupName, columnRankId}},
    {PROCESS_TYPE::CCU, {columnName, columnStart, columnDuration, columnGroupName, columnRankId}},
    {PROCESS_TYPE::OVERLAP_ANALYSIS,
        {columnName, columnStart, columnDuration, columnAnalysisType, columnRankId}},
};

/* Functions for BbTraceDataBase */
std::optional<std::string> TraceDatabaseHelper::QueryConnectionId(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::UnitFlowsParams &requestParams)
{
    std::string sql;
    auto processType = GetProcessType(requestParams.metaType);
    std::unique_ptr<SqliteResultSet> resultSet;
    switch (processType) {
        case PROCESS_TYPE::ASCEND_HARDWARE:
            sql = "select connectionId from TASK where ROWID = ?";
            resultSet = ExecuteQuery(stmt, sql, requestParams.id);
            break;
        case PROCESS_TYPE::HCCL:
            sql = "select connectionId from COMMUNICATION_OP where ROWID = ? and groupName||'group' = ?";
            resultSet = ExecuteQuery(stmt, sql, requestParams.id, requestParams.tid);
            break;
        case PROCESS_TYPE::CANN_API:
            sql = "select connectionId from CANN_API where ROWID = ?";
            resultSet = ExecuteQuery(stmt, sql, requestParams.id);
            break;
        case PROCESS_TYPE::API:
            sql = " select ids.connectionId from PYTORCH_API api "
                  " join CONNECTION_IDS ids on api.connectionId = ids.id where api.ROWID = ?";
            resultSet = ExecuteQuery(stmt, sql, requestParams.id);
            break;
        case PROCESS_TYPE::MS_TX:
            sql = " select api.connectionId from " + TABLE_MSTX_EVENTS + " api where api.ROWID = ?";
            resultSet = ExecuteQuery(stmt, sql, requestParams.id);
            break;
        default:
            return std::nullopt;
    }
    if (!resultSet->Next()) {
        return std::nullopt;
    }
    return resultSet->GetString("connectionId");
}

std::string TraceDatabaseHelper::GetSystemViewSqlByLayer(const std::string &layer, const std::string &rankId,
    const std::string &timeCondSql)
{
    std::string mainSql;
    if (layer == "Ascend Hardware") {
        mainSql = "with nameIds as ( select id, value as realName from STRING_IDS where lower(value) like ?),\n"
                  "  main as (select coalesce(a.realName, c.realName, b.realName) as name, startNs, "
                  "  endNs - startNs as duration from TASK task\n"
                  "  left join COMPUTE_TASK_INFO info on info.globalTaskId = task.globalTaskId "
                  "  left join COMMUNICATION_SCHEDULE_TASK_INFO schedule on task.globalTaskId = schedule.globalTaskId"
                  "  left join nameIds a on info.name = a.id "
                  "  left join nameIds b on task.taskType = b.id"
                  "  left join nameIds c on schedule.name = c.id"
                  "  where deviceId = ? " + timeCondSql + " ),";
    } else if (layer == "HCCL" || layer == "COMMUNICATION") {
        std::string comSql;
        if (IsDeviceIdUnique(rankId)) {
            comSql = " select realName as name, op.endNs - op.startNs as duration, op.startNs as startNs, "
                    "  op.endNs as endNs from COMMUNICATION_OP op join nameIds on op.opName = id join rankId"
                    "  group by opId";
        } else {
            comSql = "select realName as name, op.endNs - op.startNs as duration, op.startNs as startNs, "
                     "  op.endNs as endNs from COMMUNICATION_OP op join nameIds on op.opName = id join rankId"
                     "  join TASK task on task.connectionId = op.connectionId"
                     "  where task.deviceId = rankId.deviceId group by opId";
        }
        mainSql = "with nameIds as ( select id, value as realName from STRING_IDS where lower(value) like ?), "
                  "     rankId as (select ? as deviceId),\n"
                  "  main_raw as (select realName as name, endNs - startNs as duration, startNs, endNs "
                  "  from TASK task join rankId join COMMUNICATION_TASK_INFO info "
                  "  on info.globalTaskId = task.globalTaskId join nameIds on info.taskType = id "
                  "  where task.deviceId = rankId.deviceId "
                  "  UNION ALL " + comSql + "), main AS ( SELECT * FROM main_raw WHERE 1=1 " + timeCondSql + " ), ";
    } else if (layer == "CANN") {
        mainSql = "with nameIds as ( select id, value as realName from STRING_IDS where lower(value) like ?), "
                  " tmp as (select globalPid from TASK where deviceId = ? group by globalPid), "
                  " main as (select realName as name,startNs, endNs - startNs as duration from CANN_API api join tmp on "
                  " api.globalTid >> 32 = tmp.globalPid join nameIds on name = id WHERE 1=1 " + timeCondSql + " ),";
    } else if (layer == "Python") {
        mainSql = "with nameIds as ( select id, value as realName from STRING_IDS where lower(value) like ?), "
                  " tmp as (select globalPid from TASK where deviceId = ? group by globalPid), "
                  " main as (select realName as name, startNs,endNs - startNs as duration from PYTORCH_API api join tmp on "
                  " api.globalTid >> 32 = tmp.globalPid join nameIds on name = id WHERE 1=1 " + timeCondSql + " ),";
    } else if (layer == "Overlap Analysis") {
        mainSql = " with main as (select case type when 0 then 'Computing' when 1 then 'Communication' "
                  " when 2 then 'Communication(Not Overlapped)' else 'Free' end as name, startNs, endNs - startNs as duration "
                  " from OVERLAP_ANALYSIS task where name like ? and deviceId = ? " + timeCondSql + " ),";
    } else {
        throw DatabaseException("unsupported type!");
    }
    return mainSql;
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QuerySystemViewData(
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::SystemViewParams &requestParams,
    const std::string& rankId, const uint64_t &minTimestamp, const std::string &timeCondSql)
{
    std::string searchName = "%" + requestParams.searchName + "%";
    std::transform(searchName.begin(), searchName.end(), searchName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::string orderBy;
    if (!StringUtil::CheckSqlValid(requestParams.orderBy)) {
        throw DatabaseException("There is an SQL injection attack on this parameter.");
    }
    if (requestParams.order == "descend") {
        orderBy = " ORDER BY " + requestParams.orderBy + " DESC";
    } else {
        orderBy = " ORDER BY " + requestParams.orderBy + " ASC";
    }
    auto sql = " total as (select sum(case when name != 'Communication' then duration else 0 end) as totalTime, "
     " count(distinct name) as num from main where name != '') select name, round(sum(duration)*100.0/total.totalTime, 4) as time, "
     "sum(duration) / 1000.0 as totalTime, count(1) as numberCalls, round(avg(duration) / 1000.0, 2) as avg, "
     "min(duration) / 1000.0 as min, max(duration) / 1000.0 as max, total.num from main join total where main.name != '' group by name ";
    auto limitSql = " limit ? offset ?";
    std::string mainSql = GetSystemViewSqlByLayer(requestParams.layer, requestParams.rankId, timeCondSql);
    if (requestParams.startTime == requestParams.endTime) {
        return ExecuteQuery(stmt, mainSql + sql + orderBy + limitSql, searchName, rankId,
                            requestParams.pageSize, (requestParams.current - 1) * requestParams.pageSize);
    }
    return ExecuteQuery(stmt, mainSql + sql + orderBy + limitSql, searchName, rankId,
                        requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp,
                        requestParams.pageSize, (requestParams.current - 1) * requestParams.pageSize);
}

//QuerySystemViewTraceData
std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QuerySystemViewTraceData(
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::SystemViewParams &requestParams,
    const std::string& rankId, const uint64_t &minTimestamp, const std::string &timeCondSql)
{
    std::string searchName = "%" + requestParams.searchName + "%";
    std::transform(searchName.begin(), searchName.end(), searchName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::string orderBy;
    if (!StringUtil::CheckSqlValid(requestParams.orderBy)) {
        throw DatabaseException("There is an SQL injection attack on this parameter.");
    }
    if (requestParams.order == "descend") {
        orderBy = " ORDER BY " + requestParams.orderBy + " DESC";
    } else {
        orderBy = " ORDER BY " + requestParams.orderBy + " ASC";
    }
    auto sql = " total as (select sum(case when name != 'Communication' then duration else 0 end) as totalTime, "
                "count(name) as num from main where name != '') "
                "select name, startNs as startTime, round(duration/1000.0, 3) as duration, num from main join total where main.name != ''";
    auto limitSql = " limit ? offset ?";
    std::string mainSql = GetSystemViewSqlByLayer(requestParams.layer, requestParams.rankId, timeCondSql);
    if (requestParams.startTime == requestParams.endTime) {
        return ExecuteQuery(stmt, mainSql + sql + orderBy + limitSql, searchName, rankId,
                            requestParams.pageSize, (requestParams.current - 1) * requestParams.pageSize);
    }
    return ExecuteQuery(stmt, mainSql + sql + orderBy + limitSql, searchName, rankId,
                        requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp,
                        requestParams.pageSize, (requestParams.current - 1) * requestParams.pageSize);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryHostUnitCounter(
    std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::UnitCounterParams &requestParams, uint64_t minTimestamp)
{
    CounterEventHelper helper;
    helper.RegisterHostMap();
    auto processType = GetProcessType(requestParams.metaType);
    std::string sql = helper.GenerateHostCounterSQL(processType);
    return ExecuteQuery(stmt, sql, minTimestamp,
        requestParams.threadId, requestParams.startTime, requestParams.endTime);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryDeviceUnitCounter(
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitCounterParams &requestParams,
    uint64_t minTimestamp, const std::string &rankId)
{
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    auto processType = GetProcessType(requestParams.metaType);
    std::string sql = helper.GenerateDeviceCounterSQL(processType, requestParams.threadId);
    return ExecuteQuery(stmt, sql, minTimestamp,
        requestParams.threadId, requestParams.startTime, requestParams.endTime, rankId);
}

std::unique_ptr <SqliteResultSet> TraceDatabaseHelper::QueryThreadSameOperatorsDetails(
    std::unique_ptr <SqlitePreparedStatement> &stmt, const Protocol::UnitThreadsOperatorsParams &requestParams,
    const QUERY_THREAD_SAME_OPERATORS_PARAMS& params)
{
    const auto rankId = params.rankId;
    const auto minTimestamp = params.minTimestamp;
    const auto orderBy = params.orderBy;
    std::string sql;
    std::string withHeadSql = "with params as (SELECT ? as rankId, ? as minTime, ? as startTime, ? as endTime), "
        "  nameIds as (select id from STRING_IDS where value = ?) ";
    const bool uniqueDevice = IsDeviceIdUnique(requestParams.rankId);
    const int overlapType = OverlapAnsRepo::GetTypeByName(requestParams.name);
    std::vector<std::string> mainSqlList;
    for (const auto &item: requestParams.metaTypeList) {
        std::optional<PROCESS_TYPE> type = STR_TO_ENUM<PROCESS_TYPE>(item);
        if (!type.has_value()) {
            continue;
        }
        mainSqlList.emplace_back(GetQueryThreadSameOperatorsDetailsHeadSql(params, uniqueDevice,
                                                                           overlapType, type.value()));
    }
    if (mainSqlList.empty()) {
        return nullptr;
    }
    const auto sameOperatorsDetailsSql = withHeadSql +
        " SELECT * FROM (" + StringUtil::join(mainSqlList, " UNION ") + ")";
    Prepare(stmt, sameOperatorsDetailsSql + orderBy)->BindParams(rankId, minTimestamp,
        requestParams.startTime, requestParams.endTime, requestParams.name);
    return Execute(stmt);
}

std::string TraceDatabaseHelper::GetQueryThreadSameOperatorsDetailsHeadSql(
    const QUERY_THREAD_SAME_OPERATORS_PARAMS &params,
    const bool uniqueDevice, const int overlapType, const PROCESS_TYPE type)
{
    // 已经在DbTraceDataBase::QueryThreadSameOperatorsDetails中检查过tid sql注入风险
    const std::string tidListStr = StringUtil::Join4SqlGroup(params.tidList);
    // pid 从内部数据中获取，无sql注入风险
    const std::string pidListStr = StringUtil::Join4SqlGroup(params.pidList);
    switch (type) {
        case PROCESS_TYPE::ASCEND_HARDWARE:
            return GetAscendSameNameDetailSql(params.tidList);
        case PROCESS_TYPE::HCCL:
            return GetHcclSameNameDetailSql(tidListStr, uniqueDevice);
        case PROCESS_TYPE::CANN_API:
            return GetCANNSameNameDetailSql(pidListStr, tidListStr);
        case PROCESS_TYPE::MS_TX:
            return GetMstxSameNameDetailSql(pidListStr, tidListStr);
        case PROCESS_TYPE::API:
            return GetPythonSameNameDetailSql(pidListStr);
        case PROCESS_TYPE::OSRT_API:
            return GetOsrtSameNameDetailSql(pidListStr);
        case PROCESS_TYPE::OVERLAP_ANALYSIS:
            return GetOverlapAnalysisSameNameDetailSql(overlapType);
        case PROCESS_TYPE::CCU:
            return GetCcuSameNameDetailSql(tidListStr);
        default:
            return "";
    }
}

std::vector<uint64_t> TraceDatabaseHelper::GetDeviceIdList(const std::string &fileId)
{
    return npuInfoRepo->QueryDeviceIdByFileId(fileId);
}

bool TraceDatabaseHelper::IsDeviceIdUnique(const std::string &fileId)
{
    std::vector<uint64_t> deviceIdList = GetDeviceIdList(fileId);
    return deviceIdList.size() == 1;
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryLabelTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql;
    auto processType = GetProcessType(requestParams.metaType);
    switch (processType) {
        case PROCESS_TYPE::CANN_API:
            sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
                  " FROM " + TABLE_CANN_API + " WHERE globalTid = ? "
                  " AND startNs BETWEEN ( ? + ? ) AND ( ? + ? ) ORDER BY start_time;";
            return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, requestParams.processId,
                requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp);
        default:
            throw DatabaseException("unsupported type while query label trace summary!");
    }
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryProcessTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    auto processType = GetProcessType(requestParams.metaType);
    switch (processType) {
        case PROCESS_TYPE::ASCEND_HARDWARE:
            return QueryHardwareTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::HCCL:
            return QueryCommunicationTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::CCU:
            return QueryCcuTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::OVERLAP_ANALYSIS:
            return QueryOverlapTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::PROCESS:
            return QueryProcessUnitTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::CANN_API:
            return QueryCANNTracesSummary(rankId, minTimestamp, stmt, requestParams);
        case PROCESS_TYPE::MS_TX:
            return QueryMstxTracesSummary(rankId, minTimestamp, stmt, requestParams);
        default:
            throw DatabaseException("unsupported type while query process trace summary!");
    }
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryThreadTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    if (requestParams.unitType == "label") {
        return QueryLabelTracesSummary(rankId, minTimestamp, stmt, requestParams);
    } else {
        return QueryProcessTracesSummary(rankId, minTimestamp, stmt, requestParams);
    }
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryHardwareTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
        "FROM " + TABLE_TASK + " WHERE deviceId = ? AND start_time >= ? AND start_time <= ? ORDER BY startNs;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, rankId,
                        requestParams.startTime, requestParams.endTime);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryCommunicationTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql;
    if (!IsDeviceIdUnique(requestParams.cardId)) {
        sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
            "FROM " + TABLE_TASK + " main join " + TABLE_COMMUNICATION_TASK_INFO + " info "
            " on main.globalTaskId = info.globalTaskId WHERE deviceId = ? AND start_time >= ? AND start_time <= ? ORDER BY startNs;";
        return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, rankId,
                            requestParams.startTime, requestParams.endTime);
    } else {
        sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
              "FROM " + TABLE_COMMUNICATION_OP + " where start_time >= ? AND start_time <= ? ORDER BY startNs;";
        return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, requestParams.startTime,
                            requestParams.endTime);
    }
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryCcuTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, "
        "endNs - ? as end_time FROM " + TABLE_CCU +
        " WHERE deviceId = ? AND start_time >= ? AND start_time <= ? ORDER BY startNs;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, rankId, requestParams.startTime,
                        requestParams.endTime);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryOverlapTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
        "FROM " + TABLE_OVERLAP_ANALYSIS + " WHERE deviceId = ? AND start_time >= ? AND start_time <= ? ORDER BY startNs;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, rankId, requestParams.startTime,
                        requestParams.endTime);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryCANNTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    // 这个方法作用是查询Thread *泳道的缩略图，所以会查询CANN PyTorch MSTX OSRT数据
    std::string  sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
        " FROM " + TABLE_CANN_API + " WHERE globalTid = ? AND startNs BETWEEN ( ? + ? ) AND ( ? + ? ) "
        " UNION ALL SELECT startNs - ? as start_time,endNs - startNs as duration,"
        "    endNs - ? as end_time from  " + TABLE_API + " WHERE globalTid = ? "
        " AND startNs BETWEEN ( ? + ? ) AND ( ? + ? ) "
        " UNION ALL SELECT startNs - ? AS start_time, endNs - startNs AS duration, endNs - ? AS end_time FROM "
        + TABLE_MSTX_EVENTS + " WHERE globalTid = ? AND startNs BETWEEN ( ? + ? ) AND ( ? + ? )"
        " UNION ALL SELECT startNs - ? AS start_time, endNs - startNs AS duration, endNs - ? AS end_time FROM " +
        TABLE_OSRT_API + " WHERE globalTid = ? AND startNs BETWEEN ( ? + ? ) AND ( ? + ? )"
        " ORDER BY start_time;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, requestParams.processId,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, requestParams.processId,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, requestParams.processId,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, requestParams.processId,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryMstxTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    std::string sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time from " +
            TABLE_MSTX_EVENTS + " WHERE globalTid = ? AND start_time >= ? AND start_time <= ? ORDER BY startNs;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, requestParams.processId,
                        requestParams.startTime, requestParams.endTime);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryProcessUnitTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams)
{
    uint64_t pid = NumberUtil::StringToUnsignedLongLong(requestParams.processId) >> 32;
    std::string sql = "SELECT startNs - ? as start_time, endNs - startNs as duration, endNs - ? as end_time "
        " FROM " + TABLE_CANN_API + " WHERE (globalTid >> 32) = ? "
        " AND startNs BETWEEN ( ? + ? ) AND ( ? + ? ) UNION ALL SELECT startNs - ? as start_time,endNs - startNs as duration,"
        "    endNs - ? as end_time from  " + TABLE_API + " WHERE (globalTid >> 32) = ? "
        " AND startNs BETWEEN ( ? + ? ) AND ( ? + ? ) "
         " UNION ALL SELECT startNs - ? AS start_time, endNs - startNs AS duration, endNs - ? AS end_time FROM "
         + TABLE_MSTX_EVENTS + " WHERE (globalTid >> 32) = ? AND startNs BETWEEN ( ? + ? ) AND ( ? + ? )"
         " UNION ALL SELECT startNs - ? AS start_time, endNs - startNs AS duration, endNs - ? AS end_time FROM " +
         TABLE_OSRT_API + " WHERE (globalTid >> 32) = ? AND startNs BETWEEN ( ? + ? ) AND ( ? + ? )"
        " ORDER BY start_time;";
    return ExecuteQuery(stmt, sql, minTimestamp, minTimestamp, pid,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, pid,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, pid,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp,
                        minTimestamp, minTimestamp, pid,
                        requestParams.startTime, minTimestamp, requestParams.endTime, minTimestamp);
}

std::unique_ptr<SqliteResultSet> TraceDatabaseHelper::QueryThreadsByPid(std::unique_ptr<SqlitePreparedStatement> &stmt,
                                                                        uint64_t startTime,
                                                                        uint64_t endTime,
                                                                        const Dic::Protocol::Metadata &metaData,
                                                                        const std::string &rankId)
{
    std::string sql;
    auto processType = GetProcessType(metaData.metaType);
    switch (processType) {
        case PROCESS_TYPE::ASCEND_HARDWARE:
            // Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
            // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
            // 非MSTX事件查询时必须使用connectionId NOT IN显式排除MSTX事件，否则会将MSTX事件同时查询
            // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
            // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
            if (metaData.tid.find('_') != std::string::npos) {
                size_t pos = metaData.tid.find('_');
                std::string streamId = metaData.tid.substr(0, pos);
                std::string domainId = metaData.tid.substr(pos + 1);
                return ExecuteQuery(stmt, ASCEND_THREADS_MSTX_BY_PID, rankId, streamId, domainId, startTime, endTime);
            } else {
                return ExecuteQuery(stmt, ASCEND_THREADS_EXCLUDING_MSTX_BY_PID, rankId, metaData.tid,
                                    startTime, endTime);
            }
        case PROCESS_TYPE::HCCL:
            return ExecuteQuery(stmt, HCCL_THREADS_BY_PID, rankId, metaData.tid, metaData.tid,
                                startTime, endTime);
        case PROCESS_TYPE::CANN_API:
            return ExecuteQuery(stmt, CANN_API_THREADS_BY_PID, metaData.tid, metaData.pid,
                                startTime, endTime);
        case PROCESS_TYPE::API:
            if (metaData.isPythonStack) {
                return ExecuteQuery(stmt, API_THREADS_BY_PID_AND_PYTHON_FUNCTION, metaData.pid, startTime, endTime);
            } else if (metaData.hidePythonFunction) {
                return ExecuteQuery(stmt, API_THREADS_BY_PID_AND_NO_PYTHON_FUNCTION, metaData.pid, startTime, endTime);
            } else {
                return ExecuteQuery(stmt, API_THREADS_BY_PID, metaData.pid, startTime, endTime);
            }
        case PROCESS_TYPE::OVERLAP_ANALYSIS:
            return ExecuteQuery(stmt, OVERLAP_ANALYSIS_THREAD_BY_PID, rankId, metaData.tid,
                                startTime, endTime);
        case PROCESS_TYPE::MS_TX:
            return ExecuteQuery(stmt, MS_TX_THREAD_BY_PID, metaData.pid, metaData.tid, startTime,
                                endTime);
        case PROCESS_TYPE::OSRT_API:
            return ExecuteQuery(stmt, OSRT_API_THREADS_BY_PID, metaData.pid, startTime, endTime);
        case PROCESS_TYPE::CCU:
            return ExecuteQuery(stmt, CCU_THREADS_BY_PID, metaData.tid, startTime, endTime);
        default:
            throw DatabaseException("unsupported type!");
    }
};

void AppendFilterToDBEventViewSql(std::string &sql, std::string filterName, const Protocol::EventsViewParams &params)
{
    for (const auto &filter : params.filters) { // only name filter
        if (filterName.empty()) {
            sql.append(" AND lower(" + filter.first + ") LIKE lower(?) ");
        } else {
            sql.append(" AND lower(" + filterName + ") LIKE lower(?) ");
        }
    }
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Process(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    uint64_t actualPid = NumberUtil::StringToUnsignedLongLong(params.pid) >> 32;
    // pid匹配的入参为globalTid的高32位 // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    std::string sql = "SELECT pa.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, "
        "'pytorch' as threadId FROM PYTORCH_API AS pa LEFT JOIN STRING_IDS AS si ON pa.name = si.id WHERE pid = ? "
        + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append(" UNION SELECT ca.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, type as threadId "
        "FROM CANN_API AS ca LEFT JOIN STRING_IDS AS si ON ca.name = si.id WHERE pid = ? " + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append("UNION SELECT me.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, domainId as threadId "
        "FROM MSTX_EVENTS AS me LEFT JOIN STRING_IDS AS si ON me.message = si.id WHERE pid = ? " + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append(" UNION SELECT osrt.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, 0 AS depth, globalTid as processId, "
        "'OSRT_API' as threadId FROM OSRT_API AS osrt LEFT JOIN STRING_IDS AS si ON osrt.name = si.id WHERE pid = ? "
        + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for process failed to prepare sql.");
        return nullptr;
    }
    const int sqlCount = 4;
    for (int i = 0; i < sqlCount; ++i) {
        stmt->BindParams(actualPid);
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Thread(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT pa.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, "
        "'pytorch' as threadId FROM PYTORCH_API AS pa LEFT JOIN STRING_IDS AS si ON pa.name = si.id WHERE globalTid = ? " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append(" UNION SELECT ca.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, type as threadId "
        "FROM CANN_API AS ca LEFT JOIN STRING_IDS AS si ON ca.name = si.id WHERE globalTid = ? " + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append(" UNION SELECT me.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, depth, globalTid as processId, domainId as threadId "
        "FROM MSTX_EVENTS AS me LEFT JOIN STRING_IDS AS si ON me.message = si.id WHERE globalTid = ? " + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    sql.append(" UNION SELECT osrt.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, (globalTid / 4294967296) AS pid, 0 AS depth, globalTid as processId, "
        "'OSRT_API' as threadId FROM OSRT_API AS osrt LEFT JOIN STRING_IDS AS si "
        "ON osrt.name = si.id WHERE globalTid = ? " + timeCondSql);
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for thread failed to prepare sql.");
        return nullptr;
    }
    const int sqlCount = 4;
    for (int i = 0; i < sqlCount; ++i) {
        stmt->BindParams(params.pid);
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4MSTX(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT me.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, depth, globalTid as processId, domainId as threadId, "
        "(globalTid / 4294967296) AS pid FROM MSTX_EVENTS AS me LEFT JOIN STRING_IDS AS si ON me.message = si.id "
        "WHERE globalTid = ? " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for MSTX failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Pytorch(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT pa.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, depth, globalTid as processId, 'pytorch' as threadId, "
        "(globalTid / 4294967296) AS pid FROM PYTORCH_API AS pa LEFT JOIN STRING_IDS AS si ON pa.name = si.id "
        "WHERE globalTid = ? " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for MSTX failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4HostHccl(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT ca.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, depth, globalTid as processId, type as threadId, "
        "(globalTid / 4294967296) AS pid FROM CANN_API AS ca LEFT JOIN STRING_IDS AS si ON ca.name = si.id "
        " LEFT JOIN ENUM_API_TYPE AS enum ON enum.id = ca.type WHERE globalTid = ? AND enum.name = 'hccl' " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for host hccl failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4CANN(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT ca.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, depth, globalTid as processId, type as threadId, "
        "(globalTid / 4294967296) AS pid FROM CANN_API AS ca LEFT JOIN STRING_IDS AS si ON ca.name = si.id "
        " LEFT JOIN ENUM_API_TYPE AS enum ON enum.id = ca.type "
        "WHERE globalTid = ? AND enum.name IN ('runtime', 'node', 'model', 'acl') " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for cann failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4SubCANN(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT ca.ROWID as id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, depth, globalTid as processId, type as threadId, "
        "(globalTid / 4294967296) AS pid FROM CANN_API AS ca LEFT JOIN STRING_IDS AS si ON ca.name = si.id "
        "WHERE globalTid = ? AND ca.type = ? " + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for sub cann failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid, params.tid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4OSRT(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{   // pid, tid 实际用于展示，processId, threadId 才是跳转用的 pid, tid
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT osrt.ROWID AS id, value AS name, startNs AS start, (endNs - startNs) AS duration, "
        "(globalTid & 0xFFFFFFFF) AS tid, 0 AS depth, globalTid AS processId, 'OSRT_API' AS threadId, "
        "(globalTid / 4294967296) AS pid FROM OSRT_API AS osrt LEFT JOIN STRING_IDS AS si ON osrt.name = si.id "
        "WHERE globalTid = ?" + timeCondSql;
    AppendFilterToDBEventViewSql(sql, "value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for sub OSRT failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(params.pid);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Ccu(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = "SELECT ccu.ROWID as id, si.value AS name, ccu.startNs AS start, "
        "ccu.endNs - ccu.startNs as duration, 'communication' as threadName, 0 as depth, "
        "'CCU' as processId, ccu.deviceId as threadId FROM " + TABLE_CCU +
        " AS ccu LEFT JOIN STRING_IDS AS si ON si.id = ccu.name WHERE ccu.deviceId = ? ";
    sql.append(timeCondSql);
    AppendFilterToDBEventViewSql(sql, "si.value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for CCU failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(deviceId);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Hardware(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string tmpSql;
    if (params.startTime != params.endTime) {
        tmpSql = "  AND main.endNs >= ? AND main.startNs <= ? ";
    }
    std::string sql =
        "SELECT main.ROWID as id, si.value AS name, main.startNs AS start, main.endNs - main.startNs as duration, "
        "'Stream '||streamId as threadName, main.depth, 'Ascend Hardware' as processId, streamId as threadId "
        " FROM TASK AS main LEFT JOIN COMPUTE_TASK_INFO AS CTI on CTI.globalTaskId = main.globalTaskId "
        " LEFT JOIN COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId "
        " LEFT JOIN MSTX_EVENTS mstx ON main.connectionId = mstx.connectionId "
        " LEFT JOIN STRING_IDS AS si ON si.id = coalesce(CTI.name, schedule.name, mstx.message, main.taskType)"
        " WHERE main.deviceId = ? " + tmpSql;
    AppendFilterToDBEventViewSql(sql, "si.value", params);
    stmt->Prepare(sql.append(orderByCondition));
    if (stmt == nullptr) {
        ServerLog::Error("Query events view for hardware failed to prepare sql.");
        return nullptr;
    }
    stmt->BindParams(deviceId);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto &filter: params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    return stmt->ExecuteQuery();
}

std::string GetStreamEventSql(const std::string& orderByCondition, const EventsViewParams& params)
{  // Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
    // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
    // 非MSTX事件查询时必须使用connectionId NOT IN显式排除MSTX事件，否则会将MSTX事件同时查询
    // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
    // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
    std::string tmpSql;
    if (params.startTime != params.endTime) {
        tmpSql = "  AND main.endNs >= ? AND main.startNs <= ? ";
    }
    std::string sql =
        "SELECT main.ROWID as id, si.value AS name, main.startNs AS start, main.endNs - main.startNs as duration, "
        "'Stream '||streamId as threadName, main.depth, 'Ascend Hardware' as processId, "
        "streamId as threadId FROM TASK AS main "
        " LEFT JOIN COMPUTE_TASK_INFO AS CTI on CTI.globalTaskId = main.globalTaskId "
        " LEFT JOIN COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId "
        " LEFT JOIN STRING_IDS AS si ON si.id = coalesce(CTI.name, schedule.name, main.taskType) "
        "WHERE main.deviceId = ? " + tmpSql + " AND main.connectionId NOT IN (SELECT connectionId FROM MSTX_EVENTS) ";
    std::vector<std::string> temp = std::vector<std::string>(params.threadIdList.size(), "?");
    std::string tempSql = StringUtil::join(temp, ",");
    sql.append("AND main.streamId IN ( ");
    sql.append(tempSql);
    sql.append(" ) ");
    AppendFilterToDBEventViewSql(sql, "si.value", params);

    std::string sqlForMSTX =
        " UNION ALL "
        "SELECT main.ROWID as id, si.value AS name, main.startNs AS start, main.endNs - main.startNs as duration, "
        "'Stream '|| streamId || ' MSTX' || (CASE WHEN si2.value IS NULL THEN '' ELSE ' domain ' || si2.value END) "
        "AS threadName, main.depth, 'Ascend Hardware' as processId, "
        "streamId || '_' || domainId as threadId FROM TASK AS main "
        " INNER JOIN MSTX_EVENTS mstx ON main.connectionId = mstx.connectionId "
        " INNER JOIN STRING_IDS AS si ON si.id = mstx.message "
        " LEFT JOIN STRING_IDS AS si2 ON mstx.domainId = si2.id WHERE main.deviceId = ? " + tmpSql;
    sql.append(sqlForMSTX);
    sql.append("AND main.streamId || '_' || mstx.domainId IN ( ");
    sql.append(tempSql);
    sql.append(" ) ");
    AppendFilterToDBEventViewSql(sql, "si.value", params);
    sql.append(orderByCondition);
    return sql;
}

std::unique_ptr<SqliteResultSet> QueryEventsView4Stream(std::unique_ptr<SqlitePreparedStatement>& stmt,
    const Protocol::EventsViewParams& params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = GetStreamEventSql(orderByCondition, params);
    stmt->Prepare(sql);
    if (stmt == nullptr) {
        throw DatabaseException("Failed to prepare sql.");
    }
    stmt->BindParams(deviceId);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto& item : params.threadIdList) {
        stmt->BindParams(item);
    }
    for (const auto& filter : params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    stmt->BindParams(deviceId);
    if (params.startTime != params.endTime) {
        stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    for (const auto& item : params.threadIdList) {
        stmt->BindParams(item);
    }
    for (const auto& filter : params.filters) {
        stmt->BindParams("%" + filter.second + "%");
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        throw DatabaseException("Failed to ExecuteQuery.");
    }
    return resultSet;
}

std::unique_ptr <SqliteResultSet> QueryEventsView4DeviceHCCL(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    if (!TraceDatabaseHelper::IsDeviceIdUnique(params.rankId)) {
        std::string sql = QUERY_EVENTS_VIEW_FOR_DEVICE_HCCL_DEVICE_ID_NOT_UNIQUE;
        for (const auto &filter : params.filters) {
            sql.append(" WHERE lower(" + filter.first + ") LIKE lower(?) ");
        }
        sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
        stmt->Prepare(sql.append(orderByCondition));
        if (stmt == nullptr) {
            ServerLog::Error("Query events view for device hccl failed to prepare sql.");
            return nullptr;
        }
        stmt->BindParams(deviceId);
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        return stmt->ExecuteQuery();
    } else {
        std::string sql = QUERY_EVENTS_VIEW_FOR_DEVICE_HCCL_DEVICE_ID_UNIQUE;
        for (const auto &filter : params.filters) {
            sql.append(" WHERE lower(" + filter.first + ") LIKE lower(?) ");
        }
        sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
        stmt->Prepare(sql.append(orderByCondition));
        if (stmt == nullptr) {
            ServerLog::Error("Query events view for rank hccl failed to prepare sql.");
            return nullptr;
        }
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        return stmt->ExecuteQuery();
    }
}

std::unique_ptr <SqliteResultSet> QueryEventsView4Group(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    if (!TraceDatabaseHelper::IsDeviceIdUnique(params.rankId)) {
        std::string sql = QUERY_EVENTS_VIEW_FOR_GROUP_DEVICE_ID_NOT_UNIQUE;
        sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
        AppendFilterToDBEventViewSql(sql, "", params);
        stmt->Prepare(sql.append(orderByCondition));
        if (stmt == nullptr) {
            ServerLog::Error("Query events view for group failed to prepare sql.");
            return nullptr;
        }
        stmt->BindParams(deviceId, params.threadName, params.tid);
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
        return stmt->ExecuteQuery();
    } else {
        std::string sql = QUERY_EVENTS_VIEW_FOR_GROUP_DEVICE_ID_UNIQUE;
        sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
        AppendFilterToDBEventViewSql(sql, "", params);
        stmt->Prepare(sql.append(orderByCondition));
        if (stmt == nullptr) {
            ServerLog::Error("Query events view for rank group failed to prepare sql.");
            return nullptr;
        }
        stmt->BindParams(params.threadName, params.tid);
        if (params.startTime != params.endTime) {
            stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
        }
        for (const auto &filter: params.filters) {
            stmt->BindParams("%" + filter.second + "%");
        }
        return stmt->ExecuteQuery();
    }
}

std::unique_ptr<SqliteResultSet> QueryEventsView4Overlap(std::unique_ptr<SqlitePreparedStatement>& stmt,
    const Protocol::EventsViewParams& params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = QUERY_EVENTS_VIEW_FOR_OVERLAP;
    sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
    sql = TextSqlConstant::AppendOverlapFilterSql(params, sql);
    if (params.startTime == params.endTime) {
        return TraceDatabaseHelper::ExecuteQuery(stmt, sql.append(orderByCondition), deviceId);
    } else {
        return TraceDatabaseHelper::ExecuteQuery(stmt, sql.append(orderByCondition), deviceId,
            params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
}

std::unique_ptr <SqliteResultSet> QueryEventsView4OverlapSub(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    auto [orderByCondition, deviceId, minTimestamp, timeCondSql] = dbEventViewSqlParams;
    std::string sql = QUERY_EVENTS_VIEW_FOR_OVERLAP_SUB;
    sql.append(TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime));
    sql = TextSqlConstant::AppendOverlapFilterSql(params, sql);
    if (params.startTime == params.endTime) {
        return TraceDatabaseHelper::ExecuteQuery(stmt, sql.append(orderByCondition), deviceId, params.tid);
    } else {
        return TraceDatabaseHelper::ExecuteQuery(stmt, sql.append(orderByCondition), deviceId, params.tid,
            params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
}

std::unique_ptr <SqliteResultSet> GetEventsViewResult4CANNAPI(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, DbEventViewSqlParams dbEventViewSqlParams)
{
    // 根据泳道的类型，分别调用不同的query语句
    std::string processName = params.processName;
    if (StringUtil::StartWith(processName, "Thread")) {
        if (params.threadName.empty()) {
            return QueryEventsView4Thread(stmt, params, dbEventViewSqlParams);
        } else if (params.threadName == "hccl") { // 存疑，"Thread*" 下面是否有 "hccl" 的情况
            return QueryEventsView4HostHccl(stmt, params, dbEventViewSqlParams);
        }
    } else if (StringUtil::StartWith(processName, "CANN")) {
        if (params.tid.empty() && params.threadName.empty()) {
            return QueryEventsView4CANN(stmt, params, dbEventViewSqlParams);
        } else {
            return QueryEventsView4SubCANN(stmt, params, dbEventViewSqlParams);
        }
    }
    return nullptr;
}

void GetEventsViewResultSet4DbDetails(std::unique_ptr<SqliteResultSet>& resultSet,
                                      const Protocol::EventsViewParams &params,
                                      uint64_t minTimestamp, std::vector<std::unique_ptr<EventDetail>>& details)
{
    PROCESS_TYPE metaType = TraceDatabaseHelper::GetProcessType(params.metaType);
    std::string rankIdWithoutHost;
    size_t index = params.rankId.rfind(" ");
    if (index != std::string::npos) {
        rankIdWithoutHost = params.rankId.substr(index + 1);
    } else {
        rankIdWithoutHost = params.rankId;
    }
    while (resultSet->Next()) {
        auto ptr = std::make_unique<EventDetail>();
        // 根据泳道类型，创建对应子类对象的指针，填充特有数据
        if (metaType == PROCESS_TYPE::API || metaType == PROCESS_TYPE::CANN_API || metaType == PROCESS_TYPE::MS_TX ||
            metaType == PROCESS_TYPE::OSRT_API || metaType == PROCESS_TYPE::PROCESS) {
            auto hostPtr = std::make_unique<HostEventDetail>();
            hostPtr->tid = resultSet->GetString("tid");
            hostPtr->pid = resultSet->GetString("pid");
            ptr = std::move(hostPtr);
        } else {
            auto devicePtr = std::make_unique<DeviceEventDetail>();
            devicePtr->threadName = resultSet->GetString("threadName");
            devicePtr->rankId = rankIdWithoutHost;
            ptr = std::move(devicePtr);
        }
        // 父类指针，填充公共数据
        ptr->id = resultSet->GetString("id");
        ptr->name = resultSet->GetString("name");
        uint64_t tempStartTime = resultSet->GetUint64("start");
        if (tempStartTime < minTimestamp) {
            continue;
        }
        ptr->startTime = tempStartTime - minTimestamp;
        ptr->duration = resultSet->GetUint64("duration");
        ptr->depth = resultSet->GetUint64("depth");
        ptr->threadId = resultSet->GetString("threadId");
        ptr->processId = resultSet->GetString("processId");
        details.emplace_back(std::move(ptr));
    }
}

void ResolveEventsViewResultSet4Db(std::unique_ptr <SqliteResultSet> &resultSet,
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp)
{
    auto metaType = TraceDatabaseHelper::GetProcessType(params.metaType);
    std::vector<std::unique_ptr<EventDetail>> details;
    GetEventsViewResultSet4DbDetails(resultSet, params, minTimestamp, details);
    body.count = details.size();
    body.currentPage = params.currentPage;
    body.pageSize = params.pageSize;
    // 计算分页信息，获取对应分页的数据
    auto indexStart = (params.currentPage - 1) * params.pageSize;
    auto indexEnd = indexStart + params.pageSize;
    indexEnd = indexEnd > details.size() ? details.size() : indexEnd;
    for (uint64_t i = indexStart; i < indexEnd; ++i) {
        body.eventDetailList.emplace_back(std::move(details.at(i)));
    }
    if (metaType == Protocol::PROCESS_TYPE::OVERLAP_ANALYSIS) {
        for (const auto &item: body.eventDetailList) {
            auto overlapPtr = dynamic_cast<DeviceEventDetail*>(item.get());
            if (overlapPtr) {
                overlapPtr->name = analysisType.at(overlapPtr->name);
                overlapPtr->threadName = analysisType.at(overlapPtr->threadName);
            }
        }
    }
    if (eventsViewColumnsMap.find(metaType) == eventsViewColumnsMap.end()) {
        return;
    }
    for (const auto &item: eventsViewColumnsMap.at(metaType)) {
        body.columnList.emplace_back(item);
    }
}

std::string TraceDatabaseHelper::GetOrderByCondition(const Protocol::EventsViewParams &params)
{
    std::string orderBy = params.orderBy.empty() ? "start" : params.orderBy;
    if (!StringUtil::CheckSqlValid(orderBy)) {
        return std::string{};
    }
    std::string order = params.order == "descend" ? "DESC" : "ASC";
    std::string orderByCondition = " ORDER BY " + orderBy + " " + order;
    return orderByCondition;
}

std::unique_ptr<SqliteResultSet> QueryEventsViewResultSet(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, std::string &orderByCondition, const std::string& deviceId,
    uint64_t minTimestamp)
{
    auto metaType = TraceDatabaseHelper::GetProcessType(params.metaType);
    const std::string timeCondSql = TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(params.startTime, params.endTime);
    DbEventViewSqlParams dbEventViewSqlParams = { orderByCondition, deviceId, minTimestamp, timeCondSql };
    switch (metaType) { // 根据不同的泳道类型，调用不同的query查询数据表
        case Protocol::PROCESS_TYPE::PROCESS:
            return QueryEventsView4Process(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::MS_TX:
            return QueryEventsView4MSTX(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::API:
            return QueryEventsView4Pytorch(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::CANN_API:
            return GetEventsViewResult4CANNAPI(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::OSRT_API:
            return QueryEventsView4OSRT(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::ASCEND_HARDWARE:
            if (params.threadIdList.empty() && params.threadName.empty()) {
                return QueryEventsView4Hardware(stmt, params, dbEventViewSqlParams);
            } else {
                return QueryEventsView4Stream(stmt, params, dbEventViewSqlParams);
            }
        case Protocol::PROCESS_TYPE::HCCL:
            if (params.tid.empty() && params.threadName.empty()) {
                return QueryEventsView4DeviceHCCL(stmt, params, dbEventViewSqlParams);
            } else {
                return QueryEventsView4Group(stmt, params, dbEventViewSqlParams);
            }
        case Protocol::PROCESS_TYPE::CCU:
            return QueryEventsView4Ccu(stmt, params, dbEventViewSqlParams);
        case Protocol::PROCESS_TYPE::OVERLAP_ANALYSIS:
            if (params.tid.empty() && params.threadName.empty()) {
                return QueryEventsView4Overlap(stmt, params, dbEventViewSqlParams);
            } else {
                return QueryEventsView4OverlapSub(stmt, params, dbEventViewSqlParams);
            }
        default:
            ServerLog::Warn("No defined query way");
            return nullptr;
    }
}

bool TraceDatabaseHelper::QueryEventsViewData4Db(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp,
    const std::string& deviceId)
{
    std::string orderByCondition = GetOrderByCondition(params);
    if (orderByCondition.empty()) {
        return false;
    }
    std::unique_ptr <SqliteResultSet> resultSet;
    try {
        resultSet = QueryEventsViewResultSet(stmt, params, orderByCondition, deviceId, minTimestamp);
    } catch (DatabaseException &de) {
        ServerLog::Error("Query events view data for DB. Execute query failed: ", de.What());
        return false;
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Query events view data for DB. Sqlite result set is null.");
        return false;
    }
    // 解析查询结果，并封装到Response body中
    ResolveEventsViewResultSet4Db(resultSet, params, body, minTimestamp);
    return true;
}

/* Functions for JsonTraceDataBase */
PROCESS_TYPE GetProcessTypeByProcessName(const std::string &processName)
{
    for (const auto &item: metaTypeMap) {
        if (StringUtil::StartWith(processName, item.first)) {
            return item.second;
        }
    }
    return PROCESS_TYPE::NONE;
}

std::string TraceDatabaseHelper::GetTextEventViewSql(const Protocol::EventsViewParams &params, const std::string &orderBy)
{
    std::string sql4Details = GetSql4QueryEventsViewDetailsInText(params);
    // 拼接SQL语句
    sql4Details.append("WHERE t.pid = ? ");
    if (!params.threadIdList.empty()) {
        std::vector<std::string> temp = std::vector<std::string>(params.threadIdList.size(), "?");
        std::string tempSql = StringUtil::join(temp, ",");
        sql4Details.append("AND t.tid IN ( ");
        sql4Details.append(tempSql);
        sql4Details.append(" ) ");
    }

    if (params.startTime != params.endTime) {
        sql4Details.append("AND (timestamp + duration) >= ? AND timestamp <= ? ");
    }

    AppendFilterToDBEventViewSql(sql4Details, "", params);

    // 拼接分页相关的条件
    std::string order = params.order == "descend" ? "DESC" : "ASC";
    sql4Details.append("ORDER BY " + orderBy + " " + order);
    return sql4Details;
}

std::string TraceDatabaseHelper::GetSql4QueryEventsViewDetailsInText(const Protocol::EventsViewParams &params)
{
    std::string baseSql;
    auto metaType = GetProcessTypeByProcessName(params.processName);
    switch (metaType) {
        case PROCESS_TYPE::API:
        case PROCESS_TYPE::CANN_API:
            baseSql = "SELECT id, name, timestamp AS start, duration, tid, pid, s.track_id, tid AS threadId, "
                      "pid AS processId FROM slice AS s LEFT JOIN thread AS t ON s.track_id = t.track_id ";
            break;
        case PROCESS_TYPE::HCCL:
            baseSql = "SELECT id, name, timestamp AS start, duration, thread_name AS threadName, s.track_id, "
                      "tid AS threadId, pid AS processId FROM slice AS s "
                      "LEFT JOIN thread AS t ON s.track_id = t.track_id AND threadName NOT LIKE 'Plane%' ";
            break;
        case PROCESS_TYPE::ASCEND_HARDWARE:
        case PROCESS_TYPE::OVERLAP_ANALYSIS:
            baseSql = "SELECT id, name, timestamp AS start, duration, thread_name AS threadName, s.track_id, "
                      "tid AS threadId, pid AS processId FROM slice AS s "
                      "LEFT JOIN thread AS t ON s.track_id = t.track_id ";
            break;
        default:
            ServerLog::Error("Query events view data for text. Unsupported process type.");
    }
    return baseSql;
}

void ResolveEventsViewResultSet(std::unique_ptr<SqliteResultSet> &resultSet,
    const Protocol::EventsViewParams &params, EventsViewBody &body, uint64_t minTimestamp)
{
    // 封装结果
    auto metaType = GetProcessTypeByProcessName(params.processName);
    std::vector<std::unique_ptr<EventDetail>> details;
    while (resultSet->Next()) {
        auto ptr = std::make_unique<EventDetail>();
        if (metaType == PROCESS_TYPE::API || metaType == PROCESS_TYPE::CANN_API) {
            auto hostPtr = std::make_unique<HostEventDetail>();
            hostPtr->tid = resultSet->GetString("tid");
            hostPtr->pid = resultSet->GetString("pid");
            ptr = std::move(hostPtr);
        } else {
            auto devicePtr = std::make_unique<DeviceEventDetail>();
            devicePtr->threadName = resultSet->GetString("threadName");
            devicePtr->rankId = params.rankId;
            ptr = std::move(devicePtr);
        }
        ptr->name = resultSet->GetString("name");
        uint64_t tempStartTime = resultSet->GetUint64("start");
        if (tempStartTime < minTimestamp) {
            continue;
        }
        ptr->startTime = tempStartTime - minTimestamp;
        ptr->duration = resultSet->GetUint64("duration");
        ptr->threadId = resultSet->GetString("threadId");
        ptr->processId = resultSet->GetString("processId");
        ptr->id = std::to_string(resultSet->GetUint64("id"));
        details.emplace_back(std::move(ptr));
    }
    body.count = details.size();
    body.currentPage = params.currentPage;
    body.pageSize = params.pageSize;
    // 计算分页信息，获取对应分页的数据
    auto indexStart = (params.currentPage - 1) * params.pageSize;
    auto indexEnd = indexStart + params.pageSize;
    indexEnd = indexEnd > details.size() ? details.size() : indexEnd;
    for (uint64_t i = indexStart; i < indexEnd; ++i) {
        body.eventDetailList.emplace_back(std::move(details.at(i)));
    }
    for (const auto &item: eventsViewColumnsMap.at(metaType)) {
        body.columnList.emplace_back(item);
    }
}

bool TraceDatabaseHelper::QueryEventsViewData4Text(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp)
{
    std::string orderBy = params.orderBy.empty() ? "start" : params.orderBy;
    if (!StringUtil::CheckSqlValid(orderBy)) {
        ServerLog::Error("Query events view data text is an SQL injection attack");
        return false;
    }
    std::string sql4Details = GetTextEventViewSql(params, orderBy);
    stmt->Prepare(sql4Details);
    if (stmt == nullptr) {
        ServerLog::Error("Query events view data text failed to prepare sql.");
        return false;
    }
    stmt->BindParams(params.pid);
    // 查询数据库
    std::unique_ptr<SqliteResultSet> resultSet;
    try {
        if (params.threadIdList.empty()) {
            if (params.startTime != params.endTime) {
                stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
            }
            for (const auto &filter: params.filters) {
                stmt->BindParams("%" + filter.second + "%");
            }
            resultSet = stmt->ExecuteQuery();
            if (resultSet == nullptr) {
                throw DatabaseException("Failed to ExecuteQuery.");
            }
        } else {
            for (const auto &item: params.threadIdList) {
                stmt->BindParams(item);
            }
            if (params.startTime != params.endTime) {
                stmt->BindParams(params.startTime + minTimestamp, params.endTime + minTimestamp);
            }
            for (const auto &filter: params.filters) {
                stmt->BindParams("%" + filter.second + "%");
            }
            resultSet = stmt->ExecuteQuery();
            if (resultSet == nullptr) {
                throw DatabaseException("Failed to ExecuteQuery.");
            }
        }
    } catch (DatabaseException &e) {
        ServerLog::Error("Query events view data for text. Execute query failed: ", e.What());
        return false;
    }
    ResolveEventsViewResultSet(resultSet, params, body, minTimestamp);
    return true;
}

void TraceDatabaseHelper::ComputeSummarySlice(std::unique_ptr<SqliteResultSet> &resultSet,
    uint64_t unitTime, Protocol::UnitThreadTracesSummaryBody &responseBody)
{
    bool resultNotEmpty = false;
    uint64_t tempStartTime = 0;
    uint64_t tempEndTime = 0;
    while (resultSet->Next()) {
        if (!resultNotEmpty) {
            tempStartTime = resultSet->GetUint64("start_time");
            tempEndTime = resultSet->GetUint64("end_time");
            resultNotEmpty = true;
            continue;
        }
        uint64_t curEndTime = resultSet->GetUint64("end_time");
        uint64_t curStartTime = resultSet->GetUint64("start_time");
        if (curEndTime < curStartTime) {
            continue;
        }
        if (tempEndTime + unitTime >= curStartTime) {
            tempEndTime = tempEndTime > curEndTime ? tempEndTime : curEndTime;
            continue;
        }
        ThreadTracesSummary summary;
        summary.startTime = tempStartTime;
        summary.duration = tempEndTime >= tempStartTime ? tempEndTime - tempStartTime : 0;
        tempStartTime = curStartTime;
        tempEndTime = curEndTime;
        responseBody.data.emplace_back(summary);
    }
    if (resultNotEmpty) {
        ThreadTracesSummary summary;
        summary.startTime = tempStartTime;
        summary.duration = tempEndTime > tempStartTime ? tempEndTime - tempStartTime : 0;
        responseBody.data.emplace_back(summary);
    }
}

void TraceDatabaseHelper::QueryAllSliceInRangeByTrackIdHelper(std::unique_ptr<SqliteResultSet> &resultSet,
    uint64_t unitTime, uint64_t minTimestamp, Protocol::UnitThreadTracesSummaryBody &responseBody)
{
    bool resultNotEmpty = false;
    uint64_t tempStartTime = 0;
    uint64_t tempEndTime = 0;
    while (resultSet->Next()) {
        if (!resultNotEmpty) {
            tempStartTime = resultSet->GetUint64("timestamp");
            tempEndTime = resultSet->GetUint64("end_time");
            resultNotEmpty = true;
            continue;
        }
        uint64_t curStartTime = resultSet->GetUint64("timestamp");
        uint64_t curEndTime = resultSet->GetUint64("end_time");
        if (tempEndTime + unitTime >= curStartTime) {
            tempEndTime = tempEndTime > curEndTime ? tempEndTime : curEndTime;
            continue;
        }
        ThreadTracesSummary summary;
        summary.startTime = tempStartTime >= minTimestamp ? tempStartTime - minTimestamp : 0;
        summary.duration = tempEndTime >= tempStartTime ? tempEndTime - tempStartTime : 0;
        tempStartTime = curStartTime;
        tempEndTime = curEndTime;
        responseBody.data.emplace_back(summary);
    }
    if (resultNotEmpty) {
        ThreadTracesSummary summary;
        summary.startTime = tempStartTime - minTimestamp;
        summary.duration = tempEndTime - tempStartTime;
        responseBody.data.emplace_back(summary);
    }
    ServerLog::Info("Summary Size is: ", responseBody.data.size());
}

void TraceDatabaseHelper::SetSystemViewHelpler(std::unique_ptr<SqliteResultSet> resultSet, const LayerStatData &data,
    const Protocol::SystemViewParams &requestParams, Protocol::SystemViewBody &responseBody)
{
    while (resultSet->Next()) {
        Protocol::SystemViewDetail systemViewDetail;
        int col = 0;
        systemViewDetail.name = resultSet->GetString(col++);
        systemViewDetail.time = resultSet->GetDouble(col++);
        systemViewDetail.totalTime = resultSet->GetDouble(col++);
        systemViewDetail.numberCalls = resultSet->GetUint64(col++);
        systemViewDetail.avg = resultSet->GetDouble(col++);
        systemViewDetail.min = resultSet->GetDouble(col++);
        systemViewDetail.max = resultSet->GetDouble(col++);
        responseBody.systemViewDetail.emplace_back(systemViewDetail);
    }
    responseBody.total = data.total;
    responseBody.pageSize = requestParams.pageSize;
    responseBody.currentPage = requestParams.current;
}

void TraceDatabaseHelper::SetKernelDetailHelpler(std::unique_ptr<SqliteResultSet> resultSet, uint64_t minTimestamp,
                                                 Protocol::KernelDetailsBody &responseBody)
{
    while (resultSet->Next()) {
        Protocol::KernelDetail detail;
        detail.id = resultSet->GetString("id");
        detail.taskId = resultSet->GetString("taskId"),
        detail.name = resultSet->GetString("name");
        detail.type = resultSet->GetString("type");
        detail.acceleratorCore = resultSet->GetString("acceleratorCore");
        uint64_t tempStartTime = resultSet->GetUint64("startTime");
        if (tempStartTime < minTimestamp) {
            continue;
        }
        detail.startTime = tempStartTime - minTimestamp;
        detail.duration = resultSet->GetDouble("duration");
        detail.waitTime = resultSet->GetDouble("waitTime");
        detail.blockNum = resultSet->GetUint64("blockNum");
        detail.inputShapes = resultSet->GetString("inputShapes");
        detail.inputDataTypes = resultSet->GetString("inputDataTypes");
        detail.inputFormats = resultSet->GetString("inputFormats");
        detail.outputShapes = resultSet->GetString("outputShapes");
        detail.outputDataTypes = resultSet->GetString("outputDataTypes");
        detail.outputFormats = resultSet->GetString("outputFormats");
        responseBody.kernelDetails.emplace_back(detail);
    }
}

void TraceDatabaseHelper::FilterTopLevelApi(std::vector<Protocol::FlowLocation> &originData,
    const std::set<std::string> &pattern, std::vector<Protocol::FlowLocation> &filterData,
    std::vector<uint32_t> &indexes)
{
    std::vector<uint64_t> endList;
    uint32_t index = 0;
    for (auto &item : originData) {
        uint32_t depth = 0;
        while (depth < endList.size() && endList[depth] > item.timestamp) {
            depth++;
        }
        if (depth < endList.size()) {
            endList[depth] = item.duration;
        } else {
            endList.emplace_back(item.duration);
        }
        if (depth != 0) { // 过滤顶层API
            continue;
        }
        filterData.emplace_back(item);
        if (pattern.find(item.name) != pattern.end()) {
            indexes.emplace_back(index); // 标记潜在的亲和API第一个索引位置
        }
        index++;
    }
}

bool TraceDatabaseHelper::CalculateParallelParameter(const std::vector<Protocol::ThreadTraces> &fwdTraceList,
    const std::vector<Protocol::ThreadTraces> &bwdTraceList,
    uint64_t minBwdStartTime, std::pair<uint16_t, uint16_t> &parameter)
{
    uint16_t minBwdIndex = 1;
    uint16_t microBatchSize = 1;
    // 计算micro batch size
    for (size_t i = 1; i < bwdTraceList.size(); i++) {
        if (bwdTraceList.at(i).startTime > bwdTraceList.at(i - 1).startTime) {
            microBatchSize++;
        } else {
            break;
        }
    }
    for (size_t i = 0; i < bwdTraceList.size(); i++) {
        if (minBwdStartTime == bwdTraceList.at(i).startTime) {
            minBwdIndex = i; // (vppSize - 1) * microBatchSize
            break;
        }
    }
    if (microBatchSize == 0 || minBwdIndex % microBatchSize != 0) {
        return false;
    }

    uint32_t vppSize = (minBwdIndex / microBatchSize) + 1;
    if (vppSize > UINT16_MAX) {
        vppSize = 1;
    }
    parameter = {microBatchSize, static_cast<uint16_t>(vppSize)};
    return true;
}

// 使用sql命令查询前反向连线(fwdbwd)，并拼接同一条连线，按前向的start time升序排序，查询形成每一条连线FlowStartAndEndTime结构体
// 遍历所有的连线，对于同一个前反向内的数据，前向的start time越来越大，后向的start time越来越小
// 当不满足前面的条件时，表明开始了一个新的前反向，即tmp.fStartTime > first.fStartTime，此时根据第一个连线和上一个连线，计算前反向的时长
// 前向的起点应该是first中前向开始时间sStartTime，终点应该是last中前向的结束时间sEndTime
// 反向的起点应该是last中反向开始世家fStartTime，终点应该是first中反向的结束时间fEndTime
// 计算完成后tmp变成新的first，即开始新的前反向
bool TraceDatabaseHelper::ExecuteQueryFwdBwdDataByFlow(std::unique_ptr<SqlitePreparedStatement> stmt,
    const std::string &rankId, uint64_t offset, const Protocol::ExtremumTimestamp &range,
    std::vector<Protocol::ThreadTraces> &fwdBwdData)
{
    auto resultSet = stmt->ExecuteQuery(range.minTimestamp, range.maxTimestamp);
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query fwd/bwd data.", stmt->GetErrorMessage());
        return false;
    }
    std::vector<Protocol::ThreadTraces> fwdTraceList{};
    std::vector<Protocol::ThreadTraces> bwdTraceList{};
    uint64_t minBwdStartTime = UINT64_MAX;
    while (resultSet->Next()) {
        Protocol::ThreadTraces fwdTrace = {
            .name = std::to_string(0), .duration = resultSet->GetUint64("fpDuration"),
            .startTime = resultSet->GetUint64("fpStart") > offset ? resultSet->GetUint64("fpStart") - offset : 0,
            .endTime = resultSet->GetUint64("fpEnd") > offset ? resultSet->GetUint64("fpEnd") - offset : 0,
            .depth = 0, .threadId = LANE_FP_BP, .pid = rankId, .id = "", .cname = MARKER_FP
        };
        Protocol::ThreadTraces bwdTrace = {
            .name = std::to_string(0), .duration = resultSet->GetUint64("bpDuration"),
            .startTime = resultSet->GetUint64("bpStart") > offset ? resultSet->GetUint64("bpStart") - offset : 0,
            .endTime = resultSet->GetUint64("bpEnd") > offset ? resultSet->GetUint64("bpEnd") - offset : 0,
            .depth = 0, .threadId = LANE_FP_BP, .pid = rankId, .id = "", .cname = MARKER_BP
        };

        fwdTraceList.push_back(fwdTrace);
        bwdTraceList.push_back(bwdTrace);
        minBwdStartTime = std::min(minBwdStartTime, bwdTrace.startTime);
    }

    if (fwdTraceList.empty() || bwdTraceList.empty()) {
        ServerLog::Error("Failed to query fwd/bwd data due to empty result.");
        return false;
    }
    std::pair<uint16_t, uint16_t> parallelParameter = {1, 1};
    if (bwdTraceList.at(0).startTime != minBwdStartTime) {
        if (!CalculateParallelParameter(fwdTraceList, bwdTraceList, minBwdStartTime, parallelParameter)) {
            ServerLog::Error("Failed to calculate parallel parallel.");
            return false;
        }
    }
    uint32_t repetitionPeriod = parallelParameter.second * parallelParameter.first;
    for (size_t i = 0; i < bwdTraceList.size(); i++) {
        uint16_t gradientAccIndex = i / repetitionPeriod;
        uint16_t tmpMicroBatch = i % repetitionPeriod;
        uint16_t tmpMicroIndex = tmpMicroBatch % parallelParameter.first;
        uint16_t realBatchIndex = gradientAccIndex * parallelParameter.first + tmpMicroIndex;
        fwdTraceList.at(i).name = std::to_string(realBatchIndex);
        bwdTraceList.at(i).name = std::to_string(realBatchIndex);
        fwdBwdData.push_back(fwdTraceList.at(i));
        fwdBwdData.push_back(bwdTraceList.at(i));
    }

    return true;
}

bool TraceDatabaseHelper::ExecuteQueryP2POpData(std::unique_ptr<SqlitePreparedStatement> stmt,
    const std::string &rankId, uint64_t offset, const ExtremumTimestamp &range,
    std::vector<Protocol::ThreadTraces> &p2pOpData)
{
    if (stmt == nullptr || rankId.empty()) {
        Server::ServerLog::Error("Failed to query p2p operator data due to null statement or empty rand id.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(offset, range.minTimestamp, range.maxTimestamp);
    if (resultSet == nullptr) {
        Server::ServerLog::Error("Failed to get result set for query fwd/bwd data.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::ThreadTraces tmp{};
        tmp.pid = resultSet->GetString("pid");
        tmp.threadId = resultSet->GetString("tid");
        tmp.name = resultSet->GetString("name");
        tmp.startTime = resultSet->GetUint64("startTime");
        tmp.duration = resultSet->GetUint64("duration");
        if (StringUtil::StartWith(tmp.name, "hcom_send")) {
            tmp.cname = MARKER_SEND;
        } else if (StringUtil::StartWith(tmp.name, "hcom_receive")) {
            tmp.cname = MARKER_RECV;
        } else {
            tmp.cname = MARKER_BATCH_SEND_RECV;
        }
        p2pOpData.push_back(tmp);
    }
    return true;
}

void TraceDatabaseHelper::CalculateSelfTime(std::vector<Protocol::SimpleSlice> &rows,
    std::map<std::string, uint64_t> &selfTimeKeyValue, uint64_t startTime, uint64_t endTime)
{
    if (rows.empty()) {
        Server::ServerLog::Error("simpleSlice array size is zero!");
        return;
    }
    uint64_t tmpSelfTime = rows.at(0).duration;
    size_t i = 0;
    size_t j = 0;
    while (i < rows.size()) {
        // j滑完直接滑完所有i
        if (j == rows.size()) {
            // 处理当前tmpSelfTime
            AddData(selfTimeKeyValue, rows.at(i).name, tmpSelfTime);
            // 处理剩余元素
            DealLastData(rows, selfTimeKeyValue, startTime, endTime, i);
            break;
        }
        Protocol::SimpleSlice rowI = rows.at(i);
        Protocol::SimpleSlice rowJ = rows.at(j);
        // 层数相等 or 同一元素, j右移
        if (rowI.depth == rowJ.depth || i >= j) {
            j++;
            continue;
        }
        // rows[i]不属于框选范围内，跳过
        if (rows.at(i).timestamp > endTime || rows.at(i).endTime < startTime) {
            if (i + 1 == rows.size()) { // i滑完结束
                break;
            }
            i++;
            tmpSelfTime = rows.at(i).duration;
            continue;
        }
        // j元素超出i元素覆盖范围，或者j右移到下一层, 记录i元素selfTime并i右移(隐式|| rowJ.timestamp < rowI.timestamp)
        if (rowJ.endTime > rowI.endTime || rowI.depth + 1 < rowJ.depth) {
            AddData(selfTimeKeyValue, rowI.name, tmpSelfTime);
            if (i + 1 == rows.size()) { // i滑完结束
                break;
            }
            i++;
            tmpSelfTime = rows.at(i).duration;
            continue;
        }
        // 符合要求的元素
        if (rowJ.timestamp >= rowI.timestamp && rowJ.endTime <= rowI.endTime && tmpSelfTime >= rowJ.duration) {
            tmpSelfTime -= rowJ.duration;
        }
        j++;
    }
}

void TraceDatabaseHelper::ReduceThread(const std::vector<Protocol::SimpleSlice> &rows,
    const std::map<std::string, uint64_t> &selfTimeKeyValue, Protocol::UnitThreadsBody &responseBody)
{
    for (auto &cur : rows) {
        size_t index = 0;
        bool find = false;
        for (; index < responseBody.data.size(); index++) {
            if (responseBody.data[index].title == cur.name) {
                find = true;
                break;
            }
        }
        if (!find) {
            Protocol::SliceGroupItem sliceGroupItem {};
            sliceGroupItem.title = cur.name;
            sliceGroupItem.wallDuration = cur.duration;
            sliceGroupItem.occurrences = 1;
            sliceGroupItem.avgWallDuration = cur.duration;
            sliceGroupItem.maxWallDuration = cur.duration;
            sliceGroupItem.minWallDuration = cur.duration;
            if (cur.name.empty() || selfTimeKeyValue.find(cur.name) == selfTimeKeyValue.end()) {
                continue;
            } else {
                sliceGroupItem.selfTime = selfTimeKeyValue.at(cur.name);
            }
            sliceGroupItem.processMap[cur.pid] = { cur.tid };
            sliceGroupItem.metaTypeList.insert(cur.metaType);
            responseBody.data.emplace_back(sliceGroupItem);
        } else {
            responseBody.data[index].wallDuration += cur.duration;
            responseBody.data[index].occurrences += 1;
            responseBody.data[index].avgWallDuration = responseBody.data[index].occurrences == 0 ? 0 :
                responseBody.data[index].wallDuration / responseBody.data[index].occurrences;
            if (cur.duration > responseBody.data[index].maxWallDuration) {
                responseBody.data[index].maxWallDuration = cur.duration;
            }
            if (cur.duration < responseBody.data[index].minWallDuration) {
                responseBody.data[index].minWallDuration = cur.duration;
            }
            responseBody.data[index].metaTypeList.insert(cur.metaType);
            if (responseBody.data[index].processMap.count(cur.pid)) {
                responseBody.data[index].processMap[cur.pid].insert(cur.tid);
            } else {
                responseBody.data[index].processMap[cur.pid] = { cur.tid };
            }
        }
    }
}

void TraceDatabaseHelper::ReduceThread(const std::vector<CompeteSliceDomain> &rows,
    const std::map<std::string, uint64_t> &selfTimeKeyValue, Protocol::UnitThreadsBody &responseBody)
{
    for (auto &cur : rows) {
        size_t index = 0;
        bool find = false;
        for (; index < responseBody.data.size(); index++) {
            if (responseBody.data[index].title == cur.name) {
                find = true;
                break;
            }
        }
        if (!find) {
            Protocol::SliceGroupItem sliceGroupItem {};
            sliceGroupItem.title = cur.name;
            sliceGroupItem.wallDuration = cur.duration;
            sliceGroupItem.occurrences = 1;
            sliceGroupItem.avgWallDuration = cur.duration;
            sliceGroupItem.maxWallDuration = cur.duration;
            sliceGroupItem.minWallDuration = cur.duration;
            if (selfTimeKeyValue.find(cur.name) == selfTimeKeyValue.end()) {
                continue;
            }
            sliceGroupItem.selfTime = selfTimeKeyValue.at(cur.name);
            sliceGroupItem.processMap[cur.pid] = { cur.tid };
            sliceGroupItem.metaTypeList.insert(cur.metaType);
            responseBody.data.emplace_back(sliceGroupItem);
        } else {
            responseBody.data[index].wallDuration += cur.duration;
            responseBody.data[index].occurrences += 1;
            responseBody.data[index].avgWallDuration = responseBody.data[index].occurrences == 0 ? 0 :
                responseBody.data[index].wallDuration / responseBody.data[index].occurrences;
            if (cur.duration > responseBody.data[index].maxWallDuration) {
                responseBody.data[index].maxWallDuration = cur.duration;
            }
            if (cur.duration < responseBody.data[index].minWallDuration) {
                responseBody.data[index].minWallDuration = cur.duration;
            }
            if (responseBody.data[index].processMap.count(cur.pid)) {
                responseBody.data[index].processMap[cur.pid].insert(cur.tid);
            } else {
                responseBody.data[index].processMap[cur.pid] = { cur.tid };
            }
        }
    }
}

uint64_t TraceDatabaseHelper::CalculateUncoveredTime(const std::vector<Protocol::ThreadTraces> &uncovered,
    size_t &index, const ThreadTraces &element)
{
    uint64_t totalUncoveredTime = 0;
    if (uncovered.empty() || index >= uncovered.size()) {
        return totalUncoveredTime;
    }
    // sql语句能够保证uncovered按start_time升序排列
    while (index < uncovered.size()) {
        Protocol::ThreadTraces uncoveredEle = uncovered.at(index);
        // 未掩盖部分的分片小于通信Op或Task时，二者无交集，需要跳到下一个未掩盖部分的分片
        if (element.startTime >= uncoveredEle.endTime) {
            index++;
            continue;
        }
        // 未掩盖部分的分片大于通信Op或Task时，二者也无交集，退出循环
        if (element.endTime <= uncoveredEle.startTime) {
            break;
        }
        // 二者有交集时，取其交集部分，就是通信Op或Task真实的未掩盖部分
        uint64_t startMax = uncoveredEle.startTime > element.startTime ? uncoveredEle.startTime : element.startTime;
        uint64_t endMin = uncoveredEle.endTime > element.endTime ? element.endTime : uncoveredEle.endTime;
        uint64_t uncoveredTime = endMin >= startMax ? endMin - startMax : 0;

        if (UINT64_MAX - totalUncoveredTime > uncoveredTime) {
            totalUncoveredTime += uncoveredTime;
        } else {
            // 实际数据很小，正常情况下不会溢出
            ServerLog::Error("Accumulation overflow occurred when calculating total uncovered time: ", uncoveredTime);
            totalUncoveredTime += 0;
        }
        if (element.endTime > uncoveredEle.endTime) {
            index++;
        } else {
            break;
        }
    }
    return totalUncoveredTime;
}

void TraceDatabaseHelper::SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr)
{
    if (npuInfoRepoPtr != nullptr) {
        npuInfoRepo = std::move(npuInfoRepoPtr);
    }
}

std::string TraceDatabaseHelper::GetLockRangeSql(const SearchAllSliceParams &params,
    const std::vector<TrackQuery> &trackQueryVec)
{
    std::string sql;
    std::string nameMatch;
    std::string orderBy;
    if (params.order == "descend") {
        orderBy = " ORDER BY " + params.orderBy + " DESC";
    } else {
        orderBy = " ORDER BY " + params.orderBy + " ASC";
    }
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "select id, value from STRING_IDS where value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "select id, value from STRING_IDS where lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "select id, value from STRING_IDS where value like '%'||?||'%'";
    } else {
        nameMatch = "select id, value from STRING_IDS where lower(value) like lower('%'||?||'%')";
    }

    // 二级筛选 CTE
    std::string filterCte;
    std::string filterJoin;
    if (!params.nameFilter.empty()) {
        filterCte = ", filterIds as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))";
        filterJoin = " join filterIds on filterIds.id = ";
    }

    sql = "with ids as (" + nameMatch + ")" + filterCte + " ";
    std::vector<std::string> sqls;
    for (const auto &item : trackQueryVec) {
        std::string tempSql = GetSingleLockRangeSql(item, filterJoin);
        if (!tempSql.empty()) {
            sqls.emplace_back(tempSql);
        }
    }
    sql = sql + StringUtil::join(sqls, " UNION ALL ");
    std::string orderByFiled = " ORDER BY timestamp DESC  LIMIT ? OFFSET ?";
    sql += orderByFiled;
    return sql;
}

std::string TraceDatabaseHelper::GetSingleLockRangeSql(const TrackQuery &item, const std::string &filterJoin)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(item.metaType).value_or(PROCESS_TYPE::NONE);
    std::string tempSql;
    // filterJoin 格式: " join filterIds on filterIds.id = " 或空字符串
    // 当非空时，需要补上具体的字段名来完成JOIN条件
    std::string filterSuffix = filterJoin.empty() ? "" : filterJoin;
    if (type == PROCESS_TYPE::API) {
        filterSuffix += filterJoin.empty() ? "" : "api.name";
        tempSql = " SELECT api.ROWID as id, 'pytorch' as tid, api.globalTid as pid, api.startNs as timestamp, "
            "api.endNs as endTime, api.depth, '' as deviceId, ids.value as value from " + TABLE_API +
            "  api join ids on ids.id = api.name" + filterSuffix +
            " WHERE api.globalTid = ? AND api.startNs >= ? AND api.endNs <= ? ";
    } else if (type == PROCESS_TYPE::CANN_API) {
        filterSuffix += filterJoin.empty() ? "" : "cann.name";
        tempSql = " SELECT cann.connectionId as id, cann.globalTid as pid, cann.type as tid, cann.startNs as "
            "timestamp, cann.endNs as endTime, cann.depth, '' as deviceId, ids.value from " +
            TABLE_CANN_API +
            "  cann join ids on ids.id = cann.name" + filterSuffix +
            " WHERE globalTid = ? AND type = ? AND startNs >= ? AND endNs <= ? ";
    } else if (type == PROCESS_TYPE::MS_TX) {
        filterSuffix += filterJoin.empty() ? "" : "mstx.message";
        tempSql = " SELECT mstx.ROWID as id, mstx.globalTid as pid, mstx.domainId as tid, mstx.startNs as timestamp, "
            "mstx.endNs as endTime, mstx.depth, '' as deviceId, ids.value from " +
            TABLE_MSTX_EVENTS +
            "  mstx join ids on ids.id = mstx.message" + filterSuffix +
            " WHERE globalTid = ? AND startNs >= ? AND endNs <= ? ";
    } else if (type == PROCESS_TYPE::OSRT_API) {
        filterSuffix += filterJoin.empty() ? "" : "osrt.name";
        tempSql = " SELECT osrt.ROWID AS id, 'OSRT_API' AS tid, osrt.globalTid AS pid, osrt.startNs AS timestamp, "
            "osrt.endNs AS endTime, 0 AS depth, '' AS deviceId, ids.value AS value FROM " + TABLE_OSRT_API +
            "  osrt JOIN ids ON ids.id = osrt.name" + filterSuffix +
            " WHERE osrt.globalTid = ? AND osrt.startNs >= ? AND osrt.endNs <= ? ";
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        filterSuffix += filterJoin.empty() ? "" : "hadware.name";
        tempSql = "SELECT hadware.id as id, hadware.pid as pid, hadware.tid as tid, hadware.timestamp as "
            "timestamp, hadware.endTime as endTime, hadware.depth as depth, hadware.deviceId as deviceId, "
            "ids.value  FROM (SELECT coalesce(c.name, m.message, s.name, main.taskType) as "
            "name, main.ROWID AS id, 'Ascend Hardware' as pid, main.streamId as tid,main.startNs as timestamp, "
            "main.endNs as endTime, main.depth as depth, main.deviceId as deviceId FROM " +
            TABLE_TASK + " main left join " + TABLE_COMPUTE_TASK_INFO +
            " c on c.globalTaskId = main.globalTaskId left join " + TABLE_MSTX_EVENTS +
            " m on (m.connectionId = main.connectionId and  m.connectionId != " +
            WRONG_DATA + " ) left join " + TABLE_COMMUNICATION_SCHEDULE_TASK +
            " s on main.globalTaskId = s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND "
            "main.startNs >= ? AND main.endNs <= ?) hadware  join ids on ids.id = hadware.name" + filterSuffix;
    } else if (type == PROCESS_TYPE::CCU) {
        filterSuffix += filterJoin.empty() ? "" : "ccu.name";
        tempSql = " SELECT ccu.ROWID as id, 'CCU' as pid, ccu.deviceId as tid, ccu.startNs as "
            "timestamp, ccu.endNs as endTime, 0 as depth, ccu.deviceId as deviceId, ids.value as value FROM " +
            TABLE_CCU + " ccu join ids on ids.id = ccu.name" + filterSuffix +
            " WHERE ccu.deviceId = ? AND ccu.startNs >= ? AND ccu.endNs <= ? ";
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(item.threadId, "group")) {
            filterSuffix += filterJoin.empty() ? "" : "op.opName";
            tempSql = " SELECT op.opId as id, 'HCCL' as pid, op.groupName||'group' as tid, op.startNs as "
                "timestamp, op.endNs as endTime, 0 as depth, '0' as deviceId, ids.value from " +
                TABLE_COMMUNICATION_OP +
                " op join ids on id = op.opName" + filterSuffix +
                " WHERE op.groupName = ? AND op.startNs >= ? AND op.endNs <= ? ";
        } else {
            filterSuffix += filterJoin.empty() ? "" : "ci.taskType";
            tempSql = "SELECT main.ROWID as id, 'HCCL' as pid, ci.groupName||'_'||ci.planeId as tid, main.startNs "
                "as timestamp, main.endNs as endTime, main.depth, main.deviceId as deviceId, ids.value from "
                "TASK main join " + TABLE_COMMUNICATION_TASK_INFO +
                " ci on ci.globalTaskId = main.globalTaskId join ids on ids.id = ci.taskType" + filterSuffix +
                " WHERE main.deviceId = ? and ci.groupName = ? AND ci.planeId = ? AND main.startNs >= ? AND "
                "main.endNs <= ?";
        }
    }
    return tempSql;
}

void TraceDatabaseHelper::SearchAllSliceWithLockRangeBindStmt(const SearchAllSliceParams &params,
    const std::vector<TrackQuery> &trackQueryVec, std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string &deviceId)
{
    stmt->BindParams(params.searchContent);
    // 如果有二级筛选，绑定 nameFilter 参数
    if (!params.nameFilter.empty()) {
        stmt->BindParams(params.nameFilter);
    }
    for (const auto &item : trackQueryVec) {
        BindSearchAllSliceSingleTrack(stmt, deviceId, item);
    }
}

void TraceDatabaseHelper::BindSearchAllSliceSingleTrack(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string &deviceId, const TrackQuery &item)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(item.metaType).value();
    if (type == PROCESS_TYPE::API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CANN_API) {
        stmt->BindParams(item.processId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::MS_TX) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::OSRT_API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        stmt->BindParams(deviceId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CCU) {
        stmt->BindParams(item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(item.threadId, "group")) {
            std::string tid = item.threadId.substr(0, item.threadId.size() - 5);
            stmt->BindParams(tid, item.startTime, item.endTime);
        } else {
            std::string groupName = item.threadId;
            std::string threadId = item.threadId;
            size_t pos = item.threadId.find_last_of("_");
            if (pos != std::string::npos && item.threadId.size() > pos) {
                threadId = item.threadId.substr(pos + 1);
                groupName = item.threadId.substr(0, pos);
            }
            stmt->BindParams(deviceId, groupName, threadId, item.startTime, item.endTime);
        }
    }
}

std::string TraceDatabaseHelper::GetSearchSliceNameWithLockRangeSql(const SearchSliceParams &params,
    const std::vector<TrackQuery> &trackQuery, const std::string &path)
{
    std::string sql;
    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "select id from STRING_IDS where lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like '%'||?||'%'";
    } else {
        nameMatch = "select id from STRING_IDS where lower(value) like lower('%'||?||'%')";
    }
    sql = "with ids as (" + nameMatch + ") ";
    std::vector<std::string> sqls;
    for (const auto &item : trackQuery) {
        std::string tempSql = GetSingleSearchNameWithLockRangeSql(path, item);
        if (!tempSql.empty()) {
            sqls.emplace_back(tempSql);
        }
    }
    sql = sql + StringUtil::join(sqls, " UNION ALL ");
    std::string orderBy = " ORDER BY timestamp ASC LIMIT 1 OFFSET ?";
    sql += orderBy;
    return sql;
}

std::string TraceDatabaseHelper::GetSingleSearchNameWithLockRangeSql(const std::string &path,
    const TrackQuery &singleQuery)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(singleQuery.metaType).value();
    std::string tempSql;
    if (type == PROCESS_TYPE::API) {
        tempSql = " SELECT api.ROWID as id, 'pytorch' as tid, api.globalTid as pid, api.startNs as timestamp, "
            "api.endNs as endTime, api.depth, 'PYTORCH_API' as metaType from " + TABLE_API +
            "  api join ids on ids.id = api.name WHERE api.globalTid = ? AND api.startNs >= ? AND api.endNs <= ? ";
    } else if (type == PROCESS_TYPE::CANN_API) {
        tempSql = " SELECT cann.connectionId as id, cann.globalTid as pid, cann.type as tid, cann.startNs as "
            "timestamp, cann.endNs as endTime, cann.depth, 'CANN_API' as metaType from " + TABLE_CANN_API +
            "  cann join ids on ids.id = cann.name WHERE globalTid = ? AND type = ? AND startNs >= ? AND endNs <= "
            "? ";
    } else if (type == PROCESS_TYPE::MS_TX) {
        tempSql = " SELECT mstx.ROWID as id, mstx.globalTid as pid, mstx.domainId as tid, mstx.startNs as timestamp, "
            "mstx.endNs as endTime, mstx.depth, 'MSTX_EVENTS' as metaType from " + TABLE_MSTX_EVENTS +
            "  mstx join ids on ids.id = mstx.message WHERE globalTid = ? AND startNs >= ? AND endNs <= ? ";
    } else if (type == PROCESS_TYPE::OSRT_API) {
        tempSql = " SELECT osrt.ROWID AS id, 'OSRT_API' AS tid, osrt.globalTid AS pid, osrt.startNs AS timestamp, "
            "osrt.endNs AS endTime, 0 AS depth, 'OSRT_API' as metaType FROM " + TABLE_OSRT_API +
            "  osrt JOIN ids ON ids.id = osrt.name WHERE osrt.globalTid = ? AND osrt.startNs >= ? AND osrt.endNs <= ? ";
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        tempSql = "SELECT hadware.id as id, hadware.pid as pid, hadware.tid as tid, hadware.timestamp as "
            "timestamp, hadware.endTime as endTime, hadware.depth as depth, 'Ascend Hardware' as metaType  "
            "FROM (SELECT coalesce(c.name, m.message, s.name, main.taskType) as "
            "name, main.ROWID AS id, 'Ascend Hardware' as pid, main.streamId as tid,main.startNs as timestamp, "
            "main.endNs as endTime, main.depth as depth FROM " +
            TABLE_TASK + " main left join " + TABLE_COMPUTE_TASK_INFO +
            " c on c.globalTaskId = main.globalTaskId left join " + TABLE_MSTX_EVENTS + " m on "
            " (m.connectionId = main.connectionId and  m.connectionId != " +
            WRONG_DATA + " ) left join " + TABLE_COMMUNICATION_SCHEDULE_TASK +
            " s on main.globalTaskId = s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND "
            "main.startNs >= ? AND main.endNs <= ?) hadware  join ids on ids.id = hadware.name ";
    } else if (type == PROCESS_TYPE::CCU) {
        tempSql = " SELECT ccu.ROWID as id, 'CCU' as pid, ccu.deviceId as tid, ccu.startNs as timestamp, "
            "ccu.endNs as endTime, 0 as depth, 'CCU' as metaType from " + TABLE_CCU +
            " ccu join ids on ids.id = ccu.name WHERE ccu.deviceId = ? AND ccu.startNs >= ? "
            "AND ccu.endNs <= ? ";
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(singleQuery.threadId, "group")) {
            tempSql = " SELECT op.opId as id, 'HCCL' as pid, op.groupName||'group' as tid, op.startNs as "
                "timestamp, op.endNs as endTime, 0 as depth, 'HCCL' as metaType from " +
                TABLE_COMMUNICATION_OP +
                " op join ids on id = op.opName WHERE op.groupName = ? AND op.startNs >= ? AND op.endNs <= ? ";
        } else {
            tempSql = "SELECT main.ROWID as id, 'HCCL' as pid, ci.groupName||'_'||ci.planeId as tid, main.startNs "
                "as timestamp, main.endNs as endTime, main.depth, 'HCCL' as metaType from TASK main join " +
                TABLE_COMMUNICATION_TASK_INFO +
                " ci on ci.globalTaskId = main.globalTaskId join ids on ids.id = ci.taskType" +
                " WHERE main.deviceId = ? and ci.groupName = ? AND ci.planeId = ? AND main.startNs >= ? AND "
                "main.endNs <= ?";
        }
    }
    return tempSql;
}

void TraceDatabaseHelper::SearchSliceNameWithLockRangeBindStmt(const SearchSliceParams &params,
    const std::vector<TrackQuery> &trackQuery, std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &path,
    const std::string &deviceId)
{
    stmt->BindParams(params.searchContent);
    for (const auto &item : trackQuery) {
        BindSearchNameWithLockRangeStmt(stmt, path, deviceId, item);
    }
}

void TraceDatabaseHelper::BindSearchNameWithLockRangeStmt(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string &path, const std::string &deviceId, const TrackQuery &item)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(item.metaType).value();
    if (type == PROCESS_TYPE::API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CANN_API) {
        stmt->BindParams(item.processId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::MS_TX) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::OSRT_API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        stmt->BindParams(deviceId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CCU) {
        stmt->BindParams(item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(item.threadId, "group")) {
            std::string tid = item.threadId.substr(0, item.threadId.size() - 5);
            stmt->BindParams(tid, item.startTime, item.endTime);
        } else {
            std::string groupName = item.threadId;
            std::string threadId = item.threadId;
            size_t pos = item.threadId.find_last_of("_");
            if (pos != std::string::npos && item.threadId.size() > pos) {
                threadId = item.threadId.substr(pos + 1);
                groupName = item.threadId.substr(0, pos);
            }
            stmt->BindParams(deviceId, groupName, threadId, item.startTime, item.endTime);
        }
    }
}

void TraceDatabaseHelper::SearchCountWithLockRangeBindStmt(const SearchCountParams &params,
    const std::vector<TrackQuery> &trackQuery, std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string &deviceId)
{
    stmt->BindParams(params.searchContent);
    // 如果有二级筛选，绑定 nameFilter 参数
    if (!params.nameFilter.empty()) {
        stmt->BindParams(params.nameFilter);
    }
    for (const auto &item : trackQuery) {
        BindSingleTrackStmt(params, stmt, deviceId, item);
    }
}

void TraceDatabaseHelper::BindSingleTrackStmt(const SearchCountParams &params,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &deviceId, const TrackQuery &item)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(item.metaType).value();
    if (type == PROCESS_TYPE::API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CANN_API) {
        stmt->BindParams(item.processId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::MS_TX) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::OSRT_API) {
        stmt->BindParams(item.processId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        stmt->BindParams(deviceId, item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::CCU) {
        stmt->BindParams(item.threadId, item.startTime, item.endTime);
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(item.threadId, "group")) {
            std::string tid = item.threadId.substr(0, item.threadId.size() - 5);
            stmt->BindParams(tid, item.startTime, item.endTime);
        } else {
            std::string groupName = item.threadId;
            std::string threadId = item.threadId;
            size_t pos = item.threadId.find_last_of("_");
            if (pos != std::string::npos && item.threadId.size() > pos) {
                threadId = item.threadId.substr(pos + 1);
                groupName = item.threadId.substr(0, pos);
            }
            stmt->BindParams(deviceId, groupName, threadId, item.startTime, item.endTime);
        }
    }
}

std::string TraceDatabaseHelper::GetComOpSliceDetailsSql(const std::string &rankId)
{
    std::string communicationOpSql;
    std::vector<uint64_t> deviceId = TraceDatabaseHelper::GetDeviceIdList(rankId);
    if (deviceId.size() == 1) {
        communicationOpSql = " select " + std::to_string(deviceId[0]) +
            " as deviceId,opName as name,'HCCL' as pid,"
            "'HCCL' as metaType, groupName||'group' as tid, op.startNs - minTime.value as startTime, "
            "op.endNs - op.startNs as duration, 0 as depth, op.ROWID as id "
            " from COMMUNICATION_OP op join minTime";
    } else {
        communicationOpSql = " select tasks.deviceId,opName as name,'HCCL' as pid, 'HCCL' as metaType, "
            " groupName||'group' as tid, op.startNs - minTime.value as startTime, "
            " op.endNs - op.startNs as duration, 0 as depth, op.ROWID as id from COMMUNICATION_OP op "
            " join minTime join tasks on op.connectionId = tasks.connectionId group by opId";
    }
    return communicationOpSql;
}

std::string TraceDatabaseHelper::GetMsTxEventsSliceDetailSql()
{
    return " SELECT '' AS deviceId, message as name, globalTid AS pid, 'HOST' AS metaType,"
        "domainId AS tid, startNs - minTime.value AS startTime, endNs - startNs AS duration,"
        "depth, MSTX_EVENTS.ROWID AS id "
        "FROM MSTX_EVENTS JOIN minTime ";
}

void TraceDatabaseHelper::ComputeTree(std::vector<std::unique_ptr<Protocol::UnitTrack>>& metaData,
                                      std::vector<Process>& processes,
                                      std::vector<std::unique_ptr<Protocol::UnitTrack>>& tempMetaData)
{
    std::unordered_map<std::string, UnitTrack*> idToRawPtr;
    std::unordered_map<std::string, std::unique_ptr<UnitTrack>> idToOwnerPtr;
    // 创建所有节点，放到 owner map 中
    for (auto& item : tempMetaData) {
        std::string pid = item->metaData.processId;
        idToRawPtr[pid] = item.get();
        idToOwnerPtr[pid] = std::move(item);
    }
    // 建立树结构
    for (auto& item : processes) {
        std::string id = item.pid;
        UnitTrack* node = idToRawPtr[id];
        if (node->metaData.parentProcessId != "0" && idToRawPtr.count(node->metaData.parentProcessId)) {
            UnitTrack* parent = idToRawPtr[node->metaData.parentProcessId];
            parent->children.push_back(std::move(idToOwnerPtr[id]));
        } else {
            metaData.push_back(std::move(idToOwnerPtr[id]));
        }
    }
}
}
// clang-format on
