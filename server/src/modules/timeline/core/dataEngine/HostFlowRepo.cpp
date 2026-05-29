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
#include "HostFlowRepo.h"
namespace Dic::Module::Timeline {
void HostFlowRepo::QueryFwdbwd(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    uint64_t nameId = QueryEnqueueNameId(flowQuery);
    std::vector<PytorchApiPO> pythonApiPOS = QueryNotEnqueuePythonApi(flowQuery, nameId);
    std::unordered_map<uint64_t, uint64_t> connectionIdMap = QueryConnectionIdMap(flowQuery);
    auto &instance = TrackInfoManager::Instance();
    std::unordered_map<std::string, PytorchApiPO> pytorchApiPOLog;
    for (const auto &item : pythonApiPOS) {
        if (connectionIdMap.count(item.connectionId) == 0) {
            continue;
        }
        std::string flowId = std::to_string(connectionIdMap[item.connectionId]);
        if (pytorchApiPOLog.count(flowId) == 0) {
            pytorchApiPOLog[flowId] = item;
            continue;
        }
        FlowPoint startPoint;
        startPoint.type = "s";
        startPoint.flowId = flowId;
        PytorchApiPO start = pytorchApiPOLog[flowId];
        startPoint.id = start.id;
        startPoint.timestamp = start.timestamp - flowQuery.minTimestamp; // 业务上 timestamp > minTimestamp
        startPoint.trackId = instance.GetTrackId(flowQuery.fileId, std::to_string(start.globalTid), pythonApiTid);
        startPoint.rankId = flowQuery.fileId;
        FlowPoint endPoint;
        endPoint.type = "f";
        endPoint.flowId = flowId;
        endPoint.id = item.id;
        endPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        endPoint.trackId = instance.GetTrackId(flowQuery.fileId, std::to_string(item.globalTid), pythonApiTid);
        endPoint.rankId = flowQuery.fileId;
        flowPointVec.emplace_back(startPoint);
        flowPointVec.emplace_back(endPoint);
        pytorchApiPOLog.erase(flowId);
    }
    std::sort(flowPointVec.begin(), flowPointVec.end());
}

uint64_t HostFlowRepo::QueryEnqueueNameId(const FlowQuery &flowQuery) {
    uint64_t nameId = 0;
    std::vector<StringIdsPO> stringIdsPOS;
    stringIdsTable->Select(StringIdsColumn::ID)
        .Like(StringIdsColumn::VALUE, "Enqueue%")
        .ExcuteQuery(flowQuery.fileId, stringIdsPOS);
    if (!std::empty(stringIdsPOS)) {
        nameId = stringIdsPOS[0].id;
    }
    return nameId;
}

std::vector<PytorchApiPO> HostFlowRepo::QueryNotEnqueuePythonApi(const FlowQuery &flowQuery, uint64_t nameId) {
    std::vector<PytorchApiPO> pythonApiPOS;
    uint64_t minConnectionId = 0;
    pytorchApiTable->Select(PytorchApiColumn::ID, PytorchApiColumn::TIMESTAMP)
        .Select(PytorchApiColumn::CONNECTIONID, PytorchApiColumn::GLOBAL_TID)
        .NotEq(PytorchApiColumn::NAME, nameId)
        .GreaterEq(PytorchApiColumn::CONNECTIONID, minConnectionId)
        .OrderBy(PytorchApiColumn::TIMESTAMP, TableOrder::ASC)
        .ExcuteQuery(flowQuery.fileId, pythonApiPOS);
    return pythonApiPOS;
}

void HostFlowRepo::QueryAsyncTaskQueue(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    std::vector<StringIdsPO> stringIdsPOS;
    stringIdsTable->Select(StringIdsColumn::ID)
        .Like(StringIdsColumn::VALUE, "Enqueue%")
        .ExcuteQuery(flowQuery.fileId, stringIdsPOS);
    if (std::empty(stringIdsPOS)) {
        return;
    }
    uint64_t nameId = stringIdsPOS[0].id;
    std::vector<uint64_t> pythonConnectionIds = QueryEnqueueFlowConnectionIds(flowQuery, nameId);
    std::vector<uint64_t> realConnectionIds = QueryRealConnectionIds(flowQuery, pythonConnectionIds);
    std::unordered_map<uint64_t, uint64_t> connectionIdMap;
    std::vector<uint64_t> allPythonConnectionIds;
    QueryAllPythonConnectionIds(flowQuery, realConnectionIds, connectionIdMap, allPythonConnectionIds);
    std::vector<PytorchApiPO> pythonApiPOS;
    pytorchApiTable->Select(PytorchApiColumn::ID, PytorchApiColumn::TIMESTAMP)
        .Select(PytorchApiColumn::CONNECTIONID, PytorchApiColumn::GLOBAL_TID)
        .In(PytorchApiColumn::CONNECTIONID, allPythonConnectionIds)
        .OrderBy(PytorchApiColumn::TIMESTAMP, TableOrder::ASC)
        .ExcuteQuery(flowQuery.fileId, pythonApiPOS);
    std::unordered_set<std::string> connectionIdLog;
    auto &instance = TrackInfoManager::Instance();
    std::string dbPath = flowQuery.fileId;
    for (auto &item : pythonApiPOS) {
        FlowPoint flowPoint;
        flowPoint.flowId = std::to_string(connectionIdMap[item.connectionId]);
        if (connectionIdLog.count(flowPoint.flowId) == 0) {
            flowPoint.type = "s";
            connectionIdLog.emplace(flowPoint.flowId);
        } else {
            flowPoint.type = "f";
        }
        flowPoint.id = item.id;
        flowPoint.trackId = instance.GetTrackId(flowQuery.fileId, std::to_string(item.globalTid), pythonApiTid);
        flowPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        flowPoint.rankId = dbPath;
        flowPointVec.emplace_back(flowPoint);
    }
    std::sort(flowPointVec.begin(), flowPointVec.end());
}

std::unordered_map<uint64_t, uint64_t> HostFlowRepo::QueryConnectionIdMap(const FlowQuery &flowQuery) {
    std::unordered_map<uint64_t, uint64_t> connectionIdMap;
    std::vector<ConnectionIdsPO> connectionIdsPOVec;
    connectionIdsTable->Select(ConnectionIdsColumn::ID, ConnectionIdsColumn::CONNECTIONID)
        .OrderBy(ConnectionIdsColumn::ID, TableOrder::ASC)
        .ExcuteQuery(flowQuery.fileId, connectionIdsPOVec);
    for (const auto &item : connectionIdsPOVec) {
        connectionIdMap[item.id] = item.connectionId;
    }
    return connectionIdMap;
}

std::vector<uint64_t> HostFlowRepo::QueryEnqueueFlowConnectionIds(const FlowQuery &flowQuery, uint64_t nameId) {
    std::vector<PytorchApiPO> pythonApiConnectionIdPOS;
    pytorchApiTable->Select(PytorchApiColumn::CONNECTIONID)
        .Eq(PytorchApiColumn::NAME, nameId)
        .ExcuteQuery(flowQuery.fileId, pythonApiConnectionIdPOS);
    std::vector<uint64_t> pythonConnectionIds;
    pythonConnectionIds.reserve(pythonApiConnectionIdPOS.size());
    for (const auto &item : pythonApiConnectionIdPOS) {
        pythonConnectionIds.emplace_back(item.connectionId);
    }
    return pythonConnectionIds;
}

std::vector<uint64_t> HostFlowRepo::QueryRealConnectionIds(
    const FlowQuery &flowQuery, const std::vector<uint64_t> &pythonConnectionIds) {
    std::vector<ConnectionIdsPO> connectionIdsPOVec;
    connectionIdsTable->Select(ConnectionIdsColumn::CONNECTIONID)
        .In(ConnectionIdsColumn::ID, pythonConnectionIds)
        .ExcuteQuery(flowQuery.fileId, connectionIdsPOVec);
    std::vector<uint64_t> realConnectionIds;
    realConnectionIds.reserve(connectionIdsPOVec.size());
    for (const auto &item : connectionIdsPOVec) {
        realConnectionIds.emplace_back(item.connectionId);
    }
    return realConnectionIds;
}

void HostFlowRepo::QueryAllPythonConnectionIds(const FlowQuery &flowQuery,
    const std::vector<uint64_t> &realConnectionIds, std::unordered_map<uint64_t, uint64_t> &connectionIdMap,
    std::vector<uint64_t> &allPythonConnectionIds) {
    std::vector<ConnectionIdsPO> allConnectionIdsPOVec;
    connectionIdsTable->Select(ConnectionIdsColumn::ID, ConnectionIdsColumn::CONNECTIONID)
        .In(ConnectionIdsColumn::CONNECTIONID, realConnectionIds)
        .ExcuteQuery(flowQuery.fileId, allConnectionIdsPOVec);
    for (const auto &item : allConnectionIdsPOVec) {
        allPythonConnectionIds.emplace_back(item.id);
        connectionIdMap[item.id] = item.connectionId;
    }
}

void HostFlowRepo::AddAsyncNpuFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    uint64_t nameId = QueryEnqueueNameId(flowQuery);
    std::vector<PytorchApiPO> pythonApiPOS = QueryNotEnqueuePythonApi(flowQuery, nameId);
    std::unordered_map<uint64_t, uint64_t> connectionIdMap = QueryConnectionIdMap(flowQuery);
    std::unordered_set<std::string> pytorchApiFlowIdSet;
    std::unordered_set<std::string> fwdbwdFlowIdSet;
    for (const auto &item : pythonApiPOS) {
        if (connectionIdMap.count(item.connectionId) == 0) {
            continue;
        }
        std::string flowId = std::to_string(connectionIdMap[item.connectionId]);
        if (pytorchApiFlowIdSet.count(flowId) > 0) {
            fwdbwdFlowIdSet.emplace(flowId);
            continue;
        }
        pytorchApiFlowIdSet.emplace(flowId);
    }
    auto &instance = TrackInfoManager::Instance();
    for (const auto &item : pythonApiPOS) {
        uint64_t realConnectionId = connectionIdMap[item.connectionId];
        std::string flowId = std::to_string(realConnectionId);
        if (fwdbwdFlowIdSet.count(flowId) > 0) {
            continue;
        }
        FlowPoint startPoint;
        startPoint.type = "s";
        startPoint.flowId = flowId;
        startPoint.id = item.id;
        startPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        startPoint.rankId = flowQuery.fileId;
        startPoint.trackId = instance.GetTrackId(flowQuery.fileId, std::to_string(item.globalTid), pythonApiTid);
        flowPointVec.emplace_back(startPoint);
    }
}

std::vector<uint64_t> HostFlowRepo::AddMstxFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    std::vector<MstxEventsPO> mstxPOs;
    mstxEventsTable->Select(MstxEventsColumn::ID, MstxEventsColumn::CONNECTION_ID)
        .Select(MstxEventsColumn::GLOBAL_TID, MstxEventsColumn::TIMESTAMP, MstxEventsColumn::DOMAIN_ID)
        .NotEq(MstxEventsColumn::CONNECTION_ID, WRONG_DATA)
        .ExcuteQuery(flowQuery.fileId, mstxPOs);
    std::vector<uint64_t> connectionIds;
    auto &instance = TrackInfoManager::Instance();
    for (const auto &item : mstxPOs) {
        FlowPoint startPoint;
        startPoint.type = "s";
        startPoint.id = item.id;
        startPoint.flowId = std::to_string(item.connectionId);
        startPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        startPoint.rankId = flowQuery.fileId;
        startPoint.trackId =
            instance.GetTrackId(flowQuery.fileId, std::to_string(item.globalTid), std::to_string(item.domainId));
        flowPointVec.emplace_back(startPoint);
        connectionIds.emplace_back(item.connectionId);
    }
    return connectionIds;
}

void HostFlowRepo::AddCANNFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    std::vector<CANNApiPO> cannApiPOs;
    cannApiTable->Select(CANNApiColumn::ID, CANNApiColumn::TIMESTAMP)
        .Select(CANNApiColumn::GLOBAL_TID, CANNApiColumn::TYPE)
        .ExcuteQuery(flowQuery.fileId, cannApiPOs);
    auto &instance = TrackInfoManager::Instance();
    for (const auto &item : cannApiPOs) {
        FlowPoint startPoint;
        startPoint.type = "s";
        startPoint.id = item.id;
        startPoint.flowId = std::to_string(item.id);
        if (item.timestamp < flowQuery.minTimestamp) {
            ServerLog::Warn("CANN flow timestamp invalid! id: %", item.id);
        }
        startPoint.timestamp = item.timestamp - flowQuery.minTimestamp;
        startPoint.rankId = flowQuery.fileId;
        startPoint.trackId =
            instance.GetTrackId(flowQuery.fileId, std::to_string(item.globalTid), std::to_string(item.type));
        flowPointVec.emplace_back(startPoint);
    }
}
}
