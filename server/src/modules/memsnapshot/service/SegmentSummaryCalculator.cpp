/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * -------------------------------------------------------------------------
 */

#include "SegmentSummaryCalculator.h"

#include <algorithm>

namespace Dic::Module::MemSnapshot {
namespace {
void AddGap(SegmentSummary &summary, uint64_t gapSize) {
    if (gapSize == 0) {
        return;
    }
    summary.gapSize += gapSize;
    summary.gapCount += 1;
    summary.maxGapSize = std::max(summary.maxGapSize, gapSize);
}
}

SegmentSummary ComputeSegmentSummary(const Segment &segment) {
    SegmentSummary summary;
    summary.segmentSize = segment.totalSize;
    summary.allocatedSize = segment.allocated;
    summary.blockCount = segment.blocks.size();

    if (segment.totalSize == 0) {
        return summary;
    }

    auto blocks = segment.blocks;
    std::sort(
        blocks.begin(), blocks.end(), [](const Block &lhs, const Block &rhs) { return lhs.address < rhs.address; });

    uint64_t cursor = segment.address;
    const uint64_t segmentEnd = segment.address + segment.totalSize;
    for (const auto &block : blocks) {
        if (block.address > cursor) {
            AddGap(summary, block.address - cursor);
        }
        const uint64_t blockEnd = block.address + block.size;
        cursor = std::max(cursor, blockEnd);
    }
    if (segmentEnd > cursor) {
        AddGap(summary, segmentEnd - cursor);
    }

    return summary;
}
}
