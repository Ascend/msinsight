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
#include "GetModuleConfigHandler.h"
#include "JsonUtil.h"
#include "PluginsManager.h"

namespace Dic::Module::Global {

bool GetModuleConfigHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &manager = Core::PluginsManager::Instance();
    std::unique_ptr<ModuleConfigGetResponse> responsePtr = std::make_unique<ModuleConfigGetResponse>();
    std::vector<std::pair<uint8_t, std::vector<std::string>>> configWithOrder;
    for (const auto &entry : manager.GetAllPlugins()) {
        auto plugin = entry.second.get();
        auto moduleConfig = plugin->GetModuleConfig();
        if (!moduleConfig.empty()) {
            configWithOrder.push_back(std::make_pair(plugin->GetOrder(), plugin->GetModuleConfig()));
        }
    }
    std::sort(
        configWithOrder.begin(), configWithOrder.end(), [](auto cmpA, auto cmpB) { return cmpA.first < cmpB.first; });
    for (const auto &entry : configWithOrder) {
        auto configs = entry.second;
        responsePtr->configs.insert(responsePtr->configs.end(), configs.begin(), configs.end());
    }
    SetBaseResponse(*requestPtr, *responsePtr);
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
