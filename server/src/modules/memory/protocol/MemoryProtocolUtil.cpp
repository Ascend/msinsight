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
#include "CommonRequests.h"
#include "MemoryTableView.h"
#include "MemoryDef.h"
#include "MemoryProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace rapidjson;
using namespace Dic::Module::Memory;
#pragma region <<Response to json>>
template <>
std::optional<document_t> ToResponseJson<MemoryOperatorComparisonResponse>(
    const MemoryOperatorComparisonResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t headers(kArrayType);
    // 对比场景下追加一个首列Source列用于标识baseline和compare
    if (response.isCompare) {
        headers.PushBack(TABLE_VIEW_COMPARE_COLUMN.ToTableHeaderJson(allocator), allocator);
    }
    for (const auto &header : OperatorMemoryTableView::FIELD_FULL_COLUMNS) {
        if (!header.visible) {
            continue;
        }
        auto copyHeader = TableViewColumn(header);
        if (response.isCompare) {
            copyHeader.rangeFilterable = false;
            copyHeader.searchable = false;
        }
        auto headerJson = copyHeader.ToTableHeaderJson(allocator);
        if (copyHeader.key == OpMemoryColumn::SIZE) {
            JsonUtil::AddMember(headerJson, "min", OperatorMemoryTableView::DEFAULT_MIN_SIZE, allocator);
            JsonUtil::AddMember(headerJson, "max", OperatorMemoryTableView::DEFAULT_MAX_SIZE, allocator);
        }
        headers.PushBack(headerJson, allocator);
    }
    json_t operatorDiffDetail(kArrayType);
    for (const MemoryOperatorComparison &anOperator : response.operatorDiffDetails) {
        json_t basicJson = json_t(kObjectType);
        std::optional<document_t> jsonCompare = ToMemoryOperatorJson(anOperator.compare, allocator);
        std::optional<document_t> jsonBaseline = ToMemoryOperatorJson(anOperator.baseline, allocator);
        std::optional<document_t> jsonDiff = ToMemoryOperatorJson(anOperator.diff, allocator);
        if (jsonCompare.has_value()) {
            JsonUtil::AddMember(basicJson, "compare", jsonCompare.value(), allocator);
        }
        if (jsonBaseline.has_value()) {
            JsonUtil::AddMember(basicJson, "baseline", jsonBaseline.value(), allocator);
        }
        if (jsonDiff.has_value()) {
            JsonUtil::AddMember(basicJson, "diff", jsonDiff.value(), allocator);
        }
        operatorDiffDetail.PushBack(basicJson, allocator);
    }
    JsonUtil::AddMember(body, "totalNum", response.totalNum, allocator);
    JsonUtil::AddMember(body, "operatorDetail", operatorDiffDetail, allocator);
    JsonUtil::AddMember(body, "columnAttr", headers, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> ToMemoryOperatorJson(const MemoryOperator &op, Document::AllocatorType &allocator) {
    document_t json(kObjectType);
    JsonUtil::AddMember(json, "id", op.id, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::NAME, op.name.empty() ? "Unknown" : op.name, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::SIZE, op.size, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ALLOCATION_TIME, op.allocationTime, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::RELEASE_TIME, op.releaseTime, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::DURATION, op.duration, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ALLOCATION_ALLOCATED, op.allocationAllocated, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ALLOCATION_RESERVE, op.allocationReserved, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::RELEASE_ALLOCATED, op.releaseAllocated, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::RELEASE_RESERVE, op.releaseReserved, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ACTIVE_RELEASE_TIME, op.activeReleaseTime, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ACTIVE_DURATION, op.activeDuration, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::ALLOCATION_ACTIVE, op.allocationActive, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::RELEASE_ACTIVE, op.releaseActive, allocator);
    JsonUtil::AddMember(json, OpMemoryColumn::STREAM, op.streamId, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MemoryComponentComparisonResponse>(
    const MemoryComponentComparisonResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t body(kObjectType);
    json_t columnAttr(kArrayType);
    for (const auto &attr : response.columnAttr) {
        json_t attrJson(kObjectType);
        JsonUtil::AddMember(attrJson, "name", attr.name, allocator);
        JsonUtil::AddMember(attrJson, "type", attr.type, allocator);
        JsonUtil::AddMember(attrJson, "key", attr.key, allocator);
        columnAttr.PushBack(attrJson, allocator);
    }
    json_t componentDiffDetail(kArrayType);
    for (const MemoryComponentComparison &aComponent : response.componentDiffDetails) {
        json_t basicJson(kObjectType);
        std::optional<document_t> jsonCompare = ToMemoryComponentJson(aComponent.compare, allocator);
        std::optional<document_t> jsonBaseline = ToMemoryComponentJson(aComponent.baseline, allocator);
        std::optional<document_t> jsonDiff = ToMemoryComponentJson(aComponent.diff, allocator);
        if (jsonCompare.has_value()) {
            JsonUtil::AddMember(basicJson, "compare", jsonCompare.value(), allocator);
        }
        if (jsonBaseline.has_value()) {
            JsonUtil::AddMember(basicJson, "baseline", jsonBaseline.value(), allocator);
        }
        if (jsonDiff.has_value()) {
            JsonUtil::AddMember(basicJson, "diff", jsonDiff.value(), allocator);
        }
        componentDiffDetail.PushBack(basicJson, allocator);
    }
    JsonUtil::AddMember(body, "totalNum", response.totalNum, allocator);
    JsonUtil::AddMember(body, "componentDetail", componentDiffDetail, allocator);
    JsonUtil::AddMember(body, "columnAttr", columnAttr, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> ToMemoryComponentJson(const MemoryComponent &component, Document::AllocatorType &allocator) {
    document_t json(kObjectType);
    if (component.component.empty()) {
        JsonUtil::AddMember(json, "component", "Unknown", allocator);
    } else {
        JsonUtil::AddMember(json, "component", component.component, allocator);
    }
    JsonUtil::AddMember(json, "timestamp", component.timestamp, allocator);
    JsonUtil::AddMember(json, "totalReserved", component.totalReserved, allocator);
    JsonUtil::AddMember(json, "device", component.device, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<MemoryViewResponse>(const MemoryViewResponse &response) {
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
    JsonUtil::AddMember(body, "rankOffsetNs", response.rankOffsetNs, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MemoryOperatorSizeResponse>(const MemoryOperatorSizeResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "minSize", response.size.minSize, allocator);
    JsonUtil::AddMember(body, "maxSize", response.size.maxSize, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<MemoryFindSliceResponse>(const MemoryFindSliceResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "id", response.data.id, allocator);
    JsonUtil::AddMember(body, "rankId", response.data.rankId, allocator);
    JsonUtil::AddMember(body, "processId", response.data.processId, allocator);
    JsonUtil::AddMember(body, "threadId", response.data.threadId, allocator);
    JsonUtil::AddMember(body, "metaType", response.data.metaType, allocator);
    JsonUtil::AddMember(body, "depth", response.data.depth, allocator);
    JsonUtil::AddMember(body, "startTime", response.data.startTime, allocator);
    JsonUtil::AddMember(body, "duration", response.data.duration, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<MemoryTypeResponse>(const MemoryTypeResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t graphIdList(kArrayType);
    for (const auto &graphId : response.graphId) {
        graphIdList.PushBack(json_t().SetString(graphId.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "type", response.type, allocator);
    JsonUtil::AddMember(body, "graphId", graphIdList, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MemoryResourceTypeResponse>(const MemoryResourceTypeResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "type", response.type, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MemoryStaticOperatorGraphResponse>(
    const MemoryStaticOperatorGraphResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
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
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MemoryStaticOperatorSizeResponse>(
    const MemoryStaticOperatorSizeResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "minSize", response.size.minSize, allocator);
    JsonUtil::AddMember(body, "maxSize", response.size.maxSize, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}
#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
