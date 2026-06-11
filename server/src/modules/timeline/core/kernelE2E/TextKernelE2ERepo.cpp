/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "pch.h"
#include <algorithm>
#include <set>
#include "FlowAnalyzer.h"
#include "ProtocolDefs.h"
#include "ServerLog.h"
#include "TextKernelE2ERepo.h"
#include "TimelineProtocolRequest.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

namespace {
constexpr size_t KERNEL_E2E_SQLITE_PARAM_LIMIT = 900;
constexpr auto TEXT_PYTHON_FUNCTION_CAT = "python_function";
const std::string TEXT_PYTHON_STACK_THREAD_ID_PREFIX = Dic::Protocol::PYTHON_STACK_THREAD_ID_PREFIX + "text:";

std::string BuildPlaceholders(size_t count) {
    std::string placeholders;
    for (size_t index = 0; index < count; ++index) {
        if (index != 0) {
            placeholders += ",";
        }
        placeholders += "?";
    }
    return placeholders;
}

std::string BuildBaseSliceSql(const std::string &nameFilterSql, const std::string &orderBySql, bool useTimeSearch) {
    std::string sql = "SELECT s.id AS id, s.name AS name, s.timestamp AS startNs, s.end_time AS endNs, "
                      "s.track_id AS trackId, s.cat AS cat, t.pid AS pid, t.tid AS tid, p.process_name AS processName "
                      "FROM slice s LEFT JOIN thread t ON s.track_id = t.track_id "
                      "LEFT JOIN process p ON t.pid = p.pid ";
    sql += useTimeSearch ? "WHERE s.end_time >= ? AND s.timestamp <= ? AND " : "WHERE ";
    sql += nameFilterSql + " ";
    sql += orderBySql;
    return sql;
}

}

std::vector<KernelE2EEvent> TextKernelE2ERepo::QueryPythonApiEvents(const KernelE2EQuery &query) {
    auto events = QuerySliceEvents(query,
        "(p.process_name = 'Python' AND (s.name LIKE 'Enqueue%' OR s.name LIKE 'Dequeue%' OR "
        "s.name LIKE '<built-in%' OR s.name LIKE 'aten::%' OR s.name LIKE 'npu::%' OR s.name LIKE 'vllm::%'))",
        "ORDER BY s.track_id ASC, s.timestamp ASC, s.id ASC");
    std::vector<KernelE2EEvent> filteredEvents;
    filteredEvents.reserve(events.size());
    for (auto &event : events) {
        ClassifyPythonEvent(event);
        if (!event.eventType.empty()) {
            filteredEvents.emplace_back(std::move(event));
        }
    }
    return filteredEvents;
}

std::vector<KernelE2EEvent> TextKernelE2ERepo::QueryCannApiEvents(const KernelE2EQuery &query) {
    auto events = QuerySliceEvents(query,
        "(p.process_name = 'CANN' AND (s.name LIKE '%aclopCompileAndExecute%' OR "
        "s.name LIKE 'aclnn%' OR s.name LIKE 'AscendCL@aclnn%' OR "
        "s.name = 'launch' OR s.name = 'Node@launch' OR s.name LIKE '%launch%'))",
        "ORDER BY s.track_id ASC, s.timestamp ASC, s.id ASC");
    std::vector<KernelE2EEvent> filteredEvents;
    filteredEvents.reserve(events.size());
    for (auto &event : events) {
        ClassifyCannEvent(event);
        if (!event.eventType.empty()) {
            filteredEvents.emplace_back(std::move(event));
        }
    }
    return filteredEvents;
}

std::vector<KernelE2EEvent> TextKernelE2ERepo::QueryHardwareTaskEvents(const KernelE2EQuery &query,
    const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents) {
    (void)pythonEvents;
    (void)cannEvents;
    auto events = QuerySliceEvents(query, "(p.process_name = 'Ascend Hardware')", "ORDER BY s.timestamp ASC, s.id ASC");
    for (auto &event : events) {
        ClassifyHardwareEvent(event);
    }
    return events;
}

std::vector<KernelE2EFlow> TextKernelE2ERepo::QueryFlows(const KernelE2EQuery &query,
    const std::vector<KernelE2EEvent> &pythonEvents, const std::vector<KernelE2EEvent> &cannEvents,
    const std::vector<KernelE2EEvent> &hardwareTasks) {
    (void)pythonEvents;
    (void)cannEvents;
    (void)hardwareTasks;
    const auto seedPoints = QueryFlowPointsByCategories(query);
    std::set<std::string> flowIdSet;
    for (const auto &point : seedPoints) {
        if (!point.flowId.empty()) {
            flowIdSet.emplace(point.flowId);
        }
    }
    const std::vector<std::string> flowIds(flowIdSet.begin(), flowIdSet.end());
    const auto allPoints = QueryFlowPointsByFlowIds(flowIds);
    std::map<std::string, std::vector<TextFlowPoint>> pointsByFlowId;
    for (const auto &point : allPoints) {
        pointsByFlowId[point.flowId].emplace_back(point);
    }

    std::vector<KernelE2EFlow> flows;
    for (const auto &[flowId, points] : pointsByFlowId) {
        // Current text traces are expected to provide one start/end pair per flow_id; multi-endpoint flow_id should be
        // expanded by ordered start/end pairing when such traces need to be supported.
        std::optional<TextFlowPoint> startPoint;
        std::optional<TextFlowPoint> endPoint;
        for (const auto &point : points) {
            if (IsFlowStart(point.type) && !startPoint.has_value()) {
                startPoint = point;
            } else if (IsFlowEnd(point.type) && !endPoint.has_value()) {
                endPoint = point;
            }
        }
        if (!startPoint.has_value() || !endPoint.has_value()) {
            ServerLog::Warn("TextKernelE2ERepo: skip incomplete flow: ", flowId);
            continue;
        }
        auto fromEvent = MapFlowPointToEvent(startPoint.value());
        auto toEvent = MapFlowPointToEvent(endPoint.value());
        if (!fromEvent.has_value() || !toEvent.has_value()) {
            ServerLog::Warn("TextKernelE2ERepo: skip flow without slice endpoint: ", flowId);
            continue;
        }
        flows.emplace_back(
            BuildFlowFromPoints(startPoint.value(), fromEvent.value(), endPoint.value(), toEvent.value()));
    }
    return flows;
}

std::vector<KernelE2EEvent> TextKernelE2ERepo::QuerySliceEvents(
    const KernelE2EQuery &query, const std::string &nameFilterSql, const std::string &orderBySql) const {
    std::vector<KernelE2EEvent> events;
    if (database_ == nullptr) {
        return events;
    }
    const bool useTimeSearch = query.startNs != query.endNs;
    const auto sql = BuildBaseSliceSql(nameFilterSql, orderBySql, useTimeSearch);
    const auto stmt = database_->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to prepare slice event query.");
        return events;
    }
    if (useTimeSearch) {
        stmt->BindParams(query.startNs, query.endNs);
    }
    const auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to execute slice event query.");
        return events;
    }
    while (resultSet->Next()) {
        events.emplace_back(BuildEventFromResultSet(resultSet));
    }
    return events;
}

KernelE2EEvent TextKernelE2ERepo::BuildEventFromResultSet(
    const std::unique_ptr<Dic::Module::SqliteResultSet> &resultSet) const {
    KernelE2EEvent event;
    event.id = resultSet->GetUint64("id");
    event.name = resultSet->GetString("name");
    event.startNs = resultSet->GetUint64("startNs");
    event.endNs = resultSet->GetUint64("endNs");
    event.trackId = resultSet->GetUint64("trackId");
    event.pid = resultSet->GetString("pid");
    const auto tid = resultSet->GetString("tid");
    event.tid =
        resultSet->GetString("cat") == TEXT_PYTHON_FUNCTION_CAT ? TEXT_PYTHON_STACK_THREAD_ID_PREFIX + tid : tid;
    event.rankId = fileId_;
    event.globalTid = event.trackId;
    return event;
}

std::optional<KernelE2EEvent> TextKernelE2ERepo::QuerySliceEventById(uint64_t sliceId) const {
    const auto iter = eventsBySliceId_.find(sliceId);
    if (iter != eventsBySliceId_.end()) {
        return iter->second;
    }
    if (database_ == nullptr) {
        return std::nullopt;
    }
    std::string sql = "SELECT s.id AS id, s.name AS name, s.timestamp AS startNs, s.end_time AS endNs, "
                      "s.track_id AS trackId, s.cat AS cat, t.pid AS pid, t.tid AS tid, p.process_name AS processName "
                      "FROM slice s LEFT JOIN thread t ON s.track_id = t.track_id "
                      "LEFT JOIN process p ON t.pid = p.pid WHERE s.id = ?";
    auto stmt = database_->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to prepare slice by id query.");
        return std::nullopt;
    }
    stmt->BindParams(sliceId);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr || !resultSet->Next()) {
        return std::nullopt;
    }
    auto event = BuildEventFromResultSet(resultSet);
    ClassifyPythonEvent(event);
    if (event.eventType.empty()) {
        ClassifyCannEvent(event);
    }
    if (event.eventType.empty()) {
        ClassifyHardwareEvent(event);
    }
    eventsBySliceId_[sliceId] = event;
    return event;
}

std::vector<TextKernelE2ERepo::TextFlowPoint> TextKernelE2ERepo::QueryFlowPointsByCategories(
    const KernelE2EQuery &query) const {
    std::vector<TextFlowPoint> points;
    if (database_ == nullptr) {
        return points;
    }
    std::string sql = "SELECT id, flow_id AS flowId, name, cat, track_id AS trackId, timestamp, type FROM flow "
                      "WHERE cat IN ('async_task_queue', 'async_npu', 'HostToDevice') ";
    const bool useTimeSearch = query.startNs != query.endNs;
    if (useTimeSearch) {
        sql += "AND timestamp >= ? AND timestamp <= ? ";
    }
    sql += "ORDER BY flow_id ASC, timestamp ASC, track_id ASC, id ASC";
    auto stmt = database_->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to prepare flow category query.");
        return points;
    }
    if (useTimeSearch) {
        stmt->BindParams(query.startNs, query.endNs);
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to execute flow category query.");
        return points;
    }
    while (resultSet->Next()) {
        TextFlowPoint point;
        point.id = resultSet->GetUint64("id");
        point.flowId = resultSet->GetString("flowId");
        point.name = resultSet->GetString("name");
        point.cat = resultSet->GetString("cat");
        point.trackId = resultSet->GetUint64("trackId");
        point.timestamp = resultSet->GetUint64("timestamp");
        point.type = resultSet->GetString("type");
        points.emplace_back(std::move(point));
    }
    return points;
}

std::vector<TextKernelE2ERepo::TextFlowPoint> TextKernelE2ERepo::QueryFlowPointsByFlowIds(
    const std::vector<std::string> &flowIds) const {
    std::vector<TextFlowPoint> points;
    if (database_ == nullptr || flowIds.empty()) {
        return points;
    }
    for (size_t offset = 0; offset < flowIds.size(); offset += KERNEL_E2E_SQLITE_PARAM_LIMIT) {
        const size_t batchSize = std::min(KERNEL_E2E_SQLITE_PARAM_LIMIT, flowIds.size() - offset);
        std::string sql = "SELECT id, flow_id AS flowId, name, cat, track_id AS trackId, timestamp, type FROM flow "
                          "WHERE flow_id IN (" +
            BuildPlaceholders(batchSize) +
            ") "
            "ORDER BY flow_id ASC, timestamp ASC, track_id ASC, id ASC";
        auto stmt = database_->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Error("TextKernelE2ERepo: failed to prepare flow id query.");
            continue;
        }
        for (size_t index = 0; index < batchSize; ++index) {
            stmt->BindParams(flowIds[offset + index]);
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Error("TextKernelE2ERepo: failed to execute flow id query.");
            continue;
        }
        while (resultSet->Next()) {
            TextFlowPoint point;
            point.id = resultSet->GetUint64("id");
            point.flowId = resultSet->GetString("flowId");
            point.name = resultSet->GetString("name");
            point.cat = resultSet->GetString("cat");
            point.trackId = resultSet->GetUint64("trackId");
            point.timestamp = resultSet->GetUint64("timestamp");
            point.type = resultSet->GetString("type");
            points.emplace_back(std::move(point));
        }
    }
    return points;
}

std::optional<KernelE2EEvent> TextKernelE2ERepo::MapFlowPointToEvent(const TextFlowPoint &flowPoint) const {
    FlowPoint domainFlowPoint;
    domainFlowPoint.id = flowPoint.id;
    domainFlowPoint.flowId = flowPoint.flowId;
    domainFlowPoint.cat = flowPoint.cat;
    domainFlowPoint.trackId = flowPoint.trackId;
    domainFlowPoint.timestamp = flowPoint.timestamp;
    domainFlowPoint.type = flowPoint.type;
    const auto sliceVec = QuerySlicesByTrackId(flowPoint.trackId);
    if (sliceVec.empty()) {
        return std::nullopt;
    }
    FlowAnalyzer analyzer;
    const auto sliceIt = analyzer.ComputeSliceByFlowPoint(domainFlowPoint, sliceVec);
    if (sliceIt == sliceVec.end()) {
        return std::nullopt;
    }
    return QuerySliceEventById(sliceIt->id);
}

std::vector<SliceDomain> TextKernelE2ERepo::QuerySlicesByTrackId(uint64_t trackId) const {
    const auto iter = slicesByTrackId_.find(trackId);
    if (iter != slicesByTrackId_.end()) {
        return iter->second;
    }
    std::vector<SliceDomain> slices;
    if (database_ == nullptr) {
        return slices;
    }
    auto stmt = database_->CreatPreparedStatement(
        "SELECT id, timestamp, end_time AS endTime FROM slice WHERE track_id = ? ORDER BY timestamp ASC, id ASC");
    if (stmt == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to prepare slices by track query.");
        return slices;
    }
    stmt->BindParams(trackId);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("TextKernelE2ERepo: failed to execute slices by track query.");
        return slices;
    }
    while (resultSet->Next()) {
        SliceDomain slice;
        slice.id = resultSet->GetUint64("id");
        slice.timestamp = resultSet->GetUint64("timestamp");
        slice.endTime = resultSet->GetUint64("endTime");
        slices.emplace_back(slice);
    }
    slicesByTrackId_[trackId] = slices;
    return slices;
}

KernelE2EFlow TextKernelE2ERepo::BuildFlowFromPoints(const TextFlowPoint &fromPoint, const KernelE2EEvent &fromEvent,
    const TextFlowPoint &toPoint, const KernelE2EEvent &toEvent) {
    KernelE2EFlow flow;
    flow.cat = fromPoint.cat.empty() ? toPoint.cat : fromPoint.cat;
    flow.flowId = fromPoint.flowId.empty() ? toPoint.flowId : fromPoint.flowId;
    flow.from = fromEvent;
    flow.to = toEvent;
    if (flow.cat == KERNEL_E2E_FLOW_ASYNC_TASK_QUEUE) {
        flow.from.eventType = "ENQUEUE";
        flow.to.eventType = "DEQUEUE";
    } else if (flow.cat == KERNEL_E2E_FLOW_ASYNC_NPU) {
        flow.from.eventType = "PYTHON_OP";
        flow.to.eventType = "HARDWARE";
        if (IsAclnnEvent(flow.to.name)) {
            flow.to.pathType = "ACLNN";
        }
    } else if (flow.cat == KERNEL_E2E_FLOW_HOST_TO_DEVICE) {
        if (IsAclnnEvent(flow.from.name)) {
            flow.from.eventType = "CANN_API";
            flow.from.pathType = "ACLNN";
        } else if (flow.from.eventType.empty()) {
            flow.from.eventType = "LAUNCH";
        }
        flow.to.eventType = "HARDWARE";
        if (IsAclnnEvent(flow.to.name)) {
            flow.to.pathType = "ACLNN";
        }
    }
    return flow;
}

bool TextKernelE2ERepo::IsFlowStart(const std::string &type) { return type == Dic::Protocol::LINE_START; }

bool TextKernelE2ERepo::IsFlowEnd(const std::string &type) {
    return type == Dic::Protocol::LINE_END || type == Dic::Protocol::LINE_END_OPTIONAL;
}

void TextKernelE2ERepo::ClassifyPythonEvent(KernelE2EEvent &event) {
    if (IsEnqueueEvent(event.name)) {
        event.eventType = "ENQUEUE";
    } else if (IsDequeueEvent(event.name)) {
        event.eventType = "DEQUEUE";
    } else if (IsPytorchCallEvent(event.name)) {
        event.eventType = "PYTHON_CALL";
    } else if (IsPytorchOpEvent(event.name)) {
        event.eventType = "PYTHON_OP";
    }
}

void TextKernelE2ERepo::ClassifyCannEvent(KernelE2EEvent &event) {
    if (IsAclopEvent(event.name)) {
        event.eventType = "CANN_API";
        event.pathType = "ACLOP";
    } else if (IsAclnnEvent(event.name)) {
        event.eventType = "CANN_API";
        event.pathType = "ACLNN";
    } else if (IsCannLaunch(event.name)) {
        event.eventType = "LAUNCH";
    }
}

void TextKernelE2ERepo::ClassifyHardwareEvent(KernelE2EEvent &event) {
    event.eventType = "HARDWARE";
    if (IsAclnnEvent(event.name)) {
        event.pathType = "ACLNN";
    }
}

} // namespace Dic::Module::Timeline
