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

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_REQUEST_H
#define PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_REQUEST_H

#include "MemSnapshotTableColumn.h"
#include "MemSnapshotDefs.h"
#include "CommonRequests.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "pch.h"

namespace Dic::Protocol {
using namespace Dic::Module::MemSnapshot;

struct MemSnapshotLeakStatsParams {
    uint64_t startEventIdx{0};
    uint64_t endEventIdx{0};
    std::string deviceId;

    bool CommonCheck(std::string &errorMsg) const {
        if (startEventIdx > endEventIdx) {
            errorMsg = "The start idx should be less than the end idx.";
            return false;
        }
        if (endEventIdx > INT64_MAX) {
            errorMsg = "Invalid idx, detail: exceeds the maximum limit of " + std::to_string(INT64_MAX);
            return false;
        }
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        return true;
    }
};

struct MemSnapshotBlockParams : public CommonTableParams {
    // BlockViewItemDTO 按最大字段宽度序列化约 160 bytes/条；限制为 32000，
    // 为 JupyterLab 代理的 10 MiB 单消息上限保留充足余量。
    static constexpr int64_t MAX_VIEW_PAGE_SIZE = 32000;
    uint64_t startEventIdx{0};
    uint64_t endEventIdx{0};
    uint64_t minSize{0};
    uint64_t maxSize{0};
    std::string deviceId;
    std::string eventType;
    // 标识是否仅请求start、end区间内申请或释放的block
    bool onlyAllocOrFreeInRange{false};
    bool onlyUnreleasedInRange{false};

    bool CommonCheck(std::string &errorMsg) const {
        if (!CheckBaseFields(errorMsg)) {
            return false;
        }
        return PaginationParam::Check(errorMsg);
    }

    // 图视图（isTable=false）场景校验：不带分页参数(0/0)表示全量，兼容旧前端；
    // 带分页参数时放宽 pageSize 上限（前端分片拉取会逐步翻倍到 32000，全局
    // MAX_PAGESIZE=1000 不适用，与 events 列表场景的宽松校验保持一致）
    bool CommonCheckForView(std::string &errorMsg) const {
        if (!CheckBaseFields(errorMsg)) {
            return false;
        }
        if (currentPage == 0 && pageSize == 0) {
            return true; // 全量兼容路径
        }
        if (currentPage <= 0 || pageSize <= 0) {
            errorMsg = "Invalid pagination params, detail: pageSize and currentPage must be greater than 0";
            return false;
        }
        if (pageSize > MAX_VIEW_PAGE_SIZE) {
            errorMsg =
                "Invalid pagination params, detail: pageSize must not exceed " + std::to_string(MAX_VIEW_PAGE_SIZE);
            return false;
        }
        if (INT64_MAX / pageSize < currentPage) {
            errorMsg = "Invalid pagination params, detail: currentPage exceeds the maximum value";
            return false;
        }
        return true;
    }

  private:
    bool CheckBaseFields(std::string &errorMsg) const {
        if (minSize > maxSize) {
            errorMsg = "[minSize] must be less than [maxSize].";
            return false;
        }
        if (startEventIdx > endEventIdx) {
            errorMsg = "The start idx should be less than the end idx.";
            return false;
        }
        if (endEventIdx > INT64_MAX) {
            errorMsg = "Invalid idx, detail: exceeds the maximum limit of " + std::to_string(INT64_MAX);
            return false;
        }
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        if (!CheckStrParamValid(eventType, errorMsg)) {
            errorMsg = "Invalid eventType, detail: " + errorMsg;
            return false;
        }
        return true;
    }
};

struct MemSnapshotAllocationParams {
    std::string deviceId;
    std::string eventType;

    MemSnapshotAllocationParams() = default;

    bool CommonCheck(std::string &errorMsg) const {
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        if (!CheckStrParamValid(eventType, errorMsg)) {
            errorMsg = "Invalid eventType, detail: " + errorMsg;
            return false;
        }
        return true;
    }
};

struct MemSnapshotEventParams : public CommonTableParams {
    uint64_t startEventIdx{0};
    uint64_t endEventIdx{0};
    std::string deviceId;

    bool CommonCheck(std::string &errorMsg) const {
        if (startEventIdx > endEventIdx) {
            errorMsg = "The start idx should be less than the end idx.";
            return false;
        }
        if (endEventIdx > INT64_MAX) {
            errorMsg = "Invalid idx, detail: exceeds the maximum limit of " + std::to_string(INT64_MAX);
            return false;
        }
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        return PaginationParam::Check(errorMsg);
    }
};

struct MemSnapshotLeakStatsRequest : Request {
    MemSnapshotLeakStatsRequest() : Request(REQ_RES_MEM_SNAPSHOT_LEAK_STATS) {}
    MemSnapshotLeakStatsParams params;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemSnapshotLeakStatsRequest> reqPtr = std::make_unique<MemSnapshotLeakStatsRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params or deviceId.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.startEventIdx, param_json, "startTimestamp");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.endEventIdx, param_json, "endTimestamp");
        return reqPtr;
    }
};

struct MemSnapshotBlocksRequest : Request {
    MemSnapshotBlocksRequest() : Request(REQ_RES_MEM_SNAPSHOT_BLOCKS) {}
    MemSnapshotBlockParams params;
    bool isTable{};

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemSnapshotBlocksRequest> reqPtr = std::make_unique<MemSnapshotBlocksRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId") ||
            !json["params"].HasMember("eventType")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) +
                "] json lacks member params or deviceId or eventType.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->isTable, param_json, "isTable");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.startEventIdx, param_json, "startTimestamp");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.endEventIdx, param_json, "endTimestamp");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.minSize, param_json, "minSize");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.maxSize, param_json, "maxSize");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.eventType, param_json, "eventType");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.onlyUnreleasedInRange, param_json, "onlyUnreleasedInRange");
        if (reqPtr->isTable) {
            if (!reqPtr->params.SetFromJson(param_json, BlockTableColumn::FIELD_FULL_COLUMNS, error)) {
                Server::ServerLog::Error("Failed set common table params from json param: %", error);
                return nullptr;
            }
        } else {
            // 展示block图时，只可根据allocEventId排序
            reqPtr->params.orderBy = std::string(BlockTableColumn::ALLOC_EVENT_ID);
            // 图视图支持分页拉取（不带分页参数时 currentPage=pageSize=0，表示全量，兼容旧前端）
            reqPtr->params.SetPaginationParamFromJson(param_json);
        }
        return reqPtr;
    }
};

struct MemSnapshotEventsRequest : Request {
    MemSnapshotEventsRequest() : Request(REQ_RES_MEM_SNAPSHOT_EVENTS) {}
    MemSnapshotEventParams params;
    bool isTable{};

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemSnapshotEventsRequest> reqPtr = std::make_unique<MemSnapshotEventsRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params or deviceId.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->isTable, param_json, "isTable");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        // 仅在table场景下
        if (reqPtr->isTable) {
            // 为兼容memscope数据请求的开始、结束时间戳，此处api仍然接收为startTimestamp、endTimestamp，但内部转换为事件索引
            JsonUtil::SetByJsonKeyValue(reqPtr->params.startEventIdx, param_json, "startTimestamp");
            JsonUtil::SetByJsonKeyValue(reqPtr->params.endEventIdx, param_json, "endTimestamp");
            if (!reqPtr->params.SetFromJson(param_json, TraceEntryTableColumn::FIELD_FULL_COLUMNS, error)) {
                Server::ServerLog::Error("Failed set common table params from json param: %", error);
                return nullptr;
            }
        } else {
            // 展示事件列表时，固定只接收分页参数、且仅根据id排序（primary key，默认）
            reqPtr->params.SetPaginationParamFromJson(param_json);
        }
        return reqPtr;
    }
};

struct MemSnapshotAllocationsRequest : Request {
    MemSnapshotAllocationsRequest() : Request(REQ_RES_MEM_SNAPSHOT_ALLOCATIONS) {}
    MemSnapshotAllocationParams params;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemSnapshotAllocationsRequest> reqPtr = std::make_unique<MemSnapshotAllocationsRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId") ||
            !json["params"].HasMember("eventType")) {
            error = "Request[requestId=" + std::to_string(reqPtr->id) +
                "] json lacks member params or deviceId or eventType.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.eventType, param_json, "eventType");
        return reqPtr;
    }
};

struct MemSnapshotDetailParams {
    std::string deviceId;
    std::string type;
    int64_t id{0};
    uint64_t eventId{0};
    std::string segmentAddress;
    uint64_t stream{0};
    bool hasEventId{false};
    bool hasSegmentAddress{false};
    bool hasStream{false};

    bool CommonCheck(std::string &errorMsg) const {
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        if (VALID_DETAIL_TYPES.find(type) == VALID_DETAIL_TYPES.end()) {
            errorMsg = "Invalid type";
            return false;
        }
        if (type == DETAIL_TYPE_SEGMENT) {
            if (!hasEventId || !hasSegmentAddress || !hasStream) {
                errorMsg = "Invalid segment detail params";
                return false;
            }
            if (eventId > INT64_MAX) {
                errorMsg = "eventId exceeds INT64_MAX";
                return false;
            }
            if (!CheckStrParamValid(segmentAddress, errorMsg)) {
                errorMsg = "Invalid segmentAddress, detail: " + errorMsg;
                return false;
            }
        }
        return true;
    }
};

struct MemSnapshotDetailRequest : Request {
    MemSnapshotDetailRequest() : Request(REQ_RES_MEM_SNAPSHOT_DETAIL) {}
    MemSnapshotDetailParams params;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        std::unique_ptr<MemSnapshotDetailRequest> reqPtr = std::make_unique<MemSnapshotDetailRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId") || !json["params"].HasMember("type") ||
            !json["params"].HasMember("id")) {
            error =
                "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params deviceId, type or id.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.type, param_json, "type");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.id, param_json, "id");
        reqPtr->params.hasEventId = param_json.HasMember("eventId");
        reqPtr->params.hasSegmentAddress = param_json.HasMember("segmentAddress");
        reqPtr->params.hasStream = param_json.HasMember("stream");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.eventId, param_json, "eventId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.segmentAddress, param_json, "segmentAddress");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.stream, param_json, "stream");
        return reqPtr;
    }
};

struct MemSnapshotStateParams {
    uint64_t eventId{0};
    std::string deviceId;

    bool CommonCheck(std::string &errorMsg) const {
        if (!CheckStrParamValid(deviceId, errorMsg)) {
            errorMsg = "Invalid deviceId, detail: " + errorMsg;
            return false;
        }
        if (eventId > INT64_MAX) {
            errorMsg = "eventId exceeds INT64_MAX";
            return false;
        }
        return true;
    }
};

struct MemSnapshotStateRequest : Request {
    MemSnapshotStateRequest() : Request(REQ_RES_MEM_SNAPSHOT_STATE) {}
    MemSnapshotStateParams params;

    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error) {
        auto reqPtr = std::make_unique<MemSnapshotStateRequest>();
        if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
            error = "Failed to set request base info, command is: " + reqPtr->command;
            return nullptr;
        }
        if (!json.HasMember("params") || !json["params"].HasMember("deviceId") ||
            !json["params"].HasMember("eventId")) {
            error =
                "Request[requestId=" + std::to_string(reqPtr->id) + "] json lacks member params, deviceId or eventId.";
            return nullptr;
        }
        const json_t &param_json = json["params"];
        JsonUtil::SetByJsonKeyValue(reqPtr->params.eventId, param_json, "eventId");
        JsonUtil::SetByJsonKeyValue(reqPtr->params.deviceId, param_json, "deviceId");
        return reqPtr;
    }
};

} // namespace Dic::Protocol

#endif // PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_REQUEST_H
