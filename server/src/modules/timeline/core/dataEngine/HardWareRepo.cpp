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
#include <algorithm>
#include "pch.h"
#include "TableDefs.h"
#include "TrackInfoManager.h"
#include "DataBaseManager.h"
#include "NumberUtil.h"
#include "HardWareRepo.h"
namespace Dic::Module::Timeline {
void HardWareRepo::QuerySimpleSliceWithOutNameByTrackId(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        ServerLog::Warn("hardWare query all slice track info is not exist, track is: ", sliceQuery.trackId);
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (database == nullptr) {
        ServerLog::Warn("hardWare open database is failed");
        return;
    }
    auto stmt = PrepareStmtForQuerySimpleSliceWithOutNameByTrackId(trackInfo, database, sliceQuery);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to prepare hardWare query all slice");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("Failed to execute query hardWare query all slice");
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

std::unique_ptr<SqlitePreparedStatement> HardWareRepo::PrepareStmtForQuerySimpleSliceWithOutNameByTrackId(
    const TrackInfo &trackInfo, const std::shared_ptr<VirtualTraceDatabase> &database, const SliceQuery &sliceQuery) {
    // 这个函数会将Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
    // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
    // 非MSTX事件查询时必须使用connectionId NOT IN显式排除MSTX事件，否则会将MSTX事件同时查询
    // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
    // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
    std::string sql;
    if (trackInfo.threadId.find('_') != std::string::npos) {
        sql = "SELECT main.rowid AS id, main.startNs AS startNs, main.endNs AS endNs FROM " + TABLE_TASK +
            " AS main INNER JOIN " + TABLE_MSTX_EVENTS +
            " AS mstx ON main.connectionId = mstx.connectionId "
            " WHERE main.deviceId = ? AND main.streamId = ? AND mstx.domainId = ? "
            " AND main.startNs <= ? AND main.endNs >= ? "
            " ORDER BY main.startNs, main.rowid;";
    } else {
        sql = "SELECT rowid as id, startNs AS startNs, endNs AS endNs FROM " + TABLE_TASK +
            " WHERE deviceId = ? AND streamId = ? AND connectionId NOT IN (SELECT connectionId FROM " +
            TABLE_MSTX_EVENTS + ") AND startNs <= ? AND endNs >= ? ORDER BY startNs, id;";
    }
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return stmt;
    }
    if (trackInfo.threadId.find('_') != std::string::npos) {
        size_t pos = trackInfo.threadId.find('_');
        std::string streamId = trackInfo.threadId.substr(0, pos);
        std::string domainId = trackInfo.threadId.substr(pos + 1);
        stmt->BindParams(trackInfo.deviceId, streamId, domainId, sliceQuery.endTime + sliceQuery.minTimestamp,
            sliceQuery.startTime + sliceQuery.minTimestamp);
    } else {
        stmt->BindParams(trackInfo.deviceId, trackInfo.threadId, sliceQuery.endTime + sliceQuery.minTimestamp,
            sliceQuery.startTime + sliceQuery.minTimestamp);
    }
    return stmt;
}

void HardWareRepo::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    std::string sql = "SELECT main.ROWID as id, main.startNs, main.endNs,"
                      " coalesce(c.name, m.message, s.name, main.taskType) as name FROM " +
        TABLE_TASK +
        " main "
        " left join " +
        TABLE_COMPUTE_TASK_INFO +
        " c on c.globalTaskId = main.globalTaskId "
        " left join " +
        TABLE_MSTX_EVENTS +
        " m on "
        " (m.connectionId = main.connectionId and  m.connectionId != " +
        WRONG_DATA + " ) left join " + TABLE_COMMUNICATION_SCHEDULE_TASK +
        " s on main.globalTaskId = s.globalTaskId"
        " where 1 = 1 and id in (";
    std::string sliceidvecStr = StringUtil::join(sliceIds, ", ");
    sql += sliceidvecStr + ");";
    auto stmt = CreatPreparedStatement(sql, sliceQuery);
    const std::string nameKey = GetDbPath(sliceQuery);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to parpare hardWare query slice by ids");
        return;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("Failed to execute query hardWare query slice by ids");
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

bool HardWareRepo::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    std::vector<TaskPO> taskPOS;
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::MODEL_ID)
        .Select(TaskColumn::TASK_TYPE, TaskColumn::STREAM_ID)
        .Select(TaskColumn::TASK_ID, TaskColumn::CONNECTION_ID)
        .Select(TaskColumn::GLOBAL_TASK_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::ENDTIME)
        .Eq(TaskColumn::ROW_ID, sliceQuery.sliceId)
        .ExcuteQuery(sliceQuery.rankId, taskPOS);
    if (std::empty(taskPOS)) {
        ServerLog::Warn("Failed to query hard ware slice detail by id. id is: %", sliceQuery.sliceId);
        return false;
    }
    const TaskPO targetTask = taskPOS[0];
    std::vector<CompeteSliceDomain> competeSliceVec;
    QueryCompeteSliceByIds(sliceQuery, {targetTask.id}, competeSliceVec);
    if (std::empty(competeSliceVec)) {
        competeSliceDomain.name = std::to_string(targetTask.taskType);
        competeSliceDomain.timestamp = targetTask.timestamp;
        competeSliceDomain.endTime = targetTask.endTime;
    } else {
        competeSliceDomain = std::move(competeSliceVec[0]);
    }
    QuerySliceArgs(sliceQuery, competeSliceDomain, targetTask);
    QuerySliceShape(sliceQuery, competeSliceDomain, targetTask);
    QuerySlicePmuInfo(sliceQuery, competeSliceDomain, targetTask.globalTaskId);
    return true;
}

void HardWareRepo::QuerySliceShape(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask) {
    std::vector<ComputeTaskInfoPO> computePOS;
    computeTaskInfoTable->Select(ComputeTaskInfoColumn::INPUT_SHAPES)
        .Select(ComputeTaskInfoColumn::INPUT_DATA_TYPES, ComputeTaskInfoColumn::INPUT_FORMATS)
        .Select(ComputeTaskInfoColumn::OUTPUT_SHAPES, ComputeTaskInfoColumn::OUTPUT_DATA_TYPES)
        .Select(ComputeTaskInfoColumn::OUTOUT_FORMATS, ComputeTaskInfoColumn::ATTRINFO)
        .Eq(ComputeTaskInfoColumn::GLOBAL_TASK_ID, targetTask.globalTaskId)
        .ExcuteQuery(sliceQuery.rankId, computePOS);
    if (!std::empty(computePOS)) {
        const ComputeTaskInfoPO targetCompute = computePOS[0];
        std::vector<uint64_t> stringIds;
        stringIds.emplace_back(targetCompute.outputShapes);
        stringIds.emplace_back(targetCompute.outputDataTypes);
        stringIds.emplace_back(targetCompute.outputFormats);
        stringIds.emplace_back(targetCompute.inputShapes);
        stringIds.emplace_back(targetCompute.inputDataTypes);
        stringIds.emplace_back(targetCompute.inputFormats);
        stringIds.emplace_back(targetCompute.attrInfo);
        std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap(stringIds, sliceQuery.rankId);
        competeSliceDomain.sliceShape.inputFormats = strMap[targetCompute.inputFormats];
        competeSliceDomain.sliceShape.inputDataTypes = strMap[targetCompute.inputDataTypes];
        competeSliceDomain.sliceShape.inputShapes = strMap[targetCompute.inputShapes];
        competeSliceDomain.sliceShape.outputFormats = strMap[targetCompute.outputFormats];
        competeSliceDomain.sliceShape.outputDataTypes = strMap[targetCompute.outputDataTypes];
        competeSliceDomain.sliceShape.outputShapes = strMap[targetCompute.outputShapes];
        competeSliceDomain.sliceShape.attrInfo = strMap[targetCompute.attrInfo];
    }
}

void HardWareRepo::QuerySlicePmuInfo(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, uint64_t globalTaskId) {
    if (std::empty(competeSliceDomain.args)) {
        return;
    }
    std::vector<TaskPmuInfoPO> pmuInfoPOS;
    taskPmuInfoTable->Select(TaskPmuInfoColumn::GLOBAL_TASK_ID, TaskPmuInfoColumn::NAME_ID)
        .Select(TaskPmuInfoColumn::VALUE_ID)
        .Eq(TaskPmuInfoColumn::GLOBAL_TASK_ID, globalTaskId)
        .ExcuteQuery(sliceQuery.rankId, pmuInfoPOS);
    if (std::empty(pmuInfoPOS)) {
        return;
    }
    std::string error;
    auto json = JsonUtil::TryParse(competeSliceDomain.args, error);
    if (!json.has_value() || !error.empty()) {
        return;
    }
    std::vector<uint64_t> stringIds;
    for (auto &item : pmuInfoPOS) {
        stringIds.emplace_back(item.name);
    }
    std::unordered_map<uint64_t, std::string> strMap = stringIdsTable->QueryStrMap(stringIds, sliceQuery.rankId);
    auto &allocator = json.value().GetAllocator();
    for (auto &item : pmuInfoPOS) {
        JsonUtil::AddMember(json.value(), strMap[item.name], item.value, allocator);
    }
    competeSliceDomain.args = JsonUtil::JsonDump(json.value());
}

void HardWareRepo::QuerySliceArgs(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask) {
    std::string modelIdName = std::to_string(targetTask.modelId);
    std::unordered_map<uint64_t, std::string> strMap =
        stringIdsTable->QueryStrMap({targetTask.taskType}, sliceQuery.rankId);
    std::string taskTypeName = strMap[targetTask.taskType];
    std::string streamId = std::to_string(targetTask.streamId);
    std::string taskId = std::to_string(targetTask.taskId);
    std::string connectionId = std::to_string(targetTask.connectionId);
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddConstMember(json, TaskColumn::MODEL_ID, modelIdName, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::TASK_TYPE, taskTypeName, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::STREAM_ID, streamId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::TASK_ID, taskId, allocator);
    JsonUtil::AddConstMember(json, TaskColumn::CONNECTION_ID, connectionId, allocator);
    if (QueryMemoryInfo(sliceQuery, competeSliceDomain, targetTask)) {
        JsonUtil::AddMember(json, "operation", competeSliceDomain.memcpyDirection, allocator);
        JsonUtil::AddMember(json, "size(B)", competeSliceDomain.dataSize, allocator);
        if (targetTask.endTime > targetTask.timestamp) {
            double bandwidth = static_cast<double>(competeSliceDomain.dataSize) /
                static_cast<double>(targetTask.endTime - targetTask.timestamp);
            JsonUtil::AddMember(json, "bandwidth(GB/s)", NumberUtil::DoubleReservedNDigits(bandwidth), allocator);
        }
    }
    competeSliceDomain.args = JsonUtil::JsonDump(json);
}

bool HardWareRepo::QueryMemoryInfo(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask) {
    std::string sql = "SELECT OPERATION.name as memcpyDirection, size from " + TABLE_MEMCPY_INFO +
        " MI"
        " LEFT JOIN " +
        TABLE_ENUM_MEMCPY_OPERATION +
        " as OPERATION ON MI.memcpyOperation = OPERATION.id "
        " WHERE globalTaskId = ?";
    auto stmt = CreatPreparedStatement(sql, sliceQuery);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to parpare MemoryInfo by connection id");
        return false;
    }
    stmt->BindParams(targetTask.globalTaskId);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("Failed to execute query MemoryInfo by connectionId and startTime.");
        return false;
    }
    if (resultSet->Next()) {
        competeSliceDomain.memcpyDirection = resultSet->GetString("memcpyDirection");
        competeSliceDomain.dataSize = resultSet->GetUint64("size");
        return true;
    }
    return false;
}

Stmt HardWareRepo::CreatPreparedStatement(const std::string &sql, const SliceQuery &sliceQuery) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (database == nullptr) {
        ServerLog::Warn("hardWare open database is failed");
        return nullptr;
    }
    return database->CreatPreparedStatement(sql);
}

std::string HardWareRepo::GetDbPath(const SliceQuery &sliceQuery) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(sliceQuery.rankId);
    if (database == nullptr) {
        ServerLog::Warn("hardWare open database is failed");
        return "";
    }
    return database->GetDbPath();
}

bool HardWareRepo::QuerySliceDetailInfoByNameList(
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
    // 根据stringId列表查询算子表获取globalTaskId的内容
    std::vector<ComputeTaskInfoPO> computePOS;
    computeTaskInfoTable->Select(ComputeTaskInfoColumn::NAME, ComputeTaskInfoColumn::GLOBAL_TASK_ID)
        .In(ComputeTaskInfoColumn::NAME, stringIds)
        .ExcuteQuery(params.rankId, computePOS);
    if (computePOS.empty()) {
        return false;
    }
    std::vector<uint64_t> globalTaskIdList;
    std::transform(computePOS.begin(), computePOS.end(), std::back_inserter(globalTaskIdList),
        [](const ComputeTaskInfoPO &computeTaskInfoPo) { return computeTaskInfoPo.globalTaskId; });
    // 根据globalTaskId查询Task表获取耗时信息，并按算子起始时间进行排序
    std::vector<TaskPO> taskPOS;
    taskTable->Select(TaskColumn::GLOBAL_TASK_ID, TaskColumn::TIMESTAMP, TaskColumn::ENDTIME)
        .In(TaskColumn::GLOBAL_TASK_ID, globalTaskIdList)
        .OrderBy(TaskColumn::TIMESTAMP, TableOrder::ASC)
        .ExcuteQuery(params.rankId, taskPOS);

    // 先获取globalTaskId和名字的映射关系，再进行结果组装
    std::map<uint64_t, std::string> globalTaskIdMapName;
    std::transform(computePOS.begin(), computePOS.end(),
        std::inserter(globalTaskIdMapName, globalTaskIdMapName.begin()), [&strMap](const ComputeTaskInfoPO &compute) {
            return std::make_pair(compute.globalTaskId, strMap[compute.name]);
        });
    for (const auto &item : taskPOS) {
        CompeteSliceDomain domain;
        domain.name = globalTaskIdMapName[item.globalTaskId];
        domain.timestamp = item.timestamp;
        domain.duration = item.endTime - item.timestamp;
        res.push_back(domain);
    }
    return true;
}
}
