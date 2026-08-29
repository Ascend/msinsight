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

#include "MemScopeAllocationDataProcessor.h"

namespace Dic::Module::MemScope {
std::vector<MemScopeUsageLinePoint> MemScopeAllocationDataProcessor::CompressUsageLine(
    const std::vector<MemoryAllocation> &allocations, uint64_t MemoryAllocation::*valueField) {
    bool hasValue = false;
    for (const auto &allocation : allocations) {
        if (allocation.*valueField != 0) {
            hasValue = true;
            break;
        }
    }
    if (!hasValue) {
        return {};
    }
    std::vector<MemScopeUsageLinePoint> result;
    for (size_t start = 0; start < allocations.size();) {
        const uint64_t value = allocations[start].*valueField;
        size_t end = start;
        while (end + 1 < allocations.size() && allocations[end + 1].*valueField == value) {
            ++end;
        }
        result.emplace_back(allocations[start].timestamp, value);
        if (end != start) {
            result.emplace_back(allocations[end].timestamp, value);
        }
        start = end + 1;
    }
    return result;
}

std::vector<MemScopeUsageLinePoint> MemScopeAllocationDataProcessor::CompressReservedLine(
    const std::vector<MemoryAllocation> &allocations) {
    return CompressUsageLine(allocations, &MemoryAllocation::reservedSize);
}

std::vector<MemScopeUsageLinePoint> MemScopeAllocationDataProcessor::CompressProcessUsedLine(
    const std::vector<MemoryAllocation> &allocations) {
    return CompressUsageLine(allocations, &MemoryAllocation::processUsed);
}

std::vector<MemScopeUsageLinePoint> MemScopeAllocationDataProcessor::CompressDeviceUsedLine(
    const std::vector<MemoryAllocation> &allocations) {
    return CompressUsageLine(allocations, &MemoryAllocation::deviceUsed);
}
} // namespace Dic::Module::MemScope
