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

#ifndef PROFILER_SERVER_MEM_SCOPE_PLUGIN_H
#define PROFILER_SERVER_MEM_SCOPE_PLUGIN_H
#include "BasePlugin.h"
#include "ProtocolDefs.h"
#include "MemScopeModule.h"
#include "MemScopeProtocol.h"
namespace Dic::Module::MemScope {
class MemScopePlugin : public Core::BasePlugin {
  public:
    MemScopePlugin() : Core::BasePlugin(MODULE_MEM_SCOPE) {};
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<MemScopeModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override {
        return std::make_unique<MemScopeProtocolUtil>();
    }
};
} // namespace Dic::Module::MemScope
#endif // PROFILER_SERVER_MEM_SCOPE_PLUGIN_H
