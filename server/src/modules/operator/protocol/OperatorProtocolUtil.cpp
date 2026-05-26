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
#include "ProtocolMessage.h"
#include "OperatorProtocolUtil.h"

namespace Dic::Protocol {
using namespace rapidjson;
template <>
std::optional<document_t> ToResponseJson<OperatorCategoryInfoResponse>(const OperatorCategoryInfoResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const OperatorDurationRes &ele : res.data) {
        json_t dataJson(kObjectType);
        JsonUtil::AddMember(dataJson, "name", ele.name, allocator);
        JsonUtil::AddMember(dataJson, "duration", ele.duration, allocator);
        data.PushBack(dataJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<OperatorComputeUnitInfoResponse>(const OperatorComputeUnitInfoResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const OperatorDurationRes &ele : res.data) {
        json_t dataJson(kObjectType);
        JsonUtil::AddMember(dataJson, "name", ele.name, allocator);
        JsonUtil::AddMember(dataJson, "duration", ele.duration, allocator);
        data.PushBack(dataJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

void AddStatisticMemberWithLabel(rapidjson::Value &parent, const char *label, const OperatorStatisticInfoRes &ele,
    rapidjson::Document::AllocatorType &allocator) {
    rapidjson::Value dataJson(rapidjson::kObjectType);
    JsonUtil::AddMember(dataJson, "opType", ele.opType, allocator);
    JsonUtil::AddMember(dataJson, "opName", ele.opName, allocator);
    JsonUtil::AddMember(dataJson, "accCore", ele.accCore, allocator);
    JsonUtil::AddMember(dataJson, "inputShape", ele.inputShape, allocator);
    JsonUtil::AddMember(dataJson, "totalTime", ele.totalTime, allocator);
    JsonUtil::AddMember(dataJson, "count", NumberUtil::TruncateNumberString(ele.count), allocator);
    JsonUtil::AddMember(dataJson, "avgTime", ele.avgTime, allocator);
    JsonUtil::AddMember(dataJson, "maxTime", ele.maxTime, allocator);
    JsonUtil::AddMember(dataJson, "minTime", ele.minTime, allocator);
    parent.AddMember(rapidjson::Value(label, allocator).Move(), dataJson, allocator);
}

template <>
std::optional<document_t> ToResponseJson<OperatorStatisticInfoResponse>(const OperatorStatisticInfoResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "total", res.total, allocator);

    rapidjson::Document data;
    data.SetArray();
    for (const OperatorStatisticCmpInfoRes &eles : res.data) {
        rapidjson::Value cmpResJson(rapidjson::kObjectType);
        AddStatisticMemberWithLabel(cmpResJson, "diff", eles.diff, allocator);
        AddStatisticMemberWithLabel(cmpResJson, "baseline", eles.baseline, allocator);
        AddStatisticMemberWithLabel(cmpResJson, "compare", eles.compare, allocator);
        data.PushBack(cmpResJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

void AdDetaildMemberWithLabel(rapidjson::Value &parent, const char *label, const OperatorDetailInfoRes &ele,
    rapidjson::Document::AllocatorType &allocator) {
    rapidjson::Value dataJson(rapidjson::kObjectType);
    JsonUtil::AddMember(dataJson, "name", ele.name, allocator);
    JsonUtil::AddMember(dataJson, "type", ele.type, allocator);
    JsonUtil::AddMember(dataJson, "accCore", ele.accCore, allocator);
    JsonUtil::AddMember(dataJson, "startTime", ele.startTime, allocator);
    JsonUtil::AddMember(dataJson, "duration", ele.duration, allocator);
    JsonUtil::AddMember(dataJson, "waitTime", ele.waitTime, allocator);
    JsonUtil::AddMember(dataJson, "blockNum", ele.blockNum, allocator);
    JsonUtil::AddMember(dataJson, "inputShape", ele.inputShape, allocator);
    JsonUtil::AddMember(dataJson, "inputType", ele.inputType, allocator);
    JsonUtil::AddMember(dataJson, "inputFormat", ele.inputFormat, allocator);
    JsonUtil::AddMember(dataJson, "outputShape", ele.outputShape, allocator);
    JsonUtil::AddMember(dataJson, "outputType", ele.outputType, allocator);
    JsonUtil::AddMember(dataJson, "outputFormat", ele.outputFormat, allocator);
    for (auto &[colName, pmuValue] : ele.pmuDatas) {
        rapidjson::Value key(colName.c_str(), allocator);
        rapidjson::Value value(pmuValue.c_str(), allocator);
        dataJson.AddMember(key, value, allocator);
    }
    parent.AddMember(rapidjson::Value(label, allocator).Move(), dataJson, allocator);
}

template <>
std::optional<document_t> ToResponseJson<OperatorDetailInfoResponse>(const OperatorDetailInfoResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "total", res.total, allocator);
    JsonUtil::AddMember(body, "level", res.level, allocator);
    // 创建一个JSON数组存储pmuHeaders的数据
    Value pmuHeaders(kArrayType);
    // 将vector中的元素逐个添加到JSON数组中
    for (const auto &header : res.pmuHeaders) {
        Value headerValue;
        headerValue.SetString(header.c_str(), header.length(), allocator);
        pmuHeaders.PushBack(headerValue, allocator);
    }
    JsonUtil::AddMember(body, "pmuHeaders", pmuHeaders, allocator);

    rapidjson::Document data;
    data.SetArray();
    for (const OperatorDetailCmpInfoRes &eles : res.data) {
        rapidjson::Value cmpResJson(rapidjson::kObjectType);
        AdDetaildMemberWithLabel(cmpResJson, "diff", eles.diff, allocator);
        AdDetaildMemberWithLabel(cmpResJson, "baseline", eles.baseline, allocator);
        AdDetaildMemberWithLabel(cmpResJson, "compare", eles.compare, allocator);
        data.PushBack(cmpResJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<OperatorMoreInfoResponse>(const OperatorMoreInfoResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "total", res.total, allocator);
    JsonUtil::AddMember(body, "level", res.level, allocator);
    // 创建一个JSON数组存储pmuHeaders的数据
    Value pmuHeaders(kArrayType);
    // 将vector中的元素逐个添加到JSON数组中
    for (const auto &header : res.pmuHeaders) {
        Value headerValue;
        headerValue.SetString(header.c_str(), header.length(), allocator);
        pmuHeaders.PushBack(headerValue, allocator);
    }
    JsonUtil::AddMember(body, "pmuHeaders", pmuHeaders, allocator);
    json_t data(kArrayType);
    for (const OperatorDetailInfoRes &ele : res.data) {
        json_t dataJson(kObjectType);
        JsonUtil::AddMember(dataJson, "name", ele.name, allocator);
        JsonUtil::AddMember(dataJson, "type", ele.type, allocator);
        JsonUtil::AddMember(dataJson, "accCore", ele.accCore, allocator);
        JsonUtil::AddMember(dataJson, "startTime", ele.startTime, allocator);
        JsonUtil::AddMember(dataJson, "duration", ele.duration, allocator);
        JsonUtil::AddMember(dataJson, "waitTime", ele.waitTime, allocator);
        JsonUtil::AddMember(dataJson, "blockNum", ele.blockNum, allocator);
        JsonUtil::AddMember(dataJson, "inputShape", ele.inputShape, allocator);
        JsonUtil::AddMember(dataJson, "inputType", ele.inputType, allocator);
        JsonUtil::AddMember(dataJson, "inputFormat", ele.inputFormat, allocator);
        JsonUtil::AddMember(dataJson, "outputShape", ele.outputShape, allocator);
        JsonUtil::AddMember(dataJson, "outputType", ele.outputType, allocator);
        JsonUtil::AddMember(dataJson, "outputFormat", ele.outputFormat, allocator);
        for (auto &[colName, pmuValue] : ele.pmuDatas) {
            rapidjson::Value key(colName.c_str(), allocator);
            rapidjson::Value value(pmuValue.c_str(), allocator);
            dataJson.AddMember(key, value, allocator);
        }
        data.PushBack(dataJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<OperatorExportDetailsResponse>(const OperatorExportDetailsResponse &res) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(res, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "exceedingFileLimit", res.exceedingFileLimit, allocator);
    JsonUtil::AddMember(body, "filePath", res.filePath, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<OperatorParseStatusEvent>(const OperatorParseStatusEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "rankId", event.data.rankId, allocator);
    JsonUtil::AddMember(body, "status", event.data.status, allocator);
    JsonUtil::AddMember(body, "error", event.data.error, allocator);
    JsonUtil::AddMember(body, "dbPath", event.fileId, allocator);
    json_t rankList(kArrayType);
    for (const auto &rank : event.rankList) {
        rankList.PushBack(rank.SerializationToJson(allocator), allocator);
    }
    JsonUtil::AddMember(body, "rankList", rankList, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToEventJson<OperatorParseClearEvent>(const OperatorParseClearEvent &event) {
    document_t json(kObjectType);
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    return std::optional<document_t>{std::move(json)};
}
}
