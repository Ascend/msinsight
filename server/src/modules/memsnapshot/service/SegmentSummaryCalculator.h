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

#ifndef PROFILER_SERVER_SEGMENTSUMMARYCALCULATOR_H
#define PROFILER_SERVER_SEGMENTSUMMARYCALCULATOR_H

#include "MemSnapshotDefs.h"

namespace Dic::Module::MemSnapshot {
struct SegmentSummary {
    uint64_t segmentSize{0};
    uint64_t allocatedSize{0};
    uint64_t gapSize{0};
    uint64_t blockCount{0};
    uint64_t gapCount{0};
    uint64_t maxGapSize{0};
};

SegmentSummary ComputeSegmentSummary(const Segment &segment);
}

#endif // PROFILER_SERVER_SEGMENTSUMMARYCALCULATOR_H
