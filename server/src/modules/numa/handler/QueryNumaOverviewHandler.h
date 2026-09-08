/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERY_NUMA_OVERVIEW_HANDLER_H
#define PROFILER_SERVER_QUERY_NUMA_OVERVIEW_HANDLER_H

#include "NumaRequestHandler.h"

namespace Dic::Module::Numa {
class QueryNumaOverviewHandler : public NumaRequestHandler {
  public:
    QueryNumaOverviewHandler() { command = Protocol::REQ_RES_NUMA_OVERVIEW; }
    ~QueryNumaOverviewHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_QUERY_NUMA_OVERVIEW_HANDLER_H
