/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef PROFILER_SERVER_NUMA_REQUEST_HANDLER_H
#define PROFILER_SERVER_NUMA_REQUEST_HANDLER_H

#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"

namespace Dic::Module::Numa {
class NumaRequestHandler : public ModuleRequestHandler {
  public:
    NumaRequestHandler() {
        moduleName = Protocol::MODULE_NUMA;
        async = false;
    }
    ~NumaRequestHandler() override = default;
};
} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_REQUEST_HANDLER_H
