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
#include <cfloat>
#include <algorithm>
#include "NumberUtil.h"
#include "ServerLog.h"
#include "StringUtil.h"
#include "BaseParallelStrategyAlgorithm.h"

namespace Dic::Module::Summary {
void BaseParallelStrategyAlgorithm::ClearStrategyConfigCache() {
    // arrangements data
    orderIsTpPpDp = false;
    data.size = 0;
    data.indicators.clear();
    data.arrangements.clear();
    data.connections.clear();

    elementSize = 1;
    foldedTpSize = 1;
    foldedTpCpSize = 1;
    foldedTpCpDpSize = 1;
    foldedTpCpPpSize = 1;

    paraOrder.clear();
    paraOrderWithEp.clear();
    paraDetailsMap.clear();
    updatedOrder.clear();
    updatedOrderWithEp.clear();
    parallelSize.clear();
    parallelSizeWithEp.clear();

    // performance data
    wordSize = 1;
    tpSize = 1;
    tpCpSize = 1;
    tpCpDpSize = 1;
    tpCpPpSize = 1;
    reduceTpMax.clear();
    reduceTpMin.clear();
    reducePpStatistic.clear();
    reduceCpMax.clear();
    reduceCpMin.clear();
    slowRankAdvice.clear();
}

void BaseParallelStrategyAlgorithm::SetStrategyConfig(const ParallelStrategyConfig &config) { strategyConfig = config; }

ParallelStrategyConfig BaseParallelStrategyAlgorithm::GetStrategyConfig() { return strategyConfig; }

ArrangementAndConnectionData BaseParallelStrategyAlgorithm::GetArrangementData() { return data; }

void BaseParallelStrategyAlgorithm::CalStrategyConfig(
    const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig) {
    strategyConfig = tmpConfig;
    static std::vector<std::string> algTpPpDpList = {MEGATRON_LM_TP_CP_PP_EP_DP_ALG, VLLM_TP_PP_DP_EP_ALG};
    if (std::find(algTpPpDpList.begin(), algTpPpDpList.end(), strategyConfig.algorithm) != algTpPpDpList.end()) {
        orderIsTpPpDp = true;
    }
    dimension = tmpDimension;
    tpSize = tmpConfig.tpSize;
    tpCpSize = tpSize * tmpConfig.cpSize;
    tpCpDpSize = tpCpSize * tmpConfig.dpSize;
    tpCpPpSize = tpCpSize * tmpConfig.ppSize;
    // 前端入参已校验，无整数溢出风险
    wordSize = tpCpSize * tmpConfig.dpSize * tmpConfig.ppSize;
}

uint32_t BaseParallelStrategyAlgorithm::GetParallelSizeByType(const std::string &type) const {
    if (type == DP_PARA) {
        return strategyConfig.dpSize;
    } else if (type == EP_PARA) {
        return strategyConfig.epSize;
    } else if (type == PP_PARA) {
        return strategyConfig.ppSize;
    } else if (type == TP_PARA) {
        return strategyConfig.tpSize;
    } else if (type == CP_PARA) {
        return strategyConfig.cpSize;
    } else if (type == MOE_TP_PARA) {
        return strategyConfig.moeTpSize;
    }
    // 默认值为1，表征没有启用对应的并行方式
    return 1;
}

bool BaseParallelStrategyAlgorithm::UpdateShowMap(std::string &err) {
    // 按层次关闭showMap, 被关闭的层次size置1，接下来就不用感知区别不同层次size的影响了
    for (const auto &para : paraOrderWithEp) {
        paraDetailsMap[para].isShown = false;
        paraDetailsMap[para].size = 1;
    }
    SetParaDetail(EP_PARA, strategyConfig.epSize);
    SetParaDetail(DP_PARA, strategyConfig.dpSize);
    if (dimension == DIMENSIONS_DP) {
        return true;
    }
    SetParaDetail(PP_PARA, strategyConfig.ppSize);
    if (dimension == DIMENSIONS_PP) {
        return true;
    }
    SetParaDetail(CP_PARA, strategyConfig.cpSize);
    if (dimension == DIMENSIONS_CP) {
        return true;
    }
    // 目前仅支持全展开视图下返回moeTp坐标
    SetParaDetail(TP_PARA, strategyConfig.tpSize);
    SetParaDetail(MOE_TP_PARA, strategyConfig.moeTpSize);
    if (dimension == DIMENSIONS_TP) {
        return true;
    }
    err = "Failed to update show map for parallel view. Unexpected dimension.";
    SetSummaryError(ErrorCode::CONNECT_DATABASE_FAILED);
    return false;
}

void BaseParallelStrategyAlgorithm::SetParaDetail(const std::string &para, uint32_t size) {
    // 参数为1，该并行域未生效
    if (size == 1) {
        return;
    }
    paraDetailsMap[para].isShown = true;
    paraDetailsMap[para].size = size;
}

void BaseParallelStrategyAlgorithm::UpdateElementSize() {
    // 前端入参已校验，无整数溢出风险
    foldedTpSize = paraDetailsMap[TP_PARA].size;
    foldedTpCpSize = foldedTpSize * paraDetailsMap[CP_PARA].size;
    foldedTpCpPpSize = foldedTpCpSize * paraDetailsMap[PP_PARA].size;
    foldedTpCpDpSize = foldedTpCpSize * paraDetailsMap[DP_PARA].size;
    elementSize = foldedTpCpDpSize * paraDetailsMap[PP_PARA].size;
    data.size = elementSize;
}

std::string BaseParallelStrategyAlgorithm::GetElementName(std::unordered_map<std::string, uint32_t> &indexAttributes) {
    std::string name;
    for (const auto &para : LAYOUT) {
        if (paraDetailsMap.find(para) == paraDetailsMap.end()) {
            continue;
        }
        if (paraDetailsMap[para].isShown && GetParallelSizeByType(para) > 1) {
            name += para;
            name += std::to_string(indexAttributes[para + STR_INDEX]);
            name += "-";
        }
    }
    if (!name.empty()) {
        name.pop_back();
    }
    return name;
}

Position BaseParallelStrategyAlgorithm::GetElementPosition(
    std::unordered_map<std::string, uint32_t> &indexAttributes) const {
    Position position;
    // 前端入参已校验，无整数溢出风险
    position.x = indexAttributes[DP_INDEX] * foldedTpCpSize + indexAttributes[CP_INDEX] * foldedTpSize +
        indexAttributes[TP_INDEX];
    position.y = indexAttributes[PP_INDEX];
    return position;
}

void BaseParallelStrategyAlgorithm::ClearArrangementData() {
    data.indicators.clear();
    data.arrangements.clear();
    data.connections.clear();
}

// 对于TP View，展示全部原始数据，及step_trace_time中的内容
void BaseParallelStrategyAlgorithm::SetTpIndicatorAttr() {
    uint8_t index = 0;
    // 总计算、总通信不在柱状图中显示，而是通过掩盖和未掩盖的堆叠形成
    data.indicators.push_back(
        {index++, KEY_TOTAL_COMPUTING_TIME, VALUE_TOTAL_COMPUTING_TIME, true, false, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back(
        {index++, KEY_TOTAL_COMMUNICATION, VALUE_TOTAL_COMMUNICATION, true, false, true, BAR_CHART, "", TIME_AXIS});

    // 参与stack堆叠：预处理、纯计算、重叠、纯通信、下发
    data.indicators.push_back({index++, KEY_PURE_COMPUTING_TIME, VALUE_COMPUTING_NOT_OVERLAPPED, true, true, true,
        BAR_CHART, TIME_STACK, TIME_AXIS});
    data.indicators.push_back({index++, KEY_COMMUNICATION_OVERLAPPED, VALUE_COMMUNICATION_OVERLAPPED, true, true, true,
        BAR_CHART, TIME_STACK, TIME_AXIS});
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED, VALUE_COMMUNICATION_NOT_OVERLAPPED, true,
        true, true, BAR_CHART, TIME_STACK, TIME_AXIS});
    data.indicators.push_back(
        {index++, KEY_FREE_TIME, VALUE_FREE_TIME, true, true, true, BAR_CHART, TIME_STACK, TIME_AXIS});
    data.indicators.push_back(
        {index++, KEY_PREPARING_TIME, VALUE_PREPARING_TIME, true, true, true, BAR_CHART, TIME_STACK, TIME_AXIS});

    // Communication(Not Overlapped and Exclude Receive)
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE,
        VALUE_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE, true, false, false, BAR_CHART, "", TIME_AXIS});

    // stage and bubble
    data.indicators.push_back(
        {index++, KEY_STAGE_TIME, VALUE_STAGE_TIME, true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back(
        {index++, KEY_BUBBLE_TIME, VALUE_BUBBLE_TIME, true, false, false, BAR_CHART, "", TIME_AXIS});

    // ratio
    data.indicators.push_back(
        {index++, KEY_COMPUTING_RATIO, VALUE_COMPUTING_RATIO, false, true, true, LINE_CHART, "", RATIO_AXIS});
    data.indicators.push_back(
        {index++, KEY_COMMUNICATION_RATIO, VALUE_COMMUNICATION_RATIO, false, true, true, LINE_CHART, "", RATIO_AXIS});
}

// 对于CP Dimension，通过展示一个TP域内计算/通信等时间的统计值，包括最大值、最小值、极差，来分析CP域的性能瓶颈
// 对于通信，可关注最小值，有时通信最小的点，才是瓶颈所在
// 通过极差，可以反映TP域内各卡计算、通信是否均衡，尤其是计算
void BaseParallelStrategyAlgorithm::SetCpIndicatorAttr() {
    uint8_t index = 0;
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_MAX_SUFFIX,
        VALUE_MAX + VALUE_TOTAL_COMPUTING_TIME, true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_TOTAL_COMMUNICATION,
        true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_FREE_TIME, true, true, true,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_NPU_TIME, true, true, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_MIN_SUFFIX,
        VALUE_MIN + VALUE_TOTAL_COMPUTING_TIME, true, false, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_TOTAL_COMMUNICATION,
        true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_FREE_TIME, true, false, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_NPU_TIME, true, false, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_RANGE_SUFFIX,
        VALUE_TOTAL_COMPUTING_TIME + VALUE_RANGE, true, false, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_RANGE_SUFFIX,
        VALUE_TOTAL_COMMUNICATION + VALUE_RANGE, true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_RANGE_SUFFIX, VALUE_FREE_TIME + VALUE_RANGE, true, false,
        false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_RANGE_SUFFIX, VALUE_NPU_TIME + VALUE_RANGE, true, false,
        false, BAR_CHART, "", TIME_AXIS});

    // 通信掩盖、通信未掩盖
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MAX_SUFFIX,
        VALUE_MAX + VALUE_COMMUNICATION_NOT_OVERLAPPED, true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MIN_SUFFIX,
        VALUE_MIN + VALUE_COMMUNICATION_NOT_OVERLAPPED, true, false, false, BAR_CHART, "", TIME_AXIS});
}

// 对于PP Dimension，通过展示一个TP&CP域内计算/通信等时间的统计值，包括最大值、最小值、极差，来分析PP域的性能瓶颈
// 对于通信，可关注最小值，有时通信最小的点，才是瓶颈所在
// 通过极差，可以反映TP&CP域内各卡计算、通信是否均衡，尤其是计算
void BaseParallelStrategyAlgorithm::SetPpIndicatorAttr() {
    uint8_t index = 0;
    // 总计算、总通信、下发、npu总时间, 默认显示总计算
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_MAX_SUFFIX,
        VALUE_MAX + VALUE_TOTAL_COMPUTING_TIME, true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_TOTAL_COMMUNICATION,
        true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_FREE_TIME, true, true, true,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_MAX_SUFFIX, VALUE_MAX + VALUE_NPU_TIME, true, true, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_MIN_SUFFIX,
        VALUE_MIN + VALUE_TOTAL_COMPUTING_TIME, true, false, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_TOTAL_COMMUNICATION,
        true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_FREE_TIME, true, false, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_MIN_SUFFIX, VALUE_MIN + VALUE_NPU_TIME, true, false, false,
        BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMPUTING_TIME + KEY_RANGE_SUFFIX,
        VALUE_TOTAL_COMPUTING_TIME + VALUE_RANGE, true, false, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_TOTAL_COMMUNICATION + KEY_RANGE_SUFFIX,
        VALUE_TOTAL_COMMUNICATION + VALUE_RANGE, true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_FREE_TIME + KEY_RANGE_SUFFIX, VALUE_FREE_TIME + VALUE_RANGE, true, false,
        false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_NPU_TIME + KEY_RANGE_SUFFIX, VALUE_NPU_TIME + VALUE_RANGE, true, false,
        false, BAR_CHART, "", TIME_AXIS});

    // 通信掩盖、通信未掩盖
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MAX_SUFFIX,
        VALUE_MAX + VALUE_COMMUNICATION_NOT_OVERLAPPED, true, false, false, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MIN_SUFFIX,
        VALUE_MIN + VALUE_COMMUNICATION_NOT_OVERLAPPED, true, false, false, BAR_CHART, "", TIME_AXIS});
}

// 对于DP Dimension，通过展示一个TP&CP域内计算/通信等时间的统计值，包括最大值、最小值、极差，来分析PP域的性能瓶颈
void BaseParallelStrategyAlgorithm::SetDpIndicatorAttr() {
    uint8_t index = 0;
    // 总计算、总通信, 默认显示总计算
    data.indicators.push_back({index++, VALUE_SUM_OF_MAX + KEY_TOTAL_COMPUTING_TIME,
        VALUE_SUM_OF_MAX + VALUE_TOTAL_COMPUTING_TIME, true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, VALUE_SUM_OF_MAX + KEY_TOTAL_COMMUNICATION,
        VALUE_SUM_OF_MAX + VALUE_TOTAL_COMMUNICATION, true, true, true, BAR_CHART, "", TIME_AXIS});
    data.indicators.push_back({index++, VALUE_SUM_OF_MAX + KEY_FREE_TIME, VALUE_SUM_OF_MAX + VALUE_FREE_TIME, true,
        true, true, BAR_CHART, "", TIME_AXIS});
    // 通信掩盖、通信未掩盖、下发
    data.indicators.push_back({index++, VALUE_SUM_OF_MAX + KEY_COMMUNICATION_NOT_OVERLAPPED,
        VALUE_SUM_OF_MAX + VALUE_COMMUNICATION_NOT_OVERLAPPED, true, false, false, BAR_CHART, "", TIME_AXIS});
}

void BaseParallelStrategyAlgorithm::CalculatePerformanceDataWithTpDimension(
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
    std::vector<IndicatorDataStruct> &indicatorData) {
    uint32_t maxRankId = 0;
    auto it = std::max_element(
        statistic.begin(), statistic.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    if (it != statistic.end()) {
        maxRankId = NumberSafe::Add(it->first, 1);
    }
    // 为了兼容所采集性能数据未包含实际集群所有卡的情况，此处需取较大值，并用map映射
    maxRankId = std::max(wordSize, maxRankId);
    for (uint32_t i = 0; i < maxRankId; ++i) {
        if (statistic.find(i) == statistic.end()) {
            continue;
        }
        IndicatorDataStruct one{};
        one.index = i;
        const StepStatistic &item = statistic.at(i);
        one.indicators.emplace(KEY_PREPARING_TIME, NumberUtil::DoubleReservedNDigits(item.prepareTime, reservedNum));
        one.indicators.emplace(
            KEY_TOTAL_COMPUTING_TIME, NumberUtil::DoubleReservedNDigits(item.computingTime, reservedNum));
        one.indicators.emplace(KEY_PURE_COMPUTING_TIME,
            NumberUtil::DoubleReservedNDigits(item.computingTime - item.overlapCommunicationTime, reservedNum));
        one.indicators.emplace(
            KEY_TOTAL_COMMUNICATION, NumberUtil::DoubleReservedNDigits(item.communicationTime, reservedNum));
        one.indicators.emplace(KEY_COMMUNICATION_OVERLAPPED,
            NumberUtil::DoubleReservedNDigits(item.overlapCommunicationTime, reservedNum));
        one.indicators.emplace(KEY_COMMUNICATION_NOT_OVERLAPPED,
            NumberUtil::DoubleReservedNDigits(item.pureCommunicationTime, reservedNum));
        one.indicators.emplace(KEY_COMMUNICATION_NOT_OVERLAPPED_AND_RECEIVE,
            NumberUtil::DoubleReservedNDigits(item.pureCommunicationExcludeReceiveTime, reservedNum));
        one.indicators.emplace(KEY_FREE_TIME, NumberUtil::DoubleReservedNDigits(item.freeTime, reservedNum));
        one.indicators.emplace(KEY_STAGE_TIME, NumberUtil::DoubleReservedNDigits(item.stageTime, reservedNum));
        one.indicators.emplace(KEY_BUBBLE_TIME, NumberUtil::DoubleReservedNDigits(item.bubbleTime, reservedNum));
        double e2eTime = item.computingTime + item.pureCommunicationTime + item.freeTime;
        e2eTime += std::max(0.0, item.prepareTime);
        one.indicators.emplace(KEY_COMPUTING_RATIO,
            e2eTime == 0 ? 0
                         : NumberUtil::DoubleReservedNDigits(
                               item.computingTime / e2eTime * PERCENTAGE_RATIO_SCALE, reservedNum));
        one.indicators.emplace(KEY_COMMUNICATION_RATIO,
            e2eTime == 0 ? 0
                         : NumberUtil::DoubleReservedNDigits(
                               item.communicationTime / e2eTime * PERCENTAGE_RATIO_SCALE, reservedNum));
        indicatorData.emplace_back(one);
    }
}

void BaseParallelStrategyAlgorithm::ReduceTpPerformance(
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic) {
    uint32_t idx = 0;
    for (uint32_t i = 0; i < wordSize; i += tpSize) {
        // 若已经计算过，则不用重复计算
        if (reduceTpMax.find(idx) != reduceTpMax.end()) {
            idx++;
            continue;
        }
        StepStatistic maxTpOne;
        StepStatistic minTpOne = {"", "", "", DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX,
            DBL_MAX, DBL_MAX, 0, 0, 0};
        for (uint32_t j = i; j < i + tpSize && j < wordSize; j++) {
            // 跳过空数据卡
            if (statistic.find(j) == statistic.end()) {
                continue;
            }
            const StepStatistic &item = statistic.at(j);
            maxTpOne.computingTime = std::max(maxTpOne.computingTime, item.computingTime);
            maxTpOne.communicationTime = std::max(maxTpOne.communicationTime, item.communicationTime);
            maxTpOne.pureCommunicationTime = std::max(maxTpOne.pureCommunicationTime, item.pureCommunicationTime);
            maxTpOne.overlapCommunicationTime =
                std::max(maxTpOne.overlapCommunicationTime, item.overlapCommunicationTime);
            maxTpOne.freeTime = std::max(maxTpOne.freeTime, item.freeTime);
            maxTpOne.npuTotalTime = std::max(maxTpOne.npuTotalTime, item.npuTotalTime);

            minTpOne.computingTime = std::min(minTpOne.computingTime, item.computingTime);
            minTpOne.communicationTime = std::min(minTpOne.communicationTime, item.communicationTime);
            minTpOne.pureCommunicationTime = std::min(minTpOne.pureCommunicationTime, item.pureCommunicationTime);
            minTpOne.overlapCommunicationTime =
                std::min(minTpOne.overlapCommunicationTime, item.overlapCommunicationTime);
            minTpOne.freeTime = std::min(minTpOne.freeTime, item.freeTime);
            minTpOne.npuTotalTime = std::min(minTpOne.npuTotalTime, item.npuTotalTime);
        }
        // 若一组TP域不全为空，则存入reduceTpStatistic
        if (maxTpOne.computingTime != 0.0) {
            reduceTpMax[idx] = maxTpOne;
            reduceTpMin[idx] = minTpOne;
        }
        idx++;
    }
}

// 折叠CP域，形成DP+PP视图的性能数据，根据TP折叠后的最大/小值进一步计算最大值、最小值、极差等统计值
void BaseParallelStrategyAlgorithm::ReduceCpPerformance() {
    uint32_t idx = 0;
    for (uint32_t i = 0; i < wordSize / strategyConfig.tpSize; i += strategyConfig.cpSize) {
        // 若已经计算过，则不用重复计算
        if (reduceCpMax.find(idx) != reduceCpMax.end()) {
            idx++;
            continue;
        }
        StepStatistic maxCpOne;
        StepStatistic minCpOne = {
            "", "", "", DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, 0, 0, 0, 0, DBL_MAX, 0, 0, 0};
        for (uint32_t j = i; j < i + strategyConfig.cpSize && j < wordSize; j++) {
            // 跳过空数据卡，此处不判断reduceTpMin，因为它与reduceTpMax总是成对出现
            if (reduceTpMax.find(j) == reduceTpMax.end()) {
                continue;
            }
            const StepStatistic &maxItem = reduceTpMax.at(j);

            maxCpOne.computingTime = std::max(maxCpOne.computingTime, maxItem.computingTime);
            maxCpOne.communicationTime = std::max(maxCpOne.communicationTime, maxItem.communicationTime);
            maxCpOne.pureCommunicationTime = std::max(maxCpOne.pureCommunicationTime, maxItem.pureCommunicationTime);
            maxCpOne.freeTime = std::max(maxCpOne.freeTime, maxItem.freeTime);
            maxCpOne.npuTotalTime = std::max(maxCpOne.npuTotalTime, maxItem.npuTotalTime);

            const StepStatistic &minItem = reduceTpMin.at(j);
            minCpOne.computingTime = std::min(minCpOne.computingTime, minItem.computingTime);
            minCpOne.communicationTime = std::min(minCpOne.communicationTime, minItem.communicationTime);
            minCpOne.pureCommunicationTime = std::min(minCpOne.pureCommunicationTime, minItem.pureCommunicationTime);
            minCpOne.freeTime = std::min(minCpOne.freeTime, minItem.freeTime);
            minCpOne.npuTotalTime = std::min(minCpOne.npuTotalTime, minItem.npuTotalTime);
        }
        // 若一组CP域不全为空，则存入
        if (maxCpOne.computingTime != 0.0) {
            reduceCpMax[idx] = maxCpOne;
            reduceCpMin[idx] = minCpOne;
        }
        idx++;
    }
}

void BaseParallelStrategyAlgorithm::CalculatePerformanceDataWithCpDimension(
    std::vector<IndicatorDataStruct> &indicatorData) {
    // 前面的逻辑保证tpSize不为0
    for (uint32_t i = 0; i < wordSize / tpSize; ++i) {
        if (reduceTpMax.find(i) == reduceTpMax.end()) {
            continue;
        }
        IndicatorDataStruct one{};
        one.index = i;
        auto &max = reduceTpMax.at(i);
        auto &min = reduceTpMin.at(i);
        one.indicators.emplace(KEY_TOTAL_COMPUTING_TIME + KEY_MAX_SUFFIX, max.computingTime);
        one.indicators.emplace(KEY_TOTAL_COMPUTING_TIME + KEY_MIN_SUFFIX, min.computingTime);
        one.indicators.emplace(
            KEY_TOTAL_COMPUTING_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.computingTime - min.computingTime));
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_MAX_SUFFIX, max.communicationTime);
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_MIN_SUFFIX, min.communicationTime);
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_RANGE_SUFFIX,
            Reserved3DecimalPlaces(max.communicationTime - min.communicationTime));
        one.indicators.emplace(KEY_FREE_TIME + KEY_MAX_SUFFIX, max.freeTime);
        one.indicators.emplace(KEY_FREE_TIME + KEY_MIN_SUFFIX, min.freeTime);
        one.indicators.emplace(KEY_FREE_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.freeTime - min.freeTime));
        one.indicators.emplace(KEY_NPU_TIME + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.npuTotalTime));
        one.indicators.emplace(KEY_NPU_TIME + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.npuTotalTime));
        one.indicators.emplace(
            KEY_NPU_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.npuTotalTime - min.npuTotalTime));
        one.indicators.emplace(KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MAX_SUFFIX, max.pureCommunicationTime);
        one.indicators.emplace(KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MIN_SUFFIX, min.pureCommunicationTime);
        indicatorData.emplace_back(one);
    }
}

void BaseParallelStrategyAlgorithm::CalculatePerformanceDataWithPpDimension(
    std::vector<IndicatorDataStruct> &indicatorData) {
    // 前面的逻辑保证tpCpSize不为0
    for (uint32_t i = 0; i < wordSize / tpCpSize; ++i) {
        if (reduceCpMax.find(i) == reduceCpMax.end()) {
            continue;
        }
        IndicatorDataStruct one{};
        one.index = i;
        auto &max = reduceCpMax.at(i);
        auto &min = reduceCpMin.at(i);
        one.indicators.emplace(KEY_TOTAL_COMPUTING_TIME + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.computingTime));
        one.indicators.emplace(KEY_TOTAL_COMPUTING_TIME + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.computingTime));
        one.indicators.emplace(
            KEY_TOTAL_COMPUTING_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.computingTime - min.computingTime));
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.communicationTime));
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.communicationTime));
        one.indicators.emplace(KEY_TOTAL_COMMUNICATION + KEY_RANGE_SUFFIX,
            Reserved3DecimalPlaces(max.communicationTime - min.communicationTime));
        one.indicators.emplace(KEY_FREE_TIME + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.freeTime));
        one.indicators.emplace(KEY_FREE_TIME + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.freeTime));
        one.indicators.emplace(KEY_FREE_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.freeTime - min.freeTime));
        one.indicators.emplace(KEY_NPU_TIME + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.npuTotalTime));
        one.indicators.emplace(KEY_NPU_TIME + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.npuTotalTime));
        one.indicators.emplace(
            KEY_NPU_TIME + KEY_RANGE_SUFFIX, Reserved3DecimalPlaces(max.npuTotalTime - min.npuTotalTime));
        one.indicators.emplace(
            KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MAX_SUFFIX, Reserved3DecimalPlaces(max.pureCommunicationTime));
        one.indicators.emplace(
            KEY_COMMUNICATION_NOT_OVERLAPPED + KEY_MIN_SUFFIX, Reserved3DecimalPlaces(min.pureCommunicationTime));
        indicatorData.emplace_back(one);
    }
}

void BaseParallelStrategyAlgorithm::ReducePpPerformanceForDpLast() {
    uint32_t dpGroupIdx = 0;
    for (uint32_t i = 0; i < wordSize / tpCpSize; i += strategyConfig.ppSize) {
        ReducePpPerformance(i, 1, dpGroupIdx);
    }
}

void BaseParallelStrategyAlgorithm::ReducePpPerformanceForPpLast() {
    uint32_t dpGroupIdx = 0;
    // 规约后共有dp_size个数据，且规约时步长为dp_size
    for (uint32_t i = 0; i < strategyConfig.dpSize; i++) {
        ReducePpPerformance(i, strategyConfig.dpSize, dpGroupIdx);
    }
}

void BaseParallelStrategyAlgorithm::ReducePpPerformance(uint32_t startIndex, uint32_t step, uint32_t &dpGroupIdx) {
    // 若已经计算过，则不用重复计算
    if (reducePpStatistic.find(dpGroupIdx) != reducePpStatistic.end()) {
        dpGroupIdx++;
        return;
    }
    StepStatistic reducePpOne;
    // 取累加和
    for (uint32_t k = startIndex; k < wordSize / tpCpSize && k < startIndex + step * strategyConfig.ppSize; k += step) {
        // 跳过空数据卡
        if (reduceCpMax.find(k) == reduceCpMax.end()) {
            continue;
        }
        const StepStatistic &item = reduceCpMax.at(k);
        reducePpOne.computingTime += item.computingTime;
        reducePpOne.communicationTime += item.communicationTime;
        reducePpOne.pureCommunicationTime += item.pureCommunicationTime;
        reducePpOne.freeTime += item.freeTime;
    }
    // 若一组PP域不全为空，则存入reducePpStatistic
    if (reducePpOne.computingTime != 0.0) {
        reducePpStatistic[dpGroupIdx] = reducePpOne;
    }
    dpGroupIdx++;
}

void BaseParallelStrategyAlgorithm::GetPerformanceResponseDataWithDpDimension(
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic,
    std::vector<IndicatorDataStruct> &indicatorData) {
    for (const auto &item : statistic) {
        IndicatorDataStruct one{};
        one.index = item.first;
        StepStatistic indicator = item.second;
        one.indicators.emplace(
            VALUE_SUM_OF_MAX + KEY_TOTAL_COMPUTING_TIME, Reserved3DecimalPlaces(indicator.computingTime));
        one.indicators.emplace(
            VALUE_SUM_OF_MAX + KEY_TOTAL_COMMUNICATION, Reserved3DecimalPlaces(indicator.communicationTime));
        one.indicators.emplace(VALUE_SUM_OF_MAX + KEY_COMMUNICATION_NOT_OVERLAPPED,
            Reserved3DecimalPlaces(indicator.pureCommunicationTime));
        one.indicators.emplace(VALUE_SUM_OF_MAX + KEY_FREE_TIME, Reserved3DecimalPlaces(indicator.freeTime));
        indicatorData.emplace_back(one);
    }
}

double BaseParallelStrategyAlgorithm::Reserved3DecimalPlaces(double num) {
    if (NumberUtil::IsDoubleEqual(num, 0.0) || NumberUtil::IsDoubleEqual(num, DBL_MAX)) {
        return 0.0;
    }
    return NumberUtil::DoubleReservedNDigits(num, reservedNum);
}

void BaseParallelStrategyAlgorithm::AnalyzePerformanceAdviceWithDpCpPpTpDimension(Protocol::TraceStatistic &max,
    Protocol::TraceStatistic &min, double meanE2ETime, std::vector<std::string> &advices) {
    constexpr double threshold = 0.05;
    Protocol::TraceStatistic diff = {
        max.computeDiff - min.computeDiff, max.communicationDiff - min.communicationDiff, max.freeDiff - min.freeDiff};
    if (diff.computeDiff / meanE2ETime > threshold) {
        advices.emplace_back("Computing has some issues, because the max difference of \"Computing\" "
                             "has reached " +
            std::to_string(Reserved3DecimalPlaces(diff.computeDiff)) + "us.");
    }
    if (diff.communicationDiff / meanE2ETime > threshold) {
        advices.emplace_back("Communication has some issues, because the max difference of "
                             "\"Communication(Not Overlapped)\" has reached " +
            std::to_string(Reserved3DecimalPlaces(diff.communicationDiff)) + "us.");
    }
    if (diff.freeDiff / meanE2ETime > threshold) {
        advices.emplace_back("Free has some issues, because the max difference of \"Free\" "
                             "has reached " +
            std::to_string(Reserved3DecimalPlaces(diff.freeDiff)) + "us.");
    }
}

void BaseParallelStrategyAlgorithm::CalAdviceInfo(const std::string &tmpDimension, std::vector<std::string> &advices,
    std::vector<IndicatorDataStruct> &indicatorData) {
    if (tmpDimension != DIMENSIONS_TP) {
        return;
    }
    Protocol::TraceStatistic max{};
    Protocol::TraceStatistic min = {DBL_MAX, DBL_MAX, DBL_MAX};
    double sum = 0;
    for (auto &item : indicatorData) {
        max.computeDiff = std::max(max.computeDiff, item.indicators[KEY_TOTAL_COMPUTING_TIME]);
        max.communicationDiff = std::max(max.communicationDiff, item.indicators[KEY_COMMUNICATION_NOT_OVERLAPPED]);
        max.freeDiff = std::max(max.freeDiff, item.indicators[KEY_FREE_TIME]);
        min.computeDiff = std::min(min.computeDiff, item.indicators[KEY_TOTAL_COMPUTING_TIME]);
        min.communicationDiff = std::min(min.communicationDiff, item.indicators[KEY_COMMUNICATION_NOT_OVERLAPPED]);
        min.freeDiff = std::min(min.freeDiff, item.indicators[KEY_FREE_TIME]);
        sum += item.indicators[KEY_TOTAL_COMPUTING_TIME] + item.indicators[KEY_COMMUNICATION_NOT_OVERLAPPED] +
            item.indicators[KEY_FREE_TIME];
        sum += std::max(0.0, item.indicators[KEY_PREPARING_TIME]);
    }
    if (!indicatorData.empty() && sum != 0) {
        AnalyzePerformanceAdviceWithDpCpPpTpDimension(max, min, sum / indicatorData.size(), advices);
    }
}

/**
 * 慢卡专家建议
 * @param commInTpDimension 全展开维度下，按通信域拆解通信时间结果
 * @return 当前并行策略参数是否能正确按通信域拆解出通信时间
 * (若当前并行策略参数与实际模型训练参数不一致，可能无法正确按通信域拆解出通信时间，无法给出慢卡专家建议)
 */
bool BaseParallelStrategyAlgorithm::CalAdviceInfoByCommInfo(CommInfoMap &commInTpDimension) {
    slowRankAdvice.clear();
    commMatchSuccess = true;
    if (dimension == DIMENSIONS_DP) {
        return commMatchSuccess;
    }
    TopNAdviceMaintainer topNAdviceForPpDim = CalAdviceInfoByPpDim(commInTpDimension);
    if (dimension == DIMENSIONS_PP) {
        slowRankAdvice = topNAdviceForPpDim.GetTopNSlowest(topN);
        return commMatchSuccess;
    }
    TopNAdviceMaintainer topNAdviceForCpDim = CalAdviceInfoByCpDim(topNAdviceForPpDim, commInTpDimension);
    if (dimension == DIMENSIONS_CP) {
        slowRankAdvice = topNAdviceForCpDim.GetTopNSlowest(topN);
        return commMatchSuccess;
    }
    TopNAdviceMaintainer topNAdviceForTpDim = CalAdviceInfoByTpDim(topNAdviceForCpDim, commInTpDimension);
    slowRankAdvice = topNAdviceForTpDim.GetTopNSlowest(topN);
    return commMatchSuccess;
}

std::vector<AdviceInfoForSlowRank> BaseParallelStrategyAlgorithm::GetTopNAdviceInfo(bool &matchSuccess) {
    matchSuccess = commMatchSuccess;
    return slowRankAdvice;
}

void BaseParallelStrategyAlgorithm::CalTpDimAdviceInfoWithoutDpCpAdvice(
    const ParallelStrategyConfig &tmpConfig, CommInfoMap &commInTpDimension, TopNAdviceMaintainer &topNAdviceForTpDim) {
    // 若没有DP+PP+CP维度TopN慢分组，则遍历当前维度所有TP通信域
    for (uint32_t dpIndex = 0; dpIndex < strategyConfig.dpSize; dpIndex++) {
        for (uint32_t ppIndex = 0; ppIndex < strategyConfig.ppSize; ppIndex++) {
            for (uint32_t cpIndex = 0; cpIndex < strategyConfig.cpSize; cpIndex++) {
                AdviceInfoForSlowRank tmpAdvice;
                tmpAdvice.indexAttributes[DP_PARA] = dpIndex;
                tmpAdvice.indexAttributes[CP_PARA] = cpIndex;
                tmpAdvice.indexAttributes[PP_PARA] = ppIndex;
                // 求当前通信域每张卡的TP卡间同步平均时间(最大TP通信时间-该Element TP通信时间)
                CalSynchronizeTime(TP_PARA, tmpAdvice, tmpConfig, commInTpDimension, topNAdviceForTpDim);
            }
        }
    }
}

TopNAdviceMaintainer BaseParallelStrategyAlgorithm::CalAdviceInfoByTpDim(
    const TopNAdviceMaintainer &topNAdviceForCpDim, CommInfoMap &commInTpDimension) {
    if (strategyConfig.tpSize == 1) {
        return topNAdviceForCpDim;
    }
    ParallelStrategyConfig tmpConfig = strategyConfig;
    // 根据DP-PP-CP-TP维度通信指标,过滤出TP-通信时间
    for (auto &item : commInTpDimension) {
        auto &commInfo = item.second;
        commInfo.erase(std::remove_if(commInfo.begin(), commInfo.end(),
                           [](const CommInfoUnderRank &info) { return info.pgName != TP_GROUP; }),
            commInfo.end());
    }
    TopNAdviceMaintainer topNAdviceForTpDim(maxLengthOfAdvice);
    // 若所有元素都不含TP-通信时间，则当前并行策略参数与实际模型训练参数不一致，无法正确按通信域拆解出通信时间，无法给出慢卡专家建议
    commMatchSuccess = false;
    for (auto &item : commInTpDimension) {
        auto &commInfo = item.second;
        if (!commInfo.empty()) {
            commMatchSuccess = true;
        }
    }
    if (!commMatchSuccess) {
        return topNAdviceForTpDim;
    }
    if (topNAdviceForCpDim.IsEmpty()) {
        // 若没有DP+PP+CP维度TopN慢分组，则遍历当前维度所有TP通信域
        CalTpDimAdviceInfoWithoutDpCpAdvice(tmpConfig, commInTpDimension, topNAdviceForTpDim);
    } else {
        // 否则，根据DP+PP+CP维度TopN慢分组进一步排查
        std::vector<AdviceInfoForSlowRank> adviceListForCp = topNAdviceForCpDim.GetTopNSlowest(topN);
        for (auto &adviceForCp : adviceListForCp) {
            CalSynchronizeTime(TP_PARA, adviceForCp, tmpConfig, commInTpDimension, topNAdviceForTpDim);
        }
    }
    return topNAdviceForTpDim;
}

TopNAdviceMaintainer BaseParallelStrategyAlgorithm::CalAdviceInfoByCpDim(
    const TopNAdviceMaintainer &topNAdviceForPpDim, const CommInfoMap &commInTpDimension) {
    if (strategyConfig.cpSize == 1) {
        return topNAdviceForPpDim;
    }
    ParallelStrategyConfig tmpConfig = strategyConfig;
    tmpConfig.tpSize = 1;

    // 得到DP-PP-CP维度通信指标
    CommInfoMap commInCpDimension = GetCommInfoByDimension(commInTpDimension, DIMENSIONS_CP);
    // 过滤出CP-通信时间
    for (auto &item : commInCpDimension) {
        auto &commInfo = item.second;
        commInfo.erase(std::remove_if(commInfo.begin(), commInfo.end(),
                           [](const CommInfoUnderRank &info) { return info.pgName != CP_GROUP; }),
            commInfo.end());
    }
    TopNAdviceMaintainer topNAdviceForCpDim(maxLengthOfAdvice);
    // 若所有元素都不含CP-通信时间，则当前并行策略参数与实际模型训练参数不一致，无法正确按通信域拆解出通信时间，无法给出慢卡专家建议
    commMatchSuccess = false;
    for (auto &item : commInCpDimension) {
        auto &commInfo = item.second;
        if (!commInfo.empty()) {
            commMatchSuccess = true;
        }
    }
    if (!commMatchSuccess) {
        return topNAdviceForCpDim;
    }
    if (topNAdviceForPpDim.IsEmpty()) {
        // 若没有DP+PP维度TopN慢分组，则遍历当前维度所有CP分组
        for (uint32_t dpIndex = 0; dpIndex < strategyConfig.dpSize; dpIndex++) {
            for (uint32_t ppIndex = 0; ppIndex < strategyConfig.ppSize; ppIndex++) {
                AdviceInfoForSlowRank tmpAdvice;
                tmpAdvice.indexAttributes[DP_PARA] = dpIndex;
                tmpAdvice.indexAttributes[TP_PARA] = 0;
                tmpAdvice.indexAttributes[PP_PARA] = ppIndex;
                // 求当前通信域每张卡的CP组间同步平均时间(最大CP通信时间-该Element CP通信时间)
                CalSynchronizeTime(CP_PARA, tmpAdvice, tmpConfig, commInCpDimension, topNAdviceForCpDim);
            }
        }
    } else {
        // 否则，根据DP+PP维度TopN慢分组进一步排查
        std::vector<AdviceInfoForSlowRank> adviceListForPp = topNAdviceForPpDim.GetTopNSlowest(topN);
        for (auto &adviceForPp : adviceListForPp) {
            CalSynchronizeTime(CP_PARA, adviceForPp, tmpConfig, commInCpDimension, topNAdviceForCpDim);
        }
    }
    return topNAdviceForCpDim;
}

TopNAdviceMaintainer BaseParallelStrategyAlgorithm::CalAdviceInfoByPpDim(const CommInfoMap &commInTpDimension) {
    TopNAdviceMaintainer topNAdviceForPpDim(maxLengthOfAdvice);
    if (strategyConfig.dpSize == 1) {
        return topNAdviceForPpDim;
    }
    ParallelStrategyConfig tmpConfig = strategyConfig;
    tmpConfig.tpSize = 1;
    tmpConfig.cpSize = 1;
    // 得到DP-PP维度通信指标
    CommInfoMap commInPpDimension = GetCommInfoByDimension(commInTpDimension, DIMENSIONS_PP);
    // 过滤出DP-通信时间
    for (auto &item : commInPpDimension) {
        auto &commInfo = item.second;
        commInfo.erase(std::remove_if(commInfo.begin(), commInfo.end(),
                           [](const CommInfoUnderRank &info) { return info.pgName != DP_GROUP; }),
            commInfo.end());
    }
    // 若所有元素都不含DP-通信时间，则当前并行策略参数与实际模型训练参数不一致，无法正确按通信域拆解出通信时间，无法给出慢卡专家建议
    commMatchSuccess = false;
    for (auto &item : commInPpDimension) {
        auto &commInfo = item.second;
        if (!commInfo.empty()) {
            commMatchSuccess = true;
        }
    }
    if (!commMatchSuccess) {
        return topNAdviceForPpDim;
    }
    for (uint32_t ppIndex = 0; ppIndex < strategyConfig.ppSize; ppIndex++) {
        AdviceInfoForSlowRank tmpAdvice;
        tmpAdvice.indexAttributes[CP_PARA] = 0;
        tmpAdvice.indexAttributes[TP_PARA] = 0;
        tmpAdvice.indexAttributes[PP_PARA] = ppIndex;
        // 求当前通信域每张卡的DP组间同步平均时间(最大DP通信时间-该Element DP通信时间)
        CalSynchronizeTime(DP_PARA, tmpAdvice, tmpConfig, commInPpDimension, topNAdviceForPpDim);
    }
    return topNAdviceForPpDim;
}

// 求当前通信域每张卡的XX组间同步平均时间(最大XX通信时间-该Element XX通信时间)
void BaseParallelStrategyAlgorithm::CalSynchronizeTime(const std::string &para, AdviceInfoForSlowRank &adviceInfo,
    const ParallelStrategyConfig &tmpConfig, CommInfoMap &commInDimension, TopNAdviceMaintainer &topNAdvice) {
    double maxCommTime = 0.0;
    uint32_t paraSize = GetParallelSizeByType(para);
    // 遍历当前XX-通信域，找到最大XX-通信时间
    for (uint32_t index = 0; index < paraSize; index++) {
        adviceInfo.indexAttributes[para] = index;
        uint32_t eleIndex = GetElementIndex(adviceInfo.indexAttributes, tmpConfig);
        std::vector<CommInfoUnderRank> commInfoList = commInDimension[std::to_string(eleIndex)];
        if (commInfoList.empty()) {
            continue;
        }
        maxCommTime = maxCommTime > commInfoList[0].commTime ? maxCommTime : commInfoList[0].commTime;
    }
    if (NumberUtil::IsDoubleEqual(maxCommTime, 0.0)) {
        return;
    }
    const std::vector<std::string> commTimeListForAdvice = {DP_PARA, CP_PARA};
    // 求当前通信域每张卡的XX组间同步平均时间(最大XX通信时间-该Element XX-通信时间)
    for (uint32_t index = 0; index < paraSize; index++) {
        adviceInfo.indexAttributes[para] = index;
        uint32_t eleIndex = GetElementIndex(adviceInfo.indexAttributes, tmpConfig);
        std::vector<CommInfoUnderRank> commInfoList = commInDimension[std::to_string(eleIndex)];
        if (commInfoList.empty()) {
            continue;
        }
        AdviceInfoForSlowRank adviceInfoForSlowRank;
        adviceInfoForSlowRank.index = eleIndex;
        adviceInfoForSlowRank.name = GetElementNameForTopNAdvice(tmpConfig, adviceInfo.indexAttributes);
        adviceInfoForSlowRank.indexAttributes = adviceInfo.indexAttributes;
        adviceInfoForSlowRank.maxCommTime[para] = maxCommTime;
        adviceInfoForSlowRank.synchronizeTime[para] =
            NumberUtil::DoubleReservedNDigits(maxCommTime - commInfoList[0].commTime, reservedNum);
        // 任一类型通信同步时间超过阈值，视为慢卡/慢分组，需插入专家建议堆, 837行已确保maxCommTime不为0
        bool needInsert = false;
        if ((adviceInfoForSlowRank.synchronizeTime[para] / maxCommTime) > thresholdForSlowRankAdvice) {
            needInsert = true;
        }
        // 添加对应所属分组DP-通信时间、CP通信时间
        for (const auto &item : commTimeListForAdvice) {
            if (adviceInfo.synchronizeTime.find(item) != adviceInfo.synchronizeTime.end()) {
                adviceInfoForSlowRank.synchronizeTime[item] = adviceInfo.synchronizeTime[item];
                adviceInfoForSlowRank.maxCommTime[item] = adviceInfo.maxCommTime[item];
                needInsert = true;
            }
        }
        if (needInsert) {
            topNAdvice.Insert(adviceInfoForSlowRank);
        }
    }
}

uint32_t BaseParallelStrategyAlgorithm::GetElementIndex(
    std::unordered_map<std::string, uint32_t> &indexAttributes, const ParallelStrategyConfig &tmpConfig) const {
    // 前端入参已校验，无乘法整数溢出风险
    uint32_t curTpSize = tmpConfig.tpSize;
    uint32_t curTpCpSize = curTpSize * tmpConfig.cpSize;
    uint32_t curTpCpDpSize = curTpCpSize * tmpConfig.dpSize;
    uint32_t curTpCpPpSize = curTpCpSize * tmpConfig.ppSize;

    uint32_t eleIndex{};
    if (orderIsTpPpDp) {
        eleIndex = curTpCpPpSize * indexAttributes[DP_PARA] + curTpCpSize * indexAttributes[PP_PARA] +
            curTpSize * indexAttributes[CP_PARA] + indexAttributes[TP_PARA];
    } else {
        eleIndex = curTpCpDpSize * indexAttributes[PP_PARA] + curTpCpSize * indexAttributes[DP_PARA] +
            curTpSize * indexAttributes[CP_PARA] + indexAttributes[TP_PARA];
    }
    return eleIndex;
}

std::string BaseParallelStrategyAlgorithm::GetElementNameForTopNAdvice(
    const ParallelStrategyConfig &tmpConfig, std::unordered_map<std::string, uint32_t> &indexAttributes) {
    std::string name;
    for (const auto &para : LAYOUT) {
        if (GetTempParallelSizeByTypeForTopNAdvice(para, tmpConfig) > 1) {
            name = StringUtil::StrJoin(name, para, std::to_string(indexAttributes[para]), "-");
        }
    }
    if (!name.empty()) {
        name.pop_back();
    }
    return name;
}

uint32_t BaseParallelStrategyAlgorithm::GetTempParallelSizeByTypeForTopNAdvice(
    const std::string &type, const ParallelStrategyConfig &config) {
    if (type == DP_PARA) {
        return config.dpSize;
    }
    if (type == PP_PARA) {
        return config.ppSize;
    }
    if (type == TP_PARA) {
        return config.tpSize;
    }
    if (type == CP_PARA) {
        return config.cpSize;
    }
    // 默认值为1，表征没有启用对应的并行方式
    return 1;
}

CommInfoMap BaseParallelStrategyAlgorithm::GetCommInfoByDimension(
    const CommInfoMap &expandCommInfos, const std::string &curDimension) {
    // 查找对应的处理函数
    auto it = commInfoHandlers.find(curDimension);
    // 如果找到了对应的处理函数，则调用它
    if (it != commInfoHandlers.end()) {
        return it->second(expandCommInfos);
    } else {
        // 如果没有找到对应的处理函数，则默认返回空列表
        return {};
    }
}

/**
 * 默认折叠算法将输入的数据按滑动窗口进行折叠求平均
 * @param input 输入数据
 * @param w 滑动窗口宽
 * @param h 滑动窗口高
 * @return
 */
std::unordered_map<std::string, std::vector<CommInfoUnderRank>> BaseParallelStrategyAlgorithm::ReduceCommDefaultFunc(
    const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &input, uint32_t w, uint32_t h) {
    if (input.empty()) {
        Server::ServerLog::Error("Fail to reduce communication data, input is empty.");
        return {};
    }

    // 检查参数是否为0，滑动窗口是否符合折叠要求
    bool isParamInvalid = strategyConfig.ppSize == 0 || tpCpDpSize == 0 || w == 0 || h == 0 ||
        strategyConfig.ppSize % h != 0 || tpCpDpSize % w != 0;
    if (isParamInvalid) {
        Server::ServerLog::Error("Fail to reduce communication data, param error.");
        return {};
    }

    // 先进行求和，resMap的格式为 {序号: {"通信域名(tp/dp等)"： “通信数据”}}
    std::unordered_map<std::string, std::unordered_map<std::string, double>> resMap;
    // 求和的同时统计个数，格式为{"序号-通信域名": 累加的个数}
    std::unordered_map<std::string, int> countMap;
    for (const auto &item : input) {
        // 开始计算折叠后的下标：获取原始下表
        uint32_t index = StringUtil::StringToUint32(item.first);
        // 二维数组维度，计算元素的目标位置（折叠后该点数据应该被计算在第curRow行，curColumn列的点中）
        // 折叠后行数,(index / tpCpDpSize)为折叠前所在行，再除去窗口高度h则为折叠后所在行
        uint32_t curRow = index / tpCpDpSize / h;
        // 折叠后列数，index % tpCpDpSize 为折叠前所在列，再除去窗口宽度w，为折叠后所在列
        uint32_t curColumn = index % tpCpDpSize / w;
        // 计算折叠后序号：所在行号 * 行数据个数 + 所在列号
        uint32_t curIndex = curRow * (tpCpDpSize / w) + curColumn;
        std::string finalIndexStr = std::to_string(curIndex);
        for (const auto &commInfo : item.second) {
            resMap[finalIndexStr][commInfo.pgName] += commInfo.commTime;
            countMap[finalIndexStr + "-" + commInfo.pgName]++;
        }
    }

    // 求平均
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> res;
    for (const auto &item : resMap) {
        std::vector<CommInfoUnderRank> commInfos;
        for (const auto &info : item.second) {
            // 这里从countMap的key值就是由resMap的两层key值拼接成的，不会出现0的情况
            double avgComm =
                NumberUtil::DoubleReservedNDigits(info.second / countMap[item.first + "-" + info.first], reservedNum);
            commInfos.push_back({avgComm, item.first, "", info.first});
        }
        res[item.first] = commInfos;
    }
    return res;
}

std::unordered_map<std::string, std::vector<CommInfoUnderRank>> BaseParallelStrategyAlgorithm::ReduceCommTpDimensionDef(
    const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData) {
    return expendData;
}

std::unordered_map<std::string, std::vector<CommInfoUnderRank>> BaseParallelStrategyAlgorithm::ReduceCommCpDimensionDef(
    const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData) {
    return ReduceCommDefaultFunc(expendData, tpSize, 1);
}

std::unordered_map<std::string, std::vector<CommInfoUnderRank>> BaseParallelStrategyAlgorithm::ReduceCommPpDimensionDef(
    const std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &expendData) {
    return ReduceCommDefaultFunc(expendData, tpCpSize, 1);
}

/**
 * 折叠视图下，计算每个折叠分组中的rank set
 * @return
 */
uint32_t BaseParallelStrategyAlgorithm::CalculateContainingRanksByAttrs(
    uint32_t dpIndex, uint32_t ppIndex, uint32_t cpIndex, uint32_t tpIndex) const {
    return orderIsTpPpDp ? tpCpPpSize * dpIndex + tpCpSize * ppIndex + tpSize * cpIndex + tpIndex
                         : tpCpDpSize * ppIndex + tpCpSize * dpIndex + tpSize * cpIndex + tpIndex;
}

std::string BaseParallelStrategyAlgorithm::FormatRanksForInterval(uint32_t start, uint32_t end) {
    std::stringstream formatRanks;
    if (start > end) {
        return "";
    } else if (start == end) {
        formatRanks << start;
    } else if (end - start == 1) {
        formatRanks << start << "," << end;
    } else {
        formatRanks << start << "-" << end;
    }
    return formatRanks.str();
}

std::string BaseParallelStrategyAlgorithm::FormatRanksForSeveralIntervals(const std::vector<std::string> &intervals) {
    std::stringstream tmpResult;
    std::string formattedRankSet;
    for (const auto &interval : intervals) {
        tmpResult << interval << ",";
    }
    formattedRankSet = tmpResult.str();
    if (!formattedRankSet.empty()) {
        formattedRankSet.pop_back();
    }
    return formattedRankSet;
}

std::vector<uint32_t> BaseParallelStrategyAlgorithm::GetElementContainFormattedRanks(
    std::unordered_map<std::string, uint32_t> &attrs, std::string &formattedRanks, const ElementRankDetails &details) {
    uint32_t rankStart = 0;
    uint32_t rankEnd = 0;
    std::vector<uint32_t> ranks{};
    // 若涉及pp折叠，且算法排布顺序为TP-DP-PP类型，则RankSet由若干个区间组成
    if (details.ppIndexMax != details.ppIndexMin && !orderIsTpPpDp) {
        std::vector<std::string> formatRanksForDpDimension;
        for (uint32_t ppIndex = details.ppIndexMin; ppIndex <= details.ppIndexMax; ++ppIndex) {
            rankStart =
                CalculateContainingRanksByAttrs(attrs[DP_INDEX], ppIndex, details.cpIndexMin, details.tpIndexMin);
            rankEnd = CalculateContainingRanksByAttrs(attrs[DP_INDEX], ppIndex, details.cpIndexMax, details.tpIndexMax);
            formatRanksForDpDimension.push_back(FormatRanksForInterval(rankStart, rankEnd));
            for (uint32_t rankIndex = rankStart; rankIndex <= rankEnd; ++rankIndex) {
                ranks.push_back(rankIndex);
            }
        }
        formattedRanks = FormatRanksForSeveralIntervals(formatRanksForDpDimension);
        return ranks;
    }
    if (details.ppIndexMax == details.ppIndexMin) {
        // 若不涉及pp折叠，则tp折叠或cp折叠一定是一串连续序号，用一个区间表示即可
        rankStart =
            CalculateContainingRanksByAttrs(attrs[DP_INDEX], attrs[PP_INDEX], details.cpIndexMin, details.tpIndexMin);
        rankEnd =
            CalculateContainingRanksByAttrs(attrs[DP_INDEX], attrs[PP_INDEX], details.cpIndexMax, details.tpIndexMax);
    } else if (orderIsTpPpDp) {
        // 若涉及pp折叠，且算法排布顺序为TP-PP-DP类型, 则tp/cp/pp折叠卡序号都为一个连续区间
        rankStart = CalculateContainingRanksByAttrs(
            attrs[DP_INDEX], details.ppIndexMin, details.cpIndexMin, details.tpIndexMin);
        rankEnd = CalculateContainingRanksByAttrs(
            attrs[DP_INDEX], details.ppIndexMax, details.cpIndexMax, details.tpIndexMax);
    }
    formattedRanks = FormatRanksForInterval(rankStart, rankEnd);
    for (uint32_t rankIndex = rankStart; rankIndex <= rankEnd; ++rankIndex) {
        ranks.push_back(rankIndex);
    }
    return ranks;
}

/**
 * 折叠视图下，计算每个折叠分组中的rank set，结果用格式化字符串表示
 * @param index 元素序号
 * @param attrs 元素并行坐标，即dpIndex、ppIndex等
 * @return
 */
std::vector<uint32_t> BaseParallelStrategyAlgorithm::GetElementContainRanks(
    uint32_t index, std::unordered_map<std::string, uint32_t> &attrs, std::string &formattedRanks) {
    std::vector<uint32_t> ranks{};
    std::stringstream formatRanks;
    if (wordSize <= 1) {
        return ranks;
    }
    if (dimension == DIMENSIONS_TP) {
        ranks.emplace_back(index);
        formatRanks << index;
        formattedRanks = formatRanks.str();
        return ranks;
    }

    ElementRankDetails details;
    // xxIndex ∈ [xxIndexMin, xxIndexMax], 前端入参已校验，以下xxSize >= 1
    details.ppIndexMax = strategyConfig.ppSize - 1;
    details.cpIndexMax = strategyConfig.cpSize - 1;
    details.tpIndexMax = strategyConfig.tpSize - 1;

    if (dimension == DIMENSIONS_CP) {
        details.cpIndexMin = attrs[CP_INDEX];
        details.cpIndexMax = details.cpIndexMin;
        details.ppIndexMin = attrs[PP_INDEX];
        details.ppIndexMax = details.ppIndexMin;
    } else if (dimension == DIMENSIONS_PP) {
        details.ppIndexMin = attrs[PP_INDEX];
        details.ppIndexMax = details.ppIndexMin;
    }
    return GetElementContainFormattedRanks(attrs, formattedRanks, details);
}
}
