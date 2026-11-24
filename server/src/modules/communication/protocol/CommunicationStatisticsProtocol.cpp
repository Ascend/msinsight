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
#include "CommunicationStatisticsProtocol.h"
#include <cmath>
#include "JsonUtil.h"
#include "ProtocolUtil.h"

namespace Dic::Protocol {
using namespace Module::Communication;
namespace {
bool IsValidStatisticsString(const std::string &value) {
    return !value.empty() && value.size() <= 4096 && value.find('\0') == std::string::npos;
}

bool ReadStatisticsString(const json_t &params, const char *key, std::string &value, std::string &error) {
    if (!params.HasMember(key) || !params[key].IsString()) {
        error = std::string("Missing or invalid string parameter: ") + key;
        return false;
    }
    value.assign(params[key].GetString(), params[key].GetStringLength());
    return true;
}

template <typename RequestType>
std::unique_ptr<RequestType> ReadStatisticsRequest(const json_t &json, std::string &error) {
    if (!json.IsObject() || !json.HasMember("command") || !json["command"].IsString() || !json.HasMember("params") ||
        !json["params"].IsObject()) {
        error = "Invalid communication statistics request.";
        return nullptr;
    }
    if (json.HasMember("resultCallbackId") && !json["resultCallbackId"].IsInt()) {
        error = "Invalid resultCallbackId.";
        return nullptr;
    }
    auto request = std::make_unique<RequestType>();
    request->resultCallbackId = 0;
    if (!ProtocolUtil::SetRequestBaseInfo(*request, json)) {
        error = "Failed to set communication statistics request base info.";
        return nullptr;
    }
    if (!json.HasMember("resultCallbackId")) {
        request->resultCallbackId.reset();
    }
    return request;
}

bool ReadClusterParams(const json_t &params, CommunicationStatisticsParams &target, std::string &error) {
    return ReadStatisticsString(params, "clusterPath", target.clusterPath, error) &&
        ReadStatisticsString(params, "iterationId", target.iterationId, error);
}

void ReadRankOptions(const json_t &params, RankStatisticsOptions &options, std::string &error) {
    if (params.HasMember("currentPage")) {
        options.currentPage = params["currentPage"].IsInt64() ? params["currentPage"].GetInt64() : 0;
    }
    if (params.HasMember("pageSize")) {
        options.pageSize = params["pageSize"].IsInt64() ? params["pageSize"].GetInt64() : 0;
    }
    for (const auto &field : {std::pair<const char *, std::string *>{"orderBy", &options.orderBy},
             {"order", &options.order}, {"bandType", &options.bandType}}) {
        if (params.HasMember(field.first) && !ReadStatisticsString(params, field.first, *field.second, error)) {
            options.orderBy.clear();
            return;
        }
    }
}

json_t GroupStatisticToJson(const GroupStatistic &item, const char *metricKey, Document::AllocatorType &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "groupName", item.groupName, allocator);
    JsonUtil::AddMember(json, "groupId", item.groupId, allocator);
    json_t value;
    if (item.value && std::isfinite(*item.value)) {
        value.SetDouble(*item.value);
    }
    JsonUtil::AddMember(json, metricKey, value, allocator);
    return json;
}

json_t OperatorFluctuationToJson(const OperatorFluctuation &item, Document::AllocatorType &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "opName", item.opName, allocator);
    JsonUtil::AddMember(json, "rangeTime", item.rangeTime, allocator);
    return json;
}

json_t RankStatisticToJson(const RankStatistic &item, Document::AllocatorType &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "rankId", item.rankId, allocator);
    json_t bandwidths(kArrayType);
    for (const auto &bandwidth : item.bandwidths) {
        json_t transport(kObjectType);
        JsonUtil::AddMember(transport, "bandType", bandwidth.bandType, allocator);
        JsonUtil::AddMember(transport, "bandwidth", bandwidth.bandwidth, allocator);
        bandwidths.PushBack(transport, allocator);
    }
    JsonUtil::AddMember(json, "bandwidths", bandwidths, allocator);
    return json;
}

template <typename Row, typename Serialize>
json_t StatisticsBody(const Response &response, const std::vector<Row> &rows, Serialize serialize, document_t &json) {
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t items(kArrayType);
    if (response.result) {
        for (const auto &row : rows) {
            items.PushBack(serialize(row, allocator), allocator);
        }
    }
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "items", items, allocator);
    return body;
}

std::optional<document_t> GroupStatisticsToJson(
    const Response &response, const std::vector<GroupStatistic> &items, const char *metricKey) {
    document_t json(kObjectType);
    auto body = StatisticsBody(
        response, items,
        [metricKey](const GroupStatistic &item, Document::AllocatorType &allocator) {
            return GroupStatisticToJson(item, metricKey, allocator);
        },
        json);
    JsonUtil::AddMember(json, "body", body, json.GetAllocator());
    return json;
}
}

bool CommunicationStatisticsParams::CheckParams(std::string &error) const {
    const std::string step = GetStep();
    if (!IsValidStatisticsString(clusterPath) || !IsValidStatisticsString(iterationId) || step.size() == 4 ||
        step.find_first_not_of("0123456789", 4) != std::string::npos) {
        error = "Invalid clusterPath or iterationId.";
        return false;
    }
    return true;
}

std::string CommunicationStatisticsParams::GetStep() const {
    return iterationId.rfind("step", 0) == 0 ? iterationId : "step" + iterationId;
}

bool GroupBandwidthParams::CheckParams(std::string &error) const {
    if (!CommunicationStatisticsParams::CheckParams(error)) {
        return false;
    }
    if (!IsValidStatisticsString(transportType)) {
        error = "Missing or invalid transportType.";
        return false;
    }
    return true;
}

bool GroupStatisticsParams::CheckParams(std::string &error) const {
    if (!CommunicationStatisticsParams::CheckParams(error)) {
        return false;
    }
    if (!IsValidStatisticsString(groupName)) {
        error = "Missing or invalid groupName.";
        return false;
    }
    return true;
}

bool RankStatisticsParams::CheckParams(std::string &error) const {
    if (!GroupStatisticsParams::CheckParams(error) || !CheckPageValid(options.pageSize, options.currentPage, error)) {
        return false;
    }
    if (!options.IsRankSortValid()) {
        error = "Invalid orderBy, order or bandType.";
        return false;
    }
    return true;
}

std::unique_ptr<Request> CommunicationGroupRangeRatioRequest::FromJson(const json_t &json, std::string &error) {
    auto request = ReadStatisticsRequest<CommunicationGroupRangeRatioRequest>(json, error);
    if (request != nullptr) {
        ReadClusterParams(json["params"], request->params, error);
    }
    return request;
}

std::unique_ptr<Request> CommunicationGroupBandwidthRequest::FromJson(const json_t &json, std::string &error) {
    auto request = ReadStatisticsRequest<CommunicationGroupBandwidthRequest>(json, error);
    if (request != nullptr && ReadClusterParams(json["params"], request->params, error)) {
        ReadStatisticsString(json["params"], "transportType", request->params.transportType, error);
    }
    return request;
}

std::unique_ptr<Request> CommunicationOperatorFluctuationRequest::FromJson(const json_t &json, std::string &error) {
    auto request = ReadStatisticsRequest<CommunicationOperatorFluctuationRequest>(json, error);
    if (request != nullptr && ReadClusterParams(json["params"], request->params, error)) {
        ReadStatisticsString(json["params"], "groupName", request->params.groupName, error);
    }
    return request;
}

std::unique_ptr<Request> CommunicationRankStatisticsRequest::FromJson(const json_t &json, std::string &error) {
    auto request = ReadStatisticsRequest<CommunicationRankStatisticsRequest>(json, error);
    if (request != nullptr && ReadClusterParams(json["params"], request->params, error) &&
        ReadStatisticsString(json["params"], "groupName", request->params.groupName, error)) {
        ReadRankOptions(json["params"], request->params.options, error);
    }
    return request;
}

std::optional<document_t> CommunicationGroupRangeRatioResponse::ToJson() const {
    return GroupStatisticsToJson(*this, items, "rangeRatio");
}

std::optional<document_t> CommunicationGroupBandwidthResponse::ToJson() const {
    return GroupStatisticsToJson(*this, items, "bandwidth");
}

std::optional<document_t> CommunicationOperatorFluctuationResponse::ToJson() const {
    document_t json(kObjectType);
    auto body = StatisticsBody(*this, items, OperatorFluctuationToJson, json);
    JsonUtil::AddMember(json, "body", body, json.GetAllocator());
    return json;
}

std::optional<document_t> CommunicationRankStatisticsResponse::ToJson() const {
    document_t json(kObjectType);
    auto body = StatisticsBody(*this, items, RankStatisticToJson, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(body, "total", result ? total : uint64_t{0}, allocator);
    JsonUtil::AddMember(body, "currentPage", currentPage, allocator);
    JsonUtil::AddMember(body, "pageSize", pageSize, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return json;
}
}
