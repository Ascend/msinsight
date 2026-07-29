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
 * -------------------------------------------------------------------------
 */

#include "MemSnapshotAllocationDataProcessor.h"
#include <algorithm>

namespace Dic::Module::MemSnapshot {
std::vector<Protocol::AllocationRecordDTO> MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(
    const std::vector<Protocol::AllocationRecord> &records) {
    std::vector<Protocol::AllocationRecordDTO> result;
    if (records.empty()) {
        return result;
    }

    std::vector<size_t> turningPointIndices;
    const auto appendTurningPoint = [&turningPointIndices](const size_t index) {
        if (!turningPointIndices.empty() && turningPointIndices.back() == index) {
            return;
        }
        turningPointIndices.emplace_back(index);
    };

    for (size_t start = 0; start < records.size();) {
        size_t end = start;
        while (end + 1 < records.size() && records[end + 1].allocated == records[start].allocated) {
            ++end;
        }
        const bool isEndpoint = start == 0 || end + 1 == records.size();
        const bool isTurningPoint = !isEndpoint &&
            ((records[start].allocated > records[start - 1].allocated &&
                 records[start].allocated > records[end + 1].allocated) ||
                (records[start].allocated < records[start - 1].allocated &&
                    records[start].allocated < records[end + 1].allocated));
        if (isEndpoint || isTurningPoint) {
            appendTurningPoint(start);
            appendTurningPoint(end);
        }
        start = end + 1;
    }

    if (turningPointIndices.size() == 1) {
        result.emplace_back(records.front().timestamp, records.front().allocated);
        return result;
    }

    const int64_t totalTimestampSpan = records.back().timestamp - records.front().timestamp;
    const int64_t maxTimestampGap = std::max<int64_t>(1,
        totalTimestampSpan / MAX_TIMESTAMP_INTERVAL_COUNT +
            (totalTimestampSpan % MAX_TIMESTAMP_INTERVAL_COUNT == 0 ? 0 : 1));
    std::vector<size_t> sampledIndices;
    sampledIndices.reserve(turningPointIndices.size());
    sampledIndices.emplace_back(turningPointIndices.front());
    for (size_t index = 1; index < turningPointIndices.size(); ++index) {
        const size_t nextTurningPoint = turningPointIndices[index];
        size_t candidate = sampledIndices.back() + 1;
        while (candidate < nextTurningPoint &&
            records[nextTurningPoint].timestamp - records[sampledIndices.back()].timestamp > maxTimestampGap) {
            const int64_t targetTimestamp = records[sampledIndices.back()].timestamp + maxTimestampGap;
            size_t insertionIndex = candidate;
            while (insertionIndex + 1 < nextTurningPoint && records[insertionIndex + 1].timestamp <= targetTimestamp) {
                ++insertionIndex;
            }
            sampledIndices.emplace_back(insertionIndex);
            candidate = insertionIndex + 1;
        }
        sampledIndices.emplace_back(nextTurningPoint);
    }

    result.reserve(sampledIndices.size());
    for (size_t index : sampledIndices) {
        result.emplace_back(records[index].timestamp, records[index].allocated);
    }
    return result;
}

std::vector<Protocol::ReservedRecordDTO> MemSnapshotAllocationDataProcessor::CompressReservedLine(
    const std::vector<Protocol::AllocationRecord> &records) {
    std::vector<Protocol::ReservedRecordDTO> result;
    for (size_t start = 0; start < records.size();) {
        const uint64_t reserved = records[start].reserved;
        size_t end = start;
        while (end + 1 < records.size() && records[end + 1].reserved == reserved) {
            ++end;
        }
        result.emplace_back(records[start].timestamp, reserved);
        if (end != start) {
            result.emplace_back(records[end].timestamp, reserved);
        }
        start = end + 1;
    }
    return result;
}
}
