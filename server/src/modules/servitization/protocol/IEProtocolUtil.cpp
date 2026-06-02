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
#include "ProtocolMessage.h"
#include "JsonUtil.h"
#include "IEProtocolUtil.h"

namespace Dic::Protocol {
template <> std::optional<document_t> ToResponseJson<IEUsageViewResponse>(const IEUsageViewResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t legends(kArrayType);
    for (const auto &legend : response.data.legends) {
        legends.PushBack(json_t().SetString(legend.c_str(), allocator), allocator);
    }
    json_t linesList(kArrayType);
    for (const auto &lines : response.data.lines) {
        json_t lineArr(kArrayType);
        for (const auto &line : lines) {
            lineArr.PushBack(json_t().SetString(line.c_str(), allocator), allocator);
        }
        linesList.PushBack(lineArr, allocator);
    }
    JsonUtil::AddMember(body, "legends", legends, allocator);
    JsonUtil::AddMember(body, "lines", linesList, allocator);
    JsonUtil::AddMember(body, "title", response.data.title, allocator);
    JsonUtil::AddMember(body, "description", response.data.desc, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::move(json);
}

template <> std::optional<document_t> ToResponseJson<IETableViewResponse>(const IETableViewResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t columnAttr(kArrayType);
    for (const auto &attr : response.data.columnAttr) {
        json_t attrJson = json_t(kObjectType);
        JsonUtil::AddMember(attrJson, "name", attr.name, allocator);
        JsonUtil::AddMember(attrJson, "type", attr.type, allocator);
        JsonUtil::AddMember(attrJson, "key", attr.key, allocator);
        columnAttr.PushBack(attrJson, allocator);
    }
    json_t operatorDetail(kArrayType);
    for (const auto &attr : response.data.columnData) {
        json_t attrJson = json_t(kObjectType);
        for (const auto &item : response.data.columnAttr) {
            auto it = attr.find(item.key);
            if (it == attr.end()) {
                continue;
            }
            JsonUtil::AddMember(attrJson, item.key, it->second, allocator);
        }
        operatorDetail.PushBack(attrJson, allocator);
    }
    JsonUtil::AddMember(body, "totalNum", response.data.totalNum, allocator);
    JsonUtil::AddMember(body, "operatorDetail", operatorDetail, allocator);
    JsonUtil::AddMember(body, "columnAttr", columnAttr, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::move(json);
}

template <> std::optional<document_t> ToResponseJson<IEGroupResponse>(const IEGroupResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t groups(kArrayType);
    for (const auto &legend : response.data) {
        json_t group(kObjectType);
        JsonUtil::AddMember(group, "label", legend.label, allocator);
        JsonUtil::AddMember(group, "value", legend.value, allocator);
        groups.PushBack(group, allocator);
    }
    JsonUtil::AddMember(body, "groups", groups, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::move(json);
}

template <>
std::optional<document_t> ToEventJson<ParseStatisticCompletedEvent>(const ParseStatisticCompletedEvent &event) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    json_t body(kObjectType);
    json_t rankIds(kArrayType);
    for (const std::string &item : event.rankIds) {
        rankIds.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "rankIds", rankIds, allocator);
    JsonUtil::AddMember(body, "dbPath", event.fileId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::move(json);
}
}
