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
#include "CcuRepo.h"
#include "DataBaseManager.h"
#include "JsonUtil.h"
#include "TableDefs.h"
#include "TrackInfoManager.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

void CcuRepo::QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Error("CCU query all slice track info does not exist, track is: ", sliceQuery.trackId);
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("CCU open database failed.");
        return;
    }
    std::string sql =
        "SELECT ROWID AS id, startNs, endNs FROM " + TABLE_CCU + " WHERE deviceId = ? AND startNs <= ? AND endNs >= ?";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare CCU query for simple slice.");
        return;
    }
    stmt->BindParams(trackInfo.threadId, sliceQuery.endTime + sliceQuery.minTimestamp,
        sliceQuery.startTime + sliceQuery.minTimestamp);
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute CCU query for simple slice.");
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

void CcuRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("CCU open database failed.");
        return;
    }
    std::string idList = StringUtil::join(sliceIds, ", ");
    std::string sql = "SELECT ccu.ROWID AS id, ccu.startNs, ccu.endNs, nameStr.value AS name FROM " + TABLE_CCU +
        " ccu LEFT JOIN " + TABLE_STRING_IDS + " nameStr ON ccu.name = nameStr.id WHERE ccu.ROWID IN (" + idList + ")";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare CCU query for complete slice by ids.");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute CCU query for complete slice by ids.");
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

bool CcuRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("CCU open database failed.");
        return false;
    }
    std::string sql = "SELECT ccu.ROWID AS id, ccu.deviceId, ccu.globalTaskId, nameStr.value AS name, "
                      "ccu.startNs, ccu.endNs, argsStr.value AS args FROM " +
        TABLE_CCU + " ccu LEFT JOIN " + TABLE_STRING_IDS + " nameStr ON ccu.name = nameStr.id LEFT JOIN " +
        TABLE_STRING_IDS + " argsStr ON ccu.args = argsStr.id WHERE ccu.ROWID = ?";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare CCU query for detail info.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery(sliceQuery.sliceId);
    if (!resultSet) {
        ServerLog::Error("Failed to execute CCU query for detail info.");
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
    JsonUtil::AddConstMember(json, "deviceId", std::to_string(resultSet->GetUint64("deviceId")), allocator);
    JsonUtil::AddConstMember(json, "globalTaskId", std::to_string(resultSet->GetUint64("globalTaskId")), allocator);
    JsonUtil::AddConstMember(json, "name", competeSliceDomain.name, allocator);
    JsonUtil::AddConstMember(json, "startNs", std::to_string(resultSet->GetUint64("startNs")), allocator);
    JsonUtil::AddConstMember(json, "endNs", std::to_string(resultSet->GetUint64("endNs")), allocator);
    MergeArgs(json, resultSet->GetString("args"));
    competeSliceDomain.args = JsonUtil::JsonDump(json);
    return true;
}

void CcuRepo::MergeArgs(document_t &json, const std::string &args) {
    if (args.empty()) {
        return;
    }
    auto &allocator = json.GetAllocator();
    std::string error;
    auto argsJson = JsonUtil::TryParse(args, error);
    if (!argsJson.has_value() || !error.empty() || !argsJson.value().IsArray()) {
        JsonUtil::AddConstMember(json, "args", args, allocator);
        return;
    }
    const auto &argsArray = argsJson.value();
    for (rapidjson::SizeType index = 0; index + 1 < argsArray.Size(); index += 2) {
        const auto &key = argsArray[index];
        if (!key.IsString() || json.HasMember(key.GetString())) {
            continue;
        }
        rapidjson::Value jsonKey;
        jsonKey.SetString(key.GetString(), key.GetStringLength(), allocator);
        rapidjson::Value jsonValue;
        jsonValue.CopyFrom(argsArray[index + 1], allocator);
        json.AddMember(jsonKey, jsonValue, allocator);
    }
}
}
