/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef PROFILER_SERVER_NUMA_PLUGIN_H
#define PROFILER_SERVER_NUMA_PLUGIN_H

#include <memory>

#include "BasePlugin.h"
#include "NumaModule.h"
#include "NumaProtocol.h"
#include "ProtocolDefs.h"

namespace Dic::Module::Numa {
class NumaPlugin : public Core::BasePlugin {
  public:
    NumaPlugin() : Core::BasePlugin(Protocol::MODULE_NUMA) {}
    std::unique_ptr<Module::BaseModule> GetModule() override { return std::make_unique<NumaModule>(); }
    std::unique_ptr<Module::ProtocolUtil> GetProtocolUtil() override;
};
} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_PLUGIN_H
