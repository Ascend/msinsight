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
#include "pch.h"
#include "TraceTime.h"

namespace Dic {
namespace Module {
namespace Timeline {
TraceTime &TraceTime::Instance() {
    static TraceTime instance;
    return instance;
}

void TraceTime::Reset() {
    std::unique_lock<std::mutex> lock(mutex);
    maxTimestamp = 0;
    minTimestamp = UINT64_MAX;
    cardTimeDurationMap.clear();
    cardGroupTimeDurations.clear();
    rankMinTimestampMap.clear();
    isSimulation = false;
}

TraceTime::TraceTime() { Reset(); }

void TraceTime::UpdateTime(uint64_t min, uint64_t max) {
    std::unique_lock<std::mutex> lock(mutex);
    if (min > 0) {
        minTimestamp = std::min(minTimestamp, min);
    }
    maxTimestamp = std::max(maxTimestamp, max);
}

uint64_t TraceTime::GetStartTime() {
    std::unique_lock<std::mutex> lock(mutex);
    return minTimestamp;
}

uint64_t TraceTime::GetDuration() {
    std::unique_lock<std::mutex> lock(mutex);
    uint64_t maxDuration = 0;
    for (const auto &cardGroup : cardGroupTimeDurations) {
        uint64_t duration = NumberSafe::Sub(cardGroup.groupMaxTime, cardGroup.groupMinTime);
        if (duration > maxDuration) {
            maxDuration = duration;
        }
    }
    return maxDuration;
}

void TraceTime::UpdateCardTimeDuration(const std::string &fileId, uint64_t min, uint64_t max) {
    std::unique_lock<std::mutex> lock(mutex);
    if (cardTimeDurationMap.count(fileId) == 0) {
        cardTimeDurationMap[fileId] = {min, max};
    } else {
        const uint64_t tempMin = std::min(cardTimeDurationMap[fileId].first, min);
        const uint64_t tempMax = std::min(cardTimeDurationMap[fileId].second, max);
        cardTimeDurationMap[fileId] = {tempMin, tempMax};
    }
    UpdateCardGroupTime(min, max);
    minTimestamp = std::min(minTimestamp, cardGroupTimeDurations.begin()->groupMinTime);
}

std::vector<std::pair<std::string, uint64_t>> TraceTime::ComputeCardMinTimeInfo() {
    std::unique_lock<std::mutex> lock(mutex);
    std::vector<std::pair<std::string, uint64_t>> result;
    for (const auto &item : cardTimeDurationMap) {
        uint64_t offset = ComputetargetOffset(item.second);
        result.emplace_back(item.first, offset);
    }
    return result;
}

uint64_t TraceTime::GetOffsetByFileId(const std::string &fileId) {
    std::unique_lock<std::mutex> lock(mutex);
    if (cardTimeDurationMap.count(fileId) == 0) {
        return 0;
    }
    std::pair<uint64_t, uint64_t> targetTime = cardTimeDurationMap[fileId];
    return ComputetargetOffset(targetTime);
}

uint64_t TraceTime::GetOffsetByFileIdUsingMinTimestamp(const std::string &fileId) {
    std::unique_lock<std::mutex> lock(mutex);
    if (rankMinTimestampMap.find(fileId) == rankMinTimestampMap.end()) {
        return 0;
    }
    // 已判断不为空
    uint64_t deviceMinTimestamp = rankMinTimestampMap[fileId];
    if (deviceMinTimestamp > minTimestamp) {
        return deviceMinTimestamp - minTimestamp;
    }
    return 0;
}

uint64_t TraceTime::ComputetargetOffset(const std::pair<uint64_t, uint64_t> &targetTime) {
    uint64_t cardMinTime = targetTime.first;
    for (const auto &item : cardGroupTimeDurations) {
        if (item.groupMinTime <= targetTime.first && item.groupMaxTime >= targetTime.second) {
            cardMinTime = item.groupMinTime;
        }
    }
    if (cardMinTime > minTimestamp) {
        return (cardMinTime - minTimestamp);
    }
    return 0;
}

void TraceTime::SetIsSimulation(bool simulation) {
    std::unique_lock<std::mutex> lock(mutex);
    isSimulation = simulation;
}

bool TraceTime::GetIsSimulation() {
    std::unique_lock<std::mutex> lock(mutex);
    return isSimulation;
}

void TraceTime::UpdateCardGroupTime(uint64_t min, uint64_t max) {
    CardGroup cardGroup = {min, max};
    auto it = lower_bound(cardGroupTimeDurations.begin(), cardGroupTimeDurations.end(), cardGroup);
    cardGroupTimeDurations.insert(it, cardGroup);
    std::vector<CardGroup> mergeDurations;
    // 遍历现有的区间，进行合并
    for (const auto &item : cardGroupTimeDurations) {
        // 如果mergedIntervals为空，或者当前区间与最后一个合并区间不重叠
        if (mergeDurations.empty() || mergeDurations.back().groupMaxTime < item.groupMinTime) {
            mergeDurations.push_back(item);
        } else {
            // 否则，存在交集，合并当前区间和最后一个合并区间
            mergeDurations.back().groupMaxTime = std::max(mergeDurations.back().groupMaxTime, item.groupMaxTime);
        }
    }
    cardGroupTimeDurations = mergeDurations;
}

void TraceTime::UpdateCardMinTimestamp(const std::string &fileId, uint64_t minTs) {
    std::unique_lock<std::mutex> lock(mutex);
    if (rankMinTimestampMap.find(fileId) == rankMinTimestampMap.end()) {
        rankMinTimestampMap[fileId] = minTs;
    } else {
        rankMinTimestampMap[fileId] = std::min(rankMinTimestampMap[fileId], minTs);
    }
    minTimestamp = std::min(minTimestamp, rankMinTimestampMap[fileId]);
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
