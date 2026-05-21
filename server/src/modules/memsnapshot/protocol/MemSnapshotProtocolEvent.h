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

#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_EVENT_H
#define PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_EVENT_H
namespace Dic::Protocol {
struct MemSnapshotParseProgressEventBody {
    std::string fileId;
    int progress = 0;
};

struct MemSnapshotParseProgressEvent : public JsonEvent {
    MemSnapshotParseProgressEvent() : JsonEvent(EVENT_PARSE_MEM_SNAPSHOT_PROGRESS) {}
    MemSnapshotParseProgressEventBody body;

    [[nodiscard]] std::optional<document_t> ToJson() const override {
        document_t json(kObjectType);
        auto &allocator = json.GetAllocator();
        ProtocolUtil::SetEventJsonBaseInfo(*this, json);
        json_t jsonBody(kObjectType);
        JsonUtil::AddMember(jsonBody, "fileId", body.fileId, allocator);
        JsonUtil::AddMember(jsonBody, "progress", body.progress, allocator);
        JsonUtil::AddMember(json, "body", jsonBody, allocator);
        return std::optional<document_t>{std::move(json)};
    }
};
}

#endif // PROFILER_SERVER_MEM_SNAPSHOT_PROTOCOL_EVENT_H
