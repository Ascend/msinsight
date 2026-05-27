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

#include "../utils/pch.h"
#include "../utils/ProtocolEnumUtil.h"
#include "ProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
using namespace rapidjson;
void ProtocolUtil::Register() {
    std::lock_guard<std::mutex> lock(mutex);
    RegisterJsonToRequestFuncs();
    RegisterResponseToJsonFuncs();
    RegisterEventToJsonFuncs();
}

void ProtocolUtil::UnRegister() {
    std::lock_guard<std::mutex> lock(mutex);
    jsonToReqFactory.clear();
    resToJsonFactory.clear();
    eventToJsonFactory.clear();
}

std::unique_ptr<Request> ProtocolUtil::FromJson(const json_t &requestJson, std::string &error) {
    if (!IsRequest(requestJson)) {
        ServerLog::Warn("Json is not request type.");
        return nullptr;
    }
    const std::string command = Command(requestJson);
    std::optional<ProtocolUtil::JsonToRequestFunc> func = GetJsonToRequestFunc(command);
    if (!func.has_value()) {
        return nullptr;
    }
    return func.value()(requestJson, error);
}

std::optional<document_t> ProtocolUtil::ToJson(const Response &response, std::string &error) {
    std::string command = response.command;
    std::optional<ProtocolUtil::ResponseToJsonFunc> func = GetResponseToJsonFunc(command);
    if (!func.has_value()) {
        error = "Failed to find response target function, command = " + command;
        return std::nullopt;
    }
    try {
        return func.value()(response);
    } catch (std::exception &e) {
        error = "Failed to convert response to json, command = " + command;
        return std::nullopt;
    }
}

std::optional<document_t> ProtocolUtil::ToJson(const Event &event, std::string &error) {
    std::string eventString = event.event;
    std::optional<ProtocolUtil::EventToJsonFunc> func = GetEventToJsonFunc(eventString);
    if (!func.has_value()) {
        error = "Failed to find event target function, event = " + eventString + ", module:" + event.moduleName;
        return std::nullopt;
    }
    try {
        return func.value()(event);
    } catch (std::exception &e) {
        error = "Failed to convert event to json, event = " + eventString;
        return std::nullopt;
    }
}

bool ProtocolUtil::SetRequestBaseInfo(Request &request, const json_t &json) {
    if (!JsonUtil::IsJsonKeyValid(json, "id") || !JsonUtil::IsJsonKeyValid(json, "type") ||
        !JsonUtil::IsJsonKeyValid(json, "command")) {
        return false;
    }
    if (!JsonUtil::IsJsonKeyValid(json, "params") || !JsonUtil::IsJsonKeyValid(json, "moduleName")) {
        return false;
    }
    JsonUtil::SetByJsonKeyValue(request.id, json, "id");
    JsonUtil::SetByJsonKeyValue(request.command, json, "command");
    JsonUtil::SetByJsonKeyValue(request.projectName, json, "projectName");
    std::string typeStr;
    JsonUtil::SetByJsonKeyValue(typeStr, json, "type");
    auto type = STR_TO_ENUM<Dic::Protocol::ProtocolMessage::Type>(typeStr);
    request.type = type.has_value() ? type.value() : ProtocolMessage::Type::NONE;
    std::string moduleName;
    JsonUtil::SetByJsonKeyValue(moduleName, json, "moduleName");
    request.moduleName = moduleName.empty() ? MODULE_UNKNOWN : moduleName;
    JsonUtil::SetByJsonKeyValue(request.resultCallbackId, json, "resultCallbackId");
    JsonUtil::SetByJsonKeyValue(request.fileId, json, "fileId");
    return true;
}

void ProtocolUtil::SetResponseJsonBaseInfo(const Response &response, document_t &json) {
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "type", RESPONSE_NAME, allocator);
    json.AddMember("id", response.id, allocator);
    json.AddMember("requestId", response.requestId, allocator);
    json.AddMember("result", response.result, allocator);
    JsonUtil::AddMember(json, "command", response.command, allocator);
    JsonUtil::AddMember(json, "moduleName", response.moduleName, allocator);
    if (response.error.has_value() && !response.error->message.empty()) {
        json_t error(kObjectType);
        JsonUtil::AddMember(error, "code", response.error.value().code, allocator);
        JsonUtil::AddMember(error, "message", response.error.value().message, allocator);
        json.AddMember("error", error, allocator);
    }
    if (response.resultCallbackId.has_value()) {
        json.AddMember("resultCallbackId", response.resultCallbackId.value(), allocator);
    }
}

void ProtocolUtil::SetEventJsonBaseInfo(const Event &event, document_t &json) {
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "type", EVENT_NAME, allocator);
    json.AddMember("id", event.id, allocator);
    JsonUtil::AddMember(json, "event", event.event, allocator);
    JsonUtil::AddMember(json, "moduleName", event.moduleName, allocator);
    if (event.resultCallbackId.has_value()) {
        json.AddMember("resultCallbackId", event.resultCallbackId.value(), allocator);
    }
}

bool ProtocolUtil::IsRequest(const json_t &jsonRequest) {
    if (!jsonRequest.HasMember("type")) {
        return false;
    }
    if (!jsonRequest["type"].IsString()) {
        return false;
    }
    return jsonRequest["type"].GetString() == REQUEST_NAME;
}

std::string ProtocolUtil::Command(const json_t &jsonRequest) {
    if (jsonRequest.HasMember("command") && jsonRequest["command"].IsString()) {
        return jsonRequest["command"].GetString();
    }
    return "";
}

std::optional<ProtocolUtil::JsonToRequestFunc> ProtocolUtil::GetJsonToRequestFunc(const std::string &command) {
    std::lock_guard<std::mutex> lock(mutex);
    if (jsonToReqFactory.count(command) == 0) {
        ServerLog::Warn("The json to request function is not found. command is: ", command);
        return std::nullopt;
    }
    return jsonToReqFactory[command];
}

std::optional<ProtocolUtil::ResponseToJsonFunc> ProtocolUtil::GetResponseToJsonFunc(const std::string &command) {
    std::lock_guard<std::mutex> lock(mutex);
    if (resToJsonFactory.count(command) == 0) {
        ServerLog::Warn("The response to json function is not found. command:", command);
        return std::nullopt;
    }
    return resToJsonFactory[command];
}

std::optional<ProtocolUtil::EventToJsonFunc> ProtocolUtil::GetEventToJsonFunc(const std::string &event) {
    std::lock_guard<std::mutex> lock(mutex);
    if (eventToJsonFactory.count(event) == 0) {
        return std::nullopt;
    }
    return eventToJsonFactory[event];
}
}
}
