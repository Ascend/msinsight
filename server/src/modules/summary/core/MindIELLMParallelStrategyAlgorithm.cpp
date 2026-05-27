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

#include "MindIELLMParallelStrategyAlgorithm.h"

namespace Dic::Module::Summary {

const std::unordered_map<std::string, std::string> MindIELLMParallelStrategyAlgorithm::tokenExceptEp = {
    {DP_GROUP, DP_GROUP}, {TP_GROUP, TP_GROUP}, {PP_GROUP, PP_GROUP}};
const std::unordered_map<std::string, std::string> MindIELLMParallelStrategyAlgorithm::tokenWithEp = {
    {MOE_TP_GROUP, MOE_TP_GROUP_NAME}, {EP_GROUP, EP_GROUP_NAME}};

MindIELLMParallelStrategyAlgorithm::MindIELLMParallelStrategyAlgorithm() {
    commInfoHandlers[DIMENSIONS_TP] =
        std::bind(&MindIELLMParallelStrategyAlgorithm::ReduceCommTpDimensionDef, this, std::placeholders::_1);
    commInfoHandlers[DIMENSIONS_PP] =
        std::bind(&MindIELLMParallelStrategyAlgorithm::ReduceCommPpDimensionDef, this, std::placeholders::_1);
}

MindIELLMParallelStrategyAlgorithm::~MindIELLMParallelStrategyAlgorithm() = default;

bool MindIELLMParallelStrategyAlgorithm::UpdateParallelDimension(
    const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig, std::string &err) {
    // MindIE-LLM也可复用Base类中的计算逻辑，等价于cpSize恒为1
    CalStrategyConfig(tmpDimension, tmpConfig);
    if (tmpConfig.algorithm == MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG) {
        paraOrder = {TP_PARA, DP_PARA, PP_PARA};
        paraOrderWithEp = {TP_PARA, DP_PARA, PP_PARA, MOE_TP_PARA, EP_PARA};
    } else {
        err = "Failed to update parallel view. Unexpected algorithm for the MindIE-LLM.";
        SetSummaryError(ErrorCode::UPDATE_PARALLEL_VIEW_FAILED);
        return false;
    }
    bool res = UpdateShowMap(err);
    if (res) {
        // 根据 paraDetailsMap[para].isShown 删除size = 1的通信域
        UpdateOrderAndParallelSize();
        // 计算当前元素总数
        UpdateElementSize();
    }
    return res;
}

void MindIELLMParallelStrategyAlgorithm::UpdateOrderAndParallelSize() {
    // 根据 paraDetailsMap[para].isShown 删除size = 1的通信域
    updatedOrder = paraOrder;
    updatedOrder.erase(std::remove_if(updatedOrder.begin(), updatedOrder.end(),
                           [this](const std::string &group) { return !(paraDetailsMap[group].isShown); }),
        updatedOrder.end());
    parallelSize.clear();
    for (const auto &para : updatedOrder) {
        parallelSize.push_back(paraDetailsMap[para].size);
    }
    if (!paraDetailsMap[EP_PARA].isShown) {
        return;
    }
    // 若epSize不为1，处理moe_tp/ep/pp, 根据 paraDetailsMap[para].isShown删除size = 1的通信域
    // 此处不应将TP/DP纳入，以免影响后续连线生成
    updatedOrderWithEp = {MOE_TP_PARA, EP_PARA, PP_PARA};
    updatedOrderWithEp.erase(std::remove_if(updatedOrderWithEp.begin(), updatedOrderWithEp.end(),
                                 [this](const std::string &group) { return !(paraDetailsMap[group].isShown); }),
        updatedOrderWithEp.end());
    parallelSizeWithEp.clear();
    for (const auto &para : updatedOrderWithEp) {
        parallelSizeWithEp.push_back(paraDetailsMap[para].size);
    }
}

void MindIELLMParallelStrategyAlgorithm::SetIndicatorAttr() {
    if (dimension == DIMENSIONS_TP) {
        SetTpIndicatorAttr();
    } else if (dimension == DIMENSIONS_PP) {
        SetPpIndicatorAttr();
    } else if (dimension == DIMENSIONS_DP) {
        SetDpIndicatorAttr();
    } else {
        Server::ServerLog::Error("Failed to set indicator attributes for the MindIE-LLM. Unexpected dimension.");
    }
}

void MindIELLMParallelStrategyAlgorithm::UpdateIndexAttributes(
    std::unordered_map<std::string, uint32_t> &indexAttributes) {
    // 由低层次到高层次遍历各并行域，依次检查是否需要进位
    std::string curIndex;
    // 先更新TP DP PP Index
    for (const auto &curPara : paraOrder) {
        // 未达到size-1，无需进位
        if (indexAttributes[curPara + STR_INDEX] < paraDetailsMap[curPara].size - 1) {
            indexAttributes[curPara + STR_INDEX]++;
            break;
        }
        // 达到size-1，当前位置0，检查下一位
        if (curPara != paraOrder.back()) {
            indexAttributes[curPara + STR_INDEX] = 0;
        }
    }
    // 再更新moeTpIndex和epIndex, 目前仅支持全展开视图下返回
    if (dimension != DIMENSIONS_TP) {
        return;
    }
    static std::vector<std::string> updateIndexListForMoe = {MOE_TP_PARA, EP_PARA};
    for (const auto &curPara : updateIndexListForMoe) {
        // 未达到size-1，无需进位
        if (indexAttributes[curPara + STR_INDEX] < paraDetailsMap[curPara].size - 1) {
            indexAttributes[curPara + STR_INDEX]++;
            break;
        }
        // 达到size-1，当前位置0，检查下一位
        indexAttributes[curPara + STR_INDEX] = 0;
    }
}

void MindIELLMParallelStrategyAlgorithm::GetPerArrangement(
    uint32_t index, std::unordered_map<std::string, uint32_t> &indexAttributes) {
    Element element;
    element.index = index;
    if (index != 0) {
        UpdateIndexAttributes(indexAttributes);
    }
    element.indexAttributes = indexAttributes;
    element.name = GetElementName(indexAttributes);
    element.position = GetElementPosition(indexAttributes);
    element.ranks = GetElementContainRanks(index, indexAttributes, element.formattedRanks);
    data.arrangements.push_back(element);
}

bool MindIELLMParallelStrategyAlgorithm::GetConnectionsByToken(std::string &err, ParaMode mode) {
    std::unordered_map<std::string, std::string> tmpToken;
    if (mode == ParaMode::TP_DP_PP) {
        tmpToken = tokenExceptEp;
    } else {
        tmpToken = tokenWithEp;
    }
    // 按paraDetailsMap[group].isShown去除不存在的通信域
    for (const auto &[token, groupName] : tmpToken) {
        bool hasTokenGroup = true;
        std::vector<std::string> parallelGroups = StringUtil::Split(token, "-");
        for (const auto &group : parallelGroups) {
            if (!paraDetailsMap[group].isShown) {
                // 不存在当前含有当前并行域的通信组
                hasTokenGroup = false;
                break;
            }
        }
        if (!hasTokenGroup) {
            continue;
        }
        allGroupsType ranks{};
        if (mode == ParaMode::TP_DP_PP) {
            // 计算tp/dp/pp相关通信域
            ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
                parallelGroups, parallelSize, updatedOrder, wordSize);
        } else {
            // 计算moeTp/ep/pp相关通信域, pp参与连线计算，但不重复添加
            ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
                parallelGroups, parallelSizeWithEp, updatedOrderWithEp, wordSize);
        }
        if (ranks.empty()) {
            err = "Failed to get connections by token list for Megatron. Group name: " + groupName;
            return false;
        }
        for (const auto &rank : ranks) {
            data.connections.emplace_back(groupName, rank, std::vector<std::string>{});
        }
    }
    return true;
}

bool MindIELLMParallelStrategyAlgorithm::GetConnectionsByTokenList(std::string &err) {
    if (wordSize == 1) {
        err = "Failed to get connections for the MindIE-LLM. Parallel strategy configs have not been updated yet.";
        SetSummaryError(ErrorCode::GET_ALGORITHM_CONNECTIONS_FAILED);
        return false;
    }
    // 计算并行通信域, 先处理tp/dp/pp
    if (!GetConnectionsByToken(err, ParaMode::TP_DP_PP)) {
        return false;
    }
    // 计算并行通信域, 处理moeTp/ep/pp, pp connections不重复添加, 目前仅支持全展开视图下生成moe相关连线
    if (dimension == DIMENSIONS_TP && paraDetailsMap[EP_PARA].isShown &&
        !GetConnectionsByToken(err, ParaMode::MOE_TP_EP_PP)) {
        return false;
    }
    if (dimension == DIMENSIONS_TP) {
        allCommunicationGroups = data.connections;
    }
    return true;
}

bool MindIELLMParallelStrategyAlgorithm::GenerateArrangementByDimension(std::string &err) {
    ClearArrangementData();
    SetIndicatorAttr();
    // 记录并行域坐标，例如dpIndex、tpIndex、ppIndex等
    std::unordered_map<std::string, uint32_t> indexAttributes;

    // tp/dp/pp
    for (const auto &para : paraOrder) {
        indexAttributes[para + STR_INDEX] = 0;
    }
    // moeTp/ep
    indexAttributes[MOE_TP_INDEX] = 0;
    indexAttributes[EP_INDEX] = 0;
    // 与其余算法保持统一，返回cpIndex=0
    indexAttributes[CP_INDEX] = 0;
    // get arrangements
    for (uint32_t index = 0; index < elementSize; index++) {
        GetPerArrangement(index, indexAttributes);
    }
    // get connections
    if (!GetConnectionsByTokenList(err)) {
        return false;
    }
    return true;
}

bool MindIELLMParallelStrategyAlgorithm::GetPerformanceIndicatorByDimension(
    const GetPerformanceIndicatorParam &performanceParams,
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic, std::vector<IndicatorDataStruct> &indicatorData,
    std::string &err) {
    if (!(strategyConfig == performanceParams.config)) {
        err = "Failed to get parallelism performance indicator for the MindIE-LLM. Unexpected parallel config.";
        return false;
    }
    tpSize = strategyConfig.tpSize;
    wordSize = strategyConfig.tpSize * strategyConfig.ppSize * strategyConfig.dpSize;
    if (performanceParams.dimension == DIMENSIONS_TP) {
        CalculatePerformanceDataWithTpDimension(statistic, indicatorData);
        return true;
    }
    // 折叠TP
    ReduceTpPerformance(statistic);
    if (performanceParams.dimension == DIMENSIONS_PP) {
        // DP+PP视图时，折叠TP，计算最大值、最小值、极差等统计值, 此处因CP Size必为1，复用以往CP维度逻辑
        CalculatePerformanceDataWithCpDimension(indicatorData);
        return true;
    }
    // CP恒为1，无需折叠，但需把reduceTpMax reduceTpMin传递给reduceCpMax reduceCpMin
    ReduceCpPerformance();
    // 折叠PP
    ReducePpPerformanceForPpLast();
    if (performanceParams.dimension == DIMENSIONS_DP) {
        GetPerformanceResponseDataWithDpDimension(reducePpStatistic, indicatorData);
        return true;
    }
    err = "Failed to get parallelism performance indicator for the MindIE-LLM. Unexpected dimension.";
    return false;
}

void MindIELLMParallelStrategyAlgorithm::CalAdviceInfo(
    const std::string &dimension, std::vector<std::string> &advices, std::vector<IndicatorDataStruct> &indicatorData) {
    BaseParallelStrategyAlgorithm::CalAdviceInfo(dimension, advices, indicatorData);
}

std::vector<Connection> MindIELLMParallelStrategyAlgorithm::GetAllCommunicationGroups(std::string &err) {
    if (allCommunicationGroups.empty() && !GetConnectionsByTokenList(err)) {
        return {};
    }
    return allCommunicationGroups;
}

CommInfoMap MindIELLMParallelStrategyAlgorithm::GetCommInfoByDimension(
    const CommInfoMap &expandCommInfos, const std::string &dimension) {
    auto res = BaseParallelStrategyAlgorithm::GetCommInfoByDimension(expandCommInfos, dimension);
    if (dimension == DIMENSIONS_TP) {
        return res;
    }
    // 折叠场景暂不展示按moeTP/ep拆解通信时间结果
    for (auto &item : res) {
        auto &commInfo = item.second;
        commInfo.erase(std::remove_if(commInfo.begin(), commInfo.end(),
                           [](const CommInfoUnderRank &info) {
                               return (info.pgName == MOE_TP_GROUP_NAME) || (info.pgName == EP_GROUP_NAME);
                           }),
            commInfo.end());
    }
    return res;
}
}
