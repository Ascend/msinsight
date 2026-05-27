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
#ifndef PROFILER_SERVER_SOURCEPLUGIN_H
#define PROFILER_SERVER_SOURCEPLUGIN_H
#include "BasePlugin.h"
#include "SourceModule.h"
#include "SourceProtocol.h"
namespace Dic::Module::Source {
class SourcePlugin : public Core::BasePlugin {
  public:
    SourcePlugin() : Core::BasePlugin(MODULE_SOURCE) {};
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<SourceModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override { return std::make_unique<SourceProtocol>(); }
};
}
#endif // PROFILER_SERVER_SOURCEPLUGIN_H
