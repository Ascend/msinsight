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
#include "OSRTApiRepo.h"
#include "TrackInfoManager.h"
#include "DataBaseManager.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
void OSRTApiRepo::QuerySimpleSliceWithOutNameByTrackId(
    const Dic::Module::Timeline::SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Error("OSRT_API query all slice track info does not exist, track is: ", sliceQuery.trackId);
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("OSRT_API open database failed.");
        return;
    }
    QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(database, trackInfo.processId, sliceVec, sliceQuery);
}

void OSRTApiRepo::QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(const std::shared_ptr<VirtualTraceDatabase> &database,
    std::string &processId, std::vector<SliceDomain> &sliceVec, const SliceQuery &sliceQuery) {
    std::string sql = "SELECT rowid AS id, startNs, endNs FROM " + TABLE_OSRT_API +
        " WHERE globalTid = ?"
        " AND startNs <= ? AND endNs >= ? ;";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare OSRT_API query for simple slice.");
        return;
    }
    stmt->BindParams(
        processId, sliceQuery.endTime + sliceQuery.minTimestamp, sliceQuery.startTime + sliceQuery.minTimestamp);
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute OSRT_API query for simple slice.");
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

void OSRTApiRepo::QueryCompeteSliceByIds(const Dic::Module::Timeline::SliceQuery &sliceQuery,
    const std::vector<uint64_t> &sliceIds, std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("OSRT_API open database failed.");
        return;
    }
    QueryCompeteSliceByIdsExecuteSQL(database, sliceIds, competeSliceVec);
}

void OSRTApiRepo::QueryCompeteSliceByIdsExecuteSQL(std::shared_ptr<VirtualTraceDatabase> database,
    const std::vector<uint64_t> &sliceIds, std::vector<CompeteSliceDomain> &competeSliceVec) {
    std::string sliceidvecStr = StringUtil::join(sliceIds, ", ");
    std::string sql = "SELECT t2.value AS name, t1.rowid AS id, t1.startNs AS startNs, t1.endNs AS endNs FROM " +
        TABLE_OSRT_API + " AS t1 INNER JOIN " + TABLE_STRING_IDS +
        " AS t2 ON t1.name = t2.id "
        " WHERE t1.rowid IN (" +
        sliceidvecStr + ");";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare OSRT_API query for complete slice by ids.");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute OSRT_API query for complete slice by ids.");
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

bool OSRTApiRepo::QuerySliceDetailInfo(const Dic::Module::Timeline::SliceQuery &sliceQuery,
    Dic::Module::Timeline::CompeteSliceDomain &competeSliceDomain) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (!database) {
        ServerLog::Error("OSRT_API open database failed.");
        return false;
    }
    return QuerySliceDetailInfoExecuteSQL(database, sliceQuery.sliceId, competeSliceDomain);
}

bool OSRTApiRepo::QuerySliceDetailInfoExecuteSQL(std::shared_ptr<VirtualTraceDatabase> database,
    const std::string &sliceId, Dic::Module::Timeline::CompeteSliceDomain &competeSliceDomain) {
    std::string sql = "SELECT t2.value AS name, t1.rowid AS id, t1.startNs AS startNs, t1.endNs AS endNs FROM " +
        TABLE_OSRT_API + " AS t1 INNER JOIN " + TABLE_STRING_IDS +
        " AS t2 ON t1.name = t2.id "
        " WHERE t1.rowid = " +
        sliceId + ";";
    auto stmt = database->CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("Failed to prepare OSRT_API query for detail info.");
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("Failed to execute OSRT_API query for detail info.");
        return false;
    }
    while (resultSet->Next()) {
        competeSliceDomain.id = resultSet->GetUint64("id");
        competeSliceDomain.timestamp = resultSet->GetUint64("startNs");
        competeSliceDomain.endTime = resultSet->GetUint64("endNs");
        competeSliceDomain.name = resultSet->GetString("name");
        competeSliceDomain.args = "";
    }
    return true;
}
}
