/*
* -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#ifndef PROFILER_SERVER_MEMSNAPSHOTRESPONSEDTO_H
#define PROFILER_SERVER_MEMSNAPSHOTRESPONSEDTO_H

#include <utility>
#include "MemSnapshotDefs.h"
#include "NumberUtil.h"

namespace Dic::Protocol {
using namespace Dic::Module::MemSnapshot;
struct MemSnapshotLeakStatsDTO {
    double totalSize{0.0};
    double maxSize{0.0};
    double minSize{0.0};
};

/**
 * @brief 内存生命周期图-内存块项
 */
struct BlockViewItemDTO : public JsonSerializable, Block {
    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "id", id, allocator);
        JsonUtil::AddMember(json, "addr", std::to_string(address), allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "_startTimestamp", allocEventId, allocator);
        JsonUtil::AddMember(json, "_endTimestamp", freeEventId, allocator);
        return json;
    }
};
/**
 * @brief 内存生命周期图-内存分配折线
 */
struct AllocationRecordDTO : public JsonSerializable {
    int64_t timestamp{0};
    uint64_t allocated{0};

    AllocationRecordDTO(const int64_t timestamp, const uint64_t allocated)
        : timestamp(timestamp), allocated(allocated) {}

    AllocationRecordDTO(const MemoryRecord &record) : timestamp(record.id), allocated(record.allocated) {}

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "timestamp", timestamp, allocator);
        JsonUtil::AddMember(json, "totalSize", allocated, allocator);
        return json;
    }
};
/**
 * @brief 内存池状态图-事件列表项
 */
struct TraceEntryListItemDTO : public JsonSerializable {
    int64_t id{-1};
    std::string action;
    uint64_t address{0};
    uint64_t size{0};
    uint64_t stream{0};

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "id", id, allocator);
        JsonUtil::AddMember(json, "action", action, allocator);
        JsonUtil::AddMember(json, "address", address, allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "stream", stream, allocator);
        return json;
    }
};
/**
 * @brief 内存池状态图-内存段中的内存块
 */
struct SegmentBlockItemDTO : public JsonSerializable {
    int64_t id{0};
    uint64_t size{0};
    uint64_t offset{0};

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "id", id, allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "offset", offset, allocator);
        return json;
    }
};
/**
 * @brief 内存池状态图- 单个Segment信息
 */
struct SegmentItemDTO : public JsonSerializable {
    uint64_t address{0};
    uint64_t stream{0};
    uint64_t size{0};
    int64_t allocOrMapEventId{-1};
    uint64_t allocated{0};
    std::vector<SegmentBlockItemDTO> blocks;

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "address", NumberUtil::Uint64ToHexString(address), allocator);
        JsonUtil::AddMember(json, "stream", stream, allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "allocOrMapEventId", allocOrMapEventId, allocator);
        JsonUtil::AddMember(json, "allocated", allocated, allocator);
        json_t blocksJson(kArrayType);
        for (const auto &block : blocks) {
            blocksJson.PushBack(block.ToJson(allocator), allocator);
        }
        JsonUtil::AddMember(json, "blocks", blocksJson, allocator);
        return json;
    }
};
/**
 * @brief 选中详情-内存事件详情
 */
struct TraceEntryDetailDTO : public JsonSerializable {
    TraceEntry entry;
    explicit TraceEntryDetailDTO(TraceEntry entry) : entry(std::move(entry)) {}

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "ID", entry.id, allocator);
        JsonUtil::AddMember(json, "Action", entry.action, allocator);
        JsonUtil::AddMember(json, "Address", NumberUtil::Uint64ToHexString(entry.address), allocator);
        JsonUtil::AddMember(json, "Size(MBytes)", NumberUtil::ConvertBytesToMBytes(entry.size), allocator);
        JsonUtil::AddMember(json, "Stream", entry.stream, allocator);
        JsonUtil::AddMember(
            json, "Caching Allocated(MBytes)", NumberUtil::ConvertBytesToMBytes(entry.allocated), allocator);
        JsonUtil::AddMember(json, "Caching Active(MBytes)", NumberUtil::ConvertBytesToMBytes(entry.active), allocator);
        JsonUtil::AddMember(
            json, "Caching Reserved(MBytes)", NumberUtil::ConvertBytesToMBytes(entry.reserved), allocator);
        JsonUtil::AddMember(json, "CallStack", entry.callstack, allocator);
        return json;
    }
};
/**
 * @brief 选中详情-内存块详情
 */
struct BlockDetailDTO : public JsonSerializable, Block {
    explicit BlockDetailDTO(const Block &block) : Block(block) {};
    std::optional<TraceEntryDetailDTO> allocEvent;
    std::optional<TraceEntryDetailDTO> freeRequestedEvent;
    std::optional<TraceEntryDetailDTO> freeCompletedEvent;

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "ID", id, allocator);
        JsonUtil::AddMember(json, "Requested Size(MBytes)", NumberUtil::ConvertBytesToMBytes(requestedSize), allocator);
        JsonUtil::AddMember(json, "Size(MBytes)", NumberUtil::ConvertBytesToMBytes(size), allocator);
        JsonUtil::AddMember(json, "Address", std::to_string(address), allocator);
        json_t allocEventJson(kObjectType);
        if (allocEvent.has_value() && allocEvent->entry.id >= 0) {
            allocEventJson = allocEvent->ToJson(allocator);
        }
        JsonUtil::AddMember(json, "Alloc Event", allocEventJson, allocator);

        json_t freeRequestedEventJson(kObjectType);
        if (freeRequestedEvent.has_value() && freeRequestedEvent->entry.id >= 0) {
            freeRequestedEventJson = freeRequestedEvent->ToJson(allocator);
        }
        JsonUtil::AddMember(json, "Free Requested Event", freeRequestedEventJson, allocator);

        json_t freeCompletedEventJson(kObjectType);
        if (freeCompletedEvent.has_value() && freeCompletedEvent->entry.id >= 0) {
            freeCompletedEventJson = freeCompletedEvent->ToJson(allocator);
        }
        JsonUtil::AddMember(json, "Free Completed Event", freeCompletedEventJson, allocator);
        return json;
    }
};
/**
 * @brief 系统视图-内存块表项
 */
struct BlockTableItemDTO : public JsonSerializable {
    int64_t id{0};
    uint64_t address{0};
    double size{0.0};
    double requestedSize{0.0};
    std::string state;
    int64_t allocEventId{-1};
    int64_t freeEventId{-1};

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(rapidjson::kObjectType);
        JsonUtil::AddMember(json, "id", id, allocator);
        JsonUtil::AddMember(json, "address", std::to_string(address), allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "requestedSize", requestedSize, allocator);
        JsonUtil::AddMember(json, "state", state, allocator);
        JsonUtil::AddMember(json, "allocEventId", allocEventId, allocator);
        JsonUtil::AddMember(json, "freeEventId", freeEventId, allocator);
        return json;
    }
};
/**
 * @brief 系统视图-内存事件表项
 */
struct TraceEntryTableItemDTO : public JsonSerializable {
    int64_t id{-1};
    std::string action;
    uint64_t address{0};
    double size{0};
    uint64_t stream{0};
    double allocated{0.0};
    double active{0.0};
    double reserved{0.0};
    std::string callstack;

    [[nodiscard]] json_t ToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const override {
        json_t json(kObjectType);
        JsonUtil::AddMember(json, "id", id, allocator);
        JsonUtil::AddMember(json, "action", action, allocator);
        JsonUtil::AddMember(json, "address", address, allocator);
        JsonUtil::AddMember(json, "size", size, allocator);
        JsonUtil::AddMember(json, "stream", stream, allocator);
        JsonUtil::AddMember(json, "allocated", allocated, allocator);
        JsonUtil::AddMember(json, "active", active, allocator);
        JsonUtil::AddMember(json, "reserved", reserved, allocator);
        JsonUtil::AddMember(json, "callstack", callstack, allocator);
        return json;
    }
};
}
#endif //PROFILER_SERVER_MEMSNAPSHOTRESPONSEDTO_H
