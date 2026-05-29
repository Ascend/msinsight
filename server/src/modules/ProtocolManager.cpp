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
#include "PluginsManager.h"
#include "ProtocolManager.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
ProtocolManager::ProtocolManager() { Register(); }

ProtocolManager::~ProtocolManager() { UnRegister(); }

void ProtocolManager::Register() {
    std::unique_lock<std::mutex> lock(mutex);
    protocolMap.clear();
    auto &manager = Core::PluginsManager::Instance();
    for (const auto &[moduleName, plugin] : manager.GetAllPlugins()) {
        auto protocol = plugin->GetProtocolUtil();
        if (protocol != nullptr) {
            protocol->Register();
            protocolMap.emplace(moduleName, std::move(protocol));
        }
    }
}

void ProtocolManager::UnRegister() {
    std::unique_lock<std::mutex> lock(mutex);
    protocolMap.clear();
}

std::unique_ptr<Request> ProtocolManager::FromJson(const std::string &requestStr, std::string &error) {
    auto requestJson = JsonUtil::TryParse(requestStr, error);
    if (!requestJson.has_value()) {
        ServerLog::Warn("Failed to parse request json. ", requestStr);
        return nullptr;
    }
    if (!JsonUtil::IsJsonKeyValid(requestJson.value(), "moduleName")) {
        ServerLog::Warn(R"(Json Key "moduleName" is invalid, request=)", requestStr);
        return nullptr;
    }
    std::string moduleName = JsonUtil::GetString(requestJson.value(), "moduleName");
    if (moduleName.empty()) {
        ServerLog::Warn("Unknown module name, request=", requestStr);
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(mutex);
    if (protocolMap.count(moduleName) == 0) {
        ServerLog::Warn("Failed to get request module protocol. module type:", moduleName);
        return nullptr;
    }
    return protocolMap.at(moduleName)->FromJson(requestJson.value(), error);
}

std::optional<document_t> ProtocolManager::ToJson(const Response &response, std::string &error) {
    auto moduleName = response.moduleName;
    std::unique_lock<std::mutex> lock(mutex);
    if (protocolMap.count(moduleName) == 0) {
        ServerLog::Warn("Failed to get response module protocol. module type:", moduleName);
        return std::nullopt;
    }
    return protocolMap.at(moduleName)->ToJson(response, error);
}

std::optional<document_t> ProtocolManager::ToJson(const Event &event, std::string &error) {
    auto moduleName = event.moduleName;
    std::unique_lock<std::mutex> lock(mutex);
    if (protocolMap.count(moduleName) == 0) {
        ServerLog::Warn("Failed to get event module protocol. module type:", moduleName);
        return std::nullopt;
    }
    return protocolMap.at(moduleName)->ToJson(event, error);
}
}
}
