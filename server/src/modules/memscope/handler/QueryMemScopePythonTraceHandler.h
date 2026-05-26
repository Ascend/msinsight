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

#ifndef PROFILER_SERVER_QUERY_MEM_SCOPE_PYTHON_TRACE_HANDLER_H
#define PROFILER_SERVER_QUERY_MEM_SCOPE_PYTHON_TRACE_HANDLER_H

#include "MemScopeRequestHandler.h"

namespace Dic {
namespace Module {
namespace MemScope {
class QueryMemScopePythonTraceHandler : public MemScopeRequestHandler {
  public:
    QueryMemScopePythonTraceHandler() { command = Protocol::REQ_RES_MEM_SCOPE_PYTHON_TRACES; }
    ~QueryMemScopePythonTraceHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    const size_t TRIM_THRESHOLD = 20000;
    const PythonTrimCompressStrategy DEFAULT_TRIM_STRATEGY = PythonTrimCompressStrategy::COMPRESS_SMALL_FUNCTIONS;
};
} // namespace MemScope
} // namespace Module
} // namespace Dic

#endif // PROFILER_SERVER_QUERY_MEM_SCOPE_PYTHON_TRACE_HANDLER_H
