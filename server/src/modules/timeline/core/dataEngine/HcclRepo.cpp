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
#include "MetaDataCacheManager.h"
#include "HcclRepo.h"
namespace Dic::Module::Timeline {
void HcclRepo::QuerySimpleSliceWithOutNameByTrackId(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Warn("hccl query all slice track info is not exist, track is: ", sliceQuery.trackId);
        return;
    }
    if (StringUtil::EndWith(trackInfo.threadId, groupSuffix)) {
        QuerySimpleSliceFromGroupTrack(sliceVec, trackInfo, groupSuffix, sliceQuery);
    } else {
        QuerySimpleSliceFromPlaneTrack(sliceVec, trackInfo, sliceQuery);
    }
}

void HcclRepo::QuerySimpleSliceFromPlaneTrack(
    std::vector<SliceDomain> &sliceVec, TrackInfo &trackInfo, const SliceQuery &sliceQuery) {
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPoVec;
    std::string groupName = trackInfo.threadId;
    std::string threadId = trackInfo.threadId;
    size_t pos = trackInfo.threadId.find_last_of("_");
    if (pos != std::string::npos && trackInfo.threadId.size() > pos) {
        threadId = trackInfo.threadId.substr(pos + 1);
        groupName = trackInfo.threadId.substr(0, pos);
    }
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::GLOBAL_TASK_ID)
        .Eq(CommucationTaskInfoColumn::GROUPNAME, groupName)
        .Eq(CommucationTaskInfoColumn::PLANE_ID, threadId)
        .ExcuteQuery(trackInfo.cardId, commucationTaskInfoPoVec);
    std::vector<uint64_t> globalTaskIds(commucationTaskInfoPoVec.size());
    std::transform(commucationTaskInfoPoVec.begin(), commucationTaskInfoPoVec.end(), globalTaskIds.begin(),
        [](const CommucationTaskInfoPO &item) { return item.globalTaskId; });
    std::vector<TaskPO> taskPoVec;
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::ENDTIME)
        .LessEq(TaskColumn::TIMESTAMP, sliceQuery.endTime + sliceQuery.minTimestamp)
        .Greater(TaskColumn::ENDTIME, sliceQuery.startTime + sliceQuery.minTimestamp)
        .Eq(TaskColumn::DECICED_ID, trackInfo.deviceId)
        .In(TaskColumn::GLOBAL_TASK_ID, globalTaskIds)
        .ExcuteQuery(trackInfo.cardId, taskPoVec);
    for (const auto &item : taskPoVec) {
        SliceDomain sliceDomain;
        sliceDomain.id = item.id;
        sliceDomain.timestamp = item.timestamp;
        sliceDomain.endTime = item.endTime;
        sliceVec.emplace_back(sliceDomain);
    }
}

void HcclRepo::QuerySimpleSliceFromGroupTrack(std::vector<SliceDomain> &sliceVec, const TrackInfo &trackInfo,
    const std::string &suffix, const SliceQuery &sliceQuery) {
    // 获取设备id列表
    std::vector<uint64_t> deviceIdList = npuInfoRepo->QueryDeviceIdByFileId(trackInfo.cardId);
    std::vector<CommucationTaskOpPO> commucationTaskOpPOVec;
    std::string tid = trackInfo.threadId.substr(0, trackInfo.threadId.size() - suffix.size());
    if (deviceIdList.size() != 1) {
        // 设备id不唯一，使用专属关联查询，避免构造大量C++ vector
        QuerySimpleSliceFromGroupTrackByDevice(commucationTaskOpPOVec, trackInfo, tid, sliceQuery);
    } else {
        commucationOpTable->Select(CommucationTaskOpColumn::TIMESTAMP, CommucationTaskOpColumn::ENDTIME)
            .Select(CommucationTaskOpColumn::OP_ID)
            .Eq(CommucationTaskOpColumn::GROUPNAME, tid)
            .LessEq(CommucationTaskOpColumn::TIMESTAMP, sliceQuery.endTime + sliceQuery.minTimestamp)
            .Greater(CommucationTaskOpColumn::ENDTIME, sliceQuery.startTime + sliceQuery.minTimestamp)
            .ExcuteQuery(trackInfo.cardId, commucationTaskOpPOVec);
    }
    for (const auto &item : commucationTaskOpPOVec) {
        SliceDomain sliceDomain;
        sliceDomain.id = item.opId;
        sliceDomain.timestamp = item.timestamp;
        sliceDomain.endTime = item.endTime;
        sliceVec.emplace_back(sliceDomain);
    }
}

void HcclRepo::QuerySimpleSliceFromGroupTrackByDevice(std::vector<CommucationTaskOpPO> &commucationTaskOpPOVec,
    const TrackInfo &trackInfo, const std::string &tid, const SliceQuery &sliceQuery) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(trackInfo.cardId);
    if (database == nullptr) {
        return;
    }
    std::string sql = R"(
        SELECT
            co.startNs AS timestamp,
            co.endNs AS endTime,
            co.opId AS opId
        FROM COMMUNICATION_OP co
        WHERE co.groupName = ?
          AND co.startNs <= ?
          AND co.endNs > ?
          AND EXISTS (
              SELECT 1
              FROM COMMUNICATION_TASK_INFO cti
              JOIN TASK t
                ON t.globalTaskId = cti.globalTaskId
              WHERE cti.opId = co.opId
                AND t.deviceId = ?
          )
    )";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Warn("COMMUNICATION_OP Failed to get stmt.");
        return;
    }
    stmt->BindParams(tid);
    stmt->BindParams(sliceQuery.endTime + sliceQuery.minTimestamp);
    stmt->BindParams(sliceQuery.startTime + sliceQuery.minTimestamp);
    stmt->BindParams(trackInfo.deviceId);

    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("COMMUNICATION_OP Failed to get result set.", stmt->GetErrorMessage());
        return;
    }
    while (resultSet->Next()) {
        CommucationTaskOpPO po;
        po.timestamp = resultSet->GetUint64("timestamp");
        po.endTime = resultSet->GetUint64("endTime");
        po.opId = resultSet->GetUint64("opId");
        commucationTaskOpPOVec.emplace_back(std::move(po));
    }
}

std::vector<uint64_t> HcclRepo::QueryOpIdsByGlabalTaskIds(
    const TrackInfo &trackInfo, const std::vector<uint64_t> &globalIds) {
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPoVec;
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::OP_ID)
        .In(CommucationTaskInfoColumn::GLOBAL_TASK_ID, globalIds)
        .GroupBy(CommucationTaskInfoColumn::OP_ID)
        .ExcuteQuery(trackInfo.cardId, commucationTaskInfoPoVec);
    std::vector<uint64_t> opIds(commucationTaskInfoPoVec.size());
    std::transform(commucationTaskInfoPoVec.begin(), commucationTaskInfoPoVec.end(), opIds.begin(),
        [](const CommucationTaskInfoPO &item) { return item.opId; });
    return opIds;
}

std::vector<uint64_t> HcclRepo::QueryGlobalTaskIdsByRank(const TrackInfo &trackInfo) {
    std::vector<TaskPO> taskPoVec;
    taskTable->Select(TaskColumn::GLOBAL_TASK_ID)
        .Eq(TaskColumn::DECICED_ID, trackInfo.deviceId)
        .ExcuteQuery(trackInfo.cardId, taskPoVec);
    std::vector<uint64_t> globalIds(taskPoVec.size());
    std::transform(
        taskPoVec.begin(), taskPoVec.end(), globalIds.begin(), [](const TaskPO &item) { return item.globalTaskId; });
    return globalIds;
}

void HcclRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    const std::string suffix = "group";
    if (StringUtil::EndWith(trackInfo.threadId, suffix)) {
        QueryGroupSliceByIds(sliceIds, competeSliceVec, trackInfo);
    } else {
        QueryPlaneSliceByIds(sliceIds, competeSliceVec, trackInfo);
    }
}

void HcclRepo::QueryPlaneSliceByIds(const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec, const TrackInfo &trackInfo) {
    std::vector<TaskPO> taskPoVec;
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::ENDTIME, TaskColumn::GLOBAL_TASK_ID)
        .In(TaskColumn::ROW_ID, sliceIds)
        .ExcuteQuery(trackInfo.cardId, taskPoVec);
    std::string nameKey = taskTable->GetDbPath(trackInfo.cardId);
    std::vector<uint64_t> globalIds(taskPoVec.size());
    std::transform(
        taskPoVec.begin(), taskPoVec.end(), globalIds.begin(), [](const TaskPO &item) { return item.globalTaskId; });
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPoVec;
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::GLOBAL_TASK_ID)
        .Select(CommucationTaskInfoColumn::TASK_TYPE)
        .In(CommucationTaskInfoColumn::GLOBAL_TASK_ID, globalIds)
        .ExcuteQuery(trackInfo.cardId, commucationTaskInfoPoVec);
    std::unordered_map<uint64_t, uint64_t> typeNameMap;
    for (const auto &item : commucationTaskInfoPoVec) {
        typeNameMap[item.globalTaskId] = item.taskType;
    }
    for (const auto &item : taskPoVec) {
        CompeteSliceDomain competeSliceDomain;
        competeSliceDomain.id = item.id;
        competeSliceDomain.timestamp = item.timestamp;
        competeSliceDomain.endTime = item.endTime;
        competeSliceDomain.name =
            FullDb::DbTraceDataBase::GetStringCacheValue(nameKey, std::to_string(typeNameMap[item.globalTaskId]));
        competeSliceVec.emplace_back(competeSliceDomain);
    }
}

void HcclRepo::QueryGroupSliceByIds(const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec, const TrackInfo &trackInfo) {
    std::vector<CommucationTaskOpPO> commucationTaskOpPOVec;
    commucationOpTable->Select(CommucationTaskOpColumn::OP_ID, CommucationTaskOpColumn::TIMESTAMP)
        .Select(CommucationTaskOpColumn::ENDTIME, CommucationTaskOpColumn::OP_NAME)
        .In(CommucationTaskOpColumn::OP_ID, sliceIds)
        .ExcuteQuery(trackInfo.cardId, commucationTaskOpPOVec);
    std::string nameKey = commucationOpTable->GetDbPath(trackInfo.cardId);
    for (const auto &item : commucationTaskOpPOVec) {
        CompeteSliceDomain competeSliceDomain;
        competeSliceDomain.id = item.opId;
        competeSliceDomain.timestamp = item.timestamp;
        competeSliceDomain.endTime = item.endTime;
        competeSliceDomain.name = FullDb::DbTraceDataBase::GetStringCacheValue(nameKey, std::to_string(item.opName));
        competeSliceVec.emplace_back(competeSliceDomain);
    }
}

void HcclRepo::SetTaskTable(std::unique_ptr<TaskTable> taskTablePtr) {
    if (taskTablePtr != nullptr) {
        taskTable = std::move(taskTablePtr);
    }
}

void HcclRepo::SetCommucationOpTable(std::unique_ptr<CommucationOpTable> commucationOpTablePtr) {
    if (commucationOpTablePtr != nullptr) {
        commucationOpTable = std::move(commucationOpTablePtr);
    }
}

void HcclRepo::SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr) {
    if (npuInfoRepoPtr != nullptr) {
        npuInfoRepo = std::move(npuInfoRepoPtr);
    }
}

void HcclRepo::SetCommucationTaskInfoTable(std::unique_ptr<CommucationTaskInfoTable> commucationTaskInfoTablePtr) {
    if (commucationTaskInfoTablePtr != nullptr) {
        commucationTaskInfoTable = std::move(commucationTaskInfoTablePtr);
    }
}

bool HcclRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Warn("Failed to query hccl slice detail info track info, track is: ", sliceQuery.trackId);
        return false;
    }
    if (StringUtil::EndWith(trackInfo.threadId, groupSuffix)) {
        return QueryGroupSliceDetailInfo(sliceQuery, competeSliceDomain, trackInfo);
    } else {
        return QueryPlaneSliceDetailInfo(sliceQuery, competeSliceDomain);
    }
}

bool HcclRepo::QueryPlaneSliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    std::vector<TaskPO> taskPOs;
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TASK_TYPE)
        .Select(TaskColumn::TIMESTAMP, TaskColumn::ENDTIME)
        .Select(TaskColumn::STREAM_ID, TaskColumn::TASK_ID)
        .Select(TaskColumn::CONTEXT_ID, TaskColumn::GLOBAL_TASK_ID)
        .Eq(TaskColumn::ROW_ID, sliceQuery.sliceId)
        .ExcuteQuery(sliceQuery.rankId, taskPOs);
    if (std::empty(taskPOs)) {
        ServerLog::Warn("Failed to query plane slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    TaskPO targetPO = taskPOs[0];
    competeSliceDomain.id = targetPO.id;
    competeSliceDomain.timestamp = targetPO.timestamp;
    competeSliceDomain.endTime = targetPO.endTime;
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPOs;
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::SRC_RANK)
        .Select(CommucationTaskInfoColumn::DST_RANK, CommucationTaskInfoColumn::TRANSPORT_TYPE)
        .Select(CommucationTaskInfoColumn::SIZE, CommucationTaskInfoColumn::DATA_TYPE)
        .Select(CommucationTaskInfoColumn::LINK_TYPE, CommucationTaskInfoColumn::RDMA_TYPE)
        .Select(CommucationTaskInfoColumn::GROUPNAME, CommucationTaskInfoColumn::TASK_TYPE)
        .Eq(CommucationTaskInfoColumn::GLOBAL_TASK_ID, targetPO.globalTaskId)
        .ExcuteQuery(sliceQuery.rankId, commucationTaskInfoPOs);
    if (std::empty(commucationTaskInfoPOs)) {
        ServerLog::Warn("Failed to query plane slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    CommucationTaskInfoPO infoPo = commucationTaskInfoPOs[0];
    std::vector<uint64_t> strIds = {infoPo.taskType};
    std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap(strIds, sliceQuery.rankId);
    if (strMap.find(infoPo.taskType) == strMap.end()) {
        ServerLog::Warn("Failed to query plane slice name.");
        return false;
    }
    competeSliceDomain.name = strMap[infoPo.taskType];
    SetPlaneSliceArgs(sliceQuery, competeSliceDomain, targetPO, infoPo);
    return true;
}

void HcclRepo::SetPlaneSliceArgs(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain,
    const TaskPO &targetPO, CommucationTaskInfoPO &targetTaskInfo) {
    std::string notifyId = std::to_string(targetTaskInfo.notifyId);
    std::string streamId = std::to_string(targetPO.streamId);
    std::string taskId = std::to_string(targetPO.taskId);
    std::string contextId = std::to_string(targetPO.contextId);
    std::string taskType = competeSliceDomain.name;
    std::string srcRank = std::to_string(targetTaskInfo.srcRank);
    std::string dstRank = std::to_string(targetTaskInfo.dstRank);
    std::string transPortName = QueryTransportName(sliceQuery, targetTaskInfo);
    std::string size = std::to_string(targetTaskInfo.size);
    std::string dataTypeName = QueryDataTypeName(sliceQuery, targetTaskInfo);
    std::string linkTypeName = QueryLinkTypeName(sliceQuery, targetTaskInfo);
    std::string rdmaTypeName = QueryRdmaTypeName(sliceQuery, targetTaskInfo);
    std::string bandwidth = QueryBandwidth(sliceQuery, targetPO);
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::NOTIFY_ID, notifyId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::STREAM_ID, streamId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::TASK_ID, taskId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::CONTEXT_ID, contextId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::TASK_TYPE, taskType, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::SRC_RANK, srcRank, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::DST_RANK, dstRank, allocator);
    std::optional<ParallelGroupInfo> groupInfo = GetGroupInfoByGroupNameId(targetTaskInfo.groupName, sliceQuery.rankId);
    if (groupInfo.has_value()) {
        std::vector<std::string> ranks = groupInfo.value().globalRanks;
        JsonUtil::AddConstMember(json, globalSrcRank, GetRealRankByLocalRank(targetTaskInfo.srcRank, ranks), allocator);
        JsonUtil::AddConstMember(json, globalDstRank, GetRealRankByLocalRank(targetTaskInfo.dstRank, ranks), allocator);
    }
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::TRANSPORT_TYPE, transPortName, allocator);
    JsonUtil::AddConstMember(json, std::string(CommucationTaskInfoColumn::SIZE) + "(Byte)", size, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::DATA_TYPE, dataTypeName, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::LINK_TYPE, linkTypeName, allocator);
    JsonUtil::AddConstMember(json, std::string(CommucationTaskInfoColumn::BANDWIDTH) + "(B/s)", bandwidth, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskInfoColumn::RDMA_TYPE, rdmaTypeName, allocator);
    competeSliceDomain.args = JsonUtil::JsonDump(json);
}

std::string HcclRepo::GetRealRankByLocalRank(uint64_t localRank, std::vector<std::string> &realRankList) {
    if (realRankList.size() <= localRank) {
        return "-1";
    }
    return realRankList[localRank];
}

std::optional<ParallelGroupInfo> HcclRepo::GetGroupInfoByGroupNameId(
    const uint64_t groupNameId, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> strMap =
        stringIdsTable->QueryStrMap(std::vector<uint64_t>{groupNameId}, fileId);
    auto groupNameItr = strMap.find(groupNameId);
    if (groupNameItr == strMap.end()) {
        return std::nullopt;
    }
    return MetaDataCacheManager::Instance().GetParallelGroupInfo(groupNameItr->second);
}

std::string HcclRepo::QueryBandwidth(const SliceQuery &sliceQuery, const TaskPO &targetPO) {
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPOs =
        commucationTaskInfoTable->Select(CommucationTaskInfoColumn::BANDWIDTH)
            .Eq(CommucationTaskInfoColumn::GLOBAL_TASK_ID, targetPO.globalTaskId)
            .ExcuteQuery(sliceQuery.rankId);
    std::string bandwidth;
    if (!std::empty(commucationTaskInfoPOs)) {
        bandwidth = std::to_string(commucationTaskInfoPOs[0].bandwidth);
    }
    return bandwidth;
}

std::string HcclRepo::QueryRdmaTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo) {
    std::vector<EnumHcclRdmaTypePO> rdmaTypes = enumHcclRdmaTypeTable->Select(EnumHcclRdmaTypeClumn::NAME)
                                                    .Eq(EnumHcclRdmaTypeClumn::ID, targetTaskInfo.rdmaType)
                                                    .ExcuteQuery(sliceQuery.rankId);
    std::string ramaTypeName;
    if (!std::empty(rdmaTypes)) {
        ramaTypeName = rdmaTypes[0].name;
    }
    return ramaTypeName;
}

std::string HcclRepo::QueryLinkTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo) {
    std::vector<EnumHcclLinkTypePO> linkTypes = enumHcclLinkTypeTable->Select(EnumHcclLinkTypeClumn::NAME)
                                                    .Eq(EnumHcclLinkTypeClumn::ID, targetTaskInfo.linkType)
                                                    .ExcuteQuery(sliceQuery.rankId);
    std::string linkTypeName;
    if (!std::empty(linkTypes)) {
        linkTypeName = linkTypes[0].name;
    }
    return linkTypeName;
}

std::string HcclRepo::QueryDataTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo) {
    std::vector<EnumHcclDataTypePO> dataTypes = enumHcclDataTypeTable->Select(EnumHcclDataTypeClumn::NAME)
                                                    .Eq(EnumHcclDataTypeClumn::ID, targetTaskInfo.dataType)
                                                    .ExcuteQuery(sliceQuery.rankId);
    std::string dataTypeName;
    if (!std::empty(dataTypes)) {
        dataTypeName = dataTypes[0].name;
    }
    return dataTypeName;
}

std::string HcclRepo::QueryTransportName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo) {
    std::vector<EnumHcclTransportTypePO> transportTypes =
        enumHcclTransportTypeTable->Select(EnumHcclTransportTypeClumn::NAME)
            .Eq(EnumHcclTransportTypeClumn::ID, targetTaskInfo.transportType)
            .ExcuteQuery(sliceQuery.rankId);
    std::string transportName;
    if (!std::empty(transportTypes)) {
        transportName = transportTypes[0].name;
    }
    return transportName;
}

bool HcclRepo::QueryGroupSliceDetailInfo(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TrackInfo &trackInfo) {
    std::vector<CommucationTaskOpPO> commucationTaskOpPOVec;
    commucationOpTable->Select(CommucationTaskOpColumn::TIMESTAMP, CommucationTaskOpColumn::ENDTIME)
        .Select(CommucationTaskOpColumn::OP_NAME, CommucationTaskOpColumn::CONNECTION_ID)
        .Select(CommucationTaskOpColumn::DATA_TYPE, CommucationTaskOpColumn::ALG_TYPE)
        .Select(CommucationTaskOpColumn::COUNT, CommucationTaskOpColumn::OP_ID)
        .Select(CommucationTaskOpColumn::RELAY, CommucationTaskOpColumn::RETRY)
        .Eq(CommucationTaskOpColumn::OP_ID, sliceQuery.sliceId)
        .ExcuteQuery(trackInfo.cardId, commucationTaskOpPOVec);
    if (std::empty(commucationTaskOpPOVec)) {
        ServerLog::Warn("Failed to query group slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    competeSliceDomain.id = commucationTaskOpPOVec[0].opId;
    competeSliceDomain.timestamp = commucationTaskOpPOVec[0].timestamp;
    competeSliceDomain.endTime = commucationTaskOpPOVec[0].endTime;
    std::vector<uint64_t> stringIds;
    stringIds.emplace_back(commucationTaskOpPOVec[0].algType);
    stringIds.emplace_back(commucationTaskOpPOVec[0].opName);
    std::vector<uint64_t> dataTypeIds;
    dataTypeIds.emplace_back(commucationTaskOpPOVec[0].dataType);
    std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap(stringIds, sliceQuery.rankId);
    std::unordered_map<uint64_t, std::string> dataTypeMap =
        enumHcclDataTypeTable->QueryStrMap(dataTypeIds, sliceQuery.rankId);
    competeSliceDomain.name = strMap[commucationTaskOpPOVec[0].opName];
    const std::string connectionId = std::to_string(commucationTaskOpPOVec[0].connectionId);
    const std::string dataType = dataTypeMap[commucationTaskOpPOVec[0].dataType];
    const std::string algType = strMap[commucationTaskOpPOVec[0].algType];
    const std::string count = std::to_string(commucationTaskOpPOVec[0].count);
    const std::string relay = commucationTaskOpPOVec[0].relay == 0 ? "no" : "yes";
    const std::string retry = commucationTaskOpPOVec[0].retry == 0 ? "no" : "yes";
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::CONNECTION_ID, connectionId, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::DATA_TYPE, dataType, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::ALG_TYPE, algType, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::COUNT, count, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::RELAY, relay, allocator);
    JsonUtil::AddConstMember(json, CommucationTaskOpColumn::RETRY, retry, allocator);
    competeSliceDomain.args = JsonUtil::JsonDump(json);
    return true;
}
}
