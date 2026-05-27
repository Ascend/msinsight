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

#include "ServerLog.h"
#include "MindSpeedParallelStrategyAlgorithm.h"
namespace Dic::Module::Summary {
const std::unordered_map<std::string, std::string> MindSpeedParallelStrategyAlgorithm::tokenExceptEp = {
    {DP_GROUP, DP_GROUP}, {CP_GROUP, CP_GROUP}, {TP_GROUP, TP_GROUP}, {PP_GROUP, PP_GROUP}, {DP_CP_GROUP, DP_CP_GROUP},
    {MP_GROUP, MP_GROUP_NAME}, {TP_DP_CP_GROUP, TP_DP_CP_GROUP}, {TP_DP_GROUP, TP_DP_GROUP},
    {TP_CP_GROUP, TP_CP_GROUP}};
const std::unordered_map<std::string, std::string> MindSpeedParallelStrategyAlgorithm::tokenWithEp = {
    {EP_GROUP, EP_GROUP_NAME}, {TP_EP_GROUP, TP_EP_GROUP_NAME}, {DP_MODULO_EP_GROUP, DP_MODULO_EP_GROUP_NAME},
    {DP_CP_MODULO_EP_GROUP, DP_CP_MODULO_EP_GROUP_NAME}, {MP_EP_GROUP, MP_EP_GROUP_NAME}};
const std::unordered_map<std::string, std::string> MindSpeedParallelStrategyAlgorithm::tokenOfTp2dNd1 = {
    {TP_GROUP_FOR_ND1_DIM1, TP_GROUP_FOR_ND1_DIM1_NAME}, {TP_GROUP_FOR_ND1_DIM2, TP_GROUP_FOR_ND1_DIM2_NAME}};
const std::unordered_map<std::string, std::string> MindSpeedParallelStrategyAlgorithm::tokenOfTp2dNd2 = {
    {TP_GROUP_FOR_ND2_DIM1, TP_GROUP_FOR_ND2_DIM1_NAME}, {TP_GROUP_FOR_ND2_DIM2, TP_GROUP_FOR_ND2_DIM2_NAME}};
MindSpeedParallelStrategyAlgorithm::MindSpeedParallelStrategyAlgorithm() {
    commInfoHandlers[DIMENSIONS_TP] =
        std::bind(&MindSpeedParallelStrategyAlgorithm::ReduceCommTpDimensionDef, this, std::placeholders::_1);
    commInfoHandlers[DIMENSIONS_CP] =
        std::bind(&MindSpeedParallelStrategyAlgorithm::ReduceCommCpDimensionDef, this, std::placeholders::_1);
    commInfoHandlers[DIMENSIONS_PP] =
        std::bind(&MindSpeedParallelStrategyAlgorithm::ReduceCommPpDimensionDef, this, std::placeholders::_1);
}

MindSpeedParallelStrategyAlgorithm::~MindSpeedParallelStrategyAlgorithm() = default;

void MindSpeedParallelStrategyAlgorithm::SetStrategyConfig(const ParallelStrategyConfig &config) {
    BaseParallelStrategyAlgorithm::SetStrategyConfig(config);
    // 前端入参已校验
    if (config.configForMindSpeed.cpAlgo == MINDSPEED_HYBIRD_CP_ALG ||
        config.configForMindSpeed.cpAlgo == MINDSPEED_HYBIRD_ADAPTIVE_CP_ALG) {
        useHybridCp = true;
        ulyssesDegree = static_cast<uint32_t>(config.configForMindSpeed.ulyssesDegree);
        ringDegree = config.cpSize / ulyssesDegree;
    }
    if (!config.configForMindSpeed.useTp2D && config.configForMindSpeed.cpAlgo == MINDSPEED_HYBIRD_CP_ALG) {
        winSize = static_cast<uint32_t>(config.configForMindSpeed.winSize);
        interSize = ringDegree / winSize;
    }
    if (!config.configForMindSpeed.useTp2D && config.configForMindSpeed.cpAlgo == MINDSPEED_MEGATRON_CP_ALG) {
        winSize = static_cast<uint32_t>(config.configForMindSpeed.winSize);
        interSize = config.cpSize / winSize;
    }
    if (config.configForMindSpeed.useTp2D) {
        nd1dim1 = static_cast<uint32_t>(config.configForMindSpeed.nd1dim1);
        nd1dim2 = config.tpSize / nd1dim1;
        nd2dim1 = static_cast<uint32_t>(config.configForMindSpeed.nd2dim1);
        nd2dim2 = config.tpSize / nd2dim1;
    }
}

void MindSpeedParallelStrategyAlgorithm::UpdateOrderAndParallelSize() {
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
    // 仅影响连线生成，不影响布局
    // 把cpSize置为1，dpSize * cpSize作为dpSize，生成ep相关通信域
    updatedOrderWithEp = paraOrderWithEp;
    updatedOrderWithEp.erase(std::remove_if(updatedOrderWithEp.begin(), updatedOrderWithEp.end(),
                                 [this](const std::string &group) { return !(paraDetailsMap[group].isShown); }),
        updatedOrderWithEp.end());
    parallelSizeWithEp.clear();
    uint32_t cpSize = paraDetailsMap[CP_PARA].size; // 原cpSize
    for (const auto &para : updatedOrderWithEp) {
        if (para == CP_PARA) {
            parallelSizeWithEp.push_back(cpSizeWithEp); // 把cpSize置为1
        } else if (para == DP_PARA) {
            // 前端入参已校验, 原dpSize * 原cpSize作为dpSize
            parallelSizeWithEp.push_back(paraDetailsMap[DP_PARA].size * cpSize / paraDetailsMap[EP_PARA].size);
        } else {
            parallelSizeWithEp.push_back(paraDetailsMap[para].size);
        }
    }
}

bool MindSpeedParallelStrategyAlgorithm::UpdateParallelDimension(
    const std::string &tmpDimension, const ParallelStrategyConfig &tmpConfig, std::string &err) {
    CalStrategyConfig(tmpDimension, tmpConfig);
    if (tmpConfig.algorithm == MINDSPEED_TP_CP_EP_DP_PP_ALG) {
        paraOrder = {TP_PARA, CP_PARA, DP_PARA, PP_PARA};
    } else {
        err = "Failed to update parallel view. Unexpected algorithm for MindSpeed.";
        SetSummaryError(ErrorCode::UPDATE_PARALLEL_VIEW_FAILED);
        return false;
    }
    paraOrderWithEp = paraOrder;
    paraOrderWithEp.insert(paraOrderWithEp.begin() + epPosPpLast, EP_PARA);

    bool res = UpdateShowMap(err);
    if (res) {
        // 根据 paraDetailsMap[para].isShown 删除不存在的通信域
        UpdateOrderAndParallelSize();
        // 计算当前元素总数
        UpdateElementSize();
    }
    return res;
}

bool MindSpeedParallelStrategyAlgorithm::GenerateArrangementByDimension(std::string &err) {
    ClearArrangementData();
    SetIndicatorAttr();
    std::unordered_map<std::string, uint32_t> indexAttributes;
    for (const auto &para : paraOrderWithEp) {
        indexAttributes[para + STR_INDEX] = 0;
    }
    // get arrangements
    for (uint32_t index = 0; index < elementSize; index++) {
        GetPerArrangement(index, indexAttributes);
    }
    if (dimension != DIMENSIONS_DP && !GetConnectionsByTokenList(err)) {
        return false;
    }
    return true;
}

void MindSpeedParallelStrategyAlgorithm::GetPerArrangement(
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

void MindSpeedParallelStrategyAlgorithm::UpdateIndexAttributes(
    std::unordered_map<std::string, uint32_t> &indexAttributes) {
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
    // 若对CP进行折叠, 当且仅当DP Size能被EP Size整除时，返回EP框信息
    if (dimension != DIMENSIONS_TP && dimension != DIMENSIONS_CP &&
        strategyConfig.dpSize % strategyConfig.epSize != 0) {
        return;
    }
    // 添加epIndex, 前端入参已校验，分母不可能为零, dpSize * cpSize 一定能被epSize整除
    uint32_t epScale = paraDetailsMap[DP_PARA].size * paraDetailsMap[CP_PARA].size / paraDetailsMap[EP_PARA].size;
    indexAttributes[EP_INDEX] =
        (indexAttributes[CP_INDEX] + paraDetailsMap[CP_PARA].size * indexAttributes[DP_INDEX]) / epScale;
}

bool MindSpeedParallelStrategyAlgorithm::ReplaceParallelGroup(const std::string &para, std::vector<std::string> &order,
    std::vector<uint32_t> &paraSize, const std::vector<std::string> &orderList, const std::vector<uint32_t> &sizeList) {
    if (order.size() != paraSize.size()) {
        Server::ServerLog::Error("Failed to replace parallel group: ", para, ". Unexpected order or parallel size.");
        return false;
    }
    for (size_t i = 0; i < order.size(); i++) {
        if (order[i] == para) {
            order.erase(order.begin() + i);
            order.insert(order.begin() + i, orderList.begin(), orderList.end());
            paraSize.erase(paraSize.begin() + i);
            paraSize.insert(paraSize.begin() + i, sizeList.begin(), sizeList.end());
            break;
        }
    }
    return true;
}

bool MindSpeedParallelStrategyAlgorithm::GetConnectionsForCpUlyssesAndRing(
    std::vector<std::string> &updatedOrderForCp, std::vector<uint32_t> &updatedParallelSizeForCp, std::string &err) {
    allGroupsType ranks{};
    // cp -> cp_ulysses and cp_ring
    ReplaceParallelGroup(
        CP_PARA, updatedOrderForCp, updatedParallelSizeForCp, ULYSSES_RING_TOKEN, {ulyssesDegree, ringDegree});
    ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
        {CP_ULYSSES_GROUP}, updatedParallelSizeForCp, updatedOrderForCp, wordSize);
    if (ranks.empty()) {
        err = "Failed to get connections by token list. Group name: " + CP_ULYSSES_GROUP_NAME;
        return false;
    }
    for (const auto &rank : ranks) {
        data.connections.emplace_back(CP_ULYSSES_GROUP_NAME, rank, std::vector<std::string>{});
    }
    ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
        {CP_RING_GROUP}, updatedParallelSizeForCp, updatedOrderForCp, wordSize);
    if (ranks.empty()) {
        err = "Failed to get connections by token list. Group name: " + CP_RING_GROUP_NAME;
        return false;
    }
    for (const auto &rank : ranks) {
        data.connections.emplace_back(CP_RING_GROUP_NAME, rank, std::vector<std::string>{});
    }
    return true;
}

bool MindSpeedParallelStrategyAlgorithm::GetConnectionsForCpDoubleRing(
    std::vector<std::string> &updatedOrderForCp, std::vector<uint32_t> &updatedParallelSizeForCp, std::string &err) {
    allGroupsType ranks{};
    if (strategyConfig.configForMindSpeed.cpAlgo == MINDSPEED_HYBIRD_CP_ALG) {
        // cp_ring -> win and cp_ring_intra
        ReplaceParallelGroup(
            CP_RING_GROUP, updatedOrderForCp, updatedParallelSizeForCp, WIN_INTRA_TOKEN, {winSize, interSize});
    } else if (strategyConfig.configForMindSpeed.cpAlgo == MINDSPEED_MEGATRON_CP_ALG) {
        // cp -> win and cp_ring_intra
        ReplaceParallelGroup(
            CP_PARA, updatedOrderForCp, updatedParallelSizeForCp, WIN_INTRA_TOKEN, {winSize, interSize});
    }
    ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
        {CP_WIN_GROUP}, updatedParallelSizeForCp, updatedOrderForCp, wordSize);
    if (ranks.empty()) {
        err = "Failed to get connections by token list. Group name: " + CP_WIN_GROUP_NAME +
            "Cp algorithm: " + strategyConfig.configForMindSpeed.cpAlgo;
        return false;
    }
    for (const auto &rank : ranks) {
        data.connections.emplace_back(CP_WIN_GROUP_NAME, rank, std::vector<std::string>{});
    }
    return true;
}

bool MindSpeedParallelStrategyAlgorithm::GetConnectionsByMegatronToken(std::string &err, bool independentEp = false) {
    std::unordered_map<std::string, std::string> tmpToken = independentEp ? tokenWithEp : tokenExceptEp;
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
            err = "Failed to get connections by token list for MindSpeed. Group name: " + groupName;
            return false;
        }
        for (const auto &rank : ranks) {
            data.connections.emplace_back(groupName, rank, std::vector<std::string>{});
        }
    }
    return true;
}

bool MindSpeedParallelStrategyAlgorithm::GetConnectionsForTp2d(std::string &err,
    const std::vector<std::string> &tokenList, const std::vector<uint32_t> &sizeList,
    const std::unordered_map<std::string, std::string> &tokenMap) {
    allGroupsType ranks{};
    std::vector<std::string> updatedOrderForTp2d = updatedOrder;
    std::vector<uint32_t> updatedParallelSizeForTp2d = parallelSize;
    ReplaceParallelGroup(TP_PARA, updatedOrderForTp2d, updatedParallelSizeForTp2d, tokenList, sizeList);
    for (const auto &[token, groupName] : tokenMap) {
        ranks = ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(
            {token}, updatedParallelSizeForTp2d, updatedOrderForTp2d, wordSize);
        if (ranks.empty()) {
            err = "Failed to get connections for tp2d. Group name: " + groupName;
            return false;
        }
        for (const auto &item : ranks) {
            data.connections.emplace_back(groupName, item, std::vector<std::string>{});
        }
    }
    return true;
}

bool MindSpeedParallelStrategyAlgorithm::GetConnectionsByTokenList(std::string &err) {
    if (wordSize == 1) {
        err = "Failed to get connections for MindSpeed. Parallel strategy configs have not been updated yet.";
        SetSummaryError(ErrorCode::GET_ALGORITHM_CONNECTIONS_FAILED);
        return false;
    }
    // 先把ep置为1，生成与ep无关通信域
    if (!GetConnectionsByMegatronToken(err)) {
        return false;
    }
    // 特殊cp域
    if (paraDetailsMap[CP_PARA].isShown) {
        std::vector<std::string> updatedOrderForCp = updatedOrder;
        std::vector<uint32_t> updatedParallelSizeForCp = parallelSize;
        if (useHybridCp && !GetConnectionsForCpUlyssesAndRing(updatedOrderForCp, updatedParallelSizeForCp, err)) {
            return false;
        }
        if (!strategyConfig.configForMindSpeed.useTp2D &&
            (strategyConfig.configForMindSpeed.cpAlgo == MINDSPEED_HYBIRD_CP_ALG ||
                strategyConfig.configForMindSpeed.cpAlgo == MINDSPEED_MEGATRON_CP_ALG) &&
            !GetConnectionsForCpDoubleRing(updatedOrderForCp, updatedParallelSizeForCp, err)) {
            return false;
        }
    }
    // tp2d
    if (paraDetailsMap[TP_PARA].isShown && strategyConfig.configForMindSpeed.useTp2D) {
        if (!GetConnectionsForTp2d(err, TP2D_ND1_TOKEN, {nd1dim1, nd1dim2}, tokenOfTp2dNd1) ||
            !GetConnectionsForTp2d(err, TP2D_ND2_TOKEN, {nd2dim1, nd2dim2}, tokenOfTp2dNd2)) {
            return false;
        }
    }
    // 然后把cp置为1，dpSize * cpSize作为dpSize，生成ep相关通信域, EP相关连线信息在CP被折叠视图不返回
    if (paraDetailsMap[EP_PARA].isShown && (dimension == DIMENSIONS_TP || dimension == DIMENSIONS_CP) &&
        !GetConnectionsByMegatronToken(err, true)) {
        return false;
    }
    if (dimension == DIMENSIONS_TP) {
        allCommunicationGroups = data.connections;
    }
    return true;
}

void MindSpeedParallelStrategyAlgorithm::SetIndicatorAttr() {
    if (dimension == DIMENSIONS_TP) {
        SetTpIndicatorAttr();
    } else if (dimension == DIMENSIONS_CP) {
        SetCpIndicatorAttr();
    } else if (dimension == DIMENSIONS_PP) {
        SetPpIndicatorAttr();
    } else {
        SetDpIndicatorAttr();
    }
}

std::vector<Connection> MindSpeedParallelStrategyAlgorithm::GetAllCommunicationGroups(std::string &err) {
    if (allCommunicationGroups.empty() && !GetConnectionsByTokenList(err)) {
        return {};
    }
    return allCommunicationGroups;
}

void MindSpeedParallelStrategyAlgorithm::CalAdviceInfo(
    const std::string &dimension, std::vector<std::string> &advices, std::vector<IndicatorDataStruct> &indicatorData) {
    BaseParallelStrategyAlgorithm::CalAdviceInfo(dimension, advices, indicatorData);
}

bool MindSpeedParallelStrategyAlgorithm::GetPerformanceIndicatorByDimension(
    const GetPerformanceIndicatorParam &performanceParams,
    const std::unordered_map<std::uint32_t, StepStatistic> &statistic, std::vector<IndicatorDataStruct> &indicatorData,
    std::string &err) {
    if (!(strategyConfig == performanceParams.config)) {
        err = "Failed to get parallelism performance indicator for MindSpeed. Unexpected parallel config.";
        return false;
    }
    tpSize = strategyConfig.tpSize;
    wordSize = strategyConfig.tpSize * strategyConfig.ppSize * strategyConfig.cpSize * strategyConfig.dpSize;
    if (performanceParams.dimension == DIMENSIONS_TP) {
        CalculatePerformanceDataWithTpDimension(statistic, indicatorData);
        return true;
    }
    // 折叠TP
    ReduceTpPerformance(statistic);
    if (performanceParams.dimension == DIMENSIONS_CP) {
        // DP+PP+CP视图时，折叠TP，计算最大值、最小值、极差等统计值
        CalculatePerformanceDataWithCpDimension(indicatorData);
        return true;
    }
    // 折叠CP
    ReduceCpPerformance();
    if (performanceParams.dimension == DIMENSIONS_PP) {
        CalculatePerformanceDataWithPpDimension(indicatorData);
        return true;
    }
    // 折叠PP
    ReducePpPerformanceForPpLast();
    if (performanceParams.dimension == DIMENSIONS_DP) {
        GetPerformanceResponseDataWithDpDimension(reducePpStatistic, indicatorData);
        return true;
    }
    err = "Failed to get parallelism performance indicator for MindSpeed. Unexpected dimension.";
    return false;
}
}
