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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "pch.h"
#include <algorithm>
#include <unordered_map>
#include "DbKernelE2ERepo.h"
#include "DataBaseManager.h"
#include "ServerLog.h"
#include "TimelineProtocolRequest.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

namespace {
constexpr uint64_t PYTHON_STACK_API_TYPE = 50003;
}

std::vector<KernelE2EEvent> DbKernelE2ERepo::QueryPythonApiEvents(const KernelE2EQuery &query) {
    std::vector<KernelE2EEvent> events;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(query.rankId);
    if (database == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to get database for rankId: ", query.rankId);
        return events;
    }

    std::string sql = "SELECT api.ROWID AS id, str.value AS name, api.startNs AS startNs, api.endNs AS endNs, "
                      "api.globalTid AS globalTid, api.type AS type, "
                      "COALESCE(conn.connectionId, api.connectionId) AS connectionId "
                      "FROM PYTORCH_API api "
                      "JOIN STRING_IDS str ON api.name = str.id "
                      "LEFT JOIN CONNECTION_IDS conn ON api.connectionId = conn.id ";
    const bool useTimeSearch = query.startNs != query.endNs;
    sql += useTimeSearch ? "WHERE api.endNs >= ? AND api.startNs <= ? AND " : "WHERE ";
    sql += "(str.value LIKE 'Enqueue%' OR str.value LIKE 'Dequeue%' OR str.value LIKE '<built-in%' OR "
           "str.value LIKE 'aten::%' OR str.value LIKE 'npu::%' OR str.value LIKE 'vllm::%') ";
    sql += "ORDER BY api.globalTid ASC, api.startNs ASC, api.ROWID ASC";

    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to prepare Python API query");
        return events;
    }
    if (useTimeSearch) {
        stmt->BindParams(query.startNs, query.endNs);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to execute Python API query");
        return events;
    }

    while (resultSet->Next()) {
        KernelE2EEvent event;
        event.id = resultSet->GetUint64("id");
        event.name = resultSet->GetString("name");
        event.startNs = resultSet->GetUint64("startNs");
        event.endNs = resultSet->GetUint64("endNs");
        event.globalTid = resultSet->GetUint64("globalTid");
        // Keep pid/tid consistent with TraceDatabaseHelper Python slice query.
        event.pid = std::to_string(event.globalTid);
        event.tid = resultSet->GetUint64("type") == PYTHON_STACK_API_TYPE
            ? Protocol::PYTHON_STACK_THREAD_ID_PREFIX + std::to_string(event.globalTid)
            : "pytorch";
        event.connectionId = resultSet->GetInt64("connectionId");
        event.rankId = query.rankId;

        if (IsEnqueueEvent(event.name)) {
            event.eventType = "ENQUEUE";
        } else if (IsDequeueEvent(event.name)) {
            event.eventType = "DEQUEUE";
        } else if (IsPytorchCallEvent(event.name)) {
            event.eventType = "PYTHON_CALL";
        } else if (IsPytorchOpEvent(event.name)) {
            event.eventType = "PYTHON_OP";
        } else {
            continue;
        }

        events.emplace_back(std::move(event));
    }
    return events;
}

std::vector<KernelE2EEvent> DbKernelE2ERepo::QueryCannApiEvents(const KernelE2EQuery &query) {
    std::vector<KernelE2EEvent> events;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(query.rankId);
    if (database == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to get database for rankId: ", query.rankId);
        return events;
    }

    // Query CANN_API joined with STRING_IDS to get name
    // CANN_API.connectionId is stored as the id field (CannApiColumn::ID = "connectionId")
    std::string sql = "SELECT ca.connectionId AS id, str.value AS name, ca.startNs AS startNs, ca.endNs AS endNs, "
                      "ca.globalTid AS globalTid, ca.type AS type "
                      "FROM CANN_API ca "
                      "JOIN STRING_IDS str ON ca.name = str.id ";
    const bool useTimeSearch = query.startNs != query.endNs;
    sql += useTimeSearch ? "WHERE ca.endNs >= ? AND ca.startNs <= ? AND " : "WHERE ";
    sql += "(str.value LIKE '%aclopCompileAndExecute%' OR str.value LIKE 'aclnn%' OR "
           "str.value = 'launch' OR str.value = 'Node@launch' OR str.value LIKE 'launch%') ";
    sql += "ORDER BY ca.globalTid ASC, ca.startNs ASC, ca.connectionId ASC";

    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to prepare CANN API query");
        return events;
    }
    if (useTimeSearch) {
        stmt->BindParams(query.startNs, query.endNs);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to execute CANN API query");
        return events;
    }

    while (resultSet->Next()) {
        KernelE2EEvent event;
        event.id = resultSet->GetUint64("id");
        event.name = resultSet->GetString("name");
        event.startNs = resultSet->GetUint64("startNs");
        event.endNs = resultSet->GetUint64("endNs");
        event.globalTid = resultSet->GetUint64("globalTid");
        // Keep pid/tid consistent with TraceDatabaseHelper CANN slice query.
        event.pid = std::to_string(event.globalTid);
        event.tid = resultSet->GetString("type");
        event.connectionId = static_cast<int64_t>(event.id);
        event.rankId = query.rankId;

        // Classify by name
        if (IsCannLaunch(event.name)) {
            event.eventType = "LAUNCH";
        } else if (IsAclopEvent(event.name)) {
            event.eventType = "CANN_API";
            event.pathType = "ACLOP";
        } else if (IsAclnnEvent(event.name)) {
            event.eventType = "CANN_API";
            event.pathType = "ACLNN";
        } else {
            event.eventType = "CANN_API";
        }

        events.emplace_back(std::move(event));
    }
    return events;
}

std::vector<KernelE2EFlow> DbKernelE2ERepo::QueryFlows(const KernelE2EQuery &query,
    const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents,
    const std::vector<KernelE2EEvent> &hardwareTasks) {
    (void)query;
    std::vector<KernelE2EFlow> flows;
    std::unordered_map<int64_t, std::vector<const KernelE2EEvent *>> enqueuesByConnectionId;
    std::unordered_map<int64_t, std::vector<const KernelE2EEvent *>> tasksByConnectionId;
    for (const auto &event : pythonEvents) {
        if (event.eventType == "ENQUEUE") {
            enqueuesByConnectionId[event.connectionId].push_back(&event);
        }
    }
    for (const auto &task : hardwareTasks) {
        tasksByConnectionId[task.connectionId].push_back(&task);
    }
    for (auto &item : enqueuesByConnectionId) {
        std::sort(item.second.begin(), item.second.end(),
            [](const auto *left, const auto *right) { return left->startNs < right->startNs; });
    }

    for (const auto &dequeue : pythonEvents) {
        if (dequeue.eventType != "DEQUEUE") {
            continue;
        }
        const auto iter = enqueuesByConnectionId.find(dequeue.connectionId);
        if (iter == enqueuesByConnectionId.end()) {
            continue;
        }
        const auto &enqueues = iter->second;
        const auto upper = std::upper_bound(enqueues.begin(), enqueues.end(), dequeue.startNs,
            [](uint64_t startNs, const KernelE2EEvent *event) { return startNs < event->startNs; });
        if (upper == enqueues.begin()) {
            continue;
        }
        const KernelE2EEvent *bestEnqueue = *(upper - 1);
        KernelE2EFlow flow;
        flow.cat = KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE;
        flow.connectionId = dequeue.connectionId;
        flow.flowId = std::to_string(dequeue.connectionId);
        flow.from = *bestEnqueue;
        flow.to = dequeue;
        flows.emplace_back(std::move(flow));
    }

    // connectionId is only a coarse DB-side correlation key for HostToDevice recovery.
    // Precise recovery should further constrain matches by launch/task time windows or an explicit DB relation.
    for (const auto &cannEvent : cannEvents) {
        const auto iter = tasksByConnectionId.find(cannEvent.connectionId);
        if (iter == tasksByConnectionId.end()) {
            continue;
        }
        for (const auto *task : iter->second) {
            KernelE2EFlow flow;
            flow.cat = KERNEL_E2E_FLOW_HOST_TO_DEVICE;
            flow.connectionId = cannEvent.connectionId;
            flow.flowId = std::to_string(cannEvent.connectionId);
            flow.from = cannEvent;
            flow.to = *task;
            flows.emplace_back(std::move(flow));
        }
    }

    for (const auto &pythonEvent : pythonEvents) {
        if (pythonEvent.eventType == "ENQUEUE" || pythonEvent.eventType == "DEQUEUE") {
            continue;
        }
        const auto iter = tasksByConnectionId.find(pythonEvent.connectionId);
        if (iter == tasksByConnectionId.end()) {
            continue;
        }
        for (const auto *task : iter->second) {
            KernelE2EFlow flow;
            flow.cat = KERNEL_E2E_FLOW_ASYNC_NPU;
            flow.connectionId = pythonEvent.connectionId;
            flow.flowId = std::to_string(pythonEvent.connectionId);
            flow.from = pythonEvent;
            flow.to = *task;
            flows.emplace_back(std::move(flow));
        }
    }
    return flows;
}

std::vector<KernelE2EEvent> DbKernelE2ERepo::QueryHardwareTaskEvents(const KernelE2EQuery &query,
    const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) {
    std::vector<KernelE2EEvent> events;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(query.rankId);
    if (database == nullptr) {
        ServerLog::Error("DbKernelE2ERepo: failed to get database for rankId: ", query.rankId);
        return events;
    }

    std::vector<int64_t> candidateConnectionIds;
    std::unordered_map<int64_t, bool> seenConnectionIds;
    auto addConnectionId = [&](int64_t connectionId) {
        if (connectionId < 0 || seenConnectionIds[connectionId]) {
            return;
        }
        seenConnectionIds[connectionId] = true;
        candidateConnectionIds.emplace_back(connectionId);
    };
    for (const auto &event : pythonEvents) {
        if (event.eventType == "PYTHON_OP" || event.eventType == "ENQUEUE" || event.eventType == "DEQUEUE") {
            addConnectionId(event.connectionId);
        }
    }
    for (const auto &event : cannEvents) {
        addConnectionId(event.connectionId);
    }

    // 优化5：原实现当 connectionId 数量超过 900 时直接清空候选列表，退化为按时间范围
    // 全表扫描 TASK，在大数据量下性能极差。改为分批查询，每批不超过 SQLite 占位符
    // 上限（32766），每批 30000 留出余量给时间范围等参数，多批结果合并返回。
    constexpr size_t CONNECTION_ID_BATCH_SIZE = 30000;

    if (candidateConnectionIds.empty()) {
        return events;
    }

    auto queryTaskBatch = [&](const std::vector<int64_t> &batchIds) -> std::vector<KernelE2EEvent> {
        std::vector<KernelE2EEvent> batchEvents;
        // Kernel E2E only needs compute kernels, so skip MSTX and communication schedule joins.
        std::string sql = "SELECT task.ROWID AS id, COALESCE(nameStr.value, typeStr.value, '') AS name, "
                          "task.startNs AS startNs, task.endNs AS endNs, "
                          "task.connectionId AS connectionId, task.streamId AS streamId, "
                          "task.deviceId AS deviceId, task.globalTaskId AS globalTaskId "
                          "FROM TASK task "
                          "LEFT JOIN COMPUTE_TASK_INFO info ON info.globalTaskId = task.globalTaskId "
                          "LEFT JOIN STRING_IDS nameStr ON info.name = nameStr.id "
                          "LEFT JOIN STRING_IDS typeStr ON task.taskType = typeStr.id ";
        const bool useTimeSearch = query.startNs != query.endNs;
        bool hasWhere = false;
        if (useTimeSearch) {
            sql += "WHERE task.endNs >= ? AND task.startNs <= ? ";
            hasWhere = true;
        }
        sql += hasWhere ? "AND " : "WHERE ";
        sql += "task.connectionId IN (";
        for (size_t i = 0; i < batchIds.size(); ++i) {
            if (i != 0) {
                sql += ",";
            }
            sql += "?";
        }
        sql += ") ORDER BY task.startNs ASC, task.ROWID ASC";

        auto stmt = database->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Error("DbKernelE2ERepo: failed to prepare hardware task batch query");
            return batchEvents;
        }
        if (useTimeSearch) {
            stmt->BindParams(query.startNs, query.endNs);
        }
        for (const auto connectionId : batchIds) {
            stmt->BindParams(connectionId);
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Error("DbKernelE2ERepo: failed to execute hardware task batch query");
            return batchEvents;
        }

        while (resultSet->Next()) {
            KernelE2EEvent event;
            event.id = resultSet->GetUint64("id");
            event.name = resultSet->GetString("name");
            event.startNs = resultSet->GetUint64("startNs");
            event.endNs = resultSet->GetUint64("endNs");
            event.connectionId = resultSet->GetInt64("connectionId");
            event.streamId = resultSet->GetUint64("streamId");
            event.deviceId = resultSet->GetUint64("deviceId");
            event.pid = "Ascend Hardware";
            event.tid = std::to_string(event.streamId);
            event.rankId = query.rankId;
            event.eventType = "HARDWARE";

            if (IsAclnnEvent(event.name)) {
                event.pathType = "ACLNN";
            }

            batchEvents.emplace_back(std::move(event));
        }
        return batchEvents;
    };

    for (size_t i = 0; i < candidateConnectionIds.size(); i += CONNECTION_ID_BATCH_SIZE) {
        auto batchEnd = std::min(i + CONNECTION_ID_BATCH_SIZE, candidateConnectionIds.size());
        std::vector<int64_t> batch(candidateConnectionIds.begin() + i, candidateConnectionIds.begin() + batchEnd);
        auto batchResult = queryTaskBatch(batch);
        events.insert(events.end(), batchResult.begin(), batchResult.end());
    }

    return events;
}

} // namespace Dic::Module::Timeline
