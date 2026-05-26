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

#ifndef PROFILER_SERVER_QUERY_MEMORY_STATIC_OPERATOR_GRAPH_HANDLER_H
#define PROFILER_SERVER_QUERY_MEMORY_STATIC_OPERATOR_GRAPH_HANDLER_H

#include "WsSession.h"
#include "MemoryRequestHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
class QueryMemoryStaticOperatorGraphHandler : public MemoryRequestHandler {
  public:
    QueryMemoryStaticOperatorGraphHandler() { command = Protocol::REQ_RES_MEMORY_STATIC_OP_MEMORY_GRAPH; };
    ~QueryMemoryStaticOperatorGraphHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    bool GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database, StaticOperatorGraphItem &compareData,
        StaticOperatorGraphItem &baselineData, MemoryStaticOperatorGraphRequest &request, std::string &errorMsg);
    void ExecuteComparisonAlgorithm(const StaticOperatorGraphItem &compareData,
        const StaticOperatorGraphItem &baselineData, MemoryStaticOperatorGraphResponse &response);
    void GetCompareGraphLines(const Protocol::StaticOperatorGraphItem &compareData,
        const Protocol::StaticOperatorGraphItem &baselineData, Protocol::StaticOperatorGraphItem &resultData);
    void GetCompareGraphLegends(const Protocol::StaticOperatorGraphItem &compareData,
        const Protocol::StaticOperatorGraphItem &baselineData, Protocol::StaticOperatorGraphItem &resultData);
};
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_QUERY_MEMORY_STATIC_OPERATOR_GRAPH_HANDLER_H
