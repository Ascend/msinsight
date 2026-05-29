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
#include "TableDefs.h"
#include "TrackInfoManager.h"
#include "CANNApiRepo.h"
namespace Dic::Module::Timeline {
void CANNApiRepo::QuerySimpleSliceWithOutNameByTrackId(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    std::vector<CANNApiPO> cannApiPOVec;
    cannApiTable->Select(CANNApiColumn::ID, CANNApiColumn::TIMESTAMP, CANNApiColumn::ENDTIME)
        .Eq(CANNApiColumn::TYPE, trackInfo.threadId)
        .Eq(CANNApiColumn::GLOBAL_TID, trackInfo.processId)
        .LessEq(CANNApiColumn::TIMESTAMP, sliceQuery.endTime + sliceQuery.minTimestamp)
        .GreaterEq(CANNApiColumn::ENDTIME, sliceQuery.startTime + sliceQuery.minTimestamp)
        .ExcuteQuery(trackInfo.cardId, cannApiPOVec);
    for (const auto &item : cannApiPOVec) {
        SliceDomain sliceDomain;
        sliceDomain.id = item.id;
        sliceDomain.timestamp = item.timestamp;
        sliceDomain.endTime = item.endTime;
        sliceVec.emplace_back(sliceDomain);
    }
}

void CANNApiRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    const std::string nameKey = cannApiTable->GetDbPath(trackInfo.cardId);
    std::vector<CANNApiPO> cannApiPOVec;
    cannApiTable->Select(CANNApiColumn::ID, CANNApiColumn::TIMESTAMP)
        .Select(CANNApiColumn::ENDTIME, CANNApiColumn::NAME)
        .In(CANNApiColumn::ID, sliceIds)
        .ExcuteQuery(trackInfo.cardId, cannApiPOVec);
    for (const auto &item : cannApiPOVec) {
        CompeteSliceDomain competeSliceDomain;
        competeSliceDomain.id = item.id;
        competeSliceDomain.timestamp = item.timestamp;
        competeSliceDomain.endTime = item.endTime;
        competeSliceDomain.name = FullDb::DbTraceDataBase::GetStringCacheValue(nameKey, std::to_string(item.name));
        competeSliceVec.emplace_back(competeSliceDomain);
    }
}

void CANNApiRepo::SetCANNApiTable(std::unique_ptr<CANNApiTable> cannApiTablePtr) {
    if (cannApiTablePtr != nullptr) {
        cannApiTable = std::move(cannApiTablePtr);
    }
}

bool CANNApiRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    std::vector<CANNApiPO> cannApiPOVec;
    cannApiTable->Select(CANNApiColumn::ID, CANNApiColumn::TIMESTAMP)
        .Select(CANNApiColumn::ENDTIME, CANNApiColumn::NAME)
        .Select(CANNApiColumn::GLOBAL_TID, CANNApiColumn::TYPE)
        .Eq(CANNApiColumn::ID, sliceQuery.sliceId)
        .ExcuteQuery(sliceQuery.rankId, cannApiPOVec);
    if (std::empty(cannApiPOVec)) {
        ServerLog::Warn("Failed to query CANN slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    CANNApiPO target = cannApiPOVec[0];
    competeSliceDomain.id = target.id;
    competeSliceDomain.timestamp = target.timestamp;
    competeSliceDomain.endTime = target.endTime;
    std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap({target.name}, sliceQuery.rankId);
    competeSliceDomain.name = strMap[target.name];
    std::unordered_map<uint64_t, std::string> levelMap = apiTypeTable->QueryStrMap({target.type}, sliceQuery.rankId);
    std::string level = levelMap[target.type];
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, CANNApiColumn::GLOBAL_TID, std::to_string(target.globalTid), allocator);
    JsonUtil::AddConstMember(json, CANNApiColumn::TYPE, level, allocator);
    JsonUtil::AddConstMember(json, CANNApiColumn::NAME, competeSliceDomain.name, allocator);
    JsonUtil::AddConstMember(json, CANNApiColumn::ID, std::to_string(target.id), allocator);
    competeSliceDomain.args = JsonUtil::JsonDump(json);
    return true;
}

bool CANNApiRepo::QuerySliceDetailInfoByNameList(
    const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) {
    // 根据名字查询stringId的内容
    std::unordered_map<uint64_t, std::string> strMap =
        stringIdsTable->QueryStrMapByValues(params.nameList, params.rankId);
    if (strMap.empty()) {
        return false;
    }
    std::vector<uint64_t> stringIds;
    std::transform(strMap.begin(), strMap.end(), std::back_inserter(stringIds),
        [](const std::pair<uint64_t, std::string> &pair) { return pair.first; });
    // 根据stringIds查询算子
    std::vector<CANNApiPO> cannApiPOVec;
    cannApiTable->Select(CANNApiColumn::NAME, CANNApiColumn::TIMESTAMP, CANNApiColumn::ENDTIME)
        .In(CANNApiColumn::NAME, stringIds)
        .OrderBy(CANNApiColumn::TIMESTAMP, TableOrder::ASC)
        .ExcuteQuery(params.rankId, cannApiPOVec);
    for (const auto &item : cannApiPOVec) {
        CompeteSliceDomain domain;
        domain.name = strMap[item.name];
        domain.timestamp = item.timestamp;
        domain.endTime = item.endTime;
        res.push_back(domain);
    }
    return true;
}
}
