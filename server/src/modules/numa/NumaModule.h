/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef PROFILER_SERVER_NUMA_MODULE_H
#define PROFILER_SERVER_NUMA_MODULE_H

#include "BaseModule.h"
#include "ProtocolDefs.h"

namespace Dic::Module::Numa {
class NumaModule : public BaseModule {
  public:
    NumaModule() { moduleName = Protocol::MODULE_NUMA; }
    ~NumaModule() override = default;
    void RegisterRequestHandlers() override;
    void OnRequest(std::unique_ptr<Protocol::Request> request) override;
};
} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_MODULE_H
