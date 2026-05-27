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
#ifndef MSINSIGHT_BASEPLUGIN_H
#define MSINSIGHT_BASEPLUGIN_H
#include <vector>
#include "BaseModule.h"
#include "ProtocolUtil.h"
#include "ApiHandler.h"

namespace Dic::Core {
class BasePlugin {
  public:
    explicit BasePlugin(std::string pluginName) : pluginName_(pluginName) {};
    virtual ~BasePlugin() = default;
    std::string GetPluginName() { return pluginName_; }
    virtual std::unique_ptr<Module::BaseModule> GetModule() { return nullptr; };
    virtual std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() { return nullptr; };
    virtual std::map<std::string, std::shared_ptr<ApiHandler>> GetAllHandlers() { return {}; };
    virtual std::vector<std::string> GetModuleConfig() { return {}; };
    virtual uint8_t GetOrder() { return UINT8_MAX; };

  protected:
    std::string pluginName_;
};
struct PluginRegister {
    explicit PluginRegister(std::unique_ptr<BasePlugin> plugin);
};
}
#endif // MSINSIGHT_BASEPLUGIN_H
