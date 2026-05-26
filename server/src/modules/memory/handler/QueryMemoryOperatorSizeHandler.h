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

#ifndef PROFILER_SERVER_QUERY_OPERATOR_SIZE_HANDLER_H
#define PROFILER_SERVER_QUERY_OPERATOR_SIZE_HANDLER_H

#include "MemoryRequestHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
class QueryMemoryOperatorSizeHandler : public MemoryRequestHandler {
  public:
    QueryMemoryOperatorSizeHandler() { command = Protocol::REQ_RES_MEMORY_OPERATOR_MIN_MAX; };
    ~QueryMemoryOperatorSizeHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    bool GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database, OperatorSize &compareData,
        OperatorSize &baselineData, MemoryOperatorSizeRequest &request, std::string &errorMsg);
    void ExecuteComparisonAlgorithm(
        const OperatorSize &compareData, const OperatorSize &baselineData, MemoryOperatorSizeResponse &response);
};

} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_QUERY_OPERATOR_SIZE_HANDLER_H
