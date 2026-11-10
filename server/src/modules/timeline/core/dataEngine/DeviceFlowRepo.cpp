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
#include "NumberSafeUtil.h"
#include "DeviceFlowRepo.h"
#include "StringIdsTable.h"
namespace Dic::Module::Timeline {
void DeviceFlowRepo::AddDeviceFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    // task_info表中opId和globalTaskId的映射
    std::unordered_map<uint64_t, uint64_t> opIdMap = QueryOpIdMap(flowQuery);
    // task表中globalTaskId和deviceId的映射
    std::unordered_map<uint64_t, uint64_t> deviceMap = QueryDeviceMap(flowQuery);
    std::string host = hostInfoTable->GetHost(flowQuery.fileId);
    std::unordered_set<int64_t> hcclConnectionIdSet =
        AddGroupHcclFlowPoint(flowQuery, flowPointVec, opIdMap, deviceMap, host);
    AddHardWareFlowPoint(flowQuery, flowPointVec, host, hcclConnectionIdSet);
}

std::vector<TaskPO> DeviceFlowRepo::QueryModelExecuteTasks(const FlowQuery &flowQuery) {
    // 类型编号随数据变化，按名称查编号，不能硬编码样例中的 taskType 数字。
    StringIdsTable stringIdsTable;
    std::vector<StringIdsPO> types;
    stringIdsTable.Select(StringIdsColumn::ID)
        .Eq(StringIdsColumn::VALUE, "MODEL_EXECUTE")
        .ExcuteQuery(flowQuery.fileId, types);
    std::vector<uint64_t> typeIds;
    for (const auto &type : types) {
        typeIds.emplace_back(type.id);
    }
    std::vector<TaskPO> tasks;
    if (typeIds.empty()) {
        return tasks;
    }
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP, TaskColumn::CONNECTION_ID)
        .Select(TaskColumn::STREAM_ID, TaskColumn::DECICED_ID, TaskColumn::DEPTH, TaskColumn::GLOBAL_PID)
        .In(TaskColumn::TASK_TYPE, typeIds)
        .NotEq(TaskColumn::CONNECTION_ID, std::string("-1"))
        .NotEq(TaskColumn::CONNECTION_ID, static_cast<uint64_t>(UINT32_MAX))
        .GreaterEq(TaskColumn::TIMESTAMP, flowQuery.minTimestamp)
        .ExcuteQuery(flowQuery.fileId, tasks);
    return tasks;
}

void DeviceFlowRepo::AddModelExecuteFlowPoint(const FlowQuery &flowQuery, const std::vector<TaskPO> &tasks,
    const std::unordered_map<int64_t, uint64_t> &hostProcesses, std::vector<FlowPoint> &flowPointVec) {
    if (hostProcesses.empty()) {
        return;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(flowQuery.fileId);
    if (database == nullptr) {
        return;
    }
    // Hardware 将关联 MSTX 的任务展示在 stream_domain 泳道。一次查询匹配到的号码，避免逐任务查库。
    std::string connectionIds;
    for (const auto &entry : hostProcesses) {
        if (!connectionIds.empty()) {
            connectionIds += ",";
        }
        connectionIds += std::to_string(entry.first);
    }
    auto stmt = database->CreatPreparedStatement(
        "SELECT DISTINCT connectionId, domainId FROM MSTX_EVENTS WHERE connectionId IN (" + connectionIds + ")");
    auto result = stmt == nullptr ? nullptr : stmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Warn("Failed to query MSTX domains for Graph flow endpoints.");
        return;
    }
    std::unordered_map<int64_t, std::vector<std::string>> domainsByConnection;
    while (result->Next()) {
        const auto domain = result->GetString("domainId");
        auto &domains = domainsByConnection[result->GetInt64("connectionId")];
        // "0" 是有效 domain；已有标记却无有效 domain 时，不伪造已被 Hardware 排除的普通泳道。
        if (!domain.empty()) {
            domains.emplace_back(domain);
        }
    }
    // 只为匹配到 Host 起点的同进程任务生成终点，避免无起点的孤立连线。
    const std::string host = hostInfoTable->GetHost(flowQuery.fileId);
    auto &instance = TrackInfoManager::Instance();
    for (const auto &task : tasks) {
        const auto process = hostProcesses.find(task.connectionId);
        if (process == hostProcesses.end() || process->second != task.globalPid) {
            continue;
        }
        const std::string rankId = host + instance.GetRankId(host, std::to_string(task.deviceId));
        const std::string streamId = std::to_string(task.streamId);
        const auto appendPoint = [&](const std::string &threadId) {
            FlowPoint point;
            point.type = "f";
            point.flowId = std::to_string(task.connectionId);
            point.id = task.id;
            point.timestamp = task.timestamp - flowQuery.minTimestamp;
            point.rankId = rankId;
            point.trackId = instance.GetTrackId(rankId, hardWarePid, threadId);
            point.depth = task.depth;
            flowPointVec.emplace_back(std::move(point));
        };
        const auto domains = domainsByConnection.find(task.connectionId);
        if (domains == domainsByConnection.end()) {
            appendPoint(streamId);
        } else {
            for (const auto &domain : domains->second) {
                appendPoint(streamId + "_" + domain);
            }
        }
    }
}

void DeviceFlowRepo::AddHardWareMstxFlowPoint(
    const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec, const std::vector<uint64_t> &connectionIds) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(flowQuery.fileId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get database connection when querying Hardware MSTX flow point.");
        return;
    }
    AddHardWareMstxFlowPointExecuteSQL(flowQuery, flowPointVec, connectionIds, database);
}

void DeviceFlowRepo::AddHardWareMstxFlowPointExecuteSQL(const Dic::Module::Timeline::FlowQuery &flowQuery,
    std::vector<FlowPoint> &flowPointVec, const std::vector<uint64_t> &connectionIds,
    std::shared_ptr<VirtualTraceDatabase> database) {
    // 因为MSTX按照domainId展示后必须连接MSTX_EVENTS表才能获取domainId，所以这里未使用Table类
    std::vector<TaskPO> taskPOS;
    std::string sql = "SELECT main.rowid AS id, main.startNs AS startNs, main.connectionId AS connectionId, "
                      "main.streamId AS streamId, main.deviceId AS deviceId, main.depth AS depth, "
                      "m.domainId AS domainId "
                      "FROM " +
        TABLE_TASK + " AS main INNER JOIN " + TABLE_MSTX_EVENTS +
        " AS m ON main.connectionId = m.connectionId WHERE main.connectionId IN (" +
        StringUtil::join(connectionIds, ", ") + ");";

    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare Hardware MSTX flow point query.");
        return;
    }
    stmt->BindParams();
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("Failed to execute Hardware MSTX flow point query.");
        return;
    }
    while (resultSet->Next()) {
        TaskPO singleTask;
        singleTask.connectionId = resultSet->GetInt64("connectionId");
        singleTask.id = resultSet->GetUint64("id");
        singleTask.timestamp = resultSet->GetUint64("startNs");
        singleTask.deviceId = resultSet->GetUint64("deviceId");
        singleTask.streamId = resultSet->GetUint64("streamId");
        singleTask.domainId = resultSet->GetUint64("domainId");
        singleTask.depth = resultSet->GetUint32("depth");
        taskPOS.emplace_back(singleTask);
    }

    std::string host = hostInfoTable->GetHost(flowQuery.fileId);
    auto &instance = TrackInfoManager::Instance();
    for (const auto &item : taskPOS) {
        int64_t realConnectionId = item.connectionId;
        std::string flowId = std::to_string(realConnectionId);
        FlowPoint endPoint;
        endPoint.type = "f";
        endPoint.flowId = flowId;
        endPoint.id = item.id;
        endPoint.timestamp = item.timestamp > flowQuery.minTimestamp ? item.timestamp - flowQuery.minTimestamp : 0;
        endPoint.rankId = host + instance.GetRankId(host, std::to_string(item.deviceId));
        endPoint.trackId = instance.GetTrackId(
            endPoint.rankId, hardWarePid, std::to_string(item.streamId) + "_" + std::to_string(item.domainId));
        endPoint.depth = item.depth;
        flowPointVec.emplace_back(endPoint);
    }
}

std::unordered_set<int64_t> DeviceFlowRepo::AddGroupHcclFlowPoint(const FlowQuery &flowQuery,
    std::vector<FlowPoint> &flowPointVec, const std::unordered_map<uint64_t, uint64_t> &opIdMap,
    const std::unordered_map<uint64_t, uint64_t> &deviceMap, const std::string &host) {
    std::vector<CommucationTaskOpPO> commucationTaskOpPOs;
    commucationOpTable->Select(CommucationTaskOpColumn::OP_ID, CommucationTaskOpColumn::TIMESTAMP)
        .Select(CommucationTaskOpColumn::CONNECTION_ID, CommucationTaskOpColumn::GROUPNAME)
        .ExcuteQuery(flowQuery.fileId, commucationTaskOpPOs);
    std::unordered_set<int64_t> hcclConnectionIdSet;
    auto &instance = TrackInfoManager::Instance();
    std::vector<uint64_t> deviceIdList = npuInfoRepo->QueryDeviceIdByFileId(flowQuery.fileId);
    bool isUniqueDeviceId = deviceIdList.size() == 1;
    for (const auto &item : commucationTaskOpPOs) {
        FlowPoint endPoint;
        // 如果deviceId不唯一，则判断opId和device是否满足
        bool isContinue =
            !isUniqueDeviceId && (opIdMap.count(item.opId) == 0 || deviceMap.count(opIdMap.at(item.opId)) == 0);
        if (isContinue) {
            continue;
        }
        uint64_t deviceId = isUniqueDeviceId ? deviceIdList[0] : deviceMap.at(opIdMap.at(item.opId));
        endPoint.rankId = host + instance.GetRankId(host, std::to_string(deviceId));
        int64_t realConnectionId = item.connectionId;
        std::string flowId = std::to_string(realConnectionId);
        hcclConnectionIdSet.emplace(realConnectionId);
        endPoint.type = "f";
        endPoint.flowId = flowId;
        endPoint.id = item.opId;
        endPoint.timestamp = item.timestamp - flowQuery.minTimestamp; // 业务上 timestamp > minTimestamp
        endPoint.trackId = instance.GetTrackId(endPoint.rankId, hcclPid, std::to_string(item.groupName) + "group");
        flowPointVec.emplace_back(endPoint);
    }
    return hcclConnectionIdSet;
}

std::unordered_map<uint64_t, uint64_t> DeviceFlowRepo::QueryDeviceMap(const FlowQuery &flowQuery) {
    std::vector<TaskPO> deviceTaskPOS;
    taskTable->Select(TaskColumn::GLOBAL_TASK_ID, TaskColumn::DECICED_ID).ExcuteQuery(flowQuery.fileId, deviceTaskPOS);
    std::unordered_map<uint64_t, uint64_t> deviceMap;
    for (const auto &item : deviceTaskPOS) {
        deviceMap[item.globalTaskId] = item.deviceId;
    }
    return deviceMap;
}

std::unordered_map<uint64_t, uint64_t> DeviceFlowRepo::QueryOpIdMap(const FlowQuery &flowQuery) {
    std::vector<CommucationTaskInfoPO> commucationTaskInfoPOs;
    commucationTaskInfoTable->Select(CommucationTaskInfoColumn::GLOBAL_TASK_ID)
        .Select(CommucationTaskInfoColumn::OP_ID)
        .GroupBy(CommucationTaskInfoColumn::OP_ID)
        .ExcuteQuery(flowQuery.fileId, commucationTaskInfoPOs);
    std::unordered_map<uint64_t, uint64_t> opIdMap;
    for (const auto &item : commucationTaskInfoPOs) {
        opIdMap[item.opId] = item.globalTaskId;
    }
    return opIdMap;
}

void DeviceFlowRepo::AddHardWareFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec,
    const std::string &host, const std::unordered_set<int64_t> &hcclConnectionIdSet) {
    std::vector<TaskPO> taskPOS;
    taskTable->Select(TaskColumn::ROW_ID, TaskColumn::TIMESTAMP)
        .Select(TaskColumn::CONNECTION_ID, TaskColumn::STREAM_ID, TaskColumn::DECICED_ID, TaskColumn::DEPTH)
        .ExcuteQuery(flowQuery.fileId, taskPOS);
    auto &instance = TrackInfoManager::Instance();
    for (const auto &item : taskPOS) {
        int64_t realConnectionId = item.connectionId;
        std::string flowId = std::to_string(realConnectionId);
        if (hcclConnectionIdSet.count(realConnectionId) > 0) {
            continue;
        }
        FlowPoint endPoint;
        endPoint.type = "f";
        endPoint.flowId = flowId;
        endPoint.id = item.id;
        endPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        endPoint.rankId = host + instance.GetRankId(host, std::to_string(item.deviceId));
        endPoint.trackId = instance.GetTrackId(endPoint.rankId, hardWarePid, std::to_string(item.streamId));
        endPoint.depth = item.depth;
        flowPointVec.emplace_back(endPoint);
    }
}

void DeviceFlowRepo::SetTaskTable(std::unique_ptr<TaskTable> taskTablePtr) {
    if (taskTablePtr != nullptr) {
        taskTable = std::move(taskTablePtr);
    }
}

void DeviceFlowRepo::SetCommucationOpTable(std::unique_ptr<CommucationOpTable> commucationOpTablePtr) {
    if (commucationOpTablePtr != nullptr) {
        commucationOpTable = std::move(commucationOpTablePtr);
    }
}

void DeviceFlowRepo::SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr) {
    if (npuInfoRepoPtr != nullptr) {
        npuInfoRepo = std::move(npuInfoRepoPtr);
    }
}
}
