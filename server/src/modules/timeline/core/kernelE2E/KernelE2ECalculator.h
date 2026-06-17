/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_KERNELE2ECALCULATOR_H
#define PROFILER_SERVER_KERNELE2ECALCULATOR_H

#include "KernelE2EDef.h"

namespace Dic::Module::Timeline {

class KernelE2ECalculator {
  public:
    KernelE2ETimeRecord Calculate(const KernelE2EChain &chain);

    // 优化3：优先读取 chain.cachedRecord，未命中时调用 Calculate 并缓存结果
    const KernelE2ETimeRecord &GetOrCalculate(const KernelE2EChain &chain);

  private:
    KernelE2ETimeRecord CreateBaseRecord(const KernelE2EChain &chain);
    KernelE2ETimeRecord CalculatePythonParentRecord(const KernelE2EChain &chain, KernelE2ETimeRecord record);
    KernelE2ETimeRecord CalculateCannParentRecord(const KernelE2EChain &chain, KernelE2ETimeRecord record);
    void FillRecoveredEventTimes(const KernelE2EChain &chain, KernelE2ETimeRecord &record);
    void ClearLeafUpstreamTimes(KernelE2ETimeRecord &record);
    KernelE2ETimeRecord FinalizeChildRecord(const KernelE2EChain &chain, KernelE2ETimeRecord record);
    void AddChildDurations(const KernelE2ETimeRecord &childRecord, KernelE2ETimeRecord &parentRecord);
    void UpdateParentStatus(const std::vector<KernelE2ETimeRecord> &childRecords, KernelE2ETimeRecord &record);
    bool ValidateMonotonicTimestamps(const KernelE2EChain &chain, bool hasLaunch);
};

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNELE2ECALCULATOR_H
