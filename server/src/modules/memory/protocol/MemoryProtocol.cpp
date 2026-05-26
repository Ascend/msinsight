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
#include "ProtocolDefs.h"
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "MemoryProtocolUtil.h"
#include "TimelineProtocol.h"
#include "MemoryTableView.h"
#include "MemoryProtocol.h"

namespace Dic {
namespace Protocol {
void MemoryProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_MEMORY_TYPE, ToMemoryTypeRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_RESOURCE_TYPE, ToMemoryResourceTypeRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_OPERATOR, ToMemoryOperatorRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_COMPONENT, ToMemoryComponentRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_FIND_SLICE, ToMemoryFindSliceRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_VIEW, ToMemoryViewRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_OPERATOR_MIN_MAX, ToMemoryOperatorSizeRequest);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH, ToMemoryStaticOperatorGraphRequest);
    jsonToReqFactory.emplace(
        REQ_RES_MEMORY_STATIC_OP_MEMORY_LIST, BuildRequestFromJson<MemoryStaticOperatorListRequest>);
    jsonToReqFactory.emplace(REQ_RES_MEMORY_STATIC_OP_MEMORY_MIN_MAX, ToMemoryStaticOperatorSizeRequest);
}

void MemoryProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_MEMORY_TYPE, ToMemoryTypeResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_RESOURCE_TYPE, ToMemoryResourceTypeResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_OPERATOR, ToMemoryOperatorResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_COMPONENT, ToMemoryComponentResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_VIEW, ToMemoryViewResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_FIND_SLICE, ToMemoryFindSliceResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_OPERATOR_MIN_MAX, ToMemoryOperatorSizeResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH, ToMemoryStaticOperatorGraphResponseJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_STATIC_OP_MEMORY_LIST, ProtocolUtil::CommonResponseToJson);
    resToJsonFactory.emplace(REQ_RES_MEMORY_STATIC_OP_MEMORY_MIN_MAX, ToMemoryStaticOperatorSizeResponseJson);
}

void MemoryProtocol::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(EVENT_MODULE_RESET, TimelineProtocol::ToModuleResetEventJson);
    eventToJsonFactory.emplace(EVENT_ALL_SUCCESS, TimelineProtocol::ToAllSuccessEventJson);
}

#pragma region << Json To Request>>

std::unique_ptr<Request> MemoryProtocol::ToMemoryTypeRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryTypeRequest> reqPtr = std::make_unique<MemoryTypeRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->rankId, json["params"], "rankId");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryResourceTypeRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryResourceTypeRequest> reqPtr = std::make_unique<MemoryResourceTypeRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->rankId, json["params"], "rankId");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryOperatorRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryOperatorRequest> reqPtr = std::make_unique<MemoryOperatorRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    if (json["params"].HasMember("startTime")) {
        reqPtr->params.startTime = JsonUtil::GetDouble(json["params"], "startTime");
    } else {
        reqPtr->params.startTime = -1;
    }
    if (json["params"].HasMember("endTime")) {
        reqPtr->params.endTime = JsonUtil::GetDouble(json["params"], "endTime");
    } else {
        reqPtr->params.endTime = -1;
    }

    if (json["params"].HasMember("minSize")) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.minSize, json["params"], "minSize");
    } else {
        reqPtr->params.minSize = std::numeric_limits<int64_t>::min();
    }

    if (json["params"].HasMember("maxSize")) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.maxSize, json["params"], "maxSize");
    } else {
        reqPtr->params.maxSize = std::numeric_limits<int64_t>::max();
    }

    JsonUtil::SetByJsonKeyValue(reqPtr->params.isOnlyShowAllocatedOrReleasedWithinInterval, json["params"],
        "isOnlyShowAllocatedOrReleasedWithinInterval");
    reqPtr->params.SetPaginationParamFromJson(json["params"]);
    if (!reqPtr->params.SetFiltersFromJson(json["params"], OperatorMemoryTableView::FIELD_FULL_COLUMNS, error)) {
        Server::ServerLog::Error("Failed set filters from json param: %", error);
        return nullptr;
    }
    if (!reqPtr->params.SetOrderFromJson(json["params"], OperatorMemoryTableView::FIELD_FULL_COLUMNS, error)) {
        Server::ServerLog::Error("Failed set order from json param: %", error);
        return nullptr;
    }
    if (!reqPtr->params.SetRangeFiltersFromJson(json["params"], OperatorMemoryTableView::FIELD_FULL_COLUMNS, error)) {
        Server::ServerLog::Error("Failed set range filters from json param: %", error);
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.searchName, json["params"], "searchName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryComponentRequest(const Dic::json_t &json, std::string &error) {
    std::unique_ptr<MemoryComponentRequest> reqPtr = std::make_unique<MemoryComponentRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (!json.HasMember("params") || !json["params"].HasMember("rankId")) {
        error = "Request json lacks member rankId.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryViewRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryViewRequest> reqPtr = std::make_unique<MemoryViewRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.start, json["params"], "start");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.end, json["params"], "end");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryFindSliceRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryFindSliceRequest> reqPtr = std::make_unique<MemoryFindSliceRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.id, json["params"], "id");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.name, json["params"], "name");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryOperatorSizeRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryOperatorSizeRequest> reqPtr = std::make_unique<MemoryOperatorSizeRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryStaticOperatorGraphRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MemoryStaticOperatorGraphRequest> reqPtr = std::make_unique<MemoryStaticOperatorGraphRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.modelName, json["params"], "modelName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.graphId, json["params"], "graphId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    return reqPtr;
}

std::unique_ptr<Request> MemoryProtocol::ToMemoryStaticOperatorSizeRequest(
    const Dic::json_t &json, std::string &error) {
    std::unique_ptr<MemoryStaticOperatorSizeRequest> reqPtr = std::make_unique<MemoryStaticOperatorSizeRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.graphId, json["params"], "graphId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    return reqPtr;
}

#pragma endregion

#pragma region << Response To Json>>

std::optional<document_t> MemoryProtocol::ToMemoryTypeResponseJson(const Response &response) {
    return ToResponseJson<MemoryTypeResponse>(dynamic_cast<const MemoryTypeResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryResourceTypeResponseJson(const Response &response) {
    return ToResponseJson<MemoryResourceTypeResponse>(dynamic_cast<const MemoryResourceTypeResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryOperatorResponseJson(const Response &response) {
    return ToResponseJson<MemoryOperatorComparisonResponse>(
        dynamic_cast<const MemoryOperatorComparisonResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryComponentResponseJson(const Response &response) {
    return ToResponseJson<MemoryComponentComparisonResponse>(
        dynamic_cast<const MemoryComponentComparisonResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryViewResponseJson(const Response &response) {
    return ToResponseJson<MemoryViewResponse>(dynamic_cast<const MemoryViewResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryFindSliceResponseJson(const Response &response) {
    return ToResponseJson<MemoryFindSliceResponse>(dynamic_cast<const MemoryFindSliceResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryOperatorSizeResponseJson(const Response &response) {
    return ToResponseJson<MemoryOperatorSizeResponse>(dynamic_cast<const MemoryOperatorSizeResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryStaticOperatorGraphResponseJson(const Response &response) {
    return ToResponseJson<MemoryStaticOperatorGraphResponse>(
        dynamic_cast<const MemoryStaticOperatorGraphResponse &>(response));
}

std::optional<document_t> MemoryProtocol::ToMemoryStaticOperatorSizeResponseJson(const Response &response) {
    return ToResponseJson<MemoryStaticOperatorSizeResponse>(
        dynamic_cast<const MemoryStaticOperatorSizeResponse &>(response));
}
#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
