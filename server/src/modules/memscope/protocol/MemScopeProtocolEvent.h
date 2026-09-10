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

#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

#ifndef PROFILER_SERVER_MEM_SCOPE_PROTOCOL_EVENT_H
#define PROFILER_SERVER_MEM_SCOPE_PROTOCOL_EVENT_H
namespace Dic::Protocol {
struct MemSnapshotSliceEventInfo {
    int index{-1};
    int64_t startEventId{0};
    int64_t endEventId{-1};
    bool ready{false};
};

struct MemSnapshotDeviceSliceEventInfo {
    int64_t eventCount{0};
    int sliceCount{0};
    std::vector<int> readySlices;
    std::vector<MemSnapshotSliceEventInfo> slices;
};

struct MemScopeParseSuccessEventBody {
    std::string fileId;
    std::unordered_map<std::string, std::vector<std::string>> deviceIds;
    std::vector<uint64_t> threadIds;
    std::string module;
    std::string fileHash;
    bool snapshotParsingComplete{true};
    std::unordered_map<std::string, MemSnapshotDeviceSliceEventInfo> snapshotSlices;
};

struct MemScopeParseSuccessEvent : public JsonEvent {
    MemScopeParseSuccessEvent() : JsonEvent(EVENT_PARSE_MEM_SCOPE_COMPLETED) {}
    MemScopeParseSuccessEventBody body;
    std::string errMsg;

    static bool BuildMemScopeParseSuccessEventDeviceIdsJson(
        const MemScopeParseSuccessEvent &event, json_t &deviceIds, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        for (auto &devicePair : event.body.deviceIds) {
            json_t eventTypes(kArrayType);
            for (auto &eventType : devicePair.second) {
                eventTypes.PushBack(json_t().SetString(eventType.c_str(), allocator), allocator);
            }
            JsonUtil::AddMember(deviceIds, std::string_view(devicePair.first), eventTypes, allocator);
        }
        return true;
    }

    static json_t BuildSnapshotSlicesJson(
        const MemScopeParseSuccessEvent &event, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
        json_t devicesJson(kObjectType);
        for (const auto &[deviceId, device] : event.body.snapshotSlices) {
            json_t deviceJson(kObjectType);
            json_t readySlicesJson(kArrayType);
            json_t slicesJson(kArrayType);
            for (const auto sliceIndex : device.readySlices) {
                readySlicesJson.PushBack(sliceIndex, allocator);
            }
            for (const auto &slice : device.slices) {
                json_t sliceJson(kObjectType);
                JsonUtil::AddMember(sliceJson, "index", slice.index, allocator);
                JsonUtil::AddMember(sliceJson, "startEventId", slice.startEventId, allocator);
                JsonUtil::AddMember(sliceJson, "endEventId", slice.endEventId, allocator);
                JsonUtil::AddMember(sliceJson, "ready", slice.ready, allocator);
                slicesJson.PushBack(sliceJson, allocator);
            }
            JsonUtil::AddMember(deviceJson, "eventCount", device.eventCount, allocator);
            JsonUtil::AddMember(deviceJson, "sliceCount", device.sliceCount, allocator);
            JsonUtil::AddMember(deviceJson, "readySlices", readySlicesJson, allocator);
            JsonUtil::AddMember(deviceJson, "slices", slicesJson, allocator);
            devicesJson.AddMember(json_t().SetString(deviceId.c_str(), allocator), deviceJson, allocator);
        }
        return devicesJson;
    }

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetEventJsonBaseInfo(*this, json);
        json_t jsonBody(kObjectType);
        json_t deviceIds(kObjectType);
        json_t threadIds(kArrayType);
        json_t &moduleName = json["moduleName"];
        moduleName.SetString(Protocol::MODULE_MEM_SCOPE.c_str(), allocator);
        BuildMemScopeParseSuccessEventDeviceIdsJson(*this, deviceIds, allocator);
        JsonUtil::AddMember(jsonBody, "deviceIds", deviceIds, allocator);
        // 构建threadIds
        for (auto threadId : body.threadIds) {
            threadIds.PushBack(json_t().SetUint64(threadId), allocator);
        }
        JsonUtil::AddMember(jsonBody, "threadIds", threadIds, allocator);
        JsonUtil::AddMember(jsonBody, "dbPath", body.fileId, allocator);
        JsonUtil::AddMember(jsonBody, "module", body.module, allocator);
        JsonUtil::AddMember(jsonBody, "fileHash", body.fileHash, allocator);
        if (body.module == MODULE_MEM_SNAPSHOT) {
            JsonUtil::AddMember(jsonBody, "snapshotParsingComplete", body.snapshotParsingComplete, allocator);
            auto snapshotSlices = BuildSnapshotSlicesJson(*this, allocator);
            JsonUtil::AddMember(jsonBody, "snapshotSlices", snapshotSlices, allocator);
        }
        JsonUtil::AddMember(json, "body", jsonBody, allocator);
        JsonUtil::AddMember(json, "errMsg", errMsg, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};
}

#endif // PROFILER_SERVER_MEM_SCOPE_PROTOCOL_EVENT_H
