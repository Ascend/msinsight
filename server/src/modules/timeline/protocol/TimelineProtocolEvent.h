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

#ifndef DIC_PROTOCOL_EVENT_H
#define DIC_PROTOCOL_EVENT_H

#include <vector>
#include <memory>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
struct UnitMetaData {
    std::string cardId;
    std::string cardAlias;
};

struct UnitTrackMetaData {
    std::string cardId;
    std::string processId;
    std::string parentProcessId = "0";
    std::string processName; // type = process
    std::string label; // type = process
    std::string threadId; // type = thread
    std::string threadName; // type = thread, counter
    std::string groupNameValue; // type = thread, DB 在 PROCESS_TYPE::HCCL 时赋值；TEXT 看 threadName 形式符合赋值
    std::vector<std::string> rankList; // type = thread, 为HCCL中group甬道时赋值，内容为通信域内所有rankId信息
    std::string metaType;
    int maxDepth = 0; // type = thread
    std::vector<std::string> dataType; // type = counter
};

struct UnitTrack {
    std::string type;
    UnitTrackMetaData metaData;
    std::vector<std::unique_ptr<UnitTrack>> children;
};

struct Unit {
    std::string type;
    UnitMetaData metadata;
    std::vector<std::unique_ptr<UnitTrack>> children;
};

struct ThreadGroup {
    std::string cardId;
    std::string processId;
    std::vector<std::string> threadIds;

    void push(const std::string &threadId) { threadIds.push_back(threadId); }

    json_t SerializationToJson(RAPIDJSON_DEFAULT_ALLOCATOR &allocator) const {
        json_t group(kObjectType);
        JsonUtil::AddMember(group, "cardId", cardId, allocator);
        JsonUtil::AddMember(group, "processId", processId, allocator);
        json_t list(kArrayType);
        for (const auto &thread : threadIds) {
            list.PushBack(json_t().SetString(thread.c_str(), allocator), allocator);
        }
        JsonUtil::AddMember(group, "threadIds", list, allocator);
        return group;
    }
};

struct ParseSuccessEventBody {
    Unit unit;
    std::vector<ThreadGroup> threadGroupList;
    bool startTimeUpdated = false;
    bool isFullDb = false;
    bool isRl = false;
    uint64_t maxTimeStamp = 0;
    uint64_t startTime = 0;
    uint64_t offset = 0;
    std::string fileId;
    std::vector<RankInfo> rankList; // 值为 cluster + host + rankId + deviceId 拼接
};

struct ParseSuccessEvent : public Event {
    ParseSuccessEvent() : Event(EVENT_PARSE_SUCCESS) {}
    ParseSuccessEventBody body;
};

struct ParseFailEventBody {
    std::string rankId;
    std::string error;
    std::string dbPath;
};

struct ParseFailEvent : public Event {
    ParseFailEvent() : Event(EVENT_PARSE_FAIL) {}
    ParseFailEventBody body;
};

struct MemorySuccess {
    std::string rankId;
    std::string fileId;
    bool parseSuccess = false;
    bool hasFile = false;
    RankInfo rankInfo;
};

struct ParseClusterCompletedEventBody {
    std::string parseResult;
    std::string clusterPath;
    bool isShowCluster = false;
    bool isAllPageParsed = false;
};

struct ParseClusterCompletedEvent : public Event {
    ParseClusterCompletedEvent() : Event(EVENT_PARSE_CLUSTER_COMPLETED) {}
    ParseClusterCompletedEventBody body;
};

struct CardOffset {
    std::string cardId;
    uint64_t offset = 0;
};

struct AllSuccessEventEventBody {
    bool isAllPageParsed = false;
    std::vector<CardOffset> cardOffsets;
    uint64_t minTime = 0;
};

struct AllSuccessEvent : public Event {
    AllSuccessEvent() : Event(EVENT_ALL_SUCCESS) {}
    AllSuccessEventEventBody body;
};

struct ParseClusterStep2CompletedEvent : public Event {
    ParseClusterStep2CompletedEvent() : Event(EVENT_PARSE_CLUSTER_STEP2_COMPLETED) {}
    ParseClusterCompletedEventBody body;
};

struct ParseMemoryCompletedEvent : public Event {
    ParseMemoryCompletedEvent() : Event(EVENT_PARSE_MEMORY_COMPLETED) {}
    bool isCluster = false;
    std::vector<MemorySuccess> memoryResult;
    std::string fileId;
};

struct ModuleResetEvent : public Event {
    ModuleResetEvent() : Event(EVENT_MODULE_RESET) {}
    bool reset = false;
};

struct ParseProgressEventBody {
    std::string fileId;
    uint64_t parsedSize = 0;
    uint64_t totalSize = 0;
    int progress = 0;
};

struct ParseProgressEvent : public Event {
    ParseProgressEvent() : Event(EVENT_PARSE_PROGRESS) {}
    ParseProgressEventBody body;
};

struct ParseHeatmapCompletedBody {
    bool parseResult = false;
    std::string errorMsg;
};

struct ParseHeatmapCompletedEvent : public Event {
    ParseHeatmapCompletedEvent() : Event(EVENT_PARSE_HEATMAP_COMPLETED) {}
    ParseHeatmapCompletedBody body;
};

struct ParseUnitCompletedBody {
    bool parseResult = false;
    std::string errorMsg;
    std::string unitName;
    std::string dbId;
};

struct ParseUnitCompletedEvent : public Event {
    ParseUnitCompletedEvent() : Event(EVENT_PARSE_UNIT_COMPLETED) {}
    ParseUnitCompletedBody body;
};
} // end of namespace Protocol
} // end if namespace Dic

#endif // DIC_PROTOCOL_EVENT_H
