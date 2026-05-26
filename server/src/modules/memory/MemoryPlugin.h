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
#ifndef PROFILER_SERVER_MEMORYPLUGIN_H
#define PROFILER_SERVER_MEMORYPLUGIN_H
#include "BasePlugin.h"
#include "MemoryModule.h"
#include "MemoryProtocol.h"
namespace Dic::Module::Memory {
class MemoryPlugin : public Core::BasePlugin {
  public:
    MemoryPlugin() : Core::BasePlugin(MODULE_MEMORY) {};
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<MemoryModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override { return std::make_unique<MemoryProtocol>(); }
};
}
#endif // PROFILER_SERVER_MEMORYPLUGIN_H
