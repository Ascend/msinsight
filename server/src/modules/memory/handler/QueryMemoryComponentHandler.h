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

#ifndef PROFILER_SERVER_QUERY_MEMORY_COMPONENT_HANDLER_H
#define PROFILER_SERVER_QUERY_MEMORY_COMPONENT_HANDLER_H

#include "MemoryRequestHandler.h"
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "VirtualMemoryDataBase.h"

namespace Dic {
namespace Module {
namespace Memory {
class QueryMemoryComponentHandler : public MemoryRequestHandler {
  public:
    QueryMemoryComponentHandler() { command = Protocol::REQ_RES_MEMORY_COMPONENT; }
    ~QueryMemoryComponentHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    bool GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database, std::vector<MemoryComponent> &compareData,
        std::vector<MemoryComponent> &baselineData, MemoryComponentRequest &request, std::string &errorMsg);
    void ExecuteComparisonAlgorithm(const std::vector<MemoryComponent> &compareData,
        const std::vector<MemoryComponent> &baselineData, MemoryComponentRequest &request,
        MemoryComponentComparisonResponse &response);
    void GetComponentDiff(const std::vector<MemoryComponent> &compareData,
        const std::vector<MemoryComponent> &baselineData, std::vector<MemoryComponentComparison> &diffData);
    void SelectResult(MemoryComponentRequest &request, MemoryComponentComparisonResponse &response,
        std::vector<MemoryComponentComparison> &fullDiffResult);
    void SortResult(MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result);

  private:
    void Merge(
        MemoryComponent &componentCompare, MemoryComponent &componentBaseline, MemoryComponentComparison &mergeResult);
    void SortAscend(MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result);
    void SortDescend(MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result);
    const std::vector<Protocol::MemoryTableColumnAttr> tableColumnAttr = {{"Component", "string", "component"},
        {"Peak Memory Reserved(MB)", "number", "totalReserved"}, {"Timestamp(ms)", "number", "timestamp"}};
};
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_QUERY_MEMORY_COMPONENT_HANDLER_H
