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

#include "pch.h"
#include "RLProtocol.h"
#include "RLProtocolUtil.h"

namespace Dic::Protocol {
using namespace Dic::Server;
using namespace rapidjson;
#pragma region <<Response to json>>
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response) {
    ServerLog::Warn("Function to response json is not implemented. command:", response.command);
    return std::nullopt;
}

template <> std::optional<document_t> ToResponseJson<RLPipelineResponse>(const RLPipelineResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "minTime", response.body.minTime, allocator);
    JsonUtil::AddMember(body, "maxTime", response.body.maxTime, allocator);
    std::optional<document_t> taskDataJson = RLPipelineListToJson(response.body.taskData, allocator);
    std::optional<document_t> microBatchDataJson = RLPipelineListToJson(response.body.microBatchData, allocator);
    JsonUtil::AddMember(body, "taskData", taskDataJson, allocator);
    JsonUtil::AddMember(body, "microBatchData", microBatchDataJson, allocator);
    JsonUtil::AddMember(body, "stageTypeList", response.body.stageTypeList, allocator);
    JsonUtil::AddMember(body, "backendType", response.body.backendType, allocator);
    JsonUtil::AddMember(body, "framework", response.body.framework, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> RLPipelineListToJson(
    const std::vector<RLPipelineItem> &pipelineList, Document::AllocatorType &allocator) {
    document_t data(kArrayType);
    for (const auto &item : pipelineList) {
        json_t pipelineItem(kObjectType);
        JsonUtil::AddMember(pipelineItem, "rankId", item.rankId, allocator);
        JsonUtil::AddMember(pipelineItem, "hostName", item.hostName, allocator);
        json_t nodeList(kArrayType);
        for (const auto &node : item.lists) {
            json_t nodeJson(kObjectType);
            JsonUtil::AddMember(nodeJson, "fileId", node.fileId, allocator);
            JsonUtil::AddMember(nodeJson, "nodeType", node.nodeType, allocator);
            JsonUtil::AddMember(nodeJson, "startTime", node.startTime, allocator);
            JsonUtil::AddMember(nodeJson, "duration", node.duration, allocator);
            JsonUtil::AddMember(nodeJson, "name", node.name, allocator);
            JsonUtil::AddMember(nodeJson, "stageType", node.stageType, allocator);
            nodeList.PushBack(nodeJson, allocator);
        }
        JsonUtil::AddMember(pipelineItem, "lists", nodeList, allocator);
        data.PushBack(pipelineItem, allocator);
    }
    return std::optional<document_t>(std::move(data));
}
}
