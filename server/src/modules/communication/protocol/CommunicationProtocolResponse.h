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

#ifndef DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_RESPONSE_H
#define DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_RESPONSE_H

#include <vector>
#include <cfloat>
#include "GlobalDefs.h"
#include "ProtocolDefs.h"
#include "NumberUtil.h"
#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
struct OperatorTimeItem {
    std::string operatorName;
    uint64_t startTime = 0;
    uint64_t elapseTime = 0;
    std::string dbPath;
    static bool SortByTime(const OperatorTimeItem &first, const OperatorTimeItem &second) {
        if (first.startTime < second.startTime) {
            return true;
        }
        if (first.startTime == second.startTime && first.elapseTime < second.elapseTime) {
            return true;
        }
        return false;
    }
};

struct OperatorItem {
    std::string operatorName;
    double startTime;
    double elapseTime;
    double transitTime;
    double synchronizationTime;
    double waitTime;
    double idleTime;
    double synchronizationTimeRatio;
    double waitTimeRatio;
    double sdmaBw{};
    double rdmaBw{};
};

struct OperatorDetailsResBody {
    int count = 0;
    int pageSize{0};
    int currentPage{1};
    std::vector<OperatorItem> allOperators;
};

struct OperatorDetailsResponse : public Response {
    OperatorDetailsResponse() : Response(REQ_RES_COMMUNICATION_OPERATOR_DETAILS) {}
    OperatorDetailsResBody body;
};

struct GroupItem {
    GroupItem(std::string name, std::vector<int> ranks, std::string value) : name(name), ranks(ranks), value(value) {}
    std::string name;
    std::vector<int> ranks;
    std::string value;
};

struct BandwidthDataItem {
    std::string transportType;
    double transitSize = 0;
    double transitTime = 0;
    double bandwidth = 0;
    double largePacketRatio = 0;
};

struct BandwidthDataResBody {
    std::vector<BandwidthDataItem> items;
};

struct BandwidthDataResponse : public Response {
    BandwidthDataResponse() : Response(REQ_RES_COMMUNICATION_BANDWIDTH) {}
    BandwidthDataResBody body;
};

struct DistributionResBody {
    std::string distributionData;
};

struct DistributionResponse : public Response {
    DistributionResponse() : Response(REQ_RES_COMMUNICATION_DISTRIBUTION) {}
    DistributionResBody body;
};

struct IterationsOrRanksObject {
    std::string iterationOrRankId;
};

struct IterationsOrRanksResponse : public Response {
    IterationsOrRanksResponse() : Response(REQ_RES_COMMUNICATION_ITERATIONS) {}
    CompareData<std::vector<IterationsOrRanksObject>> body;
};

struct OperatorNamesObject {
    std::string operatorName;

    bool operator<(const OperatorNamesObject &other) const { return operatorName < other.operatorName; }

    bool operator==(const OperatorNamesObject &other) const { return operatorName == other.operatorName; }
};

struct OperatorNamesResponse : public Response {
    OperatorNamesResponse() : Response(REQ_RES_COMMUNICATION_OPERATORNAMES) {}
    std::vector<OperatorNamesObject> body;
};

struct MatrixSortOpNamesResponse : public Response {
    MatrixSortOpNamesResponse() : Response(REQ_RES_COMMUNICATION_SORT_OP) {}
    std::vector<OperatorNamesObject> body;
};

struct DurationData {
    double startTime{};
    double elapseTime{};
    double transitTime{};
    double synchronizationTime{};
    double waitTime{};
    double idleTime{};
    double synchronizationTimeRatio{};
    double waitTimeRatio{};
    double sdmaBw{};
    double rdmaBw{};
    double sdmaTime{};
    double rdmaTime{};

    DurationData operator-(const DurationData &durationData) const {
        int precision = 4;
        DurationData res;
        res.startTime = NumberUtil::DoubleReservedNDigits(this->startTime - durationData.startTime, precision);
        res.elapseTime = NumberUtil::DoubleReservedNDigits(this->elapseTime - durationData.elapseTime, precision);
        res.transitTime = NumberUtil::DoubleReservedNDigits(this->transitTime - durationData.transitTime, precision);
        res.synchronizationTime =
            NumberUtil::DoubleReservedNDigits(this->synchronizationTime - durationData.synchronizationTime, precision);
        res.waitTime = NumberUtil::DoubleReservedNDigits(this->waitTime - durationData.waitTime, precision);
        res.idleTime = NumberUtil::DoubleReservedNDigits(this->idleTime - durationData.idleTime, precision);
        res.synchronizationTimeRatio = NumberUtil::DoubleReservedNDigits(
            this->synchronizationTimeRatio - durationData.synchronizationTimeRatio, precision);
        res.waitTimeRatio =
            NumberUtil::DoubleReservedNDigits(this->waitTimeRatio - durationData.waitTimeRatio, precision);
        res.sdmaBw = NumberUtil::DoubleReservedNDigits(this->sdmaBw - durationData.sdmaBw, precision);
        res.rdmaBw = NumberUtil::DoubleReservedNDigits(this->rdmaBw - durationData.rdmaBw, precision);
        res.sdmaTime = NumberUtil::DoubleReservedNDigits(this->sdmaTime - durationData.sdmaTime, precision);
        res.rdmaTime = NumberUtil::DoubleReservedNDigits(this->rdmaTime - durationData.rdmaTime, precision);
        return res;
    }
};

struct Duration {
    std::string rankId;
    std::string dbPath;
    CompareData<DurationData> durationData;
};

struct BandwidthStatistic {
    std::string type; // SDMA、RDMA
    double avgBw; // 单位GB/s
    double maxBw; // 单位GB/s
    double minBw; // 单位GB/s
    double diffBw; // 单位GB/s
    double allTime;
};

struct DurationListsResponseBody {
    std::vector<Duration> durationList;
    std::vector<BandwidthStatistic> bwStatistics{};
};

struct DurationResponse : public Response {
    DurationResponse() : Response(REQ_RES_COMMUNICATION_LIST) {}
    DurationListsResponseBody body;
};

struct OperatorListsResponseBody {
    uint64_t minTime = UINT64_MAX;
    uint64_t maxTime = 0;
    std::vector<std::string> rankLists;
    std::vector<std::string> dbPathList;
    std::vector<CompareData<std::vector<OperatorTimeItem>>> opLists;
    // 此方法为了所有色块能同屏展示
    void AdjustTime(const std::string &operatorName) {
        std::vector<std::pair<uint64_t, uint64_t>> timeDurations = MergeTimeDuration();
        std::map<size_t, uint64_t> offsetMap = ComputeOffset(timeDurations);
        // 第一次调整time是为了同屏展示
        AdjustTimeByOffset(offsetMap);
        if (!std::empty(operatorName)) {
            // 第二次调整time是为了对齐通信算子的结束时间
            AdjustTimeByName(operatorName);
        }
    }

  private:
    void AdjustTimeByName(const std::string &operatorName) {
        std::map<size_t, uint64_t> offsetMap;
        uint64_t maxEndTime = 0;
        for (auto &opList : opLists) {
            for (const auto &item : opList.compare) {
                if (item.operatorName == operatorName) {
                    maxEndTime = std::max(item.startTime + item.elapseTime, maxEndTime);
                    break;
                }
            }
            for (const auto &item : opList.baseline) {
                if (item.operatorName == operatorName) {
                    maxEndTime = std::max(item.startTime + item.elapseTime, maxEndTime);
                    break;
                }
            }
        }
        if (maxEndTime == 0) {
            return;
        }
        for (size_t i = 0; i < opLists.size(); ++i) {
            const size_t compareKey = i * 2;
            const size_t baseKey = i * 2 + 1;
            for (const auto &item : opLists[i].compare) {
                if (item.operatorName == operatorName) {
                    offsetMap[compareKey] = maxEndTime - (item.startTime + item.elapseTime);
                    break;
                }
            }
            for (const auto &item : opLists[i].baseline) {
                if (item.operatorName == operatorName) {
                    offsetMap[baseKey] = maxEndTime - (item.startTime + item.elapseTime);
                    break;
                }
            }
        }
        AdjustTimeByOffset(offsetMap);
    }

    void AdjustTimeByOffset(std::map<size_t, uint64_t> &offsetMap) {
        if (std::empty(offsetMap)) {
            return;
        }
        for (size_t i = 0; i < opLists.size(); ++i) {
            const size_t compareKey = i * 2;
            const size_t baseKey = i * 2 + 1;
            auto compareIt = offsetMap.find(compareKey);
            auto baseIt = offsetMap.find(baseKey);
            if (compareIt != offsetMap.end()) {
                uint64_t offset = compareIt->second;
                for (auto &item : opLists[i].compare) {
                    item.startTime = item.startTime + offset;
                }
            }
            if (baseIt != offsetMap.end()) {
                uint64_t offset = baseIt->second;
                for (auto &item : opLists[i].baseline) {
                    item.startTime = item.startTime + offset;
                }
            }
        }
        uint64_t tempMinTime = UINT64_MAX;
        uint64_t tempMaxTime = 0;
        for (const auto &item : opLists) {
            if (!std::empty(item.compare)) {
                tempMaxTime = std::max(item.compare.back().startTime + item.compare.back().elapseTime, tempMaxTime);
                tempMinTime = std::min(item.compare.front().startTime, tempMinTime);
            }
            if (!std::empty(item.baseline)) {
                tempMaxTime = std::max(item.baseline.back().startTime + item.baseline.back().elapseTime, tempMaxTime);
                tempMinTime = std::min(item.baseline.front().startTime, tempMinTime);
            }
        }
        minTime = tempMinTime;
        maxTime = tempMaxTime;
    }

    std::vector<std::pair<uint64_t, uint64_t>> MergeTimeDuration() {
        std::vector<std::pair<uint64_t, uint64_t>> timeDurations;
        for (auto &opList : opLists) {
            if (!opList.baseline.empty()) {
                const uint64_t min = opList.baseline.front().startTime;
                const uint64_t max = opList.baseline.back().startTime + opList.baseline.back().elapseTime;
                UpdateTimeDurations(min, max, timeDurations);
            }
            if (!opList.compare.empty()) {
                const uint64_t min = opList.compare.front().startTime;
                const uint64_t max = opList.compare.back().startTime + opList.compare.back().elapseTime;
                UpdateTimeDurations(min, max, timeDurations);
            }
        }
        return timeDurations;
    }

    std::map<size_t, uint64_t> ComputeOffset(const std::vector<std::pair<uint64_t, uint64_t>> &timeDurations) {
        std::map<size_t, uint64_t> offsetMap;
        for (size_t i = 0; i < opLists.size(); i++) {
            const size_t compareKey = i * 2;
            const size_t baseKey = i * 2 + 1;
            if (!opLists[i].compare.empty()) {
                const uint64_t min = opLists[i].compare.front().startTime;
                const uint64_t max = opLists[i].compare.back().startTime + opLists[i].compare.back().elapseTime;
                uint64_t offset = ComputeTargetOffset(min, max, timeDurations);
                offsetMap[compareKey] = offset;
            }
            if (!opLists[i].baseline.empty()) {
                const uint64_t min = opLists[i].baseline.front().startTime;
                const uint64_t max = opLists[i].baseline.back().startTime + opLists[i].baseline.back().elapseTime;
                uint64_t offset = ComputeTargetOffset(min, max, timeDurations);
                offsetMap[baseKey] = offset;
            }
        }
        return offsetMap;
    }

    static void UpdateTimeDurations(
        uint64_t min, uint64_t max, std::vector<std::pair<uint64_t, uint64_t>> &timeDurations) {
        std::pair<uint64_t, uint64_t> cardGroup = {min, max};
        auto it = lower_bound(timeDurations.begin(), timeDurations.end(), cardGroup);
        timeDurations.insert(it, cardGroup);
        std::vector<std::pair<uint64_t, uint64_t>> mergeDurations;
        // 遍历现有的区间，进行合并
        for (const auto &item : timeDurations) {
            // 如果mergedIntervals为空，或者当前区间与最后一个合并区间不重叠
            if (mergeDurations.empty() || mergeDurations.back().second < item.first) {
                mergeDurations.push_back(item);
            } else {
                // 否则，存在交集，合并当前区间和最后一个合并区间
                mergeDurations.back().second = std::max(mergeDurations.back().second, item.second);
            }
        }
        timeDurations = mergeDurations;
    }

    uint64_t ComputeTargetOffset(
        uint64_t min, uint64_t max, const std::vector<std::pair<uint64_t, uint64_t>> &timeDurations) const {
        for (const auto &item : timeDurations) {
            if (item.first <= min && item.second >= max) {
                max = item.second;
            }
        }
        if (max < maxTime) {
            return (maxTime - max);
        }
        return 0;
    }
};

struct OperatorListsResponse : public Response {
    OperatorListsResponse() : Response(REQ_RES_COMMUNICATION_OPERATOR_LISTS) {}
    OperatorListsResponseBody body;
};

struct MatrixData {
    std::string transportType;
    std::string opName;
    double transitSize = 0;
    double transitTime = 0;
    double bandwidth = 0;
    MatrixData operator-(const MatrixData &matrixData) const {
        MatrixData res;
        // 精度
        int precision = 4;
        res.transitSize = NumberUtil::DoubleReservedNDigits(this->transitSize - matrixData.transitSize, precision);
        res.transitTime = NumberUtil::DoubleReservedNDigits(this->transitTime - matrixData.transitTime, precision);
        res.bandwidth = NumberUtil::DoubleReservedNDigits(this->bandwidth - matrixData.bandwidth, precision);
        return res;
    }
};

struct MatrixList {
    int srcRank;
    int dstRank;
    CompareData<MatrixData> matrixData;
};

struct MatrixListResponseBody {
    std::vector<MatrixList> matrixList;
};

struct MatrixListResponse : public Response {
    MatrixListResponse() : Response(REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH) {}

    MatrixListResponseBody body;
};

struct GroupInfo {
    std::string group;
    std::string parallelStrategy;
    CompareData<std::string> groupIdHash;
    std::string type; // BASELINE or COMPARE
    bool operator==(const GroupInfo &other) const {
        return group == other.group && parallelStrategy == other.parallelStrategy && type == other.type;
    }
};

struct MatrixGroupResponseBody {
    std::vector<GroupInfo> groupList;
};

struct MatrixGroupResponse : public Response {
    MatrixGroupResponse() : Response(REQ_RES_COMMUNICATION_MATRIX_GROUP) {}

    MatrixGroupResponseBody body;
};

struct CommunicationAdvisorInfo {
    std::string name;
    std::map<std::string, std::vector<std::string>> statistics;
};

struct CommunicationAdvisorResponseBody {
    std::vector<CommunicationAdvisorInfo> items;
};

struct CommunicationAdvisorResponse : public Response {
    CommunicationAdvisorResponse() : Response(REQ_RES_COMMUNICATION_ADVISOR) {}
    CommunicationAdvisorResponseBody body;
};

struct OpDetailsForSlowRank {
    std::string name;
    double startTime{}; // 当前卡算子start time
    double diffTime{}; // maxElapseTime - elapseTime
    double elapseTime{}; // 当前卡算子elapse time
    double maxStartTime{}; // 最快卡（最大通信时间卡）算子 start time
    double maxElapseTime{}; // 最快卡（最大通信时间卡）算子 elapse time
};

struct RankDetailsForSlowRank {
    std::string rankId;
    double totalDiffTime{};
    double totalElapseTime{};
    std::vector<OpDetailsForSlowRank> opDetails;
    bool operator<(const RankDetailsForSlowRank &other) const {
        if (!NumberUtil::IsDoubleEqual(totalElapseTime, other.totalElapseTime)) {
            return totalElapseTime > other.totalElapseTime;
        }
        return rankId < other.rankId;
    }
};

struct CommunicationSlowRankAnalysisResponseBody {
    bool hasAdvice = false;
    std::string fastRankId;
    double fastTotalElapseTime{};
    std::vector<RankDetailsForSlowRank> slowRankList;
};

struct CommunicationSlowRankAnalysisResponse : public Response {
    CommunicationSlowRankAnalysisResponse() : Response(REQ_RES_COMMUNICATION_DURATION_SLOW_RANK_LIST) {}
    CommunicationSlowRankAnalysisResponseBody body;
};

} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_RESPONSE_H
