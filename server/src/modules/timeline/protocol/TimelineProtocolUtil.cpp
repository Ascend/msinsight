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
#include "TimelineProtocol.h"
#include "TimelineProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
using namespace rapidjson;
#pragma region << Response to json>>
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response) {
    ServerLog::Warn("ToResponseJson is not implemented. command:", response.command);
    return std::nullopt;
}

static void AddKernelE2ERecordJson(
    json_t &itemJson, const KernelE2ETimeRecordDto &record, RAPIDJSON_DEFAULT_ALLOCATOR &allocator);

static void AddKernelE2EHighlightSlicesJson(
    json_t &itemJson, const std::vector<KernelE2EHighlightSliceDto> &slices, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    json_t highlightSlices(kArrayType);
    for (const auto &slice : slices) {
        json_t sliceJson(kObjectType);
        JsonUtil::AddMember(sliceJson, "role", slice.role, allocator);
        JsonUtil::AddMember(sliceJson, "name", slice.name, allocator);
        JsonUtil::AddMember(sliceJson, "startTime", slice.startTime, allocator);
        JsonUtil::AddMember(sliceJson, "duration", slice.duration, allocator);
        if (!slice.pid.empty()) {
            JsonUtil::AddMember(sliceJson, "pid", slice.pid, allocator);
        }
        if (!slice.tid.empty()) {
            JsonUtil::AddMember(sliceJson, "tid", slice.tid, allocator);
        }
        if (!slice.id.empty()) {
            JsonUtil::AddMember(sliceJson, "id", slice.id, allocator);
        }
        if (!slice.missingReason.empty()) {
            JsonUtil::AddMember(sliceJson, "missingReason", slice.missingReason, allocator);
        }
        highlightSlices.PushBack(sliceJson, allocator);
    }
    JsonUtil::AddMember(itemJson, "highlightSlices", highlightSlices, allocator);
}

static void AddKernelE2ERecordJson(
    json_t &itemJson, const KernelE2ETimeRecordDto &record, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    JsonUtil::AddMember(itemJson, "id", record.id, allocator);
    JsonUtil::AddMember(itemJson, "opName", record.opName, allocator);
    JsonUtil::AddMember(itemJson, "pathType", record.pathType, allocator);
    JsonUtil::AddMember(itemJson, "isParent", record.isParent, allocator);
    if (record.prepareTime.has_value()) {
        JsonUtil::AddMember(itemJson, "prepareTime", record.prepareTime.value(), allocator);
    }
    if (record.pythonApiTime.has_value()) {
        JsonUtil::AddMember(itemJson, "pythonApiTime", record.pythonApiTime.value(), allocator);
    }
    if (record.enqueueTime.has_value()) {
        JsonUtil::AddMember(itemJson, "enqueueTime", record.enqueueTime.value(), allocator);
    }
    if (record.queueTime.has_value()) {
        JsonUtil::AddMember(itemJson, "queueTime", record.queueTime.value(), allocator);
    }
    if (record.pipeline2Time.has_value()) {
        JsonUtil::AddMember(itemJson, "pipeline2Time", record.pipeline2Time.value(), allocator);
    }
    if (record.launchTime.has_value()) {
        JsonUtil::AddMember(itemJson, "launchTime", record.launchTime.value(), allocator);
    }
    if (record.endToEndTime.has_value()) {
        JsonUtil::AddMember(itemJson, "endToEndTime", record.endToEndTime.value(), allocator);
    }
    JsonUtil::AddMember(itemJson, "status", record.status, allocator);
    JsonUtil::AddMember(itemJson, "diagnostic", record.diagnostic, allocator);
    AddKernelE2EHighlightSlicesJson(itemJson, record.highlightSlices, allocator);
    if (!record.children.empty()) {
        json_t children(kArrayType);
        for (const auto &child : record.children) {
            json_t childJson(kObjectType);
            AddKernelE2ERecordJson(childJson, child, allocator);
            children.PushBack(childJson, allocator);
        }
        JsonUtil::AddMember(itemJson, "children", children, allocator);
    }
}

static void BuildImportActionJson(
    const std::vector<Action> &actions, json_t &result, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    for (const Action &action : actions) {
        json_t actionJson(kObjectType);
        JsonUtil::AddMember(actionJson, "cardName", action.cardName, allocator);
        JsonUtil::AddMember(actionJson, "rankId", action.rankId, allocator);
        JsonUtil::AddMember(actionJson, "cardPath", action.cardPath, allocator);
        JsonUtil::AddMember(actionJson, "result", action.result, allocator);
        JsonUtil::AddMember(actionJson, "dbPath", action.fileId, allocator);
        JsonUtil::AddMember(actionJson, "cluster", action.cluster, allocator);
        JsonUtil::AddMember(actionJson, "projectType", action.projectType, allocator);
        json_t dataPathList(kArrayType);
        for (const auto &item : action.dataPathList) {
            dataPathList.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
        }
        JsonUtil::AddMember(actionJson, "dataPathList", dataPathList, allocator);
        JsonUtil::AddMember(actionJson, "host", action.host, allocator);
        result.PushBack(actionJson, allocator);
    }
}

template <> std::optional<document_t> ToResponseJson<ImportActionResponse>(const ImportActionResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    SetBodyAtt(response, allocator, body);

    json_t coreList(kArrayType);
    for (const auto &core : response.body.coreList) {
        coreList.PushBack(json_t().SetString(core.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "coreList", coreList, allocator);

    json_t sourceList(kArrayType);
    for (const auto &source : response.body.sourceList) {
        sourceList.PushBack(json_t().SetString(source.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "sourceList", sourceList, allocator);

    json_t projectFileTree(kArrayType);
    for (const auto &fileTree : response.body.projectFileTree) {
        projectFileTree.PushBack(fileTree->SerializeToJson(allocator), allocator);
    }
    JsonUtil::AddMember(body, "children", projectFileTree, allocator);
    json_t result(kArrayType);
    BuildImportActionJson(response.body.result, result, allocator);
    JsonUtil::AddMember(body, "result", result, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

void SetBodyAtt(
    const ImportActionResponse &response, MemoryPoolAllocator<::rapidjson::CrtAllocator> &allocator, json_t &body) {
    JsonUtil::AddMember(body, "isOnlyTraceJson", response.body.isOnlyTraceJson, allocator);
    JsonUtil::AddMember(body, "isCluster", response.body.isCluster, allocator);
    JsonUtil::AddMember(body, "reset", response.body.reset, allocator);
    JsonUtil::AddMember(body, "isSimulation", response.body.isSimulation, allocator);
    JsonUtil::AddMember(body, "isBinary", response.body.isBinary, allocator);
    JsonUtil::AddMember(body, "isIpynb", response.body.isIpynb, allocator);
    JsonUtil::AddMember(body, "isPending", response.body.isPending, allocator);
    JsonUtil::AddMember(body, "hasCachelineRecords", response.body.hasCachelineRecords, allocator);
    JsonUtil::AddMember(body, "instrVersion", response.body.version, allocator);
    JsonUtil::AddMember(body, "isLeaks", response.body.isLeaks, allocator);
    JsonUtil::AddMember(body, "isIE", response.body.isIE, allocator);
    JsonUtil::AddMember(body, "isMultiDevice", response.body.isMultiDevice, allocator);
    JsonUtil::AddMember(body, "isTriton", response.body.isTriton, allocator);
    JsonUtil::AddMember(body, "isFtrace", response.body.isFtrace, allocator);
}

template <>
std::optional<document_t> ToResponseJson<UnitThreadTracesResponse>(const UnitThreadTracesResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const std::vector<ThreadTraces> &array : response.body.data) {
        json_t threadTracesArray(kArrayType);
        for (const ThreadTraces &threadTraces : array) {
            json_t threadJson(kObjectType);
            JsonUtil::AddMember(threadJson, "name", threadTraces.name, allocator);
            JsonUtil::AddMember(threadJson, "duration", threadTraces.duration, allocator);
            JsonUtil::AddMember(threadJson, "startTime", threadTraces.startTime, allocator);
            JsonUtil::AddMember(threadJson, "endTime", threadTraces.endTime, allocator);
            JsonUtil::AddMember(threadJson, "depth", threadTraces.depth, allocator);
            JsonUtil::AddMember(threadJson, "threadId", threadTraces.threadId, allocator);
            JsonUtil::AddMember(threadJson, "cname", threadTraces.cname, allocator);
            JsonUtil::AddMember(threadJson, "id", threadTraces.id, allocator);
            threadTracesArray.PushBack(threadJson, allocator);
        }
        data.PushBack(threadTracesArray, allocator);
    }
    JsonUtil::AddMember(body, "maxDepth", response.body.maxDepth, allocator);
    JsonUtil::AddMember(body, "currentMaxDepth", response.body.currentMaxDepth, allocator);
    JsonUtil::AddMember(body, "havePythonFunction", response.body.havePythonFunction, allocator);
    JsonUtil::AddMember(body, "isLoading", response.body.isLoading, allocator);
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<CreateCurveResponse>(const CreateCurveResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "curveName", response.body.curveName, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<UnitThreadTracesSummaryResponse>(
    const UnitThreadTracesSummaryResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const auto &summary : response.body.data) {
        json_t jsonSummary(kObjectType);
        JsonUtil::AddMember(jsonSummary, "startTime", summary.startTime, allocator);
        JsonUtil::AddMember(jsonSummary, "duration", summary.duration, allocator);
        data.PushBack(jsonSummary, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<UnitThreadsResponse>(const UnitThreadsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "emptyFlag", response.body.emptyFlag, allocator);
    json_t data(kArrayType);
    for (const SliceGroupItem &sliceGroupItem : response.body.data) {
        json_t threadsJson(kObjectType);
        JsonUtil::AddMember(threadsJson, "title", sliceGroupItem.title, allocator);
        JsonUtil::AddMember(threadsJson, "wallDuration", sliceGroupItem.wallDuration, allocator);
        JsonUtil::AddMember(threadsJson, "occurrences", sliceGroupItem.occurrences, allocator);
        JsonUtil::AddMember(threadsJson, "avgWallDuration", sliceGroupItem.avgWallDuration, allocator);
        JsonUtil::AddMember(threadsJson, "maxWallDuration", sliceGroupItem.maxWallDuration, allocator);
        JsonUtil::AddMember(threadsJson, "minWallDuration", sliceGroupItem.minWallDuration, allocator);
        JsonUtil::AddMember(threadsJson, "selfTime", sliceGroupItem.selfTime, allocator);
        json_t processesJson(kArrayType);
        for (const auto &[key, value] : sliceGroupItem.processMap) {
            json_t processJson(kObjectType);
            JsonUtil::AddMember(processJson, "pid", key, allocator);
            json_t tidJson(kArrayType);
            for (const auto &item : value) {
                tidJson.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
            }
            JsonUtil::AddMember(processJson, "tidList", tidJson, allocator);
            processesJson.PushBack(processJson, allocator);
        }
        JsonUtil::AddMember(threadsJson, "processes", processesJson, allocator);
        std::vector<std::string> metaTypeList(sliceGroupItem.metaTypeList.begin(), sliceGroupItem.metaTypeList.end());
        JsonUtil::AddMember(threadsJson, "metaTypeList", metaTypeList, allocator);
        data.PushBack(threadsJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<UnitThreadDetailResponse>(const UnitThreadDetailResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "emptyFlag", response.body.emptyFlag, allocator);
    json_t data(kObjectType);
    JsonUtil::AddMember(data, "selfTime", response.body.data.selfTime, allocator);
    JsonUtil::AddMember(data, "args", response.body.data.args, allocator);
    JsonUtil::AddMember(data, "title", response.body.data.title, allocator);
    JsonUtil::AddMember(data, "rawStartTime", std::to_string(response.body.data.rawTimestamp), allocator);
    JsonUtil::AddMember(data, "rawEndTime", std::to_string(response.body.data.rawEndstamp), allocator);
    JsonUtil::AddMember(data, "duration", response.body.data.duration, allocator);
    JsonUtil::AddMember(data, "cat", response.body.data.cat, allocator);
    JsonUtil::AddMember(data, "inputShapes", response.body.data.inputShapes, allocator);
    JsonUtil::AddMember(data, "inputDataTypes", response.body.data.inputDataTypes, allocator);
    JsonUtil::AddMember(data, "inputFormats", response.body.data.inputFormats, allocator);
    JsonUtil::AddMember(data, "outputShapes", response.body.data.outputShapes, allocator);
    JsonUtil::AddMember(data, "outputDataTypes", response.body.data.outputDataTypes, allocator);
    JsonUtil::AddMember(data, "outputFormats", response.body.data.outputFormats, allocator);
    JsonUtil::AddMember(data, "attrInfo", response.body.data.attrInfo, allocator);
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

json_t FlowLocationToJson(const FlowLocation &flowLocation, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "pid", flowLocation.pid, allocator);
    JsonUtil::AddMember(json, "tid", flowLocation.tid, allocator);
    JsonUtil::AddMember(json, "timestamp", flowLocation.timestamp, allocator);
    JsonUtil::AddMember(json, "duration", flowLocation.duration, allocator);
    JsonUtil::AddMember(json, "depth", flowLocation.depth, allocator);
    JsonUtil::AddMember(json, "name", flowLocation.name, allocator);
    JsonUtil::AddMember(json, "id", flowLocation.id, allocator);
    JsonUtil::AddMember(json, "metaType", flowLocation.metaType, allocator);
    JsonUtil::AddMember(json, "rankId", flowLocation.rankId, allocator);
    return json;
}

template <> std::optional<document_t> ToResponseJson<UnitFlowsResponse>(const UnitFlowsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t unitAllFlows(kArrayType);
    for (const auto &item : response.body.unitAllFlows) {
        json_t unitCatFlows(kObjectType);
        JsonUtil::AddMember(unitCatFlows, "cat", item.cat, allocator);
        json_t flows(kArrayType);
        for (const auto &flow : item.flows) {
            json_t flowJson(kObjectType);
            JsonUtil::AddMember(flowJson, "title", flow.title, allocator);
            JsonUtil::AddMember(flowJson, "cat", flow.cat, allocator);
            JsonUtil::AddMember(flowJson, "id", flow.id, allocator);
            JsonUtil::AddMember(flowJson, "from", FlowLocationToJson(flow.from, allocator), allocator);
            JsonUtil::AddMember(flowJson, "to", FlowLocationToJson(flow.to, allocator), allocator);
            flows.PushBack(flowJson, allocator);
        }
        JsonUtil::AddMember(unitCatFlows, "flows", flows, allocator);
        unitAllFlows.PushBack(unitCatFlows, allocator);
    }
    JsonUtil::AddMember(body, "unitAllFlows", unitAllFlows, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SetCardAliasResponse>(const SetCardAliasResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>(std::move(json));
}

template <> std::optional<document_t> ToResponseJson<ResetWindowResponse>(const ResetWindowResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(json, "body", body, json.GetAllocator());
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SearchCountResponse>(const SearchCountResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "totalCount", response.body.totalCount, allocator);
    json_t countList(kArrayType);
    for (const auto &data : response.body.countList) {
        json_t tmp(kObjectType);
        JsonUtil::AddMember(tmp, "rankId", data.rankId, allocator);
        JsonUtil::AddMember(tmp, "dbPath", data.dbPath, allocator);
        JsonUtil::AddMember(tmp, "count", data.count, allocator);
        countList.PushBack(tmp, allocator);
    }
    JsonUtil::AddMember(body, "countList", countList, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SearchSliceResponse>(const SearchSliceResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    JsonUtil::AddMember(body, "pid", response.body.pid, allocator);
    JsonUtil::AddMember(body, "tid", response.body.tid, allocator);
    JsonUtil::AddMember(body, "id", response.body.id, allocator);
    JsonUtil::AddMember(body, "dbPath", response.body.dbPath, allocator);
    JsonUtil::AddMember(body, "startTime", response.body.startTime, allocator);
    JsonUtil::AddMember(body, "duration", response.body.duration, allocator);
    JsonUtil::AddMember(body, "depth", response.body.depth, allocator);
    JsonUtil::AddMember(body, "metaType", response.body.metaType, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<RemoteDeleteResponse>(const RemoteDeleteResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "startTimeUpdated", response.body.startTimeUpdated, allocator);
    JsonUtil::AddMember(body, "maxTimeStamp", response.body.maxTimeStamp, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<FlowCategoryListResponse>(const FlowCategoryListResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t category(kArrayType);
    for (const std::string &cat : response.body.category) {
        category.PushBack(json_t().SetString(cat.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "category", category, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

json_t FlowEventLocationToJson(const FlowLocation &flowLocation, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "pid", flowLocation.pid, allocator);
    JsonUtil::AddMember(json, "tid", flowLocation.tid, allocator);
    JsonUtil::AddMember(json, "timestamp", flowLocation.timestamp, allocator);
    JsonUtil::AddMember(json, "depth", flowLocation.depth, allocator);
    JsonUtil::AddMember(json, "rankId", flowLocation.rankId, allocator);
    return json;
}

template <>
std::optional<document_t> ToResponseJson<FlowCategoryEventsResponse>(const FlowCategoryEventsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t flowDetailList(kArrayType);
    for (const auto &flowDetail : response.body.flowDetailList) {
        json_t flowDetailJson(kObjectType);
        JsonUtil::AddMember(flowDetailJson, "category", flowDetail->cat, allocator);
        JsonUtil::AddMember(flowDetailJson, "from", FlowEventLocationToJson(flowDetail->from, allocator), allocator);
        JsonUtil::AddMember(flowDetailJson, "to", FlowEventLocationToJson(flowDetail->to, allocator), allocator);
        flowDetailList.PushBack(flowDetailJson, allocator);
    }
    JsonUtil::AddMember(body, "flowDetailList", flowDetailList, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<UnitCounterResponse>(const UnitCounterResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    std::string error;
    for (const auto &counterData : response.body.data) {
        json_t tmp(kObjectType);
        JsonUtil::AddMember(tmp, "timestamp", counterData.timestamp, allocator);
        auto tryParse = JsonUtil::TryParse(counterData.valueJsonStr, error);
        if (tryParse.has_value()) {
            Value value(tryParse.value(), allocator);
            JsonUtil::AddMember(tmp, "value", value, allocator);
        } else {
            ServerLog::Warn("Failed to parse unit counter value. ", error);
        }
        data.PushBack(tmp, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SystemViewResponse>(const SystemViewResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t systemViewDetails(kArrayType);
    for (const SystemViewDetail &systemView : response.body.systemViewDetail) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "name", systemView.name, allocator);
        JsonUtil::AddMember(itemJson, "time", systemView.time, allocator);
        JsonUtil::AddMember(itemJson, "totalTime", systemView.totalTime, allocator);
        JsonUtil::AddMember(itemJson, "numberCalls", systemView.numberCalls, allocator);
        JsonUtil::AddMember(itemJson, "avg", systemView.avg, allocator);
        JsonUtil::AddMember(itemJson, "min", systemView.min, allocator);
        JsonUtil::AddMember(itemJson, "max", systemView.max, allocator);
        systemViewDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "systemViewDetails", systemViewDetails, allocator);
    JsonUtil::AddMember(body, "count", response.body.total, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

// SystemViewTraceResponse
template <> std::optional<document_t> ToResponseJson<SystemViewTraceResponse>(const SystemViewTraceResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t systemViewDetails(kArrayType);
    for (const SystemViewTraceDetail &systemView : response.body.systemViewDetail) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "name", systemView.name, allocator);
        JsonUtil::AddMember(itemJson, "startTime", systemView.startTime, allocator);
        JsonUtil::AddMember(itemJson, "duration", systemView.duration, allocator);
        systemViewDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "systemViewDetails", systemViewDetails, allocator);
    JsonUtil::AddMember(body, "count", response.body.total, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ExpAnaAICoreFreqResponse>(const ExpAnaAICoreFreqResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    JsonUtil::AddMember(body, "pid", response.body.pid, allocator);
    JsonUtil::AddMember(body, "tid", response.body.tid, allocator);
    JsonUtil::AddMember(body, "hasProblem", response.body.hasProblem, allocator);
    JsonUtil::AddMember(body, "percent", response.body.percent, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<EventsViewResponse>(const EventsViewResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t eventsDetailList(kArrayType);
    for (const auto &item : response.body.eventDetailList) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "id", item->id, allocator);
        JsonUtil::AddMember(itemJson, "name", item->name, allocator);
        JsonUtil::AddMember(itemJson, "start", item->startTime, allocator);
        JsonUtil::AddMember(itemJson, "duration", item->duration, allocator);
        JsonUtil::AddMember(itemJson, "depth", item->depth, allocator);
        JsonUtil::AddMember(itemJson, "threadId", item->threadId, allocator);
        JsonUtil::AddMember(itemJson, "processId", item->processId, allocator);
        if (dynamic_cast<HostEventDetail *>(item.get())) {
            auto detail = dynamic_cast<HostEventDetail *>(item.get());
            JsonUtil::AddMember(itemJson, "tid", detail->tid, allocator);
            JsonUtil::AddMember(itemJson, "pid", detail->pid, allocator);
        } else if (dynamic_cast<DeviceEventDetail *>(item.get())) {
            auto detail = dynamic_cast<DeviceEventDetail *>(item.get());
            JsonUtil::AddMember(itemJson, "threadName", detail->threadName, allocator);
            JsonUtil::AddMember(itemJson, "rankId", detail->rankId, allocator);
        } else {
            ServerLog::Warn("Events View Response.dynamic cast failed.");
            continue;
        }
        eventsDetailList.PushBack(itemJson, allocator);
    }
    json_t columnList(kArrayType);
    for (const auto &item : response.body.columnList) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "name", item.name, allocator);
        JsonUtil::AddMember(itemJson, "type", item.type, allocator);
        JsonUtil::AddMember(itemJson, "key", item.key, allocator);
        columnList.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "eventDetails", eventsDetailList, allocator);
    JsonUtil::AddMember(body, "columnList", columnList, allocator);
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<ParseCardsResponse>(const ParseCardsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "isContinueParse", response.body.isContinueParse, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<KernelDetailsResponse>(const KernelDetailsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t kernelDetails(kArrayType);
    for (const KernelDetail &detail : response.body.kernelDetails) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "id", detail.id, allocator);
        JsonUtil::AddMember(itemJson, "name", detail.name, allocator);
        JsonUtil::AddMember(itemJson, "type", detail.type, allocator);
        JsonUtil::AddMember(itemJson, "acceleratorCore", detail.acceleratorCore, allocator);
        JsonUtil::AddMember(itemJson, "startTime", detail.startTime, allocator);
        JsonUtil::AddMember(itemJson, "duration", detail.duration, allocator);
        JsonUtil::AddMember(itemJson, "waitTime", detail.waitTime, allocator);
        JsonUtil::AddMember(itemJson, "taskId", detail.taskId, allocator);
        JsonUtil::AddMember(itemJson, "blockNum", detail.blockNum, allocator);
        JsonUtil::AddMember(itemJson, "inputShapes", detail.inputShapes, allocator);
        JsonUtil::AddMember(itemJson, "inputDataTypes", detail.inputDataTypes, allocator);
        JsonUtil::AddMember(itemJson, "inputFormats", detail.inputFormats, allocator);
        JsonUtil::AddMember(itemJson, "outputShapes", detail.outputShapes, allocator);
        JsonUtil::AddMember(itemJson, "outputDataTypes", detail.outputDataTypes, allocator);
        JsonUtil::AddMember(itemJson, "outputFormats", detail.outputFormats, allocator);
        kernelDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "kernelDetails", kernelDetails, allocator);
    json_t acceleratorCoreList(kArrayType);
    for (const auto &item : response.body.acceleratorCoreList) {
        acceleratorCoreList.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "acceleratorCoreList", acceleratorCoreList, allocator);
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<KernelE2ETimeResponse>(const KernelE2ETimeResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t records(kArrayType);
    for (const auto &record : response.body.records) {
        json_t itemJson(kObjectType);
        AddKernelE2ERecordJson(itemJson, record, allocator);
        records.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "records", records, allocator);
    JsonUtil::AddMember(body, "totalCount", response.body.totalCount, allocator);
    JsonUtil::AddMember(body, "normalCount", response.body.normalCount, allocator);
    JsonUtil::AddMember(body, "fallbackCount", response.body.fallbackCount, allocator);
    JsonUtil::AddMember(body, "incompleteCount", response.body.incompleteCount, allocator);
    JsonUtil::AddMember(body, "launchMatchRate", response.body.launchMatchRate, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<CommunicationKernelResponse>(const CommunicationKernelResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "id", response.body.id, allocator);
    JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    JsonUtil::AddMember(body, "depth", response.body.depth, allocator);
    JsonUtil::AddMember(body, "threadId", response.body.threadId, allocator);
    JsonUtil::AddMember(body, "pid", response.body.pid, allocator);
    JsonUtil::AddMember(body, "step", response.body.step, allocator);
    JsonUtil::AddMember(body, "group", response.body.group, allocator);
    JsonUtil::AddMember(body, "startTime", response.body.startTime, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<OneKernelResponse>(const OneKernelResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "id", response.body.id, allocator);
    JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    JsonUtil::AddMember(body, "depth", response.body.depth, allocator);
    JsonUtil::AddMember(body, "duration", response.body.duration, allocator);
    JsonUtil::AddMember(body, "threadId", response.body.threadId, allocator);
    JsonUtil::AddMember(body, "pid", response.body.pid, allocator);
    JsonUtil::AddMember(body, "step", response.body.step, allocator);
    JsonUtil::AddMember(body, "group", response.body.group, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<UnitThreadsOperatorsResponse>(const UnitThreadsOperatorsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t sameOperatorsDetails(kArrayType);
    uint64_t index = 0;
    for (const SameOperatorsDetails &sameOperators : response.body.sameOperatorsDetails) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "timestamp", sameOperators.timestamp, allocator);
        JsonUtil::AddMember(itemJson, "duration", sameOperators.duration, allocator);
        if (!sameOperators.id.empty()) { // depth用于支持选中列表功能
            JsonUtil::AddMember(itemJson, "depth", sameOperators.depth, allocator);
            JsonUtil::AddMember(itemJson, "id", sameOperators.id, allocator);
        } else { // name、rankId用于支持overall metric more details列表
            JsonUtil::AddMember(itemJson, "name", sameOperators.name, allocator);
            JsonUtil::AddMember(itemJson, "id", index++, allocator);
        }
        JsonUtil::AddMember(itemJson, "tid", sameOperators.tid, allocator);
        JsonUtil::AddMember(itemJson, "pid", sameOperators.pid, allocator);
        sameOperatorsDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "sameOperatorsDetails", sameOperatorsDetails, allocator);
    if (!response.body.rankId.empty()) {
        JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    }
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<SearchAllSlicesResponse>(const SearchAllSlicesResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t searchAllSlicesDetails(kArrayType);
    for (const SearchAllSlices &searchAllSlices : response.body.searchAllSlices) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "name", searchAllSlices.name, allocator);
        JsonUtil::AddMember(itemJson, "timestamp", searchAllSlices.timestamp, allocator);
        JsonUtil::AddMember(itemJson, "duration", searchAllSlices.duration, allocator);
        JsonUtil::AddMember(itemJson, "depth", searchAllSlices.depth, allocator);
        JsonUtil::AddMember(itemJson, "id", searchAllSlices.id, allocator);
        JsonUtil::AddMember(itemJson, "tid", searchAllSlices.tid, allocator);
        JsonUtil::AddMember(itemJson, "pid", searchAllSlices.pid, allocator);
        JsonUtil::AddMember(itemJson, "rankId", searchAllSlices.rankId, allocator);
        JsonUtil::AddMember(itemJson, "deviceId", searchAllSlices.deviceId, allocator);
        JsonUtil::AddMember(itemJson, "dbPath", searchAllSlices.fileId, allocator);
        JsonUtil::AddMember(itemJson, "metaType", searchAllSlices.metaType, allocator);
        searchAllSlicesDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "searchAllSlicesDetails", searchAllSlicesDetails, allocator);
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "dbPath", response.body.dbPath, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<TableDataNameListResponse>(const TableDataNameListResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t layers(kArrayType);
    for (const auto &item : response.body.layers) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "name", item.first, allocator);
        JsonUtil::AddMember(itemJson, "description", item.second, allocator);
        layers.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "layers", layers, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<TableDataDetailResponse>(const TableDataDetailResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t columnAttr(kArrayType);
    for (const auto &attr : response.body.columnAttr) {
        json_t attrJson = json_t(kObjectType);
        JsonUtil::AddMember(attrJson, "name", attr.name, allocator);
        JsonUtil::AddMember(attrJson, "type", attr.type, allocator);
        JsonUtil::AddMember(attrJson, "key", attr.key, allocator);
        columnAttr.PushBack(attrJson, allocator);
    }
    json_t tableData(kArrayType);
    for (const auto &attr : response.body.columnData) {
        json_t attrJson = json_t(kObjectType);
        for (const auto &item : response.body.columnAttr) {
            auto it = attr.find(item.key);
            if (it == attr.end()) {
                continue;
            }
            JsonUtil::AddMember(attrJson, item.key, it->second, allocator);
        }
        tableData.PushBack(attrJson, allocator);
    }
    JsonUtil::AddMember(body, "totalNum", response.body.totalNum, allocator);
    JsonUtil::AddMember(body, "tableData", tableData, allocator);
    JsonUtil::AddMember(body, "columnAttr", columnAttr, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

json_t SystemViewOverallResToJson(
    const SystemViewOverallRes &res, RAPIDJSON_DEFAULT_ALLOCATOR &allocator, uint64_t depth = 1) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "totalTime", res.totalTime, allocator);
    JsonUtil::AddMember(json, "ratio", res.ratio, allocator);
    if (res.max == 0) {
        JsonUtil::AddMember(json, "nums", "-", allocator);
        JsonUtil::AddMember(json, "avg", "-", allocator);
        JsonUtil::AddMember(json, "max", "-", allocator);
        JsonUtil::AddMember(json, "min", "-", allocator);
    } else {
        JsonUtil::AddMember(json, "nums", res.nums, allocator);
        JsonUtil::AddMember(json, "avg", res.avg, allocator);
        JsonUtil::AddMember(json, "max", res.max, allocator);
        JsonUtil::AddMember(json, "min", res.min, allocator);
    }
    JsonUtil::AddMember(json, "name", res.name, allocator);
    JsonUtil::AddMember(json, "id", res.id, allocator);
    JsonUtil::AddMember(json, "level", res.level, allocator);
    json_t children(kArrayType);
    if (depth < 5) { // No more than 5 layers
        for (const auto &child : res.children) {
            children.PushBack(SystemViewOverallResToJson(child, allocator, depth + 1), allocator);
        }
    }
    if (!children.Empty()) {
        JsonUtil::AddMember(json, "children", children, allocator);
    }
    return json;
}

template <>
std::optional<document_t> ToResponseJson<SystemViewOverallResponse>(const SystemViewOverallResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const auto &item : response.details) {
        data.PushBack(SystemViewOverallResToJson(item, allocator), allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(body, "count", response.pageParam.total, allocator);
    JsonUtil::AddMember(body, "pageSize", response.pageParam.pageSize, allocator);
    JsonUtil::AddMember(body, "current", response.pageParam.current, allocator);
    JsonUtil::AddMember(body, "isLoading", response.isLoading, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<SystemViewFtraceStatResponse>(const SystemViewFtraceStatResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);

    // data
    json_t data(kArrayType);
    for (const auto &row : response.data) {
        json_t rowJson(kObjectType);
        for (const auto &kv : row) {
            JsonUtil::AddMember(rowJson, kv.first, kv.second, allocator);
        }
        data.PushBack(rowJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);

    // pageParam
    JsonUtil::AddMember(body, "current", response.pageParam.current, allocator);
    JsonUtil::AddMember(body, "pageSize", response.pageParam.pageSize, allocator);
    JsonUtil::AddMember(body, "count", response.pageParam.total, allocator);

    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

json_t MemcpyOverallResToJson(const MemcpyOverallRes &res, RAPIDJSON_DEFAULT_ALLOCATOR &allocator, uint64_t depth = 1) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "key", res.key, allocator);
    JsonUtil::AddMember(json, "name", res.name, allocator);
    JsonUtil::AddMember(json, "totalSize", res.totalSize, allocator);
    JsonUtil::AddMember(json, "totalTime", res.totalTime, allocator);
    JsonUtil::AddMember(json, "number", res.number, allocator);
    JsonUtil::AddMember(json, "avgSize", res.avgSize, allocator);
    JsonUtil::AddMember(json, "minSize", res.minSize, allocator);
    JsonUtil::AddMember(json, "maxSize", res.maxSize, allocator);
    JsonUtil::AddMember(json, "avgTime", res.avgTime, allocator);
    JsonUtil::AddMember(json, "minTime", res.minTime, allocator);
    JsonUtil::AddMember(json, "maxTime", res.maxTime, allocator);
    JsonUtil::AddMember(json, "level", res.level, allocator);
    json_t children(kArrayType);
    if (depth < 5) { // No more than 5 layers
        for (const auto &child : res.children) {
            children.PushBack(MemcpyOverallResToJson(child, allocator, depth + 1), allocator);
        }
    }
    if (!children.Empty()) {
        JsonUtil::AddMember(json, "children", children, allocator);
    }
    return json;
}

template <> std::optional<document_t> ToResponseJson<MemcpyOverallResponse>(const MemcpyOverallResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const auto &item : response.details) {
        data.PushBack(MemcpyOverallResToJson(item, allocator), allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(body, "count", response.pageParam.total, allocator);
    JsonUtil::AddMember(body, "pageSize", response.pageParam.pageSize, allocator);
    JsonUtil::AddMember(body, "current", response.pageParam.current, allocator);
    JsonUtil::AddMember(body, "isLoading", response.isLoading, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<MemcpyDetailResponse>(const MemcpyDetailResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t sameOperatorsDetails(kArrayType);
    uint64_t index = 0;
    for (const SameMemcpyDetails &sameOperators : response.body.sameOperatorsDetails) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "timestamp", sameOperators.timestamp, allocator);
        JsonUtil::AddMember(itemJson, "duration", sameOperators.duration, allocator);
        JsonUtil::AddMember(itemJson, "size", sameOperators.size, allocator);
        if (!sameOperators.id.empty()) { // depth用于支持选中列表功能
            JsonUtil::AddMember(itemJson, "id", sameOperators.id, allocator);
        } else {
            JsonUtil::AddMember(itemJson, "id", index++, allocator);
        }
        // name、rankId用于支持overall metric more details列表
        JsonUtil::AddMember(itemJson, "name", sameOperators.name, allocator);
        sameOperatorsDetails.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "sameOperatorsDetails", sameOperatorsDetails, allocator);
    if (!response.body.rankId.empty()) {
        JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    }
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<RankOffsetResponse>(const RankOffsetResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t successArray(kArrayType);
    for (const auto &item : response.body.successList) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "rankId", item.rankId, allocator);
        JsonUtil::AddMember(itemJson, "offset", item.offset, allocator);
        json_t pidArray(kArrayType);
        for (const auto &pid : item.processId) {
            pidArray.PushBack(rapidjson::Value(pid.c_str(), allocator), allocator);
        }
        JsonUtil::AddMember(itemJson, "processId", pidArray, allocator);
        successArray.PushBack(itemJson, allocator);
    }
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "result", successArray, allocator);
    JsonUtil::AddMember(body, "baseOffset", response.body.baseOffset, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    json_t error(kObjectType);
    JsonUtil::AddMember(error, "code", response.body.error.code, allocator);
    JsonUtil::AddMember(error, "message", response.body.error.message, allocator);
    JsonUtil::AddMember(json, "error", error, allocator);
    return std::optional<document_t>{std::move(json)};
}
#pragma endregion

#pragma region << Event to json>>
template <typename EVENT> std::optional<document_t> ToEventJson(const EVENT &event) { return std::nullopt; }

json_t UnitTrackToJson(const UnitTrack &unitTrack, RAPIDJSON_DEFAULT_ALLOCATOR &allocator, uint32_t depth) {
    const static int MAX_DEPTH = 20; // 限制最大递归次数
    json_t json(kObjectType);
    if (depth > MAX_DEPTH) {
        return json;
    }
    JsonUtil::AddMember(json, "type", unitTrack.type, allocator);
    json_t metadata(kObjectType);
    JsonUtil::AddMember(metadata, "cardId", unitTrack.metaData.cardId, allocator);
    JsonUtil::AddMember(metadata, "processId", unitTrack.metaData.processId, allocator);
    JsonUtil::AddMember(metadata, "processName", unitTrack.metaData.processName, allocator);
    JsonUtil::AddMember(metadata, "label", unitTrack.metaData.label, allocator);
    JsonUtil::AddMember(metadata, "threadId", unitTrack.metaData.threadId, allocator);
    JsonUtil::AddMember(metadata, "threadName", unitTrack.metaData.threadName, allocator);
    JsonUtil::AddMember(metadata, "groupNameValue", unitTrack.metaData.groupNameValue, allocator);
    JsonUtil::AddMember(metadata, "rankList", unitTrack.metaData.rankList, allocator);
    JsonUtil::AddMember(metadata, "metaType", unitTrack.metaData.metaType, allocator);
    JsonUtil::AddMember(metadata, "maxDepth", unitTrack.metaData.maxDepth, allocator);
    json_t dataType(kArrayType);
    for (const auto &type : unitTrack.metaData.dataType) {
        dataType.PushBack(json_t().SetString(type.c_str(), type.length(), allocator), allocator);
    }
    JsonUtil::AddMember(metadata, "dataType", dataType, allocator);
    json_t children(kArrayType);
    for (const auto &track : unitTrack.children) {
        children.PushBack(UnitTrackToJson(*track, allocator, depth + 1), allocator);
    }
    JsonUtil::AddMember(json, "children", children, allocator);
    JsonUtil::AddMember(json, "metadata", metadata, allocator);
    return json;
}

template <> std::optional<document_t> ToEventJson<ParseSuccessEvent>(const ParseSuccessEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "startTime", std::to_string(event.body.startTime), allocator);
    JsonUtil::AddMember(body, "maxTimeStamp", event.body.maxTimeStamp, allocator);
    JsonUtil::AddMember(body, "offset", event.body.offset, allocator);
    JsonUtil::AddMember(body, "startTimeUpdated", event.body.startTimeUpdated, allocator);
    JsonUtil::AddMember(body, "isFullDb", event.body.isFullDb, allocator);
    JsonUtil::AddMember(body, "isRl", event.body.isRl, allocator);
    // 创建 unit 开始
    json_t unit(kObjectType);
    JsonUtil::AddMember(unit, "type", event.body.unit.type, allocator);
    json_t metadata(kObjectType);
    JsonUtil::AddMember(metadata, "cardId", event.body.unit.metadata.cardId, allocator);
    JsonUtil::AddMember(metadata, "cardAlias", event.body.unit.metadata.cardAlias, allocator);
    JsonUtil::AddMember(unit, "metadata", metadata, allocator);
    json_t children(kArrayType);
    for (const auto &track : event.body.unit.children) {
        children.PushBack(UnitTrackToJson(*track, allocator, 0), allocator); // 限制最大递归次数，起始为0
    }
    JsonUtil::AddMember(unit, "children", children, allocator);
    JsonUtil::AddMember(body, "unit", unit, allocator);
    // 创建 unit 结束
    json_t rankList(kArrayType);
    for (const auto &rank : event.body.rankList) {
        rankList.PushBack(rank.SerializationToJson(allocator), allocator);
    }
    JsonUtil::AddMember(body, "rankList", rankList, allocator);
    json_t threadGroupList(kArrayType);
    for (const auto &threads : event.body.threadGroupList) {
        threadGroupList.PushBack(threads.SerializationToJson(allocator), allocator);
    }
    JsonUtil::AddMember(body, "threadGroupList", threadGroupList, allocator);
    JsonUtil::AddMember(body, "dbPath", event.body.fileId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseFailEvent>(const ParseFailEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "rankId", event.body.rankId, allocator);
    JsonUtil::AddMember(body, "error", event.body.error, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseClusterCompletedEvent>(const ParseClusterCompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "parseResult", event.body.parseResult, allocator);
    JsonUtil::AddMember(body, "isAllPageParsed", event.body.isAllPageParsed, allocator);
    JsonUtil::AddMember(body, "isShowCluster", event.body.isShowCluster, allocator);
    JsonUtil::AddMember(body, "clusterPath", event.body.clusterPath, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<AllSuccessEvent>(const AllSuccessEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "isAllPageParsed", event.body.isAllPageParsed, allocator);
    json_t dataType(kArrayType);
    for (const auto &type : event.body.cardOffsets) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "cardId", type.cardId, allocator);
        JsonUtil::AddMember(itemJson, "offset", type.offset, allocator);
        dataType.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "cardOffsets", dataType, allocator);
    JsonUtil::AddMember(body, "minTime", std::to_string(event.body.minTime), allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToEventJson<ParseClusterStep2CompletedEvent>(const ParseClusterStep2CompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "parseResult", event.body.parseResult, allocator);
    JsonUtil::AddMember(body, "isAllPageParsed", event.body.isAllPageParsed, allocator);
    JsonUtil::AddMember(body, "clusterPath", event.body.clusterPath, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseMemoryCompletedEvent>(const ParseMemoryCompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    json_t memoryResult(kArrayType);
    for (const MemorySuccess &memory : event.memoryResult) {
        json_t chartJson(kObjectType);
        JsonUtil::AddMember(chartJson, "rankId", memory.rankId, allocator);
        JsonUtil::AddMember(chartJson, "hasMemory", memory.hasFile and memory.parseSuccess, allocator);
        JsonUtil::AddMember(chartJson, "dbPath", memory.fileId, allocator);
        JsonUtil::AddMember(chartJson, "rankInfo", memory.rankInfo.SerializationToJson(allocator), allocator);
        memoryResult.PushBack(chartJson, allocator);
    }
    JsonUtil::AddMember(body, "isCluster", event.isCluster, allocator);
    JsonUtil::AddMember(body, "memoryResult", memoryResult, allocator);
    JsonUtil::AddMember(body, "dbPath", event.fileId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ModuleResetEvent>(const ModuleResetEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "reset", event.reset, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseProgressEvent>(const ParseProgressEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "fileId", event.body.fileId, allocator);
    JsonUtil::AddMember(body, "parsedSize", event.body.parsedSize, allocator);
    JsonUtil::AddMember(body, "totalSize", event.body.totalSize, allocator);
    JsonUtil::AddMember(body, "progress", event.body.progress, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseHeatmapCompletedEvent>(const ParseHeatmapCompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "parseResult", event.body.parseResult, allocator);
    JsonUtil::AddMember(body, "errorMsg", event.body.errorMsg, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<ParseUnitCompletedEvent>(const ParseUnitCompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "parseResult", event.body.parseResult, allocator);
    JsonUtil::AddMember(body, "errorMsg", event.body.errorMsg, allocator);
    JsonUtil::AddMember(body, "unitName", event.body.unitName, allocator);
    JsonUtil::AddMember(body, "dbId", event.body.dbId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
