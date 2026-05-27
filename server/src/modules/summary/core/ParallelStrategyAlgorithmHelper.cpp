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
#include "StringUtil.h"
#include "ParallelStrategyAlgorithmHelper.h"
#include "SummaryErrorManager.h"

namespace Dic::Module::Summary {
/**
  * 生成当前rank groups
  * @param token 涉及rank groups的并行域，例如['tp', 'dp']
  * @param parallelSize parallelSize 并行策略参数，例如对于order: ['tp','cp', dp','pp'], 有parallelSize: [2, 2, 3, 4]
  * @param order 全量并行通信域顺序，包含当前集群涉及的所有并行策略，例如['tp','cp', dp','pp']
  * @param worldSize 总卡数，例如对于parallelSize: [2, 2, 3, 4], 有worldSize = 2*2*3*4 = 48
  * @return
  */
allGroupsType ParallelStrategyAlgorithmHelper::GetAllGroupsRanksByToken(const std::vector<std::string> &token,
    const std::vector<uint32_t> &parallelSize, const std::vector<std::string> &order, uint32_t worldSize) {
    std::vector<bool> mask = GetMask(order, token);
    if (mask.empty()) {
        // GetMask已有日志报错
        return {};
    }
    return GenerateMaskedOrthogonalRankGroups(parallelSize, mask, worldSize);
}

/**
 * 按照order的顺序，生成当前rank groups的掩模(涉及rank groups的并行域置为true)
 * @param order 全量并行通信域顺序，例如['tp', 'cp', 'ep', 'pp', 'dp']
 * @param token 涉及rank groups的并行域，例如['tp', 'dp']
 * @return 当前rank groups的掩模, 例如[true, false, false, false, true]
 */
std::vector<bool> ParallelStrategyAlgorithmHelper::GetMask(
    const std::vector<std::string> &order, const std::vector<std::string> &token) {
    std::vector<bool> mask(order.size(), false);
    if (token.size() > order.size()) {
        Server::ServerLog::Error("Failed to get mask for generate orthogonal rank groups. Unexpected order or token.");
        SetSummaryError(ErrorCode::GET_MASK_FAILED);
        return {};
    }
    // 按照order顺序，将被计入的通信域置为true
    for (size_t i = 0; i < mask.size(); i++) {
        mask[i] = std::find(token.begin(), token.end(), order[i]) != token.end();
    }
    return mask;
}

std::vector<uint32_t> ParallelStrategyAlgorithmHelper::prefixProduct(
    const std::vector<uint32_t> &sizeList, uint32_t init = 1) {
    std::vector<uint32_t> result(sizeList.size() + 1, init);
    for (size_t i = 0; i < sizeList.size(); ++i) {
        // 并行策略参数，前端入参已校验，无整数溢出风险
        result[i + 1] = result[i] * sizeList[i];
    }
    return result;
}

uint32_t ParallelStrategyAlgorithmHelper::innerProduct(const std::vector<uint32_t> &x, const std::vector<uint32_t> &y) {
    if (x.size() != y.size()) {
        Server::ServerLog::Error("Failed to get inner product for generate orthogonal rank groups. "
                                 "Input vectors are not of the same length.");
        return 0;
    }
    uint32_t result = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        // 并行策略参数，前端入参已校验，无整数溢出风险
        result += x[i] * y[i];
    }
    return result;
}

/**
 * 若有index = innerProduct(idx, stride), 已知index, stride, 求解idx
 * @param index
 * @param shape 令stride = prefixProduct(shape)
 * @return 求解idx
 */
std::vector<uint32_t> ParallelStrategyAlgorithmHelper::Decompose(uint32_t index, const std::vector<uint32_t> &shape) {
    std::vector<uint32_t> stride = prefixProduct(shape);
    std::vector<uint32_t> idx(shape.size());
    for (size_t i = 0; i < shape.size(); i++) {
        // 已知数组stride长度为shape长度+1
        idx[i] = (index / stride[i]) % shape[i];
    }
    return idx;
}

/**
 * 参考Megatron-LM源码逻辑, 正交生成并行通信组
 * @param parallelSize 按照order顺序的并行策略参数，例如order: ['tp','cp', dp','pp'], parallelSize: [2, 2, 3, 4]
 * @param mask 当前rank groups的掩模, 例如[true, false, true, false]
 * @return 正交并行通信组, 例如对于token['tp', 'dp'],
 *         ranks = [[0, 1, 4, 5, 8, 9], [2, 3, 6, 7, 10, 11], ..., [38, 39, 42, 43, 46, 47]]
 */
allGroupsType ParallelStrategyAlgorithmHelper::GenerateMaskedOrthogonalRankGroups(
    const std::vector<uint32_t> &parallelSize, const std::vector<bool> &mask, uint32_t wordSize) {
    if (parallelSize.size() != mask.size()) {
        Server::ServerLog::Error("Failed to generate orthogonal rank groups. Unexpected parallel size or "
                                 "rank groups mask.");
        SetSummaryError(ErrorCode::GENERATE_ORTHOGONAL_FAILED);
        return {};
    }
    std::vector<uint32_t> maskedShape{};
    std::vector<uint32_t> unmaskedShape{};
    for (size_t i = 0; i < mask.size(); ++i) {
        if (mask[i]) {
            maskedShape.push_back(parallelSize[i]);
        } else {
            unmaskedShape.push_back(parallelSize[i]);
        }
    }

    std::vector<uint32_t> globalStride = prefixProduct(parallelSize);
    std::vector<uint32_t> maskedStride{};
    std::vector<uint32_t> unmaskedStride{};
    // 已知globalStride.size() = mask.size() + 1
    for (size_t i = 0; i < mask.size(); ++i) {
        if (mask[i]) {
            maskedStride.push_back(globalStride[i]);
        } else {
            unmaskedStride.push_back(globalStride[i]);
        }
    }

    uint32_t groupSize = prefixProduct(maskedShape).back(); // 单个正交并行通信域内 卡的个数
    if (wordSize % groupSize != 0) {
        Server::ServerLog::Error("Failed to generate orthogonal rank groups. Word size is not divisible by group "
                                 "size.");
        SetSummaryError(ErrorCode::GENERATE_ORTHOGONAL_FAILED);
        return {};
    }
    uint32_t numOfGroup = wordSize / groupSize;

    allGroupsType ranks;
    for (size_t groupIndex = 0; groupIndex < numOfGroup; groupIndex++) {
        std::vector<uint32_t> decomposedGroupIdx = Decompose(groupIndex, unmaskedShape);
        std::vector<uint32_t> rank; // 单个正交并行通信域内 卡序号
        for (size_t rankInGroup = 0; rankInGroup < groupSize; ++rankInGroup) {
            std::vector<uint32_t> decomposedRankIdx = Decompose(rankInGroup, maskedShape);
            // 以并行顺序'tp-cp-dp-pp'为例,
            // globalRankIndex 等于 tpIndex + cpIndex * tpSize + dpIndex * tpCpSize + ppIndex * tpCpDpSize
            uint32_t globalRankIndex =
                innerProduct(decomposedRankIdx, maskedStride) + innerProduct(decomposedGroupIdx, unmaskedStride);
            rank.push_back(globalRankIndex);
        }
        ranks.push_back(rank);
    }
    return ranks;
}
}
