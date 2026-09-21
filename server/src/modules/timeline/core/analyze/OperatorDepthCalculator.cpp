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

#include "OperatorDepthCalculator.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>

namespace Dic::Module::Timeline {
namespace {
const size_t INVALID_UNIT_INDEX = std::numeric_limits<size_t>::max();

struct SliceInterval {
    uint64_t startTime = 0;
    uint64_t endTime = 0;
};

struct SliceDepthUnit {
    SliceInterval interval;
};

SliceInterval ToInterval(const SliceDomain &slice) {
    return {slice.timestamp, slice.endTime >= slice.timestamp ? slice.endTime : slice.timestamp};
}

bool AreIntervalsOverlap(const SliceInterval &left, const SliceInterval &right) {
    return left.startTime < right.endTime && right.startTime < left.endTime;
}

bool IsUnitAvailableOnDepth(const std::vector<SliceInterval> &depthIntervals, const SliceInterval &targetInterval) {
    for (const auto &current : depthIntervals) {
        if (AreIntervalsOverlap(current, targetInterval)) {
            return false;
        }
    }
    return true;
}

uint32_t AssignUnitDepthsByIntervalScan(const std::vector<SliceDepthUnit> &units, std::vector<uint32_t> &unitDepths) {
    std::vector<std::vector<SliceInterval>> depthIntervals;
    for (size_t i = 0; i < units.size(); ++i) {
        uint32_t depth = 0;
        while (depth < depthIntervals.size() && !IsUnitAvailableOnDepth(depthIntervals[depth], units[i].interval)) {
            ++depth;
        }
        if (depth == depthIntervals.size()) {
            depthIntervals.emplace_back();
        }
        unitDepths[i] = depth;
        depthIntervals[depth].emplace_back(units[i].interval);
    }
    return static_cast<uint32_t>(depthIntervals.size());
}

uint32_t AssignUnitDepthsByHeap(const std::vector<SliceDepthUnit> &units, std::vector<uint32_t> &unitDepths) {
    using BusyDepth = std::pair<uint64_t, uint32_t>;
    std::priority_queue<BusyDepth, std::vector<BusyDepth>, std::greater<BusyDepth>> busyDepths;
    std::set<uint32_t> reusableDepths;
    uint32_t nextDepth = 0;
    for (size_t i = 0; i < units.size(); ++i) {
        const SliceInterval &interval = units[i].interval;
        while (!busyDepths.empty() && busyDepths.top().first <= interval.startTime) {
            reusableDepths.insert(busyDepths.top().second);
            busyDepths.pop();
        }
        uint32_t depth = 0;
        if (reusableDepths.empty()) {
            depth = nextDepth++;
        } else {
            auto depthIt = reusableDepths.begin();
            depth = *depthIt;
            reusableDepths.erase(depthIt);
        }
        unitDepths[i] = depth;
        busyDepths.emplace(interval.endTime, depth);
    }
    return nextDepth;
}

void BackfillSliceDepths(
    std::vector<SliceDomain> &slices, const std::vector<size_t> &sliceToUnit, const std::vector<uint32_t> &unitDepths) {
    for (size_t i = 0; i < slices.size(); ++i) {
        size_t unitIndex = sliceToUnit[i];
        if (unitIndex != INVALID_UNIT_INDEX) {
            slices[i].depth = unitDepths[unitIndex];
        }
    }
}
}

uint32_t OperatorDepthCalculator::AssignDepths(std::vector<SliceDomain> &slices) {
    std::vector<SliceDepthUnit> units;
    std::vector<size_t> sliceToUnit(slices.size(), INVALID_UNIT_INDEX);
    std::unordered_map<std::string, size_t> groupToUnit;
    bool areUnitsOrderedByStartTime = true;

    units.reserve(slices.size());
    for (size_t i = 0; i < slices.size(); ++i) {
        const SliceInterval interval = ToInterval(slices[i]);
        const std::string &groupId = slices[i].groupId;
        if (groupId.empty()) {
            if (!units.empty() && interval.startTime < units.back().interval.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            sliceToUnit[i] = units.size();
            units.emplace_back(SliceDepthUnit{interval});
            continue;
        }
        if (groupToUnit.empty()) {
            groupToUnit.reserve(slices.size() - i);
        }
        auto [it, inserted] = groupToUnit.emplace(groupId, units.size());
        if (inserted) {
            if (!units.empty() && interval.startTime < units.back().interval.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            units.emplace_back(SliceDepthUnit{interval});
        } else {
            SliceInterval &merged = units[it->second].interval;
            if (interval.startTime < merged.startTime) {
                areUnitsOrderedByStartTime = false;
            }
            merged.startTime = std::min(merged.startTime, interval.startTime);
            merged.endTime = std::max(merged.endTime, interval.endTime);
        }
        sliceToUnit[i] = it->second;
    }

    std::vector<uint32_t> unitDepths(units.size(), 0);
    uint32_t maxDepth = areUnitsOrderedByStartTime ? AssignUnitDepthsByHeap(units, unitDepths)
                                                   : AssignUnitDepthsByIntervalScan(units, unitDepths);
    BackfillSliceDepths(slices, sliceToUnit, unitDepths);
    return maxDepth;
}
}
