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
#include "IEProtocolResquest.h"
#include "IEProtocolResponse.h"
#include "IEProtocolEvent.h"
#include "IEProtocolUtil.h"
#include "JsonUtil.h"
#include "IEProtocol.h"

namespace Dic::Protocol {
void IEProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_IE_VIEW, ToIEUsageViewRequest);
    jsonToReqFactory.emplace(REQ_RES_IE_TABLE_VIEW, ToIETableRequest);
    jsonToReqFactory.emplace(REQ_RES_IE_DATA_GROUP, ToIEGroupRequest);
}

void IEProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_IE_VIEW, ToIEUsageViewResponseJson);
    resToJsonFactory.emplace(REQ_RES_IE_TABLE_VIEW, ToIETableViewResponseJson);
    resToJsonFactory.emplace(REQ_RES_IE_DATA_GROUP, ToIEGroupResponseJson);
}

void IEProtocol::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(EVENT_PARSE_IE_COMPLETED, ToParseIECompletedEventJson);
}

std::unique_ptr<Request> IEProtocol::ToIEUsageViewRequest(const json_t &json, std::string &error) {
    std::unique_ptr<IEUsageViewParamsRequest> reqPtr = std::make_unique<IEUsageViewParamsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isZh, json["params"], "isZh");
    return reqPtr;
}

std::unique_ptr<Request> IEProtocol::ToIEGroupRequest(const json_t &json, std::string &error) {
    std::unique_ptr<IEGroupRequest> reqPtr = std::make_unique<IEGroupRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    return reqPtr;
}

std::unique_ptr<Request> IEProtocol::ToIETableRequest(const json_t &json, std::string &error) {
    std::unique_ptr<IETableRequest> reqPtr = std::make_unique<IETableRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.type, json["params"], "type");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.startTime, json["params"], "startTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.endTime, json["params"], "endTime");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    return reqPtr;
}

std::optional<document_t> IEProtocol::ToIEUsageViewResponseJson(const Response &response) {
    return ToResponseJson<IEUsageViewResponse>(dynamic_cast<const IEUsageViewResponse &>(response));
}

std::optional<document_t> IEProtocol::ToIETableViewResponseJson(const Response &response) {
    return ToResponseJson<IETableViewResponse>(dynamic_cast<const IETableViewResponse &>(response));
}

std::optional<document_t> IEProtocol::ToIEGroupResponseJson(const Response &response) {
    return ToResponseJson<IEGroupResponse>(dynamic_cast<const IEGroupResponse &>(response));
}

std::optional<document_t> IEProtocol::ToParseIECompletedEventJson(const Event &event) {
    return ToEventJson<ParseStatisticCompletedEvent>(dynamic_cast<const ParseStatisticCompletedEvent &>(event));
}
}
