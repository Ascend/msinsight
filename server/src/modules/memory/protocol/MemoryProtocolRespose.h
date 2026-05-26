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

#ifndef PROFILER_SERVER_MEMORYPROTOCLRESPOSE_H
#define PROFILER_SERVER_MEMORYPROTOCLRESPOSE_H

#include <cstdint>
#include <string>
#include <vector>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "CommonRequests.h"
#include "MemoryTableView.h"
#include "MemoryDef.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Module::Memory;
// 待与MemoryDef.h中的Operator合并
struct MemoryOperator {
    std::string name;
    double size;
    std::string allocationTime;
    std::string releaseTime;
    double duration;
    std::string activeReleaseTime;
    double activeDuration;
    double allocationAllocated;
    double allocationReserved;
    double allocationActive;
    double releaseAllocated;
    double releaseReserved;
    double releaseActive;
    std::string streamId;
    std::string deviceType;
    std::string id;
};

struct MemoryOperatorComparison {
    MemoryOperator compare;
    MemoryOperator baseline;
    MemoryOperator diff;
};

struct MemoryComponent {
    std::string component;
    std::string timestamp;
    double totalReserved;
    std::string device;
};

struct MemoryComponentComparison {
    MemoryComponent compare;
    MemoryComponent baseline;
    MemoryComponent diff;
};

struct StaticOperatorItem {
    std::string deviceId;
    std::string opName;
    int64_t nodeIndexStart;
    int64_t nodeIndexEnd;
    double size;
};

struct StaticOperatorCompItem {
    StaticOperatorItem compare;
    StaticOperatorItem baseline;
    StaticOperatorItem diff;
};

struct StaticOperatorGraphItem {
    std::vector<std::string> legends;
    std::vector<std::vector<std::string>> lines;
};

struct MemoryViewData {
    std::string title;
    std::vector<std::string> legends;
    std::vector<std::vector<std::string>> lines;
    std::vector<double> tempData;
};

struct SliceInfo {
    std::string id;
    std::string rankId;
    std::string processId;
    std::string threadId;
    std::string metaType;
    uint32_t depth = 0;
    uint64_t startTime = 0;
    uint64_t duration = 0;
};

struct MemoryTableColumnAttr {
    std::string name;
    std::string type;
    std::string key;
};

struct MemoryTypeResponse : public Response {
    MemoryTypeResponse() : Response(REQ_RES_MEMORY_TYPE) {}
    std::string type = Module::Memory::MEMORY_TYPE_DYNAMIC;
    std::vector<std::string> graphId;
};

struct MemoryResourceTypeResponse : public Response {
    MemoryResourceTypeResponse() : Response(REQ_RES_MEMORY_RESOURCE_TYPE) {}
    std::string type = Module::Memory::MEMORY_RESOURCE_TYPE_PYTORCH;
};

struct MemoryOperatorComparisonResponse : public Response {
    MemoryOperatorComparisonResponse() : Response(REQ_RES_MEMORY_OPERATOR) {}
    std::vector<MemoryOperatorComparison> operatorDiffDetails;
    int64_t totalNum = 0;
    bool isCompare = false;
};

struct MemoryComponentComparisonResponse : public Response {
    MemoryComponentComparisonResponse() : Response(REQ_RES_MEMORY_COMPONENT) {}
    std::vector<MemoryTableColumnAttr> columnAttr;
    std::vector<MemoryComponentComparison> componentDiffDetails;
    int64_t totalNum = 0;
};

struct MemoryFindSliceResponse : public Response {
    MemoryFindSliceResponse() : Response(REQ_RES_MEMORY_FIND_SLICE) {}
    SliceInfo data;
};

struct MemoryViewResponse : public Response {
    MemoryViewResponse() : Response(REQ_RES_MEMORY_VIEW) {}
    MemoryViewData data;
    uint64_t rankOffsetNs{0};
};

struct OperatorSize {
    double minSize = 0.0;
    double maxSize = 0.0;
};

struct StaticOperatorSize {
    double minSize = 0.0;
    double maxSize = 0.0;
};

struct MemoryOperatorSizeResponse : public Response {
    MemoryOperatorSizeResponse() : Response(REQ_RES_MEMORY_OPERATOR_MIN_MAX) {}
    OperatorSize size;
};

struct MemoryStaticOperatorGraphResponse : public Response {
    MemoryStaticOperatorGraphResponse() : Response(REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH) {}
    StaticOperatorGraphItem data;
};

struct MemoryStaticOperatorListCompResponse : public JsonResponse {
    MemoryStaticOperatorListCompResponse() : JsonResponse(REQ_RES_MEMORY_STATIC_OP_MEMORY_LIST) {}
    std::vector<StaticOperatorCompItem> operatorDiffDetails;
    int64_t totalNum{0};
    bool isCompare{false};

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        std::vector<TableViewColumn> copyHeaders = {};
        for (auto &header : StaticOpTableView::FIELD_FULL_COLUMNS) {
            copyHeaders.push_back(header);
            if (isCompare && header.key == StaticOpColumn::OP_NAME) {
                copyHeaders.push_back(TABLE_VIEW_COMPARE_COLUMN);
            }
        }
        auto headers = TableViewColumn::CommonBuildTableHeadersJson(allocator, copyHeaders);
        json_t operatorDiffDetail(kArrayType);
        for (const StaticOperatorCompItem &anOperator : operatorDiffDetails) {
            json_t basicJson = json_t(kObjectType);
            std::optional<document_t> jsonCompare = ToMemoryStaticOperatorJson(anOperator.compare, allocator);
            std::optional<document_t> jsonBaseline = ToMemoryStaticOperatorJson(anOperator.baseline, allocator);
            std::optional<document_t> jsonDiff = ToMemoryStaticOperatorJson(anOperator.diff, allocator);
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
        JsonUtil::AddMember(body, "totalNum", totalNum, allocator);
        JsonUtil::AddMember(body, "operatorDetail", operatorDiffDetail, allocator);
        JsonUtil::AddMember(body, "columnAttr", headers, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    static std::optional<document_t> ToMemoryStaticOperatorJson(
        const StaticOperatorItem &op, Document::AllocatorType &allocator) {
        document_t json(kObjectType);
        JsonUtil::AddMember(json, StaticOpColumn::DEVICE_ID, op.deviceId, allocator);
        JsonUtil::AddMember(json, StaticOpColumn::OP_NAME, op.opName, allocator);
        JsonUtil::AddMember(json, StaticOpColumn::NODE_INDEX_START, op.nodeIndexStart, allocator);
        JsonUtil::AddMember(json, StaticOpColumn::NODE_INDEX_END, op.nodeIndexEnd, allocator);
        JsonUtil::AddMember(json, StaticOpColumn::SIZE, op.size, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemoryStaticOperatorSizeResponse : public Response {
    MemoryStaticOperatorSizeResponse() : Response(REQ_RES_MEMORY_STATIC_OP_MEMORY_MIN_MAX) {}
    StaticOperatorSize size;
};

struct ComponentDto {
    std::string component;
    double totalReserved;
    double totalAllocated;
    double totalActivated;
    std::string streamId;
    double timesTamp;
};

struct MemoryPeak {
    bool hasPtaGe = false;
    bool hasPta = false;
    bool hasGe = false;
    bool hasApp = false;
    bool hasWorkspace = false;
    double ptaGeAllocated = 0;
    double ptaGeReserved = 0;
    double ptaGeActivated = 0;
    double ptaAllocated = 0;
    double ptaReserved = 0;
    double ptaActivated = 0;
    double geAllocated = 0;
    double geReserved = 0;
    double geActivated = 0;
    double appReserved = 0;
};
} // end of namespace Protocol
} // end of namespace Dic
#endif // PROFILER_SERVER_MEMORYPROTOCLRESPOSE_H
