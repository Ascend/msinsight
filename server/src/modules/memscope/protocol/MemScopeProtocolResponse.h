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
#ifndef PROFILER_SERVER_MEM_SCOPE_PROTOCOL_RESPONSE_H
#define PROFILER_SERVER_MEM_SCOPE_PROTOCOL_RESPONSE_H
#include <vector>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "CommonRequests.h"
#include "MemScopeDefs.h"
#include "MemScopeEntities.h"
#include "MemScopeTableColumn.h"
#include "MemScopeEventTree.h"

namespace Dic::Protocol {
using namespace Dic::Module;
using namespace Dic::Module::MemScope;

static document_t ToMemScopeMemoryBlockJson(const MemoryBlock &block, Document::AllocatorType &allocator) {
    document_t json(kObjectType);
    JsonUtil::AddMember(json, "id", block.id, allocator);
    JsonUtil::AddMember(json, "addr", block.ptr, allocator);
    JsonUtil::AddMember(json, "size", block.size, allocator);
    JsonUtil::AddMember(json, "_startTimestamp", block.startTimestamp, allocator);
    JsonUtil::AddMember(json, "_endTimestamp", block.endTimestamp, allocator);
    JsonUtil::AddMember(json, "owner", block.owner, allocator);
    JsonUtil::AddMember(json, "attr", block.attrJsonString, allocator);
    JsonUtil::AddMember(json, "processId", block.processId, allocator);
    JsonUtil::AddMember(json, "threadId", block.threadId, allocator);
    JsonUtil::AddMember(json, "deviceId", block.deviceId, allocator);
    JsonUtil::AddMember(json, "eventType", block.eventType, allocator);
    JsonUtil::AddMember(json, "_firstAccessTimestamp", block.firstAccessTimestamp, allocator);
    JsonUtil::AddMember(json, "_lastAccessTimestamp", block.lastAccessTimestamp, allocator);
    JsonUtil::AddMember(json, "maxAccessInterval", block.maxAccessInterval, allocator);
    JsonUtil::AddMember(json, "lazyUsed", block.lazyUsed, allocator);
    JsonUtil::AddMember(json, "delayedFree", block.delayedFree, allocator);
    JsonUtil::AddMember(json, "longIdle", block.longIdle, allocator);
    return json;
}

struct MemScopeMemoryBlocksResponse : public JsonResponse {
    MemScopeMemoryBlocksResponse() : JsonResponse(REQ_RES_MEM_SCOPE_MEMORY_BLOCKS) {}
    std::vector<MemoryBlock> blocks;
    uint64_t minTimestamp{};
    uint64_t maxTimestamp{};
    uint64_t minSize{};
    uint64_t maxSize{};
    uint64_t total{};

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t jsonBlocks(kArrayType);
        JsonUtil::AddMember(body, "minTimestamp", minTimestamp, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", maxTimestamp, allocator);
        JsonUtil::AddMember(body, "minSize", minSize, allocator);
        JsonUtil::AddMember(body, "maxSize", maxSize, allocator);
        JsonUtil::AddMember(body, "total", total, allocator);
        json_t jsonHeaders(kArrayType);
        for (const auto &header : BLOCK_TABLE::FIELD_FULL_COLUMNS) {
            if (header.visible) {
                jsonHeaders.PushBack(header.ToTableHeaderJson(allocator), allocator);
            }
        }
        for (const auto &block : blocks) {
            auto blockJson = ToMemScopeMemoryBlockJson(block, allocator);
            jsonBlocks.PushBack(blockJson, allocator);
        }
        JsonUtil::AddMember(body, "blocks", jsonBlocks, allocator);
        JsonUtil::AddMember(body, "headers", jsonHeaders, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemScopeMemoryAllocationsResponse : public JsonResponse {
    MemScopeMemoryAllocationsResponse() : JsonResponse(REQ_RES_MEM_SCOPE_MEMORY_ALLOCATIONS) {}
    uint64_t minTimestamp{};
    uint64_t maxTimestamp{};
    std::vector<MemoryAllocation> allocations;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t allocationsJson(kArrayType);
        JsonUtil::AddMember(body, "minTimestamp", minTimestamp, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", maxTimestamp, allocator);
        for (const auto &allocation : allocations) {
            auto allocationJson = ToMemScopeMemoryAllocationJson(allocation, allocator);
            if (allocationJson.has_value()) {
                allocationsJson.PushBack(allocationJson.value(), allocator);
            }
        }
        JsonUtil::AddMember(body, "allocations", allocationsJson, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    static std::optional<document_t> ToMemScopeMemoryAllocationJson(
        const MemoryAllocation &allocation, Document::AllocatorType &allocator) {
        document_t json(kObjectType);
        JsonUtil::AddMember(json, "id", allocation.id, allocator);
        JsonUtil::AddMember(json, "timestamp", allocation.timestamp, allocator);
        JsonUtil::AddMember(json, "totalSize", allocation.totalSize, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemScopeMemoryDetailsResponse : public JsonResponse {
    MemScopeMemoryDetailsResponse() : JsonResponse(REQ_RES_MEM_SCOPE_MEMORY_DETAILS) {}
    uint64_t timestamp{};
    std::unique_ptr<MemScopeMemoryDetailTreeNode> detail;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        auto body_json = ToMemScopeDetailTreeJson(detail.get(), allocator);
        if (!body_json.has_value()) {
            body_json = document_t(kObjectType);
        }
        JsonUtil::AddMember(json, "body", body_json.value(), allocator);
        return std::optional<document_t>{std::move(json)};
    }

    // 此处转json存在递归，但treeNode在构造时确保了层数不超过8
    static std::optional<document_t> ToMemScopeDetailTreeJson(
        MemScopeMemoryDetailTreeNode *treeNode, Document::AllocatorType &allocator) {
        if (treeNode == nullptr) {
            return std::nullopt;
        }
        document_t json(kObjectType);
        JsonUtil::AddMember(json, "name", treeNode->name, allocator);
        JsonUtil::AddMember(json, "tag", treeNode->tag, allocator);
        JsonUtil::AddMember(json, "size", treeNode->size, allocator);
        json_t subNodes(kArrayType);
        for (auto &subNode : treeNode->children) {
            auto subNodeJson = ToMemScopeDetailTreeJson(subNode.get(), allocator);
            if (subNodeJson.has_value()) {
                subNodes.PushBack(subNodeJson.value(), allocator);
            }
        }
        JsonUtil::AddMember(json, "subNodes", subNodes, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemScopePythonTracesResponse : public JsonResponse {
    MemScopePythonTracesResponse() : JsonResponse(REQ_RES_MEM_SCOPE_PYTHON_TRACES) {}
    MemScopePythonTrace trace;
    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        document_t body(kObjectType);
        auto tracesJson = ToMemScopePythonTracesJson(trace.slices, allocator);
        if (!tracesJson.has_value()) {
            tracesJson = document_t(kObjectType);
        }
        JsonUtil::AddMember(body, "traces", tracesJson.value(), allocator);
        JsonUtil::AddMember(body, "minTimestamp", trace.minTimestamp, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", trace.maxTimestamp, allocator);
        JsonUtil::AddMember(body, "maxDepth", trace.maxDepth, allocator);
        JsonUtil::AddMember(json, "threadId", trace.threadId, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    static std::optional<document_t> ToMemScopePythonTracesJson(
        const std::vector<PythonTraceSlice> &slices, Document::AllocatorType &allocator) {
        document_t json(kArrayType);
        for (auto &slice : slices) {
            document_t traceJson(kObjectType);
            JsonUtil::AddMember(traceJson, "func", slice.func, allocator);
            JsonUtil::AddMember(traceJson, "startTimestamp", slice.startTimestamp, allocator);
            JsonUtil::AddMember(traceJson, "endTimestamp", slice.endTimestamp, allocator);
            JsonUtil::AddMember(traceJson, "depth", slice.depth, allocator);
            json.PushBack(traceJson, allocator);
        }
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemScopeEventResponse : public JsonResponse {
    MemScopeEventResponse() : JsonResponse(REQ_RES_MEM_SCOPE_EVENTS) {}
    int64_t total{};
    // 注意此处会将events转换为扁平data数组后返回给前端
    std::vector<MemScopeEvent> events;
    bool withCallStackC = false;
    bool withCallStackPython = false;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t headers(kArrayType);
        for (auto const &header : EVENT_TABLE::FIELD_FULL_COLUMNS) {
            bool needed = true;
            if (header.name == EVENT_TABLE::CALL_STACK_C) {
                needed = withCallStackC;
            }
            if (header.name == EVENT_TABLE::CALL_STACK_PYTHON) {
                needed = withCallStackPython;
            }
            if (header.visible && needed) {
                headers.PushBack(header.ToTableHeaderJson(allocator), allocator);
            }
        }
        JsonUtil::AddMember(body, "headers", headers, allocator);
        JsonUtil::AddMember(body, "events", ToTableDataJson(allocator), allocator);
        JsonUtil::AddMember(body, "total", total, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }

    document_t ToTableDataJson(Document::AllocatorType &allocator) const {
        document_t dataJson(kArrayType);
        for (auto &event : events) {
            document_t eventJson(kObjectType);
            JsonUtil::AddMember(eventJson, "id", event.id, allocator);
            JsonUtil::AddMember(eventJson, "event", event.event, allocator);
            JsonUtil::AddMember(eventJson, "eventType", event.eventType, allocator);
            JsonUtil::AddMember(eventJson, "name", event.name, allocator);
            JsonUtil::AddMember(eventJson, "_timestamp", event.timestamp, allocator);
            JsonUtil::AddMember(eventJson, "processId", event.processId, allocator);
            JsonUtil::AddMember(eventJson, "threadId", event.threadId, allocator);
            JsonUtil::AddMember(eventJson, "deviceId", event.deviceId, allocator);
            JsonUtil::AddMember(eventJson, "ptr", event.ptr, allocator);
            JsonUtil::AddMember(eventJson, "attr", event.attr, allocator);
            if (withCallStackC) {
                JsonUtil::AddMember(eventJson, "callStackC", event.callStackC, allocator);
            }
            if (withCallStackPython) {
                JsonUtil::AddMember(eventJson, "callStackPython", event.callStackPython, allocator);
            }
            dataJson.PushBack(eventJson, allocator);
        }
        return dataJson;
    }
};
} // end of namespace Dic::Protocol
#endif // PROFILER_SERVER_MEM_SCOPE_PROTOCOL_RESPONSE_H
