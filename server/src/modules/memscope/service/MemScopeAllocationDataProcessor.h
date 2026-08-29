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

#ifndef PROFILER_SERVER_MEMSCOPEALLOCATIONDATAPROCESSOR_H
#define PROFILER_SERVER_MEMSCOPEALLOCATIONDATAPROCESSOR_H

#include <vector>
#include "MemScopeDefs.h"

namespace Dic::Module::MemScope {
class MemScopeAllocationDataProcessor {
  public:
    static std::vector<MemScopeUsageLinePoint> CompressUsageLine(
        const std::vector<MemoryAllocation> &allocations, uint64_t MemoryAllocation::*valueField);
    static std::vector<MemScopeUsageLinePoint> CompressReservedLine(const std::vector<MemoryAllocation> &allocations);
    static std::vector<MemScopeUsageLinePoint> CompressProcessUsedLine(
        const std::vector<MemoryAllocation> &allocations);
    static std::vector<MemScopeUsageLinePoint> CompressDeviceUsedLine(const std::vector<MemoryAllocation> &allocations);
};
} // namespace Dic::Module::MemScope

#endif // PROFILER_SERVER_MEMSCOPEALLOCATIONDATAPROCESSOR_H
