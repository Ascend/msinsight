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
#include "TraceDatabaseHelper.h"
#include <cstdint>
#include "CommonDefs.h"
#include "TraceDatabaseSqlConst.h"
#include "DbAdviceSqlConstant.h"
#include "TableDefs.h"
#include "DbTraceDataBase.h"
namespace Dic::Module::FullDb {
using namespace Server;

// Expert System View实现
bool DbTraceDataBase::QueryAffinityOptimizer(const KernelDetailsParams &params, const std::string &optimizers,
    std::vector<ThreadTraces> &data, uint64_t minTimestamp) {
    if (!CheckTableExist(TABLE_API)) {
        ServerLog::Warn("The PYTORCH_API table isn't exist.");
        return false;
    }
    std::string sql = TraceDatabaseSqlConst::QueryAffinityOptimizerDbSql(optimizers, params);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for Query Affinity Optimizer by DB.", sqlite3_errmsg(db));
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for Query Affinity Optimizer by DB.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        ThreadTraces one{};
        one.id = resultSet->GetString("id");
        one.startTime = resultSet->GetUint64("startTime");
        one.name = resultSet->GetString("originOptimizer");
        one.duration = resultSet->GetUint64("duration");
        one.threadId = resultSet->GetString("tid");
        one.pid = resultSet->GetString("pid");
        one.depth = resultSet->GetUint64("depth");
        data.emplace_back(one);
    }
    return true;
}

bool DbTraceDataBase::QueryAICpuOpCanBeOptimized(const KernelDetailsParams &params,
    const std::vector<std::string> &replace, const std::map<std::string, AICpuCheckDataType> &dataType,
    std::vector<KernelBaseInfo> &data, uint64_t minTimestamp) {
    std::string sql = TraceDatabaseSqlConst::GenerateAICpuQueryDbSql(replace, params, dataType);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for AICpuOpCanBeOptimized.");
        return false;
    }
    int deviceId = StringUtil::StringToInt(params.deviceId);
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp, deviceId, AICPU_OP_DURATION_THRESHOLD / THOUSAND);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, deviceId, params.startTime + minTimestamp,
            params.endTime + minTimestamp, AICPU_OP_DURATION_THRESHOLD / THOUSAND);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for AICpuOpCanBeOptimized.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.type = resultSet->GetString("type");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        one.depth = resultSet->GetUint64("depth");
        one.inputType = resultSet->GetString("input");
        one.outputType = resultSet->GetString("output");
        data.emplace_back(one);
    }
    return true;
}

bool DbTraceDataBase::QueryAclnnOpCountExceedThreshold(
    const KernelDetailsParams &params, uint64_t threshold, std::vector<KernelBaseInfo> &data, uint64_t minTimestamp) {
    auto stmt = CreatPreparedStatement(TraceDatabaseSqlConst::GenerateAclnnQueryDbSql(params));
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
        KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        one.depth = resultSet->GetUint64("depth");
        data.emplace_back(one);
    }
    return true;
}

bool DbTraceDataBase::QueryAffinityAPIData(const KernelDetailsParams &params, const std::set<std::string> &pattern,
    uint64_t minTimestamp, std::map<uint64_t, std::vector<FlowLocation>> &data,
    std::map<uint64_t, std::vector<uint32_t>> &indexes) {
    auto stmt = CreatPreparedStatement(TraceDatabaseSqlConst::GenerateAffinityApiDbSql(params));
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for Affinity API.");
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp, minTimestamp);
    } else {
        resultSet = stmt->ExecuteQuery(
            minTimestamp, minTimestamp, params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for Affinity API data.", stmt->GetErrorMessage());
        return false;
    }
    std::map<uint64_t, std::vector<FlowLocation>> filterData;
    while (resultSet->Next()) {
        FlowLocation one{};
        uint64_t trackId = resultSet->GetUint64("pid");
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.timestamp = resultSet->GetUint64("startTime");
        // Protocol::FlowLocation数据结构中只定义start time和duration，绝大多数场景下也是只用上述两个字段，
        // 此处需要比较start time和end time，是个特例，在不修改数据结构的情况下，duration中实际存的是end time，
        // 过滤顶层API后，在根据end time和start time求出duration
        one.duration = resultSet->GetUint64("endTime");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        one.depth = resultSet->GetUint64("depth");

        if (data.count(trackId) == 0) {
            filterData.emplace(trackId, std::vector<FlowLocation>{});
            data.emplace(trackId, std::vector<FlowLocation>{});
            indexes.emplace(trackId, std::vector<uint32_t>{});
        }

        filterData[trackId].emplace_back(one);
    }
    for (const auto &item : filterData) {
        std::vector<FlowLocation> originData = item.second;
        TraceDatabaseHelper::FilterTopLevelApi(originData, pattern, data[item.first], indexes[item.first]);
    }

    return true;
}

bool DbTraceDataBase::QueryFusibleOpData(const KernelDetailsParams &params,
    const std::vector<Timeline::FuseableOpRule> &rule, Protocol::OperatorFusionResBody &resBody,
    uint64_t minTimestamp) {
    std::string sql = DbAdviceSqlConstant::GenerateFusibleOpFilterDbSql(params, rule);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query Fusible Operator.");
        return false;
    }
    return QueryFusibleOpDataForDB(params, stmt, rule, resBody, minTimestamp);
}

bool DbTraceDataBase::QueryOperatorDispatchData(
    const KernelDetailsParams &params, std::vector<KernelBaseInfo> &data, uint64_t minTimestamp, uint64_t threshold) {
    auto stmt = CreatPreparedStatement(TraceDatabaseSqlConst::GenerateOperatorDispatchQueryDbSql(params));
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for Operator Dispatch data.");
        return false;
    }
    return QueryOpDispatchDataForDB(stmt, minTimestamp, params, threshold, data);
}

bool DbTraceDataBase::QueryFwdBwdDataByFlow(
    const std::string &rankId, uint64_t offset, const ExtremumTimestamp &range, std::vector<ThreadTraces> &fwdBwdData) {
    std::vector<std::string> tableList = {TABLE_API, TABLE_CONNECTION_CATS, TABLE_CONNECTION_IDS, TABLE_ENUM_API_TYPE};
    if (!CheckTablesExist(tableList)) {
        ServerLog::Error("Failed to check dependent table for query fwdbwd data in the DB scenario.");
        return false;
    }
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!ExecSql(CREATE_TEMP_FWDBWD_FLOW_TABLE_DB_SQL)) {
        ServerLog::Error("Failed to create temp fwdbwd table in the DB scenario.");
        return false;
    }
    auto stmt = CreatPreparedStatement(QUERY_FWDBWD_FLOW_DATA_SQL);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query fwd/bwd data by flow in the DB scenario.");
        return false;
    }
    return TraceDatabaseHelper::ExecuteQueryFwdBwdDataByFlow(std::move(stmt), rankId, offset, range, fwdBwdData);
}

bool DbTraceDataBase::QueryP2PCommunicationOpData(
    const std::string &rankId, uint64_t offset, const ExtremumTimestamp &range, std::vector<ThreadTraces> &p2pOpData) {
    auto stmt = CreatPreparedStatement(QUERY_P2P_COMMUNICATION_OP_DB_SQL);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query p2p communication op data in the DB scenario.");
        return false;
    }
    return TraceDatabaseHelper::ExecuteQueryP2POpData(std::move(stmt), rankId, offset, range, p2pOpData);
}

bool DbTraceDataBase::QueryByteAlignmentAnalyzerData(std::vector<CommunicationLargeOperatorInfo> &data) {
    std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> largeOpInfo;
    std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> smallOpInfo;
    QueryByteAlignmentAnalyzerRawData(largeOpInfo, smallOpInfo);
    ProcessByteAlignmentAnalyzerDataForDb(data, largeOpInfo, smallOpInfo);
    return true;
}

bool DbTraceDataBase::QueryByteAlignmentAnalyzerRawData(
    std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> &largeOpInfo,
    std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> &smallOpInfo) {
    std::string sqlForLargeOp = QUERY_BYTE_ALIGNMENT_ANALYZER_LARGE_OPERATOR_FOR_DB_SQL;
    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sqlForLargeOp.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare sql for query byte alignment analyzer large operator data. Error: ", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        ByteAlignmentAnalyzerLargeOperatorInfo item;
        item.name = sqlite3_column_string(stmt, col++);
        largeOpInfo.emplace_back(item);
    }
    sqlite3_finalize(stmt);

    std::string sqlForSmallOp = QUERY_BYTE_ALIGNMENT_ANALYZER_SMALL_OPERATOR_FOR_DB_SQL;
    sqlite3_stmt *stmt2 = nullptr;
    int result2 = sqlite3_prepare_v2(db, sqlForSmallOp.c_str(), -1, &stmt2, nullptr);
    if (result2 != SQLITE_OK) {
        ServerLog::Error(
            "Failed to prepare sql for query byte alignment analyzer small operator data. Error: ", sqlite3_errmsg(db));
        return false;
    }
    while (sqlite3_step(stmt2) == SQLITE_ROW) {
        int col = resultStartIndex;
        ByteAlignmentAnalyzerSmallOperatorInfo item;
        item.name = sqlite3_column_string(stmt2, col++);
        item.taskType = sqlite3_column_string(stmt2, col++);
        int64_t tempSize = sqlite3_column_int64(stmt2, col++);
        if (tempSize < 0) {
            item.size = 0;
        } else {
            item.size = static_cast<uint64_t>(tempSize);
        }
        item.transportType = sqlite3_column_string(stmt2, col++);
        item.linkType = sqlite3_column_string(stmt2, col++);
        smallOpInfo.emplace_back(item);
    }
    sqlite3_finalize(stmt2);
    return true;
}

bool DbTraceDataBase::QueryFwdBwdFromMstx(std::vector<ThreadTraces> &traceList) {
    std::string sql = "Select name, startNs, endNs, type from " + TABLE_STEP_TASK_INFO + " order by startNs";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query fwd/bwd data from mstx in the DB scenario.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query fwd/bwd data from mstx.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        std::string name = resultSet->GetString("name");
        uint64_t startNs = resultSet->GetUint64("startNs");
        uint64_t endNs = resultSet->GetUint64("endNs");
        std::string type = std::to_string(resultSet->GetUint64("type"));
        ThreadTraces trace = {name, endNs - startNs, startNs, endNs, 0, LANE_FP_BP, "", "", type};
        traceList.push_back(trace);
    }
    return true;
}

bool DbTraceDataBase::QueryP2PCommunicationOpHaveConnectionId(std::vector<ThreadTraces> &traceList) {
    std::string sql = "select str.value as name, op.startNs, op.endNs, op.opConnectionId from " +
        TABLE_COMMUNICATION_OP + " as op LEFT JOIN " + TABLE_STRING_IDS +
        " as str ON str.id = op.opName WHERE LOWER(name) LIKE '%send%' OR LOWER(name) LIKE '%receive%'";
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query communication op data "
                         "have connection id in the DB scenario.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error(
            "Failed to get result set for query communication op data have connection id in the DB scenario.",
            stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        ThreadTraces trace;
        trace.name = resultSet->GetString("name");
        trace.startTime = resultSet->GetUint64("startNs");
        trace.endTime = resultSet->GetUint64("endNs");
        trace.duration = trace.endTime - trace.startTime;
        if (StringUtil::StartWith(trace.name, "hcom_send")) {
            trace.cname = MARKER_SEND;
        } else if (StringUtil::StartWith(trace.name, "hcom_receive")) {
            trace.cname = MARKER_RECV;
        } else {
            trace.cname = MARKER_BATCH_SEND_RECV;
        }
        trace.opConnectionId = resultSet->GetString("opConnectionId");
        traceList.push_back(trace);
    }
    return true;
}

bool DbTraceDataBase::QueryFusibleOpDataForDB(const KernelDetailsParams &params,
    std::unique_ptr<SqlitePreparedStatement> &stmt, const std::vector<Timeline::FuseableOpRule> &rule,
    Protocol::OperatorFusionResBody &resBody, uint64_t minTimestamp) {
    int deviceId = StringUtil::StringToInt(params.deviceId);
    uint64_t offset = (params.current - 1) * params.pageSize;
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) { // default request, not time range analysis
        resultSet = stmt->ExecuteQuery(minTimestamp, deviceId, params.pageSize, offset);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, deviceId, params.startTime + minTimestamp,
            params.endTime + minTimestamp, params.pageSize, offset);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query Fusible Operator.", stmt->GetErrorMessage());
        return false;
    }

    while (resultSet->Next()) {
        Protocol::OperatorFusionData one{};
        one.baseInfo.id = resultSet->GetString("id");
        one.baseInfo.rankId = params.rankId;
        one.baseInfo.startTime = resultSet->GetUint64("startTime");
        one.baseInfo.duration = resultSet->GetUint64("duration");
        one.baseInfo.pid = resultSet->GetString("pid");
        one.baseInfo.tid = resultSet->GetString("tid");
        one.baseInfo.depth = resultSet->GetUint64("depth");
        one.name = resultSet->GetString("name");
        one.originOpList = resultSet->GetString("originOpList");
        one.fusedOp = resultSet->GetString("fusedOp");
        one.note = "";
        resBody.data.emplace_back(one);
        resBody.size = resultSet->GetUint64("total_count");
    }

    return true;
}

bool DbTraceDataBase::QueryOpDispatchDataForDB(std::unique_ptr<SqlitePreparedStatement> &stmt, uint64_t minTimestamp,
    const KernelDetailsParams &params, uint64_t threshold, std::vector<KernelBaseInfo> &data) {
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
        KernelBaseInfo one{};
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.startTime = resultSet->GetUint64("startTime");
        one.duration = resultSet->GetUint64("duration");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        one.depth = resultSet->GetUint64("depth");
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

void DbTraceDataBase::ProcessByteAlignmentAnalyzerDataForDb(std::vector<CommunicationLargeOperatorInfo> &result,
    std::vector<ByteAlignmentAnalyzerLargeOperatorInfo> &largeOpInfo,
    std::vector<ByteAlignmentAnalyzerSmallOperatorInfo> &smallOpInfo) {
    std::map<std::string, CommunicationLargeOperatorInfo> resultMap;
    for (const auto &singleLargeOp : largeOpInfo) {
        CommunicationLargeOperatorInfo info;
        info.name = singleLargeOp.name;
        resultMap[singleLargeOp.name] = info;
    }
    for (const auto &singleSmallOp : smallOpInfo) {
        if (resultMap.find(singleSmallOp.name) == resultMap.end()) {
            continue;
        }
        CommunicationSmallOperatorInfo smallOpInfo;
        smallOpInfo.size = singleSmallOp.size;
        smallOpInfo.transportType = singleSmallOp.transportType;
        smallOpInfo.linkType = singleSmallOp.linkType;
        if (singleSmallOp.taskType.find("Memcpy") == 0) {
            resultMap[singleSmallOp.name].memcpyTasks.emplace_back(smallOpInfo);
        } else {
            resultMap[singleSmallOp.name].reduceInlineTasks.emplace_back(smallOpInfo);
        }
    }
    for (const auto &item : resultMap) {
        result.emplace_back(item.second);
    }
}
}
