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
bool HcclRepo::IsTimestampInTask(const TaskPO &task, uint64_t timestamp) {
    return timestamp >= task.timestamp && timestamp <= task.endTime;
}

HcclRepo::CommunicationMatchMap HcclRepo::MatchCommunicationWithTimestamp(
    const std::vector<TaskPO> &tasks, const std::vector<CommucationTaskInfoPO> &infos) {
    // 每个 globalTaskId 独立配对：TASK 按 <startNs, ROWID> 排序，通信明细按 <timestampNs, ROWID> 排序，
    // 第 n 个 TASK 与第 n 条明细一一对应。配对后只校验 timestampNs 是否落在该 TASK 的闭区间内，
    // 不再为单个 TASK 扫描或搜索其他候选，避免序号错位和 O(T * C) 复杂度。
    std::unordered_map<uint64_t, std::vector<const TaskPO *>> tasksById;
    std::unordered_map<uint64_t, std::vector<size_t>> infosById;
    for (const auto &task : tasks) {
        tasksById[task.globalTaskId].emplace_back(&task);
    }
    for (size_t i = 0; i < infos.size(); ++i) {
        infosById[infos[i].globalTaskId].emplace_back(i);
    }
    CommunicationMatchMap result;
    for (auto &[globalId, occurrences] : tasksById) {
        auto &candidates = infosById[globalId];
        MatchCommunicationTimestampBucket(globalId, occurrences, candidates, infos, result);
    }
    return result;
}

void HcclRepo::MatchCommunicationTimestampBucket(uint64_t globalTaskId, std::vector<const TaskPO *> &tasks,
    std::vector<size_t> &infoIndexes, const std::vector<CommucationTaskInfoPO> &infos, CommunicationMatchMap &matches) {
    std::sort(tasks.begin(), tasks.end(), [](const auto *left, const auto *right) {
        return std::tie(left->timestamp, left->id) < std::tie(right->timestamp, right->id);
    });
    std::sort(infoIndexes.begin(), infoIndexes.end(), [&](size_t left, size_t right) {
        return std::tie(infos[left].timestamp, infos[left].id) < std::tie(infos[right].timestamp, infos[right].id);
    });
    const bool countMatches = tasks.size() == infoIndexes.size();
    if (!countMatches) {
        ServerLog::Warn("Communication row count mismatches TASK. globalTaskId: ", globalTaskId,
            ", task count: ", tasks.size(), ", info count: ", infoIndexes.size());
    }
    std::vector<bool> used(infoIndexes.size(), false);
    for (size_t ordinal = 0; ordinal < tasks.size(); ++ordinal) {
        if (countMatches && ordinal < infoIndexes.size() && !used[ordinal] &&
            IsTimestampInTask(*tasks[ordinal], infos[infoIndexes[ordinal]].timestamp)) {
            matches[tasks[ordinal]->id] = infoIndexes[ordinal];
            used[ordinal] = true;
            continue;
        }
        size_t matchedIndex = 0;
        if (RecoverCommunicationByTimestamp(*tasks[ordinal], infoIndexes, infos, used, matchedIndex)) {
            matches[tasks[ordinal]->id] = infoIndexes[matchedIndex];
            used[matchedIndex] = true;
        } else {
            ServerLog::Warn("Cannot recover communication row by timestamp. globalTaskId: ", globalTaskId,
                ", task rowId: ", tasks[ordinal]->id);
        }
    }
}

bool HcclRepo::RecoverCommunicationByTimestamp(const TaskPO &task, const std::vector<size_t> &infoIndexes,
    const std::vector<CommucationTaskInfoPO> &infos, std::vector<bool> &used, size_t &matchedIndex) {
    size_t matchedCount = 0;
    for (size_t index = 0; index < infoIndexes.size(); ++index) {
        if (!used[index] && IsTimestampInTask(task, infos[infoIndexes[index]].timestamp)) {
            matchedIndex = index;
            ++matchedCount;
        }
    }
    return matchedCount == 1;
}

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
    // 简单切片只需要确认 globalTaskId 属于当前 plane，不需要精确匹配某条通信详情。
    // 该逻辑依赖业务约束：同一 globalTaskId 的重复调用必须属于相同的 groupName/planeId，不能跨 plane。
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

void HcclRepo::QueryPlaneTasks(const std::vector<uint64_t> &globalIds, const TrackInfo &trackInfo,
    std::vector<TaskPO> &taskVec, bool withTimestamp) {
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::ENDTIME, TaskColumn::GLOBAL_TASK_ID, TaskColumn::DECICED_ID)
        .In(TaskColumn::GLOBAL_TASK_ID, globalIds)
        .Eq(TaskColumn::DECICED_ID, trackInfo.deviceId)
        .OrderBy(withTimestamp ? TaskColumn::TIMESTAMP : TaskColumn::ROW_ID, TableOrder::ASC)
        .OrderBy(TaskColumn::ROW_ID, TableOrder::ASC)
        .ExcuteQuery(trackInfo.cardId, taskVec);
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
    auto database = sliceQuery.dbPath.empty() ? DataBaseManager::Instance().GetTraceDatabaseByRankId(trackInfo.cardId)
                                              : DataBaseManager::Instance().GetTraceDatabaseByFileId(sliceQuery.dbPath);
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
          AND co.deviceId = ?
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
    // 批量切片名称只依赖 taskType；业务保证同一 globalTaskId 的所有 Communication 明细 taskType 相同，
    // 因此无需按调用精确配对，使用哈希映射即可保持 O(1) 查询。
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
        .Select(TaskColumn::STREAM_ID, TaskColumn::TASK_ID, TaskColumn::DECICED_ID)
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
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    const bool hasTimestamp =
        database != nullptr && database->CheckColumnExist("COMMUNICATION_TASK_INFO", "timestampNs");
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPOs;
    QueryPlaneDetail(sliceQuery, targetPO, commucationTaskInfoPOs, hasTimestamp);
    if (std::empty(commucationTaskInfoPOs)) {
        ServerLog::Warn("Failed to query plane slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    size_t infoIndex = 0;
    // 旧表没有 timestampNs，或新表按时间戳匹配失败时，使用查询结果第一条兜底。
    bool useFallback = !hasTimestamp;
    if (hasTimestamp) {
        std::vector<TaskPO> occurrences;
        QueryTaskOccurrences(targetPO, sliceQuery.rankId, occurrences, true);
        auto matches = MatchCommunicationWithTimestamp(occurrences, commucationTaskInfoPOs);
        auto matched = matches.find(targetPO.id);
        if (matched == matches.end()) {
            ServerLog::Warn("No communication row ordinal matches TASK. globalTaskId: ", targetPO.globalTaskId);
            useFallback = true;
        } else {
            infoIndex = matched->second;
        }
    }
    // 兜底查询只有一条明细时没有其他候选，不存在归属歧义。
    const bool ambiguous = useFallback && commucationTaskInfoPOs.size() > 1;
    auto &infoPo = commucationTaskInfoPOs[infoIndex];
    return FillPlaneSliceDetail(sliceQuery, competeSliceDomain, targetPO, infoPo, ambiguous);
}

bool HcclRepo::FillPlaneSliceDetail(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain,
    const TaskPO &targetTask, CommucationTaskInfoPO &taskInfo, bool ambiguous) {
    std::vector<uint64_t> strIds = {taskInfo.taskType};
    std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap(strIds, sliceQuery.rankId);
    if (strMap.find(taskInfo.taskType) == strMap.end()) {
        ServerLog::Warn("Failed to query plane slice name.");
        return false;
    }
    competeSliceDomain.name = strMap[taskInfo.taskType];
    SetPlaneSliceArgs(sliceQuery, competeSliceDomain, targetTask, taskInfo, ambiguous);
    return true;
}

void HcclRepo::QueryPlaneDetail(const SliceQuery &sliceQuery, const TaskPO &targetTask,
    std::vector<CommucationTaskInfoPO> &taskInfoVec, bool withTimestamp) {
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::ROW_ID, CommucationTaskInfoColumn::GLOBAL_TASK_ID)
        .Select(CommucationTaskInfoColumn::SRC_RANK)
        .Select(CommucationTaskInfoColumn::DST_RANK, CommucationTaskInfoColumn::TRANSPORT_TYPE)
        .Select(CommucationTaskInfoColumn::SIZE, CommucationTaskInfoColumn::DATA_TYPE)
        .Select(CommucationTaskInfoColumn::LINK_TYPE, CommucationTaskInfoColumn::RDMA_TYPE)
        .Select(CommucationTaskInfoColumn::GROUPNAME, CommucationTaskInfoColumn::TASK_TYPE)
        .Select(CommucationTaskInfoColumn::BANDWIDTH);
    if (withTimestamp) {
        commucationTaskInfoTable->Select(CommucationTaskInfoColumn::TIMESTAMP);
    }
    commucationTaskInfoTable->Eq(CommucationTaskInfoColumn::GLOBAL_TASK_ID, targetTask.globalTaskId)
        .OrderBy(
            withTimestamp ? CommucationTaskInfoColumn::TIMESTAMP : CommucationTaskInfoColumn::ROW_ID, TableOrder::ASC)
        .OrderBy(CommucationTaskInfoColumn::ROW_ID, TableOrder::ASC)
        .ExcuteQuery(sliceQuery.rankId, taskInfoVec);
}

void HcclRepo::QueryTaskOccurrences(
    const TaskPO &targetTask, const std::string &fileId, std::vector<TaskPO> &tasks, bool withTimestamp) {
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::ENDTIME, TaskColumn::GLOBAL_TASK_ID, TaskColumn::DECICED_ID)
        .Eq(TaskColumn::GLOBAL_TASK_ID, targetTask.globalTaskId)
        .Eq(TaskColumn::DECICED_ID, targetTask.deviceId)
        .OrderBy(withTimestamp ? TaskColumn::TIMESTAMP : TaskColumn::ROW_ID, TableOrder::ASC)
        .OrderBy(TaskColumn::ROW_ID, TableOrder::ASC)
        .ExcuteQuery(fileId, tasks);
}

void HcclRepo::SetPlaneSliceArgs(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain,
    const TaskPO &targetPO, CommucationTaskInfoPO &targetTaskInfo, bool ambiguous) {
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
    std::string bandwidth = QueryBandwidth(targetTaskInfo);
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    // 以下带 addDetailMember 的字段都取自匹配到的那条 COMMUNICATION_TASK_INFO 记录，记录这些字段名，
    // 匹配不确定时随 args 下发，前端据此在对应行展示警示；streamId/taskId/contextId 来自 TASK 精确行，不受影响。
    std::vector<std::string> ambiguousKeys;
    const auto addDetailMember = [&json, &allocator, &ambiguousKeys](std::string_view key, const std::string &value) {
        JsonUtil::AddConstMember(json, key, value, allocator);
        ambiguousKeys.emplace_back(key);
    };
    addDetailMember(CommucationTaskInfoColumn::NOTIFY_ID, notifyId);
    JsonUtil::AddConstMember(json, TaskColumn::STREAM_ID, streamId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::TASK_ID, taskId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::CONTEXT_ID, contextId, allocator);
    addDetailMember(TaskColumn::TASK_TYPE, taskType);
    addDetailMember(CommucationTaskInfoColumn::SRC_RANK, srcRank);
    addDetailMember(CommucationTaskInfoColumn::DST_RANK, dstRank);
    std::optional<ParallelGroupInfo> groupInfo = GetGroupInfoByGroupNameId(targetTaskInfo.groupName, sliceQuery.rankId);
    if (groupInfo.has_value()) {
        std::vector<std::string> ranks = groupInfo.value().globalRanks;
        addDetailMember(globalSrcRank, GetRealRankByLocalRank(targetTaskInfo.srcRank, ranks));
        addDetailMember(globalDstRank, GetRealRankByLocalRank(targetTaskInfo.dstRank, ranks));
    }
    addDetailMember(CommucationTaskInfoColumn::TRANSPORT_TYPE, transPortName);
    addDetailMember(std::string(CommucationTaskInfoColumn::SIZE) + "(Byte)", size);
    addDetailMember(CommucationTaskInfoColumn::DATA_TYPE, dataTypeName);
    addDetailMember(CommucationTaskInfoColumn::LINK_TYPE, linkTypeName);
    addDetailMember(std::string(CommucationTaskInfoColumn::BANDWIDTH) + "(GB/s)", bandwidth);
    addDetailMember(CommucationTaskInfoColumn::RDMA_TYPE, rdmaTypeName);
    if (ambiguous) {
        json_t ambiguousKeyList(kArrayType);
        for (const auto &key : ambiguousKeys) {
            ambiguousKeyList.PushBack(json_t().SetString(key.c_str(), allocator), allocator);
        }
        JsonUtil::AddMember(json, "_ambiguousKeys", ambiguousKeyList, allocator);
    }
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

std::string HcclRepo::QueryBandwidth(const CommucationTaskInfoPO &targetTaskInfo) {
    constexpr double bytesPerGb = 1e9;
    // 注意：此处的 bandwidth 来自 QueryPlaneSliceDetailInfo 中按时间匹配好的那条记录，
    // 而不是向 DB 重新发起按 globalTaskId IN 的查询。
    // 避免详情页 bandwidth 和其他字段来自不同调用导致数据自相矛盾。
    if (targetTaskInfo.bandwidth <= 0) {
        return "";
    }
    return StringUtil::DoubleToStringWithTwoDecimalPlaces(targetTaskInfo.bandwidth / bytesPerGb);
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
    competeSliceDomain.isCommunicationGroup = true;
    return true;
}
}
