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

#ifndef PROFILER_SERVER_QUERY_CACHELINE_RECORD_HANDLER_H
#define PROFILER_SERVER_QUERY_CACHELINE_RECORD_HANDLER_H

#include "ModuleRequestHandler.h"

namespace Dic {
namespace Module {
namespace Source {
class QueryCachelineRecordHandler : public ModuleRequestHandler {
  public:
    QueryCachelineRecordHandler() { command = Protocol::REQ_RES_CACHELINE_RECORD; }

    ~QueryCachelineRecordHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    std::string QueryCachelineRecord(const std::string &filePath);
};
} // Source
} // Module
} // Dic

#endif // PROFILER_SERVER_QUERY_CACHELINE_RECORD_HANDLER_H
