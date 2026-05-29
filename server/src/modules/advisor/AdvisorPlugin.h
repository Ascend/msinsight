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
#ifndef PROFILER_SERVER_ADVISORPLUGIN_H
#define PROFILER_SERVER_ADVISORPLUGIN_H
#include "BasePlugin.h"
#include "ProtocolDefs.h"
#include "AdvisorModule.h"
#include "AdvisorProtocolUtil.h"
namespace Dic::Module::Advisor {
class AdvisorPlugin : public Core::BasePlugin {
  public:
    AdvisorPlugin() : Core::BasePlugin(MODULE_ADVISOR) {};
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<AdvisorModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override { return std::make_unique<AdvisorProtocolUtil>(); }
};
}
#endif // PROFILER_SERVER_ADVISORPLUGIN_H
