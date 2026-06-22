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
#ifndef PROFILER_SERVICE_DETAILSSERVICE_H
#define PROFILER_SERVICE_DETAILSSERVICE_H
#include <string>
#include <vector>
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Protocol;

class DetailsService {
  public:
    static bool QueryDetailsLoadInfo(const SourceDetailsLoadInfoRequest &request, DetailsLoadInfoResponse &response);
    static bool QueryMemoryGraph(const DetailsMemoryGraphRequest &request, DetailsMemoryGraphResponse &response);
    static bool QueryMemoryTable(const DetailsMemoryTableRequest &request, DetailsMemoryTableResponse &response);
    static bool QueryCoreLoadAnalysisGraph(
        const DetailsInterCoreLoadGraphRequest &request, DetailsInterCoreLoadGraphResponse &response);
    static bool QueryTopWarpStallReason(
        const SourceTopWarpStallReasonRequest &request, TopWarpStallReasonResponse &response);

  private:
    const static inline std::string underline = "_";
    static SubBlockData MergeSubBlockData(const SubBlockData &compare, const SubBlockData &baseline);
    static std::string GetSubBlockDataKey(const SubBlockUnitData &data);
    static std::vector<MemoryGraph> MergeMemoryGraph(
        const std::vector<MemoryGraph> &compare, const std::vector<MemoryGraph> &baseline);
    static CompareData<L2Cache> MergeL2Cache(const L2Cache &compare, const L2Cache &baseline);
    static CompareData<UtilizationRate> MergeUtilizationRate(
        const UtilizationRate &compare, const UtilizationRate &baseline);
    static std::vector<CompareData<MemoryUnit>> MergeMemoryUnit(
        const std::vector<CompareData<MemoryUnit>> &compare, const std::vector<CompareData<MemoryUnit>> &baseline);
    static std::vector<MemoryTable> MergeMemoryTables(
        const std::vector<MemoryTable> &compare, const std::vector<MemoryTable> &baseline);
    static std::vector<TableDetail<CompareData<TableRow>>> MergeCompareTableList(
        const std::vector<TableDetail<CompareData<TableRow>>> &compare,
        const std::vector<TableDetail<CompareData<TableRow>>> &baseline);
    static std::vector<CompareData<TableRow>> MergeCompareRows(
        const std::vector<CompareData<TableRow>> &compare, const std::vector<CompareData<TableRow>> &baseline);
    static std::vector<DetailsInterCoreLoadOpDetail> MergeCoreLoadOpDetail(
        const std::vector<DetailsInterCoreLoadOpDetail> &compare,
        const std::vector<DetailsInterCoreLoadOpDetail> &baseline);
    static std::vector<DetailsInterCoreLoadSubCoreDetail> MergeCoreDetail(
        const std::vector<DetailsInterCoreLoadSubCoreDetail> &compare,
        const std::vector<DetailsInterCoreLoadSubCoreDetail> &baseline);
    static std::vector<CompareData<StallReasonItem>> MergeTopWarpStallReason(
        const std::vector<StallReasonItem> &compare, const std::vector<StallReasonItem> &baseline);
};
}
}
}
#endif // PROFILER_SERVICE_DETAILSSERVICE_H
