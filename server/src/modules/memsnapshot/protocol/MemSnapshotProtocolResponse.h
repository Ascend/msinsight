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

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_RESPONSE_H
#define PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_RESPONSE_H
#include <vector>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "CommonRequests.h"
#include "MemSnapshotDefs.h"
#include "MemSnapshotTableColumn.h"
#include "MemSnapshotResponseDTO.h"

namespace Dic::Protocol {
using namespace Dic::Module;
using namespace Dic::Module::MemSnapshot;
using ColumnBounds = std::unordered_map<std::string_view, std::pair<int64_t, int64_t>>;

struct MemSnapshotLeakStatsResponse : public JsonResponse, MemSnapshotLeakStatsDTO {
    MemSnapshotLeakStatsResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_LEAK_STATS) {}

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        JsonUtil::AddMember(body, "totalSize", totalSize, allocator);
        JsonUtil::AddMember(body, "maxSize", maxSize, allocator);
        JsonUtil::AddMember(body, "minSize", minSize, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemSnapshotBlocksResponse : public JsonResponse {
    MemSnapshotBlocksResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_BLOCKS) {}
    std::vector<BlockTableItemDTO> tableBlocks;
    std::vector<BlockViewItemDTO> viewBlocks;
    uint64_t minSize{0};
    uint64_t maxSize{0};
    uint64_t minTimestamp{0};
    uint64_t maxTimestamp{0};
    uint64_t total{0};
    bool isTable{false};

    ColumnBounds rangeFiltersBoundsMap;

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
        if (isTable) {
            json_t jsonHeaders(kArrayType);
            for (const auto &header : BlockTableColumn::FIELD_FULL_COLUMNS) {
                if (!header.visible) {
                    continue;
                }
                auto headerJson = header.ToTableHeaderJson(allocator);
                if (header.rangeFilterable && rangeFiltersBoundsMap.find(header.key) != rangeFiltersBoundsMap.end()) {
                    JsonUtil::AddMember(headerJson, "min", rangeFiltersBoundsMap.at(header.key).first, allocator);
                    JsonUtil::AddMember(headerJson, "max", rangeFiltersBoundsMap.at(header.key).second, allocator);
                }
                jsonHeaders.PushBack(headerJson, allocator);
            }
            JsonUtil::AddMember(body, "headers", jsonHeaders, allocator);
            for (const auto &block : tableBlocks) {
                auto blockJson = block.ToJson(allocator);
                jsonBlocks.PushBack(blockJson, allocator);
            }
        } else {
            for (auto &block : viewBlocks) {
                if (block.allocEventId >= 0 && block.freeEventId >= 0) {
                    auto blockJson = block.ToJson(allocator);
                    jsonBlocks.PushBack(blockJson, allocator);
                    continue;
                }
                auto tmpBlock = BlockViewItemDTO(block);
                tmpBlock.allocEventId = block.allocEventId < 0 ? 0 : block.allocEventId;
                // maxTimestamp取自最大事件ID，不可能超过INT64_MAX，此处无溢出风险
                tmpBlock.freeEventId = block.freeEventId < 0 ? static_cast<int64_t>(maxTimestamp) : block.freeEventId;
                auto blockJson = tmpBlock.ToJson(allocator);
                jsonBlocks.PushBack(blockJson, allocator);
            }
        }
        JsonUtil::AddMember(body, "blocks", jsonBlocks, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemSnapshotEventsResponse : public JsonResponse {
    MemSnapshotEventsResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_EVENTS) {}
    std::vector<TraceEntryListItemDTO> listEntries;
    std::vector<TraceEntryTableItemDTO> tableEntries;
    uint64_t total{0};
    uint64_t minTimestamp{0};
    uint64_t maxTimestamp{0};

    bool isTable{false};

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t jsonEntries(kArrayType);
        JsonUtil::AddMember(body, "minTimestamp", minTimestamp, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", maxTimestamp, allocator);
        JsonUtil::AddMember(body, "total", total, allocator);
        json_t jsonHeaders(kArrayType);
        if (isTable) {
            for (const auto &header : TraceEntryTableColumn::FIELD_FULL_COLUMNS) {
                if (header.visible) {
                    jsonHeaders.PushBack(header.ToTableHeaderJson(allocator), allocator);
                }
            }
            JsonUtil::AddMember(body, "headers", jsonHeaders, allocator);
            for (const auto &entry : tableEntries) {
                auto entryJson = entry.ToJson(allocator);
                jsonEntries.PushBack(entryJson, allocator);
            }
        } else {
            for (const auto &entry : listEntries) {
                auto entryJson = entry.ToJson(allocator);
                jsonEntries.PushBack(entryJson, allocator);
            }
        }
        JsonUtil::AddMember(body, "events", jsonEntries, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemSnapshotAllocationsResponse : public JsonResponse {
    MemSnapshotAllocationsResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_ALLOCATIONS) {}
    uint64_t minEventId{0};
    uint64_t maxEventId{0};
    std::vector<AllocationRecordDTO> records;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t recordsJson(kArrayType);
        JsonUtil::AddMember(body, "minTimestamp", minEventId, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", maxEventId, allocator);
        for (const auto &record : records) {
            auto recordJson = record.ToJson(allocator);
            recordsJson.PushBack(recordJson, allocator);
        }
        JsonUtil::AddMember(body, "allocations", recordsJson, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemSnapshotDetailResponse : public JsonResponse {
    MemSnapshotDetailResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_DETAIL) {}
    std::string type;
    std::optional<std::unique_ptr<JsonSerializable>> detail;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        if (detail.has_value()) {
            auto detailJson = detail.value()->ToJson(allocator);
            JsonUtil::AddMember(json, "body", detailJson, allocator);
        }
        return std::optional<document_t>{std::move(json)};
    }
};

struct MemSnapshotStateResponse : public JsonResponse {
    MemSnapshotStateResponse() : JsonResponse(REQ_RES_MEM_SNAPSHOT_STATE) {}
    std::vector<SegmentItemDTO> segments;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t segmentsJson(kArrayType);
        for (const auto &segment : segments) {
            segmentsJson.PushBack(segment.ToJson(allocator), allocator);
        }
        JsonUtil::AddMember(body, "segments", segmentsJson, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};
}
#endif // PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_RESPONSE_H
