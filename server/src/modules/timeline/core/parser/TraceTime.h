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

#ifndef PROFILER_SERVER_TRACE_TIME_H
#define PROFILER_SERVER_TRACE_TIME_H

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>
namespace Dic {
namespace Module {
namespace Timeline {
struct CardGroup {
    uint64_t groupMinTime = 0;
    uint64_t groupMaxTime = 0;
    bool operator<(const CardGroup &right) const {
        if (groupMinTime < right.groupMinTime) {
            return true;
        }
        return groupMinTime == right.groupMinTime && groupMaxTime < right.groupMaxTime;
    }
};
class TraceTime {
  public:
    static TraceTime &Instance();
    void Reset();
    void UpdateTime(uint64_t min, uint64_t max);
    void UpdateCardTimeDuration(const std::string &fileId, uint64_t min, uint64_t max);
    std::vector<std::pair<std::string, uint64_t>> ComputeCardMinTimeInfo();
    uint64_t GetOffsetByFileId(const std::string &fileId);
    uint64_t GetOffsetByFileIdUsingMinTimestamp(const std::string &fileId);
    uint64_t GetStartTime();
    uint64_t GetDuration();
    void SetIsSimulation(bool simulation);
    void UpdateCardMinTimestamp(const std::string &fileId, uint64_t minTs);
    bool GetIsSimulation();

  private:
    TraceTime();
    ~TraceTime() = default;
    std::mutex mutex;
    uint64_t maxTimestamp{};
    uint64_t minTimestamp{};
    // cardTimeDurationMap记录原始卡起止时间，key代表fileId，其中Host和Device分别拥有不同的fileId，
    // pair分别记录区间原始开始时间戳和原始结束时间戳，UTC时间，单位ns
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> cardTimeDurationMap;
    std::unordered_map<std::string, uint64_t> rankMinTimestampMap;
    // cardGroupTimeDurations记录合并区间后的起止时间
    std::vector<CardGroup> cardGroupTimeDurations;
    bool isSimulation = false;
    void UpdateCardGroupTime(uint64_t min, uint64_t max);

    uint64_t ComputetargetOffset(const std::pair<uint64_t, uint64_t> &targetTime);
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_TRACE_TIME_H
