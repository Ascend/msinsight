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

#ifndef PROFILER_SERVER_TRACEDATABASEHELPER_H
#define PROFILER_SERVER_TRACEDATABASEHELPER_H

#include "TableDefs.h"
#include "SqlitePreparedStatement.h"
#include "VirtualTraceDatabase.h"
#include "NumberUtil.h"
#include "FullDbEnumUtil.h"
#include "CommonDefs.h"
#include "DbSqlDefs.h"
#include "JsonUtil.h"
#include "ServerLog.h"
#include "NpuInfoRepo.h"
#include "DataBaseManager.h"
#include "StringUtil.h"

// clang-format off
namespace Dic::Module::Timeline {
using namespace Protocol;
const std::string LANE_FP_BP = "FP/BP";
const std::string LANE_P2P_OP = "P2P Op";
const std::string MARKER_FP = "FP";
const std::string MARKER_BP = "BP";
const std::string MARKER_SEND = "SEND";
const std::string MARKER_RECV = "RECV";
const std::string MARKER_BATCH_SEND_RECV = "BATCH_SEND_RECV";

// 目前根据通信算子名进行过滤，此种方式不够准确，待后续进一步优化为锁定通信域后便锁定p2p通信算子
const std::string QUERY_P2P_COMMUNICATION_OP_TEXT_SQL =
    "SELECT t.pid as pid, t.tid as tid, s.timestamp - ? as startTime, s.duration as duration, s.name as name "
    "FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE + " t ON s.track_id = t.track_id WHERE s.track_id in ( "
    "    SELECT t.track_id FROM " + THREAD_TABLE + " t JOIN " + PROCESS_TABLE + " p ON t.pid = p.pid "
    "    WHERE p.process_name in ('HCCL', 'COMMUNICATION', 'Communication') and t.thread_name like 'Group%' "
    ") AND ( "
    "LOWER(s.name) like 'hcom_send%' or LOWER(s.name) like 'hcom_receive%' or LOWER(s.name) like 'hcom_batchsendrecv%' "
    ") AND s.timestamp >= ? AND s.end_time <= ? ORDER BY s.timestamp ASC";
const std::string QUERY_P2P_COMMUNICATION_OP_DB_SQL =
    "SELECT task.globalPid as pid, 0 as tid, op.startNs - ? as startTime, op.endNs - op.startNs as duration, "
    "str.value as name FROM " + TABLE_COMMUNICATION_OP + " op JOIN " + TABLE_STRING_IDS + " str ON op.opName = str.id "
    "JOIN " + TABLE_TASK + " task ON op.connectionId = task.connectionId AND op.startNs = task.startNs "
    "WHERE (LOWER(str.value) like 'hcom_send%' or LOWER(str.value) like 'hcom_receive%' "
    "or LOWER(str.value) like 'hcom_batchsendrecv%') AND op.startNs >= ? AND op.endNs <= ? ORDER BY op.startNs ASC";

const std::string QUERY_BYTE_ALIGNMENT_ANALYZER_LARGE_OPERATOR_FOR_DB_SQL =
    "SELECT " + TABLE_STRING_IDS + ".value FROM " + TABLE_COMMUNICATION_OP + " INNER JOIN " + TABLE_STRING_IDS +
    " ON " + TABLE_COMMUNICATION_OP + ".opName = " + TABLE_STRING_IDS + ".id WHERE SUBSTR(value, 1, 4) = 'hcom'";
const std::string QUERY_BYTE_ALIGNMENT_ANALYZER_SMALL_OPERATOR_FOR_DB_SQL = "SELECT ID1.value, ID2.value, " +
    TABLE_COMMUNICATION_TASK_INFO + ".size, " + TABLE_ENUM_HCCL_TRANSPORT_TYPE + ".name, " + TABLE_ENUM_HCCL_LINK_TYPE +
    ".name FROM " + TABLE_COMMUNICATION_TASK_INFO + " INNER JOIN " + TABLE_STRING_IDS + " AS ID1 ON " +
    TABLE_COMMUNICATION_TASK_INFO + ".name = ID1.id INNER JOIN " + TABLE_STRING_IDS +
    " AS ID2 ON " + TABLE_COMMUNICATION_TASK_INFO + ".taskType = ID2.id INNER JOIN " +
    TABLE_ENUM_HCCL_TRANSPORT_TYPE + " ON " + TABLE_COMMUNICATION_TASK_INFO + ".transportType = " +
    TABLE_ENUM_HCCL_TRANSPORT_TYPE + ".id INNER JOIN " + TABLE_ENUM_HCCL_LINK_TYPE + " ON " +
    TABLE_COMMUNICATION_TASK_INFO + ".linkType = " + TABLE_ENUM_HCCL_LINK_TYPE +
    ".id WHERE (SUBSTR(ID2.value, 1, 6) = 'Memcpy' OR SUBSTR(ID2.value, 1, 6) = 'Reduce')";

struct ParamsForCOTData {
    uint64_t groupId;
    uint64_t offset;
    uint64_t startTime;
    uint64_t endTime;
    std::string name;
};

struct DbEventViewSqlParams {
    std::string &orderByCondition;
    const std::string &deviceId;
    uint64_t minTimestamp;
    const std::string timeCondSql;
};

class TraceDatabaseHelper {
public:
/* Functions for BbTraceDataBase */
static std::optional<std::string> QueryConnectionId(std::unique_ptr<SqlitePreparedStatement> &stmt,
                                                    const Protocol::UnitFlowsParams &requestParams);

static std::unique_ptr<SqliteResultSet>
QueryThreadsByPid(std::unique_ptr<SqlitePreparedStatement> &stmt, uint64_t startTime, uint64_t endTime,
                  const Dic::Protocol::Metadata &metaData, const std::string &rankId);

static std::unique_ptr<SqliteResultSet> QueryHostUnitCounter(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::UnitCounterParams &requestParams, uint64_t minTimestamp);
static std::unique_ptr<SqliteResultSet> QueryDeviceUnitCounter(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::UnitCounterParams &requestParams, uint64_t minTimestamp, const std::string &rankId);
static std::unique_ptr<SqliteResultSet> QuerySystemViewData(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const Protocol::SystemViewParams &requestParams, const std::string& rankId, const uint64_t &minTimestamp,
    const std::string &timeCondSql);

static std::unique_ptr<SqliteResultSet> QueryThreadTracesSummary(const std::string& rankId, uint64_t minTimestamp,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
static std::vector<uint64_t> GetDeviceIdList(const std::string &fileId);
static bool IsDeviceIdUnique(const std::string &fileId);
static void CalculateSelfTime(std::vector<Protocol::SimpleSlice> &rows,
    std::map<std::string, uint64_t> &selfTimeKeyValue, uint64_t startTime, uint64_t endTime);

static void ReduceThread(const std::vector<Protocol::SimpleSlice> &rows,
const std::map<std::string, uint64_t> &selfTimeKeyValue, Protocol::UnitThreadsBody &responseBody);

static void ReduceThread(const std::vector<CompeteSliceDomain> &rows,
    const std::map<std::string, uint64_t> &selfTimeKeyValue, Protocol::UnitThreadsBody &responseBody);
static void SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr);
static std::string GetLockRangeSql(const Protocol::SearchAllSliceParams &params,
    const std::vector<TrackQuery> &trackQueryVec);
static void SearchAllSliceWithLockRangeBindStmt(const SearchAllSliceParams &params,
    const std::vector<TrackQuery> &trackQueryVec, std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string &deviceId);

static std::string GetSearchSliceNameWithLockRangeSql(const SearchSliceParams &params,
    const std::vector<TrackQuery> &trackQuery, const std::string &path);
static void SearchSliceNameWithLockRangeBindStmt(const SearchSliceParams &params,
    const std::vector<TrackQuery> &trackQuery, std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &path,
    const std::string &deviceId);

    static void SearchCountWithLockRangeBindStmt(const SearchCountParams &params, const std::vector<TrackQuery> &trackQuery,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &deviceId);

    static std::string GetComOpSliceDetailsSql(const std::string &rankId);
static std::string GetMsTxEventsSliceDetailSql();

    static inline std::vector<Protocol::SimpleSlice> ThreadsInfoFilter(const Protocol::UnitThreadsParams &requestParams,
        const std::vector<Protocol::SimpleSlice> &simpleSliceVec, uint64_t startTime, uint64_t endTime)
{
    std::vector<Protocol::SimpleSlice> nRows;
    uint32_t startDepth = NumberUtil::StringToUint32(requestParams.startDepth);
    uint32_t endDepth = NumberUtil::StringToUint32(requestParams.endDepth);
    for (auto &row : simpleSliceVec) {
        if (requestParams.startDepth.empty() && requestParams.endDepth.empty()) {
            if (row.timestamp <= endTime && row.endTime >= startTime) {
                nRows.emplace_back(row);
            }
        } else {
            if (row.timestamp <= endTime && row.endTime >= startTime && row.depth >= startDepth && row.depth <= endDepth) {
                nRows.emplace_back(row);
            }
        }
    }
    return nRows;
}

template <typename... Args>
static inline std::unique_ptr<SqliteResultSet> Execute(std::unique_ptr<SqlitePreparedStatement> &stmt,
                                                       Args&&... args)
{
    stmt->BindParams(std::forward<Args>(args)...);
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        throw DatabaseException("Failed to ExecuteQuery.");
    }
    return result;
};

template <typename... Args>
static inline std::unique_ptr<SqliteResultSet> ExecuteQuery(std::unique_ptr<SqlitePreparedStatement> &stmt,
                                                            const std::string &sql, Args&&... args)
{
    Prepare(stmt, sql);
    return Execute(stmt, std::forward<Args>(args)...);
};

static inline std::unique_ptr<SqlitePreparedStatement>& Prepare(std::unique_ptr<SqlitePreparedStatement> &stmt,
                                                               const std::string &sql)
{
    if (stmt == nullptr) {
        throw DatabaseException("Failed to prepare sql.");
    }
    if (!stmt->Prepare(sql)) {
        throw DatabaseException("Failed to prepare sql.");
    }
    stmt->Reset();
    return stmt;
};

static inline PROCESS_TYPE GetProcessType(const std::string &metaType)
{
    auto processType = STR_TO_ENUM<PROCESS_TYPE>(metaType);
    if (!processType.has_value()) {
        return static_cast<PROCESS_TYPE>(NumberUtil::StringToLong(metaType));
    }
    return processType.value();
}
static std::unique_ptr<SqliteResultSet> QueryThreadSameOperatorsDetails(std::unique_ptr<SqlitePreparedStatement> &stmt,
     const Protocol::UnitThreadsOperatorsParams &requestParams, const QUERY_THREAD_SAME_OPERATORS_PARAMS& params);
static bool QueryEventsViewData4Db(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp,
    const std::string& deviceId);
/* Functions for JsonTraceDataBase */
static bool QueryEventsViewData4Text(std::unique_ptr <SqlitePreparedStatement> &stmt,
    const Protocol::EventsViewParams &params, Protocol::EventsViewBody &body, uint64_t minTimestamp);
static void QueryAllSliceInRangeByTrackIdHelper(std::unique_ptr<SqliteResultSet> &resultSet,
    uint64_t unitTime, uint64_t minTimestamp, Protocol::UnitThreadTracesSummaryBody &responseBody);
static void SetSystemViewHelpler(std::unique_ptr<SqliteResultSet> resultSet, const LayerStatData &data,
    const Protocol::SystemViewParams &requestParams, Protocol::SystemViewBody &responseBody);
static void SetKernelDetailHelpler(std::unique_ptr<SqliteResultSet> resultSet, uint64_t minTimestamp,
                            Protocol::KernelDetailsBody &responseBody);
static void FilterTopLevelApi(std::vector<Protocol::FlowLocation> &originData, const std::set<std::string> &pattern,
    std::vector<Protocol::FlowLocation> &filterData, std::vector<uint32_t> &indexes);
// 内部接口不对外，调用处保证stmt不为空
static bool ExecuteQueryFwdBwdDataByFlow(std::unique_ptr<SqlitePreparedStatement> stmt,
    const std::string &rankId, uint64_t offset, const Protocol::ExtremumTimestamp &range,
    std::vector<Protocol::ThreadTraces> &fwdBwdData);
static bool ExecuteQueryP2POpData(std::unique_ptr<SqlitePreparedStatement> stmt, const std::string &rankId,
    uint64_t offset, const ExtremumTimestamp &range, std::vector<Protocol::ThreadTraces> &p2pOpData);

static void ComputeSummarySlice(std::unique_ptr<SqliteResultSet> &resultSet, uint64_t unitTime,
    UnitThreadTracesSummaryBody &responseBody);
static inline bool IsValidHCCLGroupNameValue(const std::string &groupNameValue)
{
    const std::string regexStr = "[^0-9]";
    const std::regex pattern(regexStr);
    return std::regex_search(groupNameValue, pattern);
}
// 给定一个通信算子或Task，计算其未被通信掩盖部分的耗时
static uint64_t CalculateUncoveredTime(const std::vector<Protocol::ThreadTraces> &uncovered, size_t &index,
                                const Protocol::ThreadTraces &element);
template<class T>
static uint64_t QueryCommunicationGroupIdByName(std::unique_ptr<SqlitePreparedStatement> &stmt,
    const std::string& name, T &deviceId)
{
    auto resultSet = stmt->ExecuteQuery(deviceId);
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for Query Communication Group Id By Name.", stmt->GetErrorMessage());
        return UINT64_MAX;
    }
    while (resultSet->Next()) {
        std::string tmpName = resultSet->GetString("groupName");
        uint64_t groupId = resultSet->GetUint64("groupId");
        auto splitResult = StringUtil::Split(name, ":");
        // 如果包含冒号(:)，使用冒号后的部分进行比较；否则使用整个字符串
        std::string targetName = splitResult.size() > 1 ? splitResult[1] : splitResult[0];
        if (targetName == tmpName) {
            return groupId;
        }
    }

    return UINT64_MAX;
};
template<class T>
static bool QueryCommunicationOpTimeDataByGroupId(std::unique_ptr<SqlitePreparedStatement> &stmt,
    ParamsForCOTData paramsForCotData, T &deviceId, const std::vector<Protocol::ThreadTraces> &notOverlapData,
    std::vector<SameOperatorsDetails> &details)
{
    stmt->BindParams(paramsForCotData.offset, paramsForCotData.offset, deviceId, paramsForCotData.groupId);
    if (!paramsForCotData.name.empty()) {
        std::string pattern = "%" + paramsForCotData.name + "%";
        stmt->BindParams(pattern);
    }
    if (paramsForCotData.startTime != paramsForCotData.endTime) { // time range analysis
        stmt->BindParams(paramsForCotData.startTime + paramsForCotData.offset,
            paramsForCotData.endTime + paramsForCotData.offset);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query communication ops time data.",
                         stmt->GetErrorMessage());
        return false;
    }
    size_t index = 0;
    while (resultSet->Next()) {
        Protocol::ThreadTraces one{};
        one.name = resultSet->GetString("name");
        one.duration = resultSet->GetUint64("duration");
        one.startTime = resultSet->GetUint64("startNs");
        one.endTime = resultSet->GetUint64("endNs");
        if (!notOverlapData.empty()) { // calculate not overlapped time
            uint64_t time = CalculateUncoveredTime(notOverlapData, index, one);
            // 与未掩盖部分无交集，说明此通信算子被掩盖，无需计入数据
            if (time == 0) {
                continue;
            }
        }
        SameOperatorsDetails tmp = {one.startTime, one.duration, "", one.name, 0, ""};
        details.push_back(tmp);
    }

    return true;
};

static void ComputeTree(std::vector<std::unique_ptr<Protocol::UnitTrack>>& metaData, std::vector<Process>& processes,
                        std::vector<std::unique_ptr<Protocol::UnitTrack>>& tempMetaData);

private:
/* Functions for BbTraceDataBase */
    static inline std::unique_ptr<NpuInfoRepo> npuInfoRepo = std::make_unique<NpuInfoRepo>();
    static inline void DealLastData(std::vector<Protocol::SimpleSlice> &rows,
                                    std::map<std::string, uint64_t> &selfTimeKeyValue,
                                    uint64_t startTime, uint64_t endTime, uint64_t index)
    {
        while (++index < rows.size()) {
            if (rows.at(index).timestamp <= endTime && rows.at(index).endTime >= startTime) {
                AddData(selfTimeKeyValue, rows.at(index).name, rows.at(index).duration);
            }
        }
    }

    static inline void AddData(std::map<std::string, uint64_t> &selfTimeKeyValue, const std::string &name,
                               uint64_t tmpSelfTime)
    {
        if (selfTimeKeyValue.find(name) != selfTimeKeyValue.end()) {
            selfTimeKeyValue.at(name) = selfTimeKeyValue.at(name) + tmpSelfTime;
        } else {
            selfTimeKeyValue.emplace(name, tmpSelfTime);
        }
    }

    static std::string GetOrderByCondition(const EventsViewParams &params);
    static std::string GetTextEventViewSql(const Protocol::EventsViewParams &params, const std::string &orderBy);
    static std::string GetSql4QueryEventsViewDetailsInText(const Protocol::EventsViewParams &params);
    static std::string GetSystemViewSqlByLayer(const std::string &layer, const std::string &rankId, const std::string &timeCondSql);
    static std::string GetQueryThreadSameOperatorsDetailsHeadSql(const QUERY_THREAD_SAME_OPERATORS_PARAMS &params,
        bool uniqueDevice, int overlapType, PROCESS_TYPE type);

    static std::string GetSingleSearchNameWithLockRangeSql(const std::string &path, const TrackQuery &singleQuery);

    static std::string GetSingleLockRangeSql(const TrackQuery &item, const std::string &filterJoin = "");

    static void BindSingleTrackStmt(const SearchCountParams &params, std::unique_ptr<SqlitePreparedStatement> &stmt,
        const std::string &deviceId, const TrackQuery &item);

    static void BindSearchAllSliceSingleTrack(std::unique_ptr<SqlitePreparedStatement> &stmt,
        const std::string &deviceId, const TrackQuery &item);

    static void BindSearchNameWithLockRangeStmt(std::unique_ptr<SqlitePreparedStatement> &stmt, const std::string &path,
        const std::string &deviceId, const TrackQuery &item);
    static bool CalculateParallelParameter(const std::vector<Protocol::ThreadTraces> &fwdTraceList,
        const std::vector<Protocol::ThreadTraces> &bwdTraceList,
        uint64_t minBwdStartTime, std::pair<uint16_t, uint16_t> &parameter);
    static std::unique_ptr<SqliteResultSet> QueryProcessTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryLabelTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryHardwareTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryCommunicationTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryOverlapTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryCANNTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryMstxTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
    static std::unique_ptr<SqliteResultSet> QueryProcessUnitTracesSummary(const std::string& rankId, uint64_t minTimestamp,
        std::unique_ptr<SqlitePreparedStatement> &stmt, const Protocol::UnitThreadTracesSummaryParams &requestParams);
};
};
// clang-format on

#endif // PROFILER_SERVER_TRACEDATABASEHELPER_H
