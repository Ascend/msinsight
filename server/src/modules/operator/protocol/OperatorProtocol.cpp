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
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolUtil.h"
#include "ProtocolDefs.h"
#include "TimelineProtocol.h"
#include "OperatorProtocol.h"

namespace Dic::Protocol {

void OperatorProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_CATEGORY_INFO, ToOperatorCategoryInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_COMPUTE_UNIT_INFO, ToOperatorComputeUnitInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_STATISTIC_INFO, ToOperatorStatisticInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_DETAIL_INFO, ToOperatorDetailInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_MORE_INFO, ToOperatorMoreInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_OPERATOR_EXPORT_DETAILS, ToOperatorExportDetailsRequest);
}

void OperatorProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_OPERATOR_CATEGORY_INFO, ToOperatorCategoryInfoResponse);
    resToJsonFactory.emplace(REQ_RES_OPERATOR_COMPUTE_UNIT_INFO, ToOperatorComputeUnitInfoResponse);
    resToJsonFactory.emplace(REQ_RES_OPERATOR_STATISTIC_INFO, ToOperatorStatisticInfoResponse);
    resToJsonFactory.emplace(REQ_RES_OPERATOR_DETAIL_INFO, ToOperatorDetailInfoResponse);
    resToJsonFactory.emplace(REQ_RES_OPERATOR_MORE_INFO, ToOperatorMoreInfoResponse);
    resToJsonFactory.emplace(REQ_RES_OPERATOR_EXPORT_DETAILS, ToOperatorExportDetailsResponse);
}

void OperatorProtocol::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(EVENT_PARSE_OPERATOR_STATUS, ToOperatorParseStatusEvent);
    eventToJsonFactory.emplace(EVENT_PARSE_OPERATOR_CLEAR, ToOperatorParseClearEvent);
    eventToJsonFactory.emplace(EVENT_MODULE_RESET, TimelineProtocol::ToModuleResetEventJson);
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorCategoryInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorCategoryInfoRequest> reqPtr = std::make_unique<OperatorCategoryInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of Category Info.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    return reqPtr;
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorComputeUnitInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorComputeUnitInfoRequest> reqPtr = std::make_unique<OperatorComputeUnitInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of Compute Unit Info.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    return reqPtr;
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorStatisticInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorStatisticInfoRequest> reqPtr = std::make_unique<OperatorStatisticInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of Statistic Info.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    ToOperatorInfoRequestFilters<OperatorStatisticInfoRequest>(reqPtr, json, error);
    return reqPtr;
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorDetailInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorDetailInfoRequest> reqPtr = std::make_unique<OperatorDetailInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of Detail Info.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    ToOperatorInfoRequestFilters<OperatorDetailInfoRequest>(reqPtr, json, error);
    return reqPtr;
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorMoreInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorMoreInfoRequest> reqPtr = std::make_unique<OperatorMoreInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of More Info.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.opType, json["params"], "opType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.opName, json["params"], "opName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.shape, json["params"], "shape");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.accCore, json["params"], "accCore");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.current, json["params"], "current");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    ToOperatorInfoRequestFilters<OperatorMoreInfoRequest>(reqPtr, json, error);
    return reqPtr;
}

std::unique_ptr<Request> OperatorProtocol::ToOperatorExportDetailsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorExportDetailsRequest> reqPtr = std::make_unique<OperatorExportDetailsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info in export op detail.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.group, json["params"], "group");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.topK, json["params"], "topK");
    return reqPtr;
}

template <typename T>
void OperatorProtocol::ToOperatorInfoRequestFilters(
    std::unique_ptr<T> &reqPtr, const json_t &json, std::string &error) {
    if (json["params"].HasMember("filters") && json["params"]["filters"].IsArray()) {
        for (const auto &filter : json["params"]["filters"].GetArray()) {
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
}

std::optional<document_t> OperatorProtocol::ToOperatorCategoryInfoResponse(const Response &response) {
    return ToResponseJson<OperatorCategoryInfoResponse>(dynamic_cast<const OperatorCategoryInfoResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorComputeUnitInfoResponse(const Response &response) {
    return ToResponseJson<OperatorComputeUnitInfoResponse>(
        dynamic_cast<const OperatorComputeUnitInfoResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorStatisticInfoResponse(const Response &response) {
    return ToResponseJson<OperatorStatisticInfoResponse>(dynamic_cast<const OperatorStatisticInfoResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorDetailInfoResponse(const Response &response) {
    return ToResponseJson<OperatorDetailInfoResponse>(dynamic_cast<const OperatorDetailInfoResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorMoreInfoResponse(const Response &response) {
    return ToResponseJson<OperatorMoreInfoResponse>(dynamic_cast<const OperatorMoreInfoResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorExportDetailsResponse(const Response &response) {
    return ToResponseJson<OperatorExportDetailsResponse>(dynamic_cast<const OperatorExportDetailsResponse &>(response));
}

std::optional<document_t> OperatorProtocol::ToOperatorParseStatusEvent(const Event &event) {
    return ToEventJson<OperatorParseStatusEvent>(dynamic_cast<const OperatorParseStatusEvent &>(event));
}

std::optional<document_t> OperatorProtocol::ToOperatorParseClearEvent(const Event &event) {
    return ToEventJson<OperatorParseClearEvent>(dynamic_cast<const OperatorParseClearEvent &>(event));
}
}
