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
#include "DataBaseManager.h"
#include "MstxRepo.h"
namespace Dic::Module::Timeline {
void MstxRepo::QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Warn("mstx query all slice track info is not exist, track is: ", sliceQuery.trackId);
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (database == nullptr) {
        ServerLog::Warn("mstx open database is failed");
        return;
    }
    std::string sql = "SELECT ROWID as id, startNs, endNs from " + TABLE_MSTX_EVENTS +
        " where globalTid = ? and domainId = ? "
        " AND startNs <= ? AND endNs >= ? order by startNs , id";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to parpare mstx query all slice");
        return;
    }
    stmt->BindParams(trackInfo.processId, trackInfo.threadId, sliceQuery.endTime + sliceQuery.minTimestamp,
        sliceQuery.startTime + sliceQuery.minTimestamp);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("Failed to execute query mstx query all slice");
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

void MstxRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    std::string sql = "select message as name, mstx.ROWID as id, startNs, endNs "
                      " from " +
        TABLE_MSTX_EVENTS +
        "  mstx  "
        "    where 1 = 1 and id in (";
    std::string sliceidvecStr = StringUtil::join(sliceIds, ", ");
    sql += sliceidvecStr + ");";
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (database == nullptr) {
        ServerLog::Warn("mstx open database is failed");
        return;
    }
    const std::string nameKey = database->GetDbPath();
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to parpare mstx query slice by ids");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("mstx query slice by ids is failed! ");
        return;
    }
    while (resultSet->Next()) {
        CompeteSliceDomain competeSlice;
        competeSlice.id = resultSet->GetUint64("id");
        competeSlice.timestamp = resultSet->GetUint64("startNs");
        competeSlice.endTime = resultSet->GetUint64("endNs");
        competeSlice.name = FullDb::DbTraceDataBase::GetStringCacheValue(nameKey, resultSet->GetString("name"));
        competeSliceVec.emplace_back(competeSlice);
    }
}

bool MstxRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    std::vector<MstxEventsPO> mstxPOs;
    mstxEventsTable->Select(MstxEventsColumn::ID, MstxEventsColumn::TIMESTAMP)
        .Select(MstxEventsColumn::ENDTIME, MstxEventsColumn::MESSAGE)
        .Select(MstxEventsColumn::EVENT_TYPE)
        .Eq(MstxEventsColumn::ID, sliceQuery.sliceId)
        .ExcuteQuery(sliceQuery.rankId, mstxPOs);
    if (std::empty(mstxPOs)) {
        ServerLog::Warn("Failed to query mstx slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    competeSliceDomain.id = mstxPOs[0].id;
    competeSliceDomain.timestamp = mstxPOs[0].timestamp;
    competeSliceDomain.endTime = mstxPOs[0].endTime;
    std::unordered_map<uint64_t, std::string> strMap =
        stringIdsTable->QueryStrMap({mstxPOs[0].message}, sliceQuery.rankId);
    competeSliceDomain.name = strMap[mstxPOs[0].message];
    std::unordered_map<uint64_t, std::string> mstxTypeMap =
        enumMstxEventTypeTable->QueryStrMap({mstxPOs[0].eventType}, sliceQuery.rankId);
    const std::string typeName = mstxTypeMap[mstxPOs[0].eventType];
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, MstxEventsColumn::EVENT_TYPE, typeName, allocator);
    competeSliceDomain.args = JsonUtil::JsonDump(json);
    return true;
}

bool MstxRepo::QuerySliceDetailInfoByNameList(
    const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) {
    std::unordered_map<uint64_t, std::string> strMap =
        stringIdsTable->QueryStrMapByValues(params.nameList, params.rankId);
    if (strMap.empty()) {
        return false;
    }
    std::vector<uint64_t> stringIds;
    std::transform(strMap.begin(), strMap.end(), std::back_inserter(stringIds),
        [](const std::pair<uint64_t, std::string> &pair) { return pair.first; });
    std::vector<MstxEventsPO> mstxPOs;
    mstxEventsTable->Select(MstxEventsColumn::TIMESTAMP, MstxEventsColumn::ENDTIME, MstxEventsColumn::MESSAGE)
        .In(MstxEventsColumn::MESSAGE, stringIds);
    if (params.startTime < params.endTime) {
        mstxEventsTable->GreaterEq(MstxEventsColumn::TIMESTAMP, params.startTime)
            .LessEq(MstxEventsColumn::ENDTIME, params.endTime);
    }
    mstxEventsTable->OrderBy(MstxEventsColumn::TIMESTAMP, TableOrder::ASC).ExcuteQuery(params.rankId, mstxPOs);
    for (const auto &item : mstxPOs) {
        CompeteSliceDomain domain;
        domain.name = strMap[item.message];
        domain.timestamp = item.timestamp;
        domain.endTime = item.endTime;
        domain.duration = NumberSafe::Sub(domain.endTime, domain.timestamp);
        res.push_back(domain);
    }
    return true;
}
}
