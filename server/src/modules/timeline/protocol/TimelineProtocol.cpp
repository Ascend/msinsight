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
#include "TimelineProtocolUtil.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "TimelineProtocol.h"

namespace Dic {
namespace Protocol {
void TimelineProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_IMPORT_ACTION, ToImportActionRequest);
    jsonToReqFactory.emplace(REQ_RES_PARSE_CARDS, ToParseCardsRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_THREAD_TRACES, ToUnitThreadTracesRequest);
    jsonToReqFactory.emplace(REQ_RES_CREATE_CURVE, ToCreateCurveRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_THREAD_TRACES_SUMMARY, ToUnitThreadTracesSummaryRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_THREADS, ToUnitThreadsRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_THREAD_DETAIL, ToThreadDetailRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_FLOWS, ToUnitFlowsRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_SET_CARD_ALIAS, ToSetCardAliasRequest);
    jsonToReqFactory.emplace(REQ_RES_RESET_WINDOW, ToResetWindowRequest);
    jsonToReqFactory.emplace(REQ_RES_SEARCH_COUNT, ToSearchCountRequest);
    jsonToReqFactory.emplace(REQ_RES_SEARCH_SLICE, ToSearchSliceRequest);
    jsonToReqFactory.emplace(REQ_RES_REMOTE_DELETE, ToRemoteDeleteRequest);
    jsonToReqFactory.emplace(REQ_RES_FLOW_CATEGORY_LIST, ToFlowCategoryListRequest);
    jsonToReqFactory.emplace(REQ_RES_FLOW_CATEGORY_EVENTS, ToFlowCategoryEventsRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_COUNTER, ToUnitCounterRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_SYSTEM_VIEW, ToSystemViewRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_SYSTEM_VIEW, ToExpAnaAICoreFreqRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_EVENTS_VIEW, ToEventsViewRequest);
    jsonToReqFactory.emplace(REQ_RES_UNIT_KERNEL_DETAILS, ToKernelDetailRequest);
    jsonToReqFactory.emplace(REQ_RES_ONE_KERNEL_DETAILS, ToOneKernelRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_KERNEL_DETAIL, ToCommunicationKernelRequest);
    jsonToReqFactory.emplace(REQ_RES_SAME_OPERATORS_DURATION, ToUnitThreadsOperatorsRequest);
    jsonToReqFactory.emplace(REQ_RES_TABLE_DATA_NAME_LIST, ToTableDataNameListRequest);
    jsonToReqFactory.emplace(REQ_RES_TABLE_DATA_DETAIL, ToTableDataDetailRequest);
    jsonToReqFactory.emplace(REQ_RES_SEARCH_ALL_SLICES, ToSearchAllSlicesRequest);
    jsonToReqFactory.emplace(REQ_RES_SYSTEM_VIEW_OVERALL, ToSystemViewOverallRequest);
    jsonToReqFactory.emplace(REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS, ToSystemViewOverallMoreDetailsRequest);
    jsonToReqFactory.emplace(REQ_RES_SYSTEM_VIEW_FTRACE_STAT, ToSystemViewFtraceStatRequest);
    jsonToReqFactory.emplace(REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ, ToExpAnaAICoreFreqRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMCPY_OVERALL, ToMemcpyOverallRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMCPY_DETAIL, ToSystemViewOverallMoreDetailsRequest);
    jsonToReqFactory.emplace(REQ_RES_RANK_OFFSET, ToRankOffsetRequest);
}

void TimelineProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_IMPORT_ACTION, ToImportActionResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_THREAD_TRACES, ToUnitThreadTracesResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_THREAD_TRACES_SUMMARY, ToUnitThreadTracesSummaryResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_THREADS, ToUnitThreadsResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_THREAD_DETAIL, ToThreadDetailResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_FLOWS, ToUnitFlowsResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_SET_CARD_ALIAS, ToSetCardAliasResponseJson);
    resToJsonFactory.emplace(REQ_RES_RESET_WINDOW, ToResetWindowResponseJson);
    resToJsonFactory.emplace(REQ_RES_SEARCH_COUNT, ToSearchCountResponseJson);
    resToJsonFactory.emplace(REQ_RES_SEARCH_SLICE, ToSearchSliceResponseJson);
    resToJsonFactory.emplace(REQ_RES_REMOTE_DELETE, ToRemoteDeleteResponseJson);
    resToJsonFactory.emplace(REQ_RES_FLOW_CATEGORY_LIST, ToFlowCategoryListResponse);
    resToJsonFactory.emplace(REQ_RES_FLOW_CATEGORY_EVENTS, ToFlowCategoryEventsResponse);
    resToJsonFactory.emplace(REQ_RES_UNIT_COUNTER, ToUnitCounterResponse);
    resToJsonFactory.emplace(REQ_RES_UNIT_SYSTEM_VIEW, ToSystemViewResponseJson);
    resToJsonFactory.emplace(REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ, ToExpAnaAICoreFreqResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_EVENTS_VIEW, ToEventsViewResponseJson);
    resToJsonFactory.emplace(REQ_RES_UNIT_KERNEL_DETAILS, ToKernelDetailResponseJson);
    resToJsonFactory.emplace(REQ_RES_ONE_KERNEL_DETAILS, ToOneKernelResponseJson);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_KERNEL_DETAIL, ToCommunicationKernelResponseJson);
    resToJsonFactory.emplace(REQ_RES_SAME_OPERATORS_DURATION, ToUnitThreadsOperatorsResponseJson);
    resToJsonFactory.emplace(REQ_RES_SEARCH_ALL_SLICES, ToSearchAllSlicesResponseJson);
    resToJsonFactory.emplace(REQ_RES_TABLE_DATA_NAME_LIST, ToTableDataNameListResponseJson);
    resToJsonFactory.emplace(REQ_RES_TABLE_DATA_DETAIL, ToTableDataDetailResponseJson);
    resToJsonFactory.emplace(REQ_RES_PARSE_CARDS, ToParseCardsResponseJson);
    resToJsonFactory.emplace(REQ_RES_SYSTEM_VIEW_OVERALL, ToSystemViewOverallResponseJson);
    resToJsonFactory.emplace(REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS, ToOverallMoreDetailsResponseJson);
    resToJsonFactory.emplace(REQ_RES_SYSTEM_VIEW_FTRACE_STAT, ToSystemViewFtraceStatResponseJson);
    resToJsonFactory.emplace(REQ_RES_EXPERT_ANALYSIS_AICORE_FREQ, ToExpAnaAICoreFreqResponseJson);
    resToJsonFactory.emplace(REQ_RES_CREATE_CURVE, ToCreateCurveResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMCPY_OVERALL, ToMemcpyOverallListResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMCPY_DETAIL, ToMemcpyDetailListResponseJson);
    resToJsonFactory.emplace(REQ_RES_RANK_OFFSET, ToRankOffsetResponseJson);
}

void TimelineProtocol::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(EVENT_PARSE_SUCCESS, ToParseSuccessEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_FAIL, ToParseFailEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_CLUSTER_COMPLETED, ToParseClusterCompletedEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_CLUSTER_STEP2_COMPLETED, ToParseClusterStep2CompletedEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_MEMORY_COMPLETED, ToParseMemoryCompletedEventJson);
    eventToJsonFactory.emplace(EVENT_MODULE_RESET, ToModuleResetEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_PROGRESS, ToParseProgressEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_HEATMAP_COMPLETED, ToParseHeatmapCompletedEventJson);
    eventToJsonFactory.emplace(EVENT_PARSE_UNIT_COMPLETED, ToParseUnitCompletedEventJson);
}

#pragma region << Json To Request>>

std::unique_ptr<Request> TimelineProtocol::ToImportActionRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ImportActionRequest> reqPtr = std::make_unique<ImportActionRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("path") && json["params"]["path"].IsArray()) {
        for (const auto &path : json["params"]["path"].GetArray()) {
            reqPtr->params.path.emplace_back(JsonUtil::GetStringWithoutKey(path));
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.projectName, json["params"], "projectName");
    int projectAction = static_cast<int>(ProjectActionEnum::UNKNOWN);
    JsonUtil::SetByJsonKeyValue(projectAction, json["params"], "projectAction");
    if (projectAction != static_cast<int>(ProjectActionEnum::ADD_FILE) &&
        projectAction != static_cast<int>(ProjectActionEnum::TRANSFER_PROJECT) &&
        projectAction != static_cast<int>(ProjectActionEnum::UNKNOWN)) {
        error = "Project action invalid!";
        return nullptr;
    }
    reqPtr->params.projectAction = static_cast<ProjectActionEnum>(projectAction);
    if (json["params"].HasMember("isConflict") && json["params"]["isConflict"].IsBool()) {
        reqPtr->params.isConflict = json["params"]["isConflict"].GetBool();
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToParseCardsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ParseCardsRequest> reqPtr = std::make_unique<ParseCardsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("cards") && json["params"]["cards"].IsArray()) {
        for (const auto &card : json["params"]["cards"].GetArray()) {
            reqPtr->params.cards.emplace_back(JsonUtil::GetStringWithoutKey(card));
        }
    }
    if (json["params"].HasMember("dbPaths") && json["params"]["dbPaths"].IsArray()) {
        for (const auto &dbPath : json["params"]["dbPaths"].GetArray()) {
            reqPtr->params.fileIds.emplace_back(JsonUtil::GetStringWithoutKey(dbPath));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitThreadTracesRequest(const json_t &json, std::string &error) {
    std::unique_ptr<UnitThreadTracesRequest> reqPtr = std::make_unique<UnitThreadTracesRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("threadIdList") && json["params"]["threadIdList"].IsArray()) {
        for (const auto &threadId : json["params"]["threadIdList"].GetArray()) {
            reqPtr->params.threadIdList.emplace_back(JsonUtil::GetStringWithoutKey(threadId));
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.cardId, json["params"], "cardId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.processId, json["params"], "processId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.threadId, json["params"], "threadId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timePerPx, json["params"], "timePerPx");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isFilterPythonFunction, json["params"], "isFilterPythonFunction");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isHideFlagEvents, json["params"], "isHideFlagEvents");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToCreateCurveRequest(const json_t &json, std::string &error) {
    std::unique_ptr<CreateCurveRequest> reqPtr = std::make_unique<CreateCurveRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("y") && json["params"]["y"].IsArray()) {
        for (const auto &item : json["params"]["y"].GetArray()) {
            reqPtr->params.y.emplace_back(JsonUtil::GetStringWithoutKey(item));
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.fileId, json["params"], "fileId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.tid, json["params"], "tid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.x, json["params"], "x");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitThreadTracesSummaryRequest(const json_t &json, std::string &error) {
    std::unique_ptr<UnitThreadTracesSummaryRequest> reqPtr = std::make_unique<UnitThreadTracesSummaryRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.cardId, json["params"], "cardId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.processId, json["params"], "processId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.unitType, json["params"], "unitType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitThreadsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<UnitThreadsRequest> reqPtr = std::make_unique<UnitThreadsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startDepth, json["params"], "startDepth");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endDepth, json["params"], "endDepth");
    if (json["params"].HasMember("metadataList") && json["params"]["metadataList"].IsArray()) {
        for (const auto &metaData : json["params"]["metadataList"].GetArray()) {
            Metadata data{.tid = JsonUtil::GetString(metaData, "tid"),
                .pid = JsonUtil::GetString(metaData, "pid"),
                .metaType = JsonUtil::GetString(metaData, "metaType")};
            JsonUtil::SetByJsonKeyValue(data.hidePythonFunction, metaData, "hidePythonFunction");
            reqPtr->params.metadataList.emplace_back(std::move(data));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToThreadDetailRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ThreadDetailRequest> reqPtr = std::make_unique<ThreadDetailRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.tid, json["params"], "tid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.depth, json["params"], "depth");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.id, json["params"], "id");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitFlowsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<UnitFlowsRequest> reqPtr = std::make_unique<UnitFlowsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.tid, json["params"], "tid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.id, json["params"], "id");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isSimulation, json["params"], "isSimulation");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSetCardAliasRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SetCardAliasRequest> reqPtr = std::make_unique<SetCardAliasRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.cardAlias, json["params"], "cardAlias");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToResetWindowRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ResetWindowRequest> reqPtr = std::make_unique<ResetWindowRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSearchCountRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SearchCountRequest> reqPtr = std::make_unique<SearchCountRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchCase, json["params"], "isMatchCase");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchExact, json["params"], "isMatchExact");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchContent, json["params"], "searchContent");
    if (json["params"].HasMember("metadataList") && json["params"]["metadataList"].IsArray()) {
        for (const auto &metaData : json["params"]["metadataList"].GetArray()) {
            Metadata data{
                .tid = JsonUtil::GetString(metaData, "tid"),
                .pid = JsonUtil::GetString(metaData, "pid"),
                .metaType = JsonUtil::GetString(metaData, "metaType"),
                .rankId = JsonUtil::GetString(metaData, "rankId"),
            };
            JsonUtil::SetByJsonKeyValue(data.lockStartTime, metaData, "lockStartTime");
            JsonUtil::SetByJsonKeyValue(data.lockEndTime, metaData, "lockEndTime");
            reqPtr->params.metadataList.emplace_back(std::move(data));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSearchSliceRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SearchSliceRequest> reqPtr = std::make_unique<SearchSliceRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchCase, json["params"], "isMatchCase");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchExact, json["params"], "isMatchExact");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchContent, json["params"], "searchContent");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.index, json["params"], "index");
    if (json["params"].HasMember("metadataList") && json["params"]["metadataList"].IsArray()) {
        for (const auto &metaData : json["params"]["metadataList"].GetArray()) {
            Metadata data{
                .tid = JsonUtil::GetString(metaData, "tid"),
                .pid = JsonUtil::GetString(metaData, "pid"),
                .metaType = JsonUtil::GetString(metaData, "metaType"),
                .rankId = JsonUtil::GetString(metaData, "rankId"),
            };
            JsonUtil::SetByJsonKeyValue(data.lockStartTime, metaData, "lockStartTime");
            JsonUtil::SetByJsonKeyValue(data.lockEndTime, metaData, "lockEndTime");
            reqPtr->params.metadataList.emplace_back(std::move(data));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToRemoteDeleteRequest(const json_t &json, std::string &error) {
    std::unique_ptr<RemoteDeleteRequest> reqPtr = std::make_unique<RemoteDeleteRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("rankId") && json["params"]["rankId"].IsArray()) {
        for (const auto &id : json["params"]["rankId"].GetArray()) {
            reqPtr->params.rankId.emplace_back(JsonUtil::GetStringWithoutKey(id));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToFlowCategoryListRequest(const json_t &json, std::string &error) {
    std::unique_ptr<FlowCategoryListRequest> reqPtr = std::make_unique<FlowCategoryListRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToFlowCategoryEventsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<FlowCategoryEventsRequest> reqPtr = std::make_unique<FlowCategoryEventsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.host, json["params"], "host");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.category, json["params"], "category");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timePerPx, json["params"], "timePerPx");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isSimulation, json["params"], "isSimulation");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    if (json["params"].HasMember("metadataList") && json["params"]["metadataList"].IsArray()) {
        for (const auto &metaData : json["params"]["metadataList"].GetArray()) {
            Metadata data{.tid = JsonUtil::GetString(metaData, "tid"),
                .pid = JsonUtil::GetString(metaData, "pid"),
                .metaType = JsonUtil::GetString(metaData, "metaType")};
            reqPtr->params.metadataList.emplace_back(std::move(data));
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.lockStartTime, json["params"], "lockStartTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.lockEndTime, json["params"], "lockEndTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitCounterRequest(const json_t &json, std::string &error) {
    std::unique_ptr<UnitCounterRequest> reqPtr = std::make_unique<UnitCounterRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.threadName, json["params"], "threadName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.threadId, json["params"], "threadId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSystemViewRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SystemViewRequest> reqPtr = std::make_unique<SystemViewRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isQueryTotal, json["params"], "isQueryTotal");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.layer, json["params"], "layer");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchName, json["params"], "searchName");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToExpAnaAICoreFreqRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<ExpAnaAICoreFreqRequest> reqPtr = std::make_unique<ExpAnaAICoreFreqRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToEventsViewRequest(const Dic::json_t &json, std::string &error) {
    auto reqPtr = std::make_unique<EventsViewRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("threadIdList") && json["params"]["threadIdList"].IsArray()) {
        for (const auto &threadId : json["params"]["threadIdList"].GetArray()) {
            reqPtr->params.threadIdList.emplace_back(JsonUtil::GetStringWithoutKey(threadId));
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.processName, json["params"], "processName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.tid, json["params"], "tid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.threadName, json["params"], "threadName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    if (json["params"].HasMember("filterCondition") && json["params"]["filterCondition"].IsArray()) {
        for (const auto &filter : json["params"]["filterCondition"].GetArray()) {
            if (!filter.IsString()) {
                continue;
            }
            auto fil = JsonUtil::TryParse(filter.GetString(), error);
            if (!fil) {
                error = "Failed to set request base info because invalid filter json, command is: " + reqPtr->command;
                continue;
            }
            std::pair<std::string, std::string> pFilter("", "");
            pFilter.first = JsonUtil::GetString(fil->GetObj(), "columnName");
            pFilter.second = JsonUtil::GetString(fil->GetObj(), "value");
            reqPtr->params.filters.emplace_back(pFilter);
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToKernelDetailRequest(const json_t &json, std::string &error) {
    std::unique_ptr<KernelDetailsRequest> reqPtr = std::make_unique<KernelDetailsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.coreType, json["params"], "coreType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchName, json["params"], "searchName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    if (json["params"].HasMember("filterCondition") && json["params"]["filterCondition"].IsArray()) {
        for (const auto &filter : json["params"]["filterCondition"].GetArray()) {
            if (!filter.IsString()) {
                continue;
            }
            auto fil = JsonUtil::TryParse(filter.GetString(), error);
            if (!fil) {
                error = "Failed to set request base info because invalid filter json, command is: " + reqPtr->command;
                continue;
            }
            std::pair<std::string, std::string> pFilter("", "");
            pFilter.first = JsonUtil::GetString(fil->GetObj(), "columnName");
            pFilter.second = JsonUtil::GetString(fil->GetObj(), "value");
            reqPtr->params.filters.emplace_back(pFilter);
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToCommunicationKernelRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<CommunicationKernelRequest> reqPtr = std::make_unique<CommunicationKernelRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.name, json["params"], "name");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToOneKernelRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<KernelRequest> reqPtr = std::make_unique<KernelRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.name, json["params"], "name");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timestamp, json["params"], "timestamp");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.duration, json["params"], "duration");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToTableDataNameListRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<TableDataNameListRequest> reqPtr = std::make_unique<TableDataNameListRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isZh, json["params"], "isZh");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToTableDataDetailRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<TableDataDetailRequest> reqPtr = std::make_unique<TableDataDetailRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("filterconditions") && json["params"]["filterconditions"].IsArray()) {
        for (const auto &item : json["params"]["filterconditions"].GetArray()) {
            Filtercondition filtercondition;
            JsonUtil::SetByJsonKeyValue(filtercondition.col, item, "col");
            JsonUtil::SetByJsonKeyValue(filtercondition.content, item, "content");
            reqPtr->params.filterconditions.emplace_back(filtercondition);
        }
    }
    if (json["params"].HasMember("equalConditions") && json["params"]["equalConditions"].IsArray()) {
        for (const auto &item : json["params"]["equalConditions"].GetArray()) {
            EqualCondition equalCondition;
            JsonUtil::SetByJsonKeyValue(equalCondition.col, item, "col");
            JsonUtil::SetByJsonKeyValue(equalCondition.content, item, "content");
            reqPtr->params.equalConditions.emplace_back(equalCondition);
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.tableIndex, json["params"], "selectKey");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToUnitThreadsOperatorsRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<UnitThreadsOperatorsRequest> reqPtr = std::make_unique<UnitThreadsOperatorsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    if (json["params"].HasMember("processes") && json["params"]["processes"].IsArray()) {
        for (const auto &process : json["params"]["processes"].GetArray()) {
            SimpleProcess p;
            JsonUtil::SetByJsonKeyValue(p.pid, process, "pid");
            if (!process.HasMember("tidList") || !process["tidList"].IsArray()) {
                reqPtr->params.processes.emplace_back(p);
                continue;
            }
            for (const auto &tid : process["tidList"].GetArray()) {
                p.tidList.emplace(JsonUtil::GetStringWithoutKey(tid));
            }
            reqPtr->params.processes.emplace_back(p);
        }
    }
    if (json["params"].HasMember("metaTypeList") && json["params"]["metaTypeList"].IsArray()) {
        for (const auto &metaType : json["params"]["metaTypeList"].GetArray()) {
            if (metaType.IsString()) {
                reqPtr->params.metaTypeList.emplace_back(metaType.GetString());
            }
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startDepth, json["params"], "startDepth");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endDepth, json["params"], "endDepth");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.name, json["params"], "name");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSearchAllSlicesRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SearchAllSlicesRequest> reqPtr = std::make_unique<SearchAllSlicesRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchCase, json["params"], "isMatchCase");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isMatchExact, json["params"], "isMatchExact");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchContent, json["params"], "searchContent");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    if (json["params"].HasMember("metadataList") && json["params"]["metadataList"].IsArray()) {
        for (const auto &metaData : json["params"]["metadataList"].GetArray()) {
            Metadata data{
                .tid = JsonUtil::GetString(metaData, "tid"),
                .pid = JsonUtil::GetString(metaData, "pid"),
                .metaType = JsonUtil::GetString(metaData, "metaType"),
                .rankId = JsonUtil::GetString(metaData, "rankId"),
            };
            JsonUtil::SetByJsonKeyValue(data.lockStartTime, metaData, "lockStartTime");
            JsonUtil::SetByJsonKeyValue(data.lockEndTime, metaData, "lockEndTime");
            reqPtr->params.metadataList.emplace_back(std::move(data));
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSystemViewOverallRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SystemViewOverallRequest> reqPtr = std::make_unique<SystemViewOverallRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info for system view overall command.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSystemViewOverallMoreDetailsRequest(
    const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SystemViewOverallMoreDetailsRequest> reqPtr =
        std::make_unique<SystemViewOverallMoreDetailsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info for system view overall command.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    if (json["params"].HasMember("categoryList") && json["params"]["categoryList"].IsArray()) {
        for (const auto &cat : json["params"]["categoryList"].GetArray()) {
            reqPtr->params.categoryList.emplace_back(cat.GetString());
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.name, json["params"], "name");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order.orderType, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToSystemViewFtraceStatRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<SystemViewFtraceStatRequest> reqPtr = std::make_unique<SystemViewFtraceStatRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info for system view ftrace stat command.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.layer, json["params"], "layer");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    reqPtr->params.SetDataType();
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToMemcpyOverallRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<MemcpyOverallRequest> reqPtr = std::make_unique<MemcpyOverallRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info for memcpy overall command.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.page.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    return reqPtr;
}

std::unique_ptr<Request> TimelineProtocol::ToRankOffsetRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<RankOffsetRequest> reqPtr = std::make_unique<RankOffsetRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info for rank offset command.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.sliceName, json["params"], "sliceName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.fileId, json["params"], "fileId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pid, json["params"], "pid");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.metaType, json["params"], "metaType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.alignType, json["params"], "alignType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.id, json["params"], "id");
    std::string startTime = JsonUtil::GetString(json["params"], "startTime");
    reqPtr->params.startTime = NumberUtil::StringToLongLong(startTime);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.duration, json["params"], "duration");
    return reqPtr;
}

#pragma endregion

#pragma region << Response To Json>>

std::optional<document_t> TimelineProtocol::ToImportActionResponseJson(const Response &response) {
    return ToResponseJson<ImportActionResponse>(dynamic_cast<const ImportActionResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitThreadTracesResponseJson(const Response &response) {
    return ToResponseJson<UnitThreadTracesResponse>(dynamic_cast<const UnitThreadTracesResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitThreadTracesSummaryResponseJson(const Response &response) {
    return ToResponseJson<UnitThreadTracesSummaryResponse>(
        dynamic_cast<const UnitThreadTracesSummaryResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitThreadsResponseJson(const Response &response) {
    return ToResponseJson<UnitThreadsResponse>(dynamic_cast<const UnitThreadsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToThreadDetailResponseJson(const Response &response) {
    return ToResponseJson<UnitThreadDetailResponse>(dynamic_cast<const UnitThreadDetailResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitFlowsResponseJson(const Response &response) {
    return ToResponseJson<UnitFlowsResponse>(dynamic_cast<const UnitFlowsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSetCardAliasResponseJson(const Response &response) {
    return ToResponseJson<SetCardAliasResponse>(dynamic_cast<const SetCardAliasResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToResetWindowResponseJson(const Response &response) {
    return ToResponseJson<ResetWindowResponse>(dynamic_cast<const ResetWindowResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSearchCountResponseJson(const Response &response) {
    return ToResponseJson<SearchCountResponse>(dynamic_cast<const SearchCountResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToCreateCurveResponseJson(const Response &response) {
    return ToResponseJson<CreateCurveResponse>(dynamic_cast<const CreateCurveResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSearchSliceResponseJson(const Response &response) {
    return ToResponseJson<SearchSliceResponse>(dynamic_cast<const SearchSliceResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToRemoteDeleteResponseJson(const Response &response) {
    return ToResponseJson<RemoteDeleteResponse>(dynamic_cast<const RemoteDeleteResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToFlowCategoryListResponse(const Response &response) {
    return ToResponseJson<FlowCategoryListResponse>(dynamic_cast<const FlowCategoryListResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToFlowCategoryEventsResponse(const Response &response) {
    return ToResponseJson<FlowCategoryEventsResponse>(dynamic_cast<const FlowCategoryEventsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitCounterResponse(const Response &response) {
    return ToResponseJson<UnitCounterResponse>(dynamic_cast<const UnitCounterResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSystemViewResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<SystemViewResponse>(dynamic_cast<const SystemViewResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToExpAnaAICoreFreqResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<ExpAnaAICoreFreqResponse>(dynamic_cast<const ExpAnaAICoreFreqResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToEventsViewResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<EventsViewResponse>(dynamic_cast<const EventsViewResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToKernelDetailResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<KernelDetailsResponse>(dynamic_cast<const KernelDetailsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToOneKernelResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<OneKernelResponse>(dynamic_cast<const OneKernelResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToCommunicationKernelResponseJson(const Dic::Protocol::Response &response) {
    return ToResponseJson<CommunicationKernelResponse>(dynamic_cast<const CommunicationKernelResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToUnitThreadsOperatorsResponseJson(
    const Dic::Protocol::Response &response) {
    return ToResponseJson<UnitThreadsOperatorsResponse>(dynamic_cast<const UnitThreadsOperatorsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSearchAllSlicesResponseJson(const Response &response) {
    return ToResponseJson<SearchAllSlicesResponse>(dynamic_cast<const SearchAllSlicesResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToTableDataNameListResponseJson(const Response &response) {
    return ToResponseJson<TableDataNameListResponse>(dynamic_cast<const TableDataNameListResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToTableDataDetailResponseJson(const Response &response) {
    return ToResponseJson<TableDataDetailResponse>(dynamic_cast<const TableDataDetailResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToParseCardsResponseJson(const Response &response) {
    return ToResponseJson<ParseCardsResponse>(dynamic_cast<const ParseCardsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToMemcpyOverallListResponseJson(const Response &response) {
    return ToResponseJson<MemcpyOverallResponse>(dynamic_cast<const MemcpyOverallResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToMemcpyDetailListResponseJson(const Response &response) {
    return ToResponseJson<MemcpyDetailResponse>(dynamic_cast<const MemcpyDetailResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToRankOffsetResponseJson(const Response &response) {
    return ToResponseJson<RankOffsetResponse>(dynamic_cast<const RankOffsetResponse &>(response));
}
#pragma endregion

#pragma region << Event To Json>>
std::optional<document_t> TimelineProtocol::ToParseSuccessEventJson(const Event &event) {
    return ToEventJson<ParseSuccessEvent>(dynamic_cast<const ParseSuccessEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseFailEventJson(const Event &event) {
    return ToEventJson<ParseFailEvent>(dynamic_cast<const ParseFailEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseClusterCompletedEventJson(const Event &event) {
    return ToEventJson<ParseClusterCompletedEvent>(dynamic_cast<const ParseClusterCompletedEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToAllSuccessEventJson(const Event &event) {
    return ToEventJson<AllSuccessEvent>(dynamic_cast<const AllSuccessEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseClusterStep2CompletedEventJson(const Event &event) {
    return ToEventJson<ParseClusterStep2CompletedEvent>(dynamic_cast<const ParseClusterStep2CompletedEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseMemoryCompletedEventJson(const Event &event) {
    return ToEventJson<ParseMemoryCompletedEvent>(dynamic_cast<const ParseMemoryCompletedEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToModuleResetEventJson(const Event &event) {
    return ToEventJson<ModuleResetEvent>(dynamic_cast<const ModuleResetEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseProgressEventJson(const Event &event) {
    return ToEventJson<ParseProgressEvent>(dynamic_cast<const ParseProgressEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseHeatmapCompletedEventJson(const Event &event) {
    return ToEventJson<ParseHeatmapCompletedEvent>(dynamic_cast<const ParseHeatmapCompletedEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToParseUnitCompletedEventJson(const Event &event) {
    return ToEventJson<ParseUnitCompletedEvent>(dynamic_cast<const ParseUnitCompletedEvent &>(event));
}

std::optional<document_t> TimelineProtocol::ToSystemViewOverallResponseJson(const Response &response) {
    return ToResponseJson<SystemViewOverallResponse>(dynamic_cast<const SystemViewOverallResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToOverallMoreDetailsResponseJson(const Response &response) {
    return ToResponseJson<UnitThreadsOperatorsResponse>(dynamic_cast<const UnitThreadsOperatorsResponse &>(response));
}

std::optional<document_t> TimelineProtocol::ToSystemViewFtraceStatResponseJson(const Response &response) {
    return ToResponseJson<SystemViewFtraceStatResponse>(dynamic_cast<const SystemViewFtraceStatResponse &>(response));
}
#pragma endregion
} // namespace Protocol
} // namespace Dic
