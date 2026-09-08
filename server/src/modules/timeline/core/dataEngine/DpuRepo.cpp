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
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "pch.h"
#include "DpuRepo.h"
#include "DataBaseManager.h"
#include "JsonUtil.h"
#include "TableDefs.h"
#include "TrackInfoManager.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

void DpuRepo::QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    if (!TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId)) {
        ServerLog::Error("DPU query all slice track info does not exist, track is: ", sliceQuery.trackId);
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("DPU open database failed.");
        return;
    }
    const std::string sql = "SELECT ROWID AS id, startNs, endNs FROM " + TABLE_DPU_TASK +
        " WHERE ('DPU_' || CAST(globalTid AS TEXT) || '_' || CAST(dpuDeviceId AS TEXT)) = ? "
        "AND streamId = ? AND startNs <= ? AND endNs >= ?";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare DPU query for simple slice.");
        return;
    }
    stmt->BindParams(trackInfo.processId, trackInfo.threadId, sliceQuery.endTime + sliceQuery.minTimestamp,
        sliceQuery.startTime + sliceQuery.minTimestamp);
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute DPU query for simple slice.");
        return;
    }
    while (resultSet->Next()) {
        SliceDomain sliceDomain;
        sliceDomain.id = resultSet->GetUint64("id");
        sliceDomain.timestamp = resultSet->GetUint64("startNs");
        sliceDomain.endTime = resultSet->GetUint64("endNs");
        sliceVec.emplace_back(sliceDomain);
    }
}

void DpuRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("DPU open database failed.");
        return;
    }
    const std::string idList = StringUtil::join(sliceIds, ", ");
    const std::string sql = "SELECT dpu.ROWID AS id, dpu.startNs, dpu.endNs, "
                            "COALESCE(nameStr.value, CAST(dpu.opName AS TEXT)) AS name FROM " +
        TABLE_DPU_TASK + " dpu LEFT JOIN " + TABLE_STRING_IDS +
        " nameStr ON dpu.opName = nameStr.id WHERE dpu.ROWID IN (" + idList + ")";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare DPU query for complete slice by ids.");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute DPU query for complete slice by ids.");
        return;
    }
    while (resultSet->Next()) {
        CompeteSliceDomain competeSlice;
        competeSlice.id = resultSet->GetUint64("id");
        competeSlice.timestamp = resultSet->GetUint64("startNs");
        competeSlice.endTime = resultSet->GetUint64("endNs");
        competeSlice.name = resultSet->GetString("name");
        competeSliceVec.emplace_back(competeSlice);
    }
}

bool DpuRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("DPU open database failed.");
        return false;
    }
    const std::string sql =
        "SELECT dpu.ROWID AS id, dpu.dpuDeviceId, dpu.globalTid, dpu.globalTaskId, dpu.streamId, dpu.taskId, "
        "COALESCE(nameStr.value, CAST(dpu.opName AS TEXT)) AS name, dpu.startNs, dpu.endNs, "
        "COALESCE(argsStr.value, CAST(dpu.args AS TEXT)) AS args FROM " +
        TABLE_DPU_TASK + " dpu LEFT JOIN " + TABLE_STRING_IDS + " nameStr ON dpu.opName = nameStr.id LEFT JOIN " +
        TABLE_STRING_IDS + " argsStr ON dpu.args = argsStr.id WHERE dpu.ROWID = ?";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare DPU query for detail info.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(sliceQuery.sliceId);
    if (!resultSet) {
        ServerLog::Error("Failed to execute DPU query for detail info.");
        return false;
    }
    if (!resultSet->Next()) {
        return false;
    }
    competeSliceDomain.id = resultSet->GetUint64("id");
    competeSliceDomain.timestamp = resultSet->GetUint64("startNs");
    competeSliceDomain.endTime = resultSet->GetUint64("endNs");
    competeSliceDomain.name = resultSet->GetString("name");

    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, "dpuDeviceId", resultSet->GetString("dpuDeviceId"), allocator);
    JsonUtil::AddConstMember(json, "globalTid", resultSet->GetString("globalTid"), allocator);
    JsonUtil::AddConstMember(json, "globalTaskId", resultSet->GetString("globalTaskId"), allocator);
    JsonUtil::AddConstMember(json, "streamId", resultSet->GetString("streamId"), allocator);
    JsonUtil::AddConstMember(json, "taskId", resultSet->GetString("taskId"), allocator);
    JsonUtil::AddConstMember(json, "name", competeSliceDomain.name, allocator);
    JsonUtil::AddConstMember(json, "startNs", resultSet->GetString("startNs"), allocator);
    JsonUtil::AddConstMember(json, "endNs", resultSet->GetString("endNs"), allocator);
    MergeArgs(json, resultSet->GetString("args"));
    competeSliceDomain.args = JsonUtil::JsonDump(json);
    return true;
}

void DpuRepo::MergeArgs(document_t &json, const std::string &args) {
    if (args.empty()) {
        return;
    }
    auto &allocator = json.GetAllocator();
    std::string error;
    auto argsJson = JsonUtil::TryParse(args, error);
    if (!argsJson.has_value() || !error.empty()) {
        JsonUtil::AddConstMember(json, "args", args, allocator);
        return;
    }
    const auto &parsedArgs = argsJson.value();
    if (parsedArgs.IsObject()) {
        for (auto member = parsedArgs.MemberBegin(); member != parsedArgs.MemberEnd(); ++member) {
            if (json.HasMember(member->name.GetString())) {
                continue;
            }
            rapidjson::Value key;
            key.CopyFrom(member->name, allocator);
            rapidjson::Value value;
            value.CopyFrom(member->value, allocator);
            json.AddMember(key, value, allocator);
        }
        return;
    }
    JsonUtil::AddConstMember(json, "args", args, allocator);
}
}
