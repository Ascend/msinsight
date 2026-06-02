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

#ifndef PROFILER_SERVER_IEPLUGIN_H
#define PROFILER_SERVER_IEPLUGIN_H
#include "BasePlugin.h"
#include "IEModule.h"
#include "IEProtocol.h"
namespace Dic::Module::IE {
class IEPlugin : public Core::BasePlugin {
  public:
    IEPlugin() : Core::BasePlugin(MODULE_IE) {};
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<IEModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override { return std::make_unique<IEProtocol>(); }
};
}
#endif // PROFILER_SERVER_IEPLUGIN_H
