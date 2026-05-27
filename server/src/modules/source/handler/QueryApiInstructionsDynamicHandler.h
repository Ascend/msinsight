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

#ifndef PROFILER_SERVER_QUERY_API_INSTRUCTIONS_DYNAMIC_H
#define PROFILER_SERVER_QUERY_API_INSTRUCTIONS_DYNAMIC_H

#include "SourceRequestHandler.h"
#include "SourceProtocolResponse.h"
#include "SourceProtocolRequest.h"

namespace Dic {
namespace Module {
namespace Source {
class QueryApiInstructionsDynamicHandler : public SourceRequestHandler {
  public:
    QueryApiInstructionsDynamicHandler() { command = Protocol::REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC; }

    ~QueryApiInstructionsDynamicHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  protected:
    void SetResponseBody(SourceApiInstrDynamicResponse &response, SourceApiInstrDynamicRequest &request);
    template <typename T>
    void TransformColumnData(
        const std::unordered_map<std::string, std::vector<T>> &source, std::unordered_map<std::string, T> &target);
};
} // Source
} // Module
} // Dic
#endif // PROFILER_SERVER_QUERY_API_INSTRUCTIONS_DYNAMIC_H
