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
#include "StringUtil.h"
#include "TrackInfoManager.h"
#include "TraceDatabaseHelper.h"
#include "TraceDatabaseSqlConst.h"
#include "SystemViewOverallDbRepo.h"
#include "TraceTime.h"

using namespace Dic::Protocol;
using namespace Dic::Module;
namespace Dic::Module::Timeline {
void SystemViewOverallDbRepo::UpdateStringCacheValue(
    const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &path) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto sql = "select id, value from STRING_IDS";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Update strings cache value. Failed to prepare sql.");
        return;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Error("Update strings cache value. Failed to get result set.", stmt->GetErrorMessage());
        return;
    }
    while (result->Next()) {
        stringsCache[path].emplace(result->GetString("id"), result->GetString("value"));
    }
}
std::string SystemViewOverallDbRepo::GetOrUpdateStringCacheValue(
    const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &path, const std::string &key) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (stringsCache.find(path) == stringsCache.end()) {
        UpdateStringCacheValue(database, path);
    }
    if (stringsCache[path].find(key) == stringsCache[path].end()) {
        ServerLog::Warn("Get strings cache value. Failed to get db string value by key.");
        return "";
    }
    return stringsCache[path][key];
}

std::vector<OverallTmpInfo> SystemViewOverallDbRepo::QueryOverlapAnalysisDataForOverallMetric(
    const Protocol::SystemViewOverallReqParam &requestParams, const std::shared_ptr<VirtualTraceDatabase> &database) {
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const std::string timeCondSql =
        TraceDatabaseSqlConst::AppendDbTimeRangeConditionSql(requestParams.startTime, requestParams.endTime);
    /**
     * Db场景中Overlap Analysis, type = 0 代表 Computing Time, type = 1 代表 Communication Time（此处未选择）,
     * type = 2 代表 Communication(Not Overlapped), type = 3 代表 Free Time。
     */
    std::string sql = " select case type when 0 then 'Computing' "
                      "    when 2 then 'Communication(Not Overlapped)' "
                      "    when 3 then 'Free' end as category, "
                      "    round(sum(endNs - startNs)/1000.0, 2) as duration "
                      "from OVERLAP_ANALYSIS where type != 1 and deviceId = ? " +
        timeCondSql + " group by type order by category;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql while querying overlap analysis data for overall metrics.");
        return {};
    }
    stmt->BindParams(StringUtil::StringToInt(requestParams.deviceId));
    if (requestParams.startTime != requestParams.endTime) { // time range analysis
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query while querying overlap analysis data for overall metrics.");
        return {};
    }
    std::vector<OverallTmpInfo> overlapInfos;
    while (resultSet->Next()) {
        OverallTmpInfo tmpInfo;
        tmpInfo.categoryList.push_back(resultSet->GetString("category"));
        tmpInfo.duration = resultSet->GetDouble("duration");
        overlapInfos.push_back(tmpInfo);
    }
    return overlapInfos;
}

bool SystemViewOverallDbRepo::QueryDataForComputingOverallMetric(
    const Protocol::SystemViewOverallReqParam &requestParams, SystemViewOverallHelper &computeHelper,
    const std::shared_ptr<VirtualTraceDatabase> &database) {
    // 检查是否存在表TASK_PMU_INFO, 列aiv_vec_time或列mac_time，若不存在，则日志报警并直接返回No data。
    if (!CheckDataForSystemViewOverall(database)) {
        return true;
    }
    if (!GetTmpTableForOverall(database)) {
        return false;
    }
    int deviceId = StringUtil::StringToInt(requestParams.deviceId);
    // <key: flow end time, value: flow start time>
    std::map<uint64_t, uint64_t> flowDict = QueryFlowDict(requestParams, database, deviceId);
    computeHelper.cpuCubeOps = QueryCpuCubeOp(requestParams, database);
    computeHelper.kernelEvents = QueryKernelEventsForSystemViewOverall(requestParams, database, flowDict, deviceId);

    // 查询backward track id并分类kernelEvents，无需时间范围过滤
    QueryBwdTrackIdForComputingOverall(database, computeHelper.bwdTrackId);
    return true;
}

bool SystemViewOverallDbRepo::CheckDataForSystemViewOverall(const std::shared_ptr<VirtualTraceDatabase> &database) {
    if (!database->CheckTableExist(TABLE_TASK_PMU_INFO)) {
        ServerLog::Warn(
            "Missing key table while querying computing data in system view overall. Can't find ", TABLE_TASK_PMU_INFO);
        return false;
    }
    if (database->CheckStringInColumn(TABLE_STRING_IDS, "value", "aiv_vec_time")) {
        return true;
    }
    if (database->CheckStringInColumn(TABLE_STRING_IDS, "value", "mac_time")) {
        database->hasMacTime = true;
        return true;
    }
    ServerLog::Warn("Missing key columns while querying computing data in system view overall. Please ensure "
                    "that the profiling data is set to level 1 or higher and aic_metrics is set to PipeUtilization.");
    return false;
}

bool SystemViewOverallDbRepo::GetTmpTableForOverall(const std::shared_ptr<VirtualTraceDatabase> &database) {
    std::string creatPmuSql =
        " CREATE temporary table tmpPmu as select globalTaskId, SUM(tpi.value) as cubeTime from TASK_PMU_INFO tpi "
        " left join STRING_IDS pmuSi on tpi.name = pmuSi.id where pmuSi.value in ";
    if (!database->hasMacTime) {
        creatPmuSql += " ('aic_mac_time', 'aic_total_time') group by globalTaskId; ";
    } else {
        creatPmuSql += " ('mac_time', 'aic_total_time') group by globalTaskId; ";
    }
    std::vector<std::string> createTmpTable = {"DROP TABLE IF EXISTS tmpPmu;", creatPmuSql,
        " DROP TABLE IF EXISTS asyncNpuConnect; ",
        " CREATE temporary table asyncNpuConnect as select id, ci.connectionId from CONNECTION_IDS ci "
        " join connectionCats cc on ci.connectionId = cc.connectionId where cat = 'async_npu'; ",
        " DROP TABLE IF EXISTS fwdbwdConnect; ",
        " create temporary table fwdbwdConnect as select * from connectionCats cCats "
        " where cCats.cat = 'fwdbwd' limit 1;"};
    if (!std::all_of(createTmpTable.begin(), createTmpTable.end(),
            [&](const auto &query) { return database->ExecSql(query); })) {
        ServerLog::Error("Failed to create temp table for system view overall.");
        return false;
    }
    return true;
}

std::map<uint64_t, uint64_t> SystemViewOverallDbRepo::QueryFlowDict(
    const Protocol::SystemViewOverallReqParam &requestParams, const std::shared_ptr<VirtualTraceDatabase> &database,
    int deviceId) {
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string timeCondSql;
    if (requestParams.startTime != requestParams.endTime) {
        timeCondSql += " AND t.startNs >= ? AND pa.startNs <= ? ";
    }
    std::string sql = "select t.startNs as flowEnd, pa.startNs as flowStart from TASK t join "
                      " asyncNpuConnect asyncConn on asyncConn.connectionId = t.connectionId join PYTORCH_API pa on "
                      "pa.connectionId = asyncConn.id "
                      " where t.deviceId = ? " +
        timeCondSql + " ;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql while querying flow dictionary for system view overall.");
        return {};
    }
    stmt->BindParams(deviceId);
    if (requestParams.startTime != requestParams.endTime) { // time range analysis
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query while querying flow dictionary for system view overall.");
        return {};
    }
    std::map<uint64_t, uint64_t> flowDict;
    while (resultSet->Next()) {
        flowDict[resultSet->GetUint64("flowEnd")] = resultSet->GetUint64("flowStart");
    }
    return flowDict;
}

std::vector<CpuCubeOpInfo> SystemViewOverallDbRepo::QueryCpuCubeOp(
    const Protocol::SystemViewOverallReqParam &requestParams, const std::shared_ptr<VirtualTraceDatabase> &database) {
    if (!database->CheckTableExist(TABLE_API)) {
        ServerLog::Warn("Skip query cpu cube operators for system view overall. Can't find ", TABLE_API);
        return {};
    }
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string timeCondSql;
    if (requestParams.startTime != requestParams.endTime) {
        timeCondSql += " AND pa.endNs >= ? AND pa.startNs <= ? ";
    }
    std::string sql =
        "select pa.startNs as start, pa.endNs as end, pa.name, pa.globalTid as "
        " track_id from PYTORCH_API pa join ENUM_API_TYPE apiT on pa.type = apiT.id where apiT.name = 'op' " +
        timeCondSql + " ;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql while querying cpu cube operators for system view overall.");
        return {};
    }
    if (requestParams.startTime != requestParams.endTime) { // time range analysis
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query while querying cpu cube operators for system view overall.");
        return {};
    }
    std::vector<CpuCubeOpInfo> cpuCubeOps;
    while (resultSet->Next()) {
        CpuCubeOpInfo cubeOp;
        cubeOp.pythonApi = GetOrUpdateStringCacheValue(database, database->GetDbPath(), resultSet->GetString("name"));
        if (cubeOp.pythonApi.empty()) {
            ServerLog::Warn("Get empty python api when query cpu cube operators for system view overall. name: %",
                resultSet->GetString("name"));
        }
        cubeOp.CheckCubeOp();
        if (cubeOp.isCubeOp) {
            cubeOp.start = resultSet->GetUint64("start");
            cubeOp.end = resultSet->GetUint64("end");
            cubeOp.trackId = resultSet->GetUint64("track_id");
            cpuCubeOps.push_back(cubeOp);
        }
    }
    return cpuCubeOps;
}

std::vector<OverallTmpInfo> SystemViewOverallDbRepo::QueryKernelEventsForSystemViewOverall(
    const Protocol::SystemViewOverallReqParam &requestParams, const std::shared_ptr<VirtualTraceDatabase> &database,
    const std::map<uint64_t, uint64_t> &flowDict, int deviceId) {
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string timeCondSql;
    if (requestParams.startTime != requestParams.endTime) {
        timeCondSql += " AND t.endNs >= ? AND t.startNs <= ? ";
    }
    std::string sql =
        "select t.rowid as opId, depth, cti.name as opName, cti.opType, t.startNs as startTime, "
        " round((t.endNs - t.startNs)/1000.0, 2) as duration, cubeTime from TASK t join COMPUTE_TASK_INFO cti on "
        " cti.globalTaskId = t.globalTaskId join tmpPmu pmu on pmu.globalTaskId = t.globalTaskId "
        " where t.deviceId = ? " +
        timeCondSql + " ;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql while querying kernel events for system view overall.");
        return {};
    }
    stmt->BindParams(deviceId);
    if (requestParams.startTime != requestParams.endTime) { // time range analysis
        stmt->BindParams(requestParams.startTime + minTimestamp, requestParams.endTime + minTimestamp);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query while querying kernel events for system view overall.");
        return {};
    }
    std::vector<OverallTmpInfo> kernelEvents;
    while (resultSet->Next()) {
        OverallTmpInfo kernelEvent;
        kernelEvent.opName =
            GetOrUpdateStringCacheValue(database, database->GetDbPath(), resultSet->GetString("opName"));
        kernelEvent.opType =
            GetOrUpdateStringCacheValue(database, database->GetDbPath(), resultSet->GetString("opType"));
        if (kernelEvent.opName.empty() || kernelEvent.opType.empty()) {
            Server::ServerLog::Warn("Get empty operator name or type when query kernel events for system view overall."
                                    " opName: %, opType: %",
                resultSet->GetString("opName"), resultSet->GetString("opType"));
        }
        kernelEvent.startTime = resultSet->GetUint64("startTime");
        auto it = flowDict.find(kernelEvent.startTime);
        if (it != flowDict.end()) {
            kernelEvent.flowStartTime = it->second;
        }
        kernelEvent.duration = resultSet->GetDouble("duration");
        kernelEvent.cubeTime = resultSet->GetDouble("cubeTime");
        kernelEvents.push_back(kernelEvent);
    }
    // 按flow start time升序排序
    sort(kernelEvents.begin(), kernelEvents.end());
    return kernelEvents;
}

void SystemViewOverallDbRepo::QueryBwdTrackIdForComputingOverall(
    const std::shared_ptr<VirtualTraceDatabase> &database, uint64_t &bwdTrackId) {
    // 查询backward track id
    std::string sql =
        "select pa.startNs, pa.globalTid as track_id from PYTORCH_API pa join CONNECTION_IDS ci on pa.connectionId "
        " = ci.id join fwdbwdConnect fbc on fbc.connectionId = ci.connectionId order by pa.startNs desc limit 1;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql while querying backward track id for system view overall.");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute query while querying backward track id for system view overall.");
        return;
    }
    while (resultSet->Next()) {
        bwdTrackId = resultSet->GetUint64("track_id");
    }
}
void SystemViewOverallDbRepo::QueryCommunicationOverlapOverallInfos(
    const Protocol::SystemViewOverallReqParam &requestParams, SystemViewOverallHelper &overallHelper,
    std::vector<Protocol::SystemViewOverallRes> &responseBody, const std::shared_ptr<VirtualTraceDatabase> &database) {
    if (!database->CheckTableExist(TABLE_OVERLAP_ANALYSIS) ||
        !database->CheckTableExist(TABLE_COMMUNICATION_TASK_INFO)) {
        ServerLog::Error("Failed to query communication overlap overall info due to no table.");
        return;
    }
    // 查询通信未掩盖数据
    std::vector<Protocol::ThreadTraces> uncovered{};
    uint64_t totalTime = 0;
    int deviceId = StringUtil::StringToInt(requestParams.deviceId);
    ParamsForOAData paramsForOaData = {TraceDatabaseSqlConst::GetOverlapAnalysisDbSqlByType(requestParams), "2",
        TraceTime::Instance().GetStartTime(), requestParams.startTime, requestParams.endTime};
    // "2" for not overlap
    if (!database->QueryOverlapAnalysisData(paramsForOaData, deviceId, uncovered, totalTime)) { // 2
        return; // QueryOverlapAnalysisData has all needed log
    }
    auto it = std::find_if(responseBody.begin(), responseBody.end(),
        [](const Protocol::SystemViewOverallRes &item) { return item.name == COMMUNICATION_NOT_OVERLAP_TIME; });
    if (it == responseBody.end()) {
        double ratio = 0.0;
        double notOverlapTime = totalTime * NS_TO_US;
        if (overallHelper.e2eTime != 0.0) {
            ratio =
                NumberUtil::DoubleReservedNDigits(notOverlapTime / overallHelper.e2eTime * PERCENTAGE_RATIO_SCALE, TWO);
        }
        Protocol::SystemViewOverallRes notOverlapped = {.totalTime = notOverlapTime,
            .ratio = ratio,
            .nums = 0,
            .avg = 0,
            .max = 0,
            .min = 0,
            .name = COMMUNICATION_NOT_OVERLAP_TIME,
            .children = {},
            .level = 1, // level 1
            .id = std::to_string(overallHelper.idCounter++)};
        responseBody.emplace_back(notOverlapped);
    }
    BindParamsForGMAndCS bindParamsForGmAndCs = {deviceId, overallHelper, requestParams};
    QueryGroupMapAndCalculateSummary(database, responseBody, it, uncovered, bindParamsForGmAndCs);
}
void SystemViewOverallDbRepo::QueryGroupMapAndCalculateSummary(const std::shared_ptr<VirtualTraceDatabase> &database,
    std::vector<Protocol::SystemViewOverallRes> &responseBody, std::vector<Protocol::SystemViewOverallRes>::iterator it,
    const std::vector<Protocol::ThreadTraces> &uncovered, BindParamsForGMAndCS bindParamsForGmAndCs) {
    // 查询泳道与通信Group间的对应关系，无需时间范围过滤
    std::map<std::string, std::string> groupMap{};
    std::string groupMapSql;
    if (database->GetMetaVersion() == "1.0.0") {
        groupMapSql = QUERY_COMMUNICATION_GROUP_MAP_DB_1_0_SQL;
    } else {
        groupMapSql = QUERY_COMMUNICATION_GROUP_MAP_DB_SQL;
    }
    if (!database->QueryCommunicationGroupMap(groupMapSql, bindParamsForGmAndCs.deviceId, groupMap)) {
        return;
    }

    // 查询HCCL层通信算子和通信Task，并与通信未掩盖数据取交集，对于通信Task进一步区分wait和Transmit
    std::string commSummarySql4Db;
    if (database->GetMetaVersion() == "1.0.0") {
        commSummarySql4Db = QUERY_COMMUNICATION_SUMMARY_DB_1_0_SQL;
    } else {
        commSummarySql4Db = QUERY_COMMUNICATION_SUMMARY_DB_SQL;
    }
    std::string sql4Summary = TraceDatabaseSqlConst::GeneratorCommunicationSummarySql4Db(
        bindParamsForGmAndCs.requestParams, commSummarySql4Db);
    // 如果不存在上文就会添加，因此此处一定能找到
    it = std::find_if(responseBody.begin(), responseBody.end(),
        [](const Protocol::SystemViewOverallRes &item) { return item.name == COMMUNICATION_NOT_OVERLAP_TIME; });
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    ParamsForCalCSData paramsForCalCsData = {sql4Summary, bindParamsForGmAndCs.overallHelper, minTimestamp,
        bindParamsForGmAndCs.requestParams.startTime, bindParamsForGmAndCs.requestParams.endTime};
    database->CalculateCommunicationSummaryData(
        uncovered, groupMap, paramsForCalCsData, bindParamsForGmAndCs.deviceId, *it);
}
bool SystemViewOverallDbRepo::QueryCommunicationOpsTimeDataByGroupName(const SystemViewOverallReqParam &params,
    uint64_t offset, const std::vector<Protocol::ThreadTraces> &notOverlapData,
    std::vector<SameOperatorsDetails> &opsDetails, const std::shared_ptr<VirtualTraceDatabase> &database) {
    std::vector<std::string> tables = {TABLE_COMMUNICATION_OP, TABLE_STRING_IDS, TABLE_META_DATA};
    if (!database->CheckTablesExist(tables)) {
        ServerLog::Error("Failed to check tables for Query Communication Ops Time Data By Group Name.");
        return false;
    }
    if (!database->QueryMetaVersion()) {
        return false;
    }
    // 此处为预先查询分组，无需时间范围过滤
    std::string sql;
    if (database->GetMetaVersion() == "1.0.0") {
        sql = QUERY_COMMUNICATION_GROUP_ID_DB_1_0_SQL;
    } else {
        sql = QUERY_COMMUNICATION_GROUP_ID_DB_SQL;
    }
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for Query Communication Group Id By Name.");
        return false;
    }
    int deviceId = StringUtil::StringToInt(params.deviceId);
    uint64_t groupId = TraceDatabaseHelper::QueryCommunicationGroupIdByName(stmt, params.categoryList[1], deviceId);
    if (groupId == UINT64_MAX) {
        ServerLog::Error(
            "Group Name doesn't exist for Query Communication Ops Time Data By Group Name: %", params.categoryList[1]);
        return false;
    }
    auto stmt2 = database->CreatPreparedStatement(TraceDatabaseSqlConst::GetCommunicationOpDbSqlByGroupId(params));
    if (stmt2 == nullptr) {
        ServerLog::Error("Failed to prepare sql for query communication ops time data for db scene.");
        return false;
    }
    ParamsForCOTData paramsForCotData = {groupId, offset, params.startTime, params.endTime, params.name};
    if (!TraceDatabaseHelper::QueryCommunicationOpTimeDataByGroupId(
            stmt2, paramsForCotData, deviceId, notOverlapData, opsDetails)) {
        ServerLog::Error(
            "Failed to query data for Query Communication Ops Time Data By Group Name: ", params.categoryList[1]);
        return false;
    }
    return true;
}
}
