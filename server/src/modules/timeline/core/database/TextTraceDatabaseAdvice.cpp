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

#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "TraceDatabaseSqlConst.h"
#include "ServerLog.h"
#include "TextTraceDatabase.h"
#include "TextAdviceSqlConstant.h"
#include "TraceDatabaseHelper.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;

void TextTraceDatabase::ProcessByteAlignmentAnalyzerDataForText(
    std::vector<CommunicationLargeOperatorInfo> &result, std::vector<std::pair<std::string, std::string>> rawData) {
    bool hasOneHcom = false;
    CommunicationLargeOperatorInfo op;
    for (const auto &item : rawData) {
        if (item.first.find("hcom") == 0) {
            if (hasOneHcom) {
                result.push_back(op);
            } else {
                hasOneHcom = true;
            }
            op.name = item.first;
            op.memcpyTasks.clear();
            op.reduceInlineTasks.clear();
        } else {
            if (!hasOneHcom) {
                continue;
            }
            std::string err;
            std::optional<document_t> jsonOptional = JsonUtil::TryParse(item.second, err);
            if (jsonOptional == std::nullopt) {
                ServerLog::Error("Failed to parse args. ", err);
                continue;
            }
            document_t &json = jsonOptional.value();
            if (!json.IsObject()) {
                ServerLog::Error("Args is not valid json format. raw: %", item.second);
                continue;
            }
            CommunicationSmallOperatorInfo info;
            int64_t tempSize = NumberUtil::StringToLongLong(JsonUtil::GetString(json, "size(Byte)"));
            info.size = (tempSize < 0 ? 0 : static_cast<uint64_t>(tempSize));
            info.transportType = JsonUtil::GetString(json, "transport type");
            info.linkType = JsonUtil::GetString(json, "link type");
            if (item.first.find("Memcpy") == 0) {
                op.memcpyTasks.emplace_back(info);
            } else {
                op.reduceInlineTasks.emplace_back(info);
            }
        }
    }
    result.push_back(op);
}

bool TextTraceDatabase::QueryAffinityAPIData(const Protocol::KernelDetailsParams &params,
    const std::set<std::string> &pattern, uint64_t minTimestamp,
    std::map<uint64_t, std::vector<Protocol::FlowLocation>> &data, std::map<uint64_t, std::vector<uint32_t>> &indexes) {
    auto stmt = CreatPreparedStatement(TextSqlConstant::GenerateAffinityApiTextSql(params));
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
    std::map<uint64_t, std::vector<Protocol::FlowLocation>> filterData;
    while (resultSet->Next()) {
        Protocol::FlowLocation one{};
        uint64_t trackId = resultSet->GetUint64("track");
        one.id = resultSet->GetString("id");
        one.name = resultSet->GetString("name");
        one.timestamp = resultSet->GetUint64("startTime");
        // Protocol::FlowLocation数据结构中只定义start time和duration，绝大多数场景下也是只用上述两个字段，
        // 此处需要比较start time和end time，是个特例，在不修改数据结构的情况下，duration中实际存的是end time，
        // 过滤顶层API后，在根据end time和start time求出duration
        one.duration = resultSet->GetUint64("endTime");
        one.pid = resultSet->GetString("pid");
        one.tid = resultSet->GetString("tid");
        if (data.count(trackId) == 0) {
            filterData.emplace(trackId, std::vector<Protocol::FlowLocation>{});
            data.emplace(trackId, std::vector<Protocol::FlowLocation>{});
            indexes.emplace(trackId, std::vector<uint32_t>{});
        }
        filterData[trackId].emplace_back(one);
    }
    for (const auto &item : filterData) {
        std::vector<Protocol::FlowLocation> originData = item.second;
        TraceDatabaseHelper::FilterTopLevelApi(originData, pattern, data[item.first], indexes[item.first]);
    }
    return true;
}

bool TextTraceDatabase::QueryAffinityOptimizer(const Protocol::KernelDetailsParams &params,
    const std::string &optimizers, std::vector<Protocol::ThreadTraces> &data, uint64_t minTimestamp) {
    std::string sql = TextSqlConstant::QueryAffinityOptimizerTextSql(optimizers, params);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Fail to prepare sql for query affinity optimizer.", sqlite3_errmsg(db));
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.startTime + minTimestamp, params.endTime + minTimestamp);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query affinity optimizer.", stmt->GetErrorMessage());
        return false;
    }
    while (resultSet->Next()) {
        Protocol::ThreadTraces one{};
        one.id = resultSet->GetString("id");
        one.startTime = resultSet->GetUint64("startTime");
        one.name = resultSet->GetString("name");
        one.duration = resultSet->GetUint64("duration");
        one.threadId = resultSet->GetString("tid");
        one.pid = resultSet->GetString("pid");
        data.emplace_back(one);
    }
    return true;
}

bool TextTraceDatabase::QueryFusibleOpData(const KernelDetailsParams &params,
    const std::vector<Timeline::FuseableOpRule> &rule, Protocol::OperatorFusionResBody &resBody,
    uint64_t minTimestamp) {
    std::string sql = TextAdviceSqlConstant::GenerateFusibleOpFilterTextSql(params, rule);
    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare sql for query Fusionable Operator.");
        return false;
    }
    uint64_t offset = (params.current - 1) * params.pageSize;
    std::unique_ptr<SqliteResultSet> resultSet;
    if (params.startTime == params.endTime) {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.deviceId, params.pageSize, offset);
    } else {
        resultSet = stmt->ExecuteQuery(minTimestamp, params.deviceId, params.startTime + minTimestamp,
            params.endTime + minTimestamp, params.pageSize, offset);
    }
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to get result set for query Fuseable Operator.", stmt->GetErrorMessage());
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
        one.baseInfo.depth = 0;
        one.name = resultSet->GetString("name");
        one.originOpList = resultSet->GetString("originOpList");
        one.fusedOp = resultSet->GetString("fusedOp");
        one.note = "";
        resBody.data.emplace_back(one);
        resBody.size = resultSet->GetUint64("total_count");
    }
    return true;
}
}
