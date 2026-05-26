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

#ifndef PROFILER_SERVER_QUERY_MEMORY_OPERATOR_HANDLER_H
#define PROFILER_SERVER_QUERY_MEMORY_OPERATOR_HANDLER_H

#include "MemoryRequestHandler.h"
#include "VirtualMemoryDataBase.h"

namespace Dic {
namespace Module {
namespace Memory {
class QueryMemoryOperatorHandler : public MemoryRequestHandler {
  public:
    QueryMemoryOperatorHandler() { command = Protocol::REQ_RES_MEMORY_OPERATOR; };
    ~QueryMemoryOperatorHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
    static bool GetRespectiveDataNotCompare(std::shared_ptr<VirtualMemoryDataBase> database,
        MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &response);
    static bool GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
        std::vector<MemoryOperator> &compareData, std::vector<MemoryOperator> &baselineData,
        MemoryOperatorRequest &request, std::string &errorMsg);
    void ExecuteComparisonAlgorithm(const std::vector<MemoryOperator> &compareData,
        const std::vector<MemoryOperator> &baselineData, MemoryOperatorRequest &request,
        MemoryOperatorComparisonResponse &response);
    void GetOperatorDiff(const std::vector<MemoryOperator> &compareData,
        const std::vector<MemoryOperator> &baselineData, std::vector<MemoryOperatorComparison> &resultData);
    void SelectDiffResult(MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &response,
        std::vector<MemoryOperatorComparison> &fullDiffResult);
    void SortResult(MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &result);

  private:
    void VectorMerge(std::vector<MemoryOperator> &compareVec, std::vector<MemoryOperator> &baselineVec,
        std::vector<MemoryOperatorComparison> &diffData);
    void Subtract(MemoryOperatorComparison &element);
    bool IsSelected(MemoryOperatorRequest &request, const MemoryOperatorComparison &op);
    void SortAscend(MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &result);
    void SortDescend(MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &result);
    static bool IsWithinInterval(long double num, double start, double end);
    const std::vector<Protocol::MemoryTableColumnAttr> tableColumnAttr = {{"Name", "string", "name"},
        {"Size(KB)", "number", "size"}, {"Allocation Time(ms)", "number", "allocationTime"},
        {"Release Time(ms)", "number", "releaseTime"}, {"Duration(ms)", "number", "duration"},
        {"Active Release Time(ms)", "number", "activeReleaseTime"}, {"Active Duration(ms)", "number", "activeDuration"},
        {"Allocation Total Allocated(MB)", "number", "allocationAllocated"},
        {"Allocation Total Reserved(MB)", "number", "allocationReserved"},
        {"Allocation Total Active(MB)", "number", "allocationActive"},
        {"Release Total Allocated(MB)", "number", "releaseAllocated"},
        {"Release Total Reserved(MB)", "number", "releaseReserved"},
        {"Release Total Active(MB)", "number", "releaseActive"}, {"Stream", "string", "streamId"}};
};
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_QUERY_MEMORY_OPERATOR_HANDLER_H
