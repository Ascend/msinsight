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

#include "VLLMParallelStrategyAlgorithm.h"

namespace Dic::Module::Summary {

const std::unordered_map<std::string, std::string> VLLMParallelStrategyAlgorithm::tokenExceptEp = {
    {DP_GROUP, DP_GROUP}, {TP_GROUP, TP_GROUP}, {PP_GROUP, PP_GROUP}};
const std::unordered_map<std::string, std::string> VLLMParallelStrategyAlgorithm::tokenWithEp = {
    {VLLM_EP_GROUP, VLLM_EP_GROUP_NAME}};

VLLMParallelStrategyAlgorithm::VLLMParallelStrategyAlgorithm() {
    commInfoHandlers[DIMENSIONS_TP] =
        std::bind(&VLLMParallelStrategyAlgorithm::ReduceCommTpDimensionDef, this, std::placeholders::_1);
    commInfoHandlers[DIMENSIONS_PP] =
        std::bind(&VLLMParallelStrategyAlgorithm::ReduceCommPpDimensionDef, this, std::placeholders::_1);
}

VLLMParallelStrategyAlgorithm::~VLLMParallelStrategyAlgorithm() = default;

bool VLLMParallelStrategyAlgorithm::UpdateParallelDimension(
    const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig, std::string &err) {
    // vLLM也可复用Base类中的计算逻辑，等价于cpSize恒为1
    CalStrategyConfig(tmpDimension, tmpConfig);
    if (tmpConfig.algorithm == VLLM_TP_PP_DP_EP_ALG) {
        paraOrder = {TP_PARA, PP_PARA, DP_PARA};
        paraOrderWithEp = {TP_PARA, PP_PARA, DP_PARA, EP_PARA};
    } else {
        err = "Failed to update parallel view. Unexpected algorithm for the vLLM.";
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

/**
 * world_size = pp_size * tp_size * dp_size
                = pp_size * tp_size * innerDP_size * externalDP_size （非MOE层）
                = pp_size * ep_size * externalDP_size （MOE层）
	ep_size = tp_size * innerDP_size = (tp_size * dp_size) / externalDP_size
    即innerDP_size = ep_size / tp_size
    externalDP_size = dp_size / innerDP_size
    前端入参已校验, tp_size * dp_size能被ep_size整除，且ep_size能被tp_size整除
 * @param config
 */
void VLLMParallelStrategyAlgorithm::SetStrategyConfig(const ParallelStrategyConfig &config) {
    BaseParallelStrategyAlgorithm::SetStrategyConfig(config);
    // 未开启EP, 直接返回
    if (config.epSize == 1) {
        return;
    }
    // 前端入参已校验, 无除零、整数溢出风险
    innerDpSize = static_cast<uint32_t>(config.epSize / config.tpSize);
    externalDpSize = static_cast<uint32_t>(config.dpSize / innerDpSize);
}

void VLLMParallelStrategyAlgorithm::SetIndicatorAttr() {
    if (dimension == DIMENSIONS_TP) {
        SetTpIndicatorAttr();
    } else if (dimension == DIMENSIONS_PP) {
        SetPpIndicatorAttr();
    } else if (dimension == DIMENSIONS_DP) {
        SetDpIndicatorAttr();
    } else {
        Server::ServerLog::Error("Failed to set indicator attributes for the vLLM. Unexpected dimension.");
    }
}

bool VLLMParallelStrategyAlgorithm::GenerateArrangementByDimension(std::string &err) {
    ClearArrangementData();
    SetIndicatorAttr();
    std::unordered_map<std::string, uint32_t> indexAttributes;
    for (const auto &para : paraOrderWithEp) {
        indexAttributes[para + STR_INDEX] = 0;
    }
    // 与其余算法保持统一，返回cpIndex=0
    indexAttributes[CP_INDEX] = 0;
    // get arrangements
    for (uint32_t index = 0; index < elementSize; index++) {
        GetPerArrangement(index, indexAttributes);
    }
    // get connections
    if (dimension != DIMENSIONS_DP && !GetConnectionsByTokenList(err)) {
        return false;
    }
    return true;
}

std::vector<Connection> VLLMParallelStrategyAlgorithm::GetAllCommunicationGroups(std::string &err) {
    if (allCommunicationGroups.empty() && !GetConnectionsByTokenList(err)) {
        return {};
    }
    return allCommunicationGroups;
}

bool VLLMParallelStrategyAlgorithm::GetConnectionsByTokenList(std::string &err) {
    if (wordSize == 1) {
        err = "Failed to get connections for vLLM. Parallel strategy configs have not been updated yet.";
        SetSummaryError(ErrorCode::GET_ALGORITHM_CONNECTIONS_FAILED);
        return false;
    }
    // 计算并行通信域时无需考虑ep
    if (!GetConnectionsByToken(err, false)) {
        return false;
    }
    // 计算并行通信域时需考虑ep
    if (paraDetailsMap[EP_PARA].isShown && !GetConnectionsByToken(err, true)) {
        return false;
    }
    if (dimension == DIMENSIONS_TP) {
        allCommunicationGroups = data.connections;
    }
    return true;
}

bool VLLMParallelStrategyAlgorithm::GetConnectionsByToken(std::string &err, bool independentEp = false) {
    std::unordered_map<std::string, std::string> tmpToken;
    // EP相关连线信息在折叠视图不返回
    if (independentEp && dimension == DIMENSIONS_TP) {
        tmpToken = tokenWithEp;
    } else if (!independentEp) {
        tmpToken = tokenExceptEp;
        for (auto it = tmpToken.begin(); it != tmpToken.end();) {
            if (!paraDetailsMap[it->first].isShown) {
                it = tmpToken.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto &[token, groupName] : tmpToken) {
        std::vector<std::string> parallelGroups = StringUtil::Split(token, "-");
        allGroupsType ranks{};
        if (independentEp) {
            // 计算并行通信域时需考虑ep
            ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
                parallelGroups, parallelSizeWithEp, updatedOrderWithEp, wordSize);
        } else {
            // 计算并行通信域时无需考虑ep
            ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
                parallelGroups, parallelSize, updatedOrder, wordSize);
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

void VLLMParallelStrategyAlgorithm::GetPerArrangement(
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

void VLLMParallelStrategyAlgorithm::UpdateIndexAttributes(std::unordered_map<std::string, uint32_t> &indexAttributes) {
    // 由低层次到高层次遍历各并行域，依次检查是否需要进位
    std::string curIndex;
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
    // 若对TP进行折叠, 当且仅当DP Size能被EP Size整除时，返回EP框信息
    if (dimension != DIMENSIONS_TP && strategyConfig.dpSize % strategyConfig.epSize != 0) {
        return;
    }
    // 添加epIndex, 前端入参已校验，分母不可能为零, dpSize * tpSize 一定能被epSize整除
    uint32_t epScale = paraDetailsMap[DP_PARA].size * paraDetailsMap[TP_PARA].size / paraDetailsMap[EP_PARA].size;
    indexAttributes[EP_INDEX] =
        (indexAttributes[TP_INDEX] + paraDetailsMap[TP_PARA].size * indexAttributes[DP_INDEX]) / epScale;
}

void VLLMParallelStrategyAlgorithm::UpdateOrderAndParallelSize() {
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
    // 若epSize不为1，处理MOE层，根据源码，可将其拆解为TP-PP-innerDP-externalDP，其中EP通信域等效为TP-innerDP混合通信域
    // updatedOrderWithEp仅影响连线生成，不影响布局
    updatedOrderWithEp = {TP_PARA, PP_PARA};
    // 根据 paraDetailsMap[para].isShown删除size = 1的通信域
    updatedOrderWithEp.erase(std::remove_if(updatedOrderWithEp.begin(), updatedOrderWithEp.end(),
                                 [this](const std::string &group) { return !(paraDetailsMap[group].isShown); }),
        updatedOrderWithEp.end());
    parallelSizeWithEp.clear();
    for (const auto &para : updatedOrderWithEp) {
        parallelSizeWithEp.push_back(paraDetailsMap[para].size);
    }
    updatedOrderWithEp.push_back(INNER_DP_GROUP);
    updatedOrderWithEp.push_back(EXTERNAL_DP_GROUP);
    parallelSizeWithEp.push_back(innerDpSize);
    parallelSizeWithEp.push_back(externalDpSize);
}

bool VLLMParallelStrategyAlgorithm::GetPerformanceIndicatorByDimension(
    const GetPerformanceIndicatorParam &performanceParams,
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic, std::vector<IndicatorDataStruct> &indicatorData,
    std::string &err) {
    if (!(strategyConfig == performanceParams.config)) {
        err = "Failed to get parallelism performance indicator for the vLLM. Unexpected parallel config.";
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
    ReducePpPerformanceForDpLast();
    if (performanceParams.dimension == DIMENSIONS_DP) {
        GetPerformanceResponseDataWithDpDimension(reducePpStatistic, indicatorData);
        return true;
    }
    err = "Failed to get parallelism performance indicator for the vLLM. Unexpected dimension.";
    return false;
}

CommInfoMap VLLMParallelStrategyAlgorithm::GetCommInfoByDimension(
    const CommInfoMap &expandCommInfos, const std::string &dimension) {
    auto res = BaseParallelStrategyAlgorithm::GetCommInfoByDimension(expandCommInfos, dimension);
    if (dimension == DIMENSIONS_TP) {
        return res;
    }
    // 折叠场景暂不展示按ep拆解通信时间结果
    for (auto &item : res) {
        auto &commInfo = item.second;
        commInfo.erase(std::remove_if(commInfo.begin(), commInfo.end(),
                           [](const CommInfoUnderRank &info) { return info.pgName == VLLM_EP_GROUP_NAME; }),
            commInfo.end());
    }
    return res;
}

void VLLMParallelStrategyAlgorithm::CalAdviceInfo(
    const std::string &dimension, std::vector<std::string> &advices, std::vector<IndicatorDataStruct> &indicatorData) {
    BaseParallelStrategyAlgorithm::CalAdviceInfo(dimension, advices, indicatorData);
}
}
