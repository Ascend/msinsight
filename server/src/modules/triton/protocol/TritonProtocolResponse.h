// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *
#ifndef PROFILER_SERVER_TRITON_PROTOCOL_RESPONSE_H
#define PROFILER_SERVER_TRITON_PROTOCOL_RESPONSE_H
#include <vector>
#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "TritonService.h"
namespace Dic::Protocol {
using namespace Dic::Module::Triton;
struct TritonMemoryBlocksResponse : public JsonResponse {
    TritonMemoryBlocksResponse() : JsonResponse(REQ_RES_TRITON_MEMORY_BLOCKS) {}
    std::vector<TritonTensorBlock> blocks;
    uint64_t maxSize{0};
    uint64_t minSize{std::numeric_limits<uint64_t>::max()};
    uint64_t maxTimeStamp{0};
    uint64_t minTimeStamp{std::numeric_limits<uint64_t>::max()};
    uint64_t totalSize{0};
    std::string status;
    std::string errMsg;
    void UpdateInfo() {
        std::for_each(blocks.begin(), blocks.end(), [this](const TritonTensorBlock &blk) {
            maxSize = std::max(maxSize, blk.size);
            minSize = std::min(minSize, blk.size);
            maxTimeStamp = std::max(maxTimeStamp, blk.end);
            minTimeStamp = std::min(minTimeStamp, blk.start);
            totalSize++;
        });
    }
    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t arr(kArrayType);
        for (const auto &b : blocks) {
            json_t item(kObjectType);
            JsonUtil::AddMember(item, "id", b.id, allocator);
            JsonUtil::AddMember(item, "addr", b.virtualAddress, allocator);
            JsonUtil::AddMember(item, "size", b.size, allocator);
            JsonUtil::AddMember(item, "_startTimestamp", b.start, allocator);
            JsonUtil::AddMember(item, "_endTimestamp", b.end, allocator);
            JsonUtil::AddMember(item, "attr", b.GetAttrStr(), allocator);
            arr.PushBack(item, allocator);
        }
        JsonUtil::AddMember(body, "blocks", arr, allocator);
        JsonUtil::AddMember(body, "maxSize", maxSize, allocator);
        JsonUtil::AddMember(body, "maxTimestamp", maxTimeStamp, allocator);
        JsonUtil::AddMember(body, "minSize", minSize, allocator);
        JsonUtil::AddMember(body, "minTimestamp", minTimeStamp, allocator);
        JsonUtil::AddMember(body, "status", status, allocator);
        JsonUtil::AddMember(body, "errMsg", errMsg, allocator);
        JsonUtil::AddMember(body, "total", totalSize, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct TritonBasicInfoResponse : public JsonResponse {
    TritonBasicInfoResponse() : JsonResponse(REQ_RES_TRITON_MEMORY_BASIC_INFO) {}
    std::string kernelName;
    std::vector<std::string> scopeTypes;
    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        JsonUtil::AddMember(body, "kernelName", kernelName, allocator);
        json_t memArray(kArrayType);
        for (const auto &scope : scopeTypes) {
            memArray.PushBack(json_t(kStringType).SetString(scope.c_str(), scope.size()), allocator);
        }
        JsonUtil::AddMember(body, "scopeTypes", memArray, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};

struct TritonMemoryUsageResponse : public JsonResponse {
    TritonMemoryUsageResponse() : JsonResponse(REQ_RES_TRITON_MEMORY_USAGE) {}
    std::vector<TritonTensorSegment> segments;
    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetResponseJsonBaseInfo(*this, json);
        json_t body(kObjectType);
        json_t arr(kArrayType);
        for (const auto &s : segments) {
            json_t item(kObjectType);
            JsonUtil::AddMember(item, "address", std::to_string(s.allocate), allocator);
            JsonUtil::AddMember(item, "totalSize", s.size, allocator);
            JsonUtil::AddMember(item, "callstack", s.sourceLocation, allocator);
            JsonUtil::AddMember(item, "_startTimestamp", s.start, allocator);
            JsonUtil::AddMember(item, "_endTimestamp", s.end, allocator);
            json_t blocksJson(kArrayType);
            for (const auto &b : s.blocks) {
                json_t blk(kObjectType);
                JsonUtil::AddMember(blk, "id", b.id, allocator);
                JsonUtil::AddMember(blk, "addr", b.virtualAddress, allocator);
                JsonUtil::AddMember(blk, "offset", b.offset, allocator);
                JsonUtil::AddMember(blk, "size", b.size, allocator);
                JsonUtil::AddMember(blk, "_startTimestamp", b.start, allocator);
                JsonUtil::AddMember(blk, "_endTimestamp", b.end, allocator);
                JsonUtil::AddMember(blk, "attr", b.GetAttrStr(), allocator);
                blocksJson.PushBack(blk, allocator);
            }
            JsonUtil::AddMember(item, "blocks", blocksJson, allocator);
            arr.PushBack(item, allocator);
        }
        JsonUtil::AddMember(body, "segments", arr, allocator);
        JsonUtil::AddMember(json, "body", body, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};
} // namespace Dic::Protocol
#endif
