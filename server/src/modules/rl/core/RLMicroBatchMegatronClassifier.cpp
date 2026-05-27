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

#include "RLMicroBatchMegatronClassifier.h"
#include "RLMstxConfigManager.h"
#include "ServerLog.h"
#include "DataBaseManager.h"
#include "RenderEngine.h"

namespace Dic::Module::RL {
using namespace Server;
using namespace FullDb;
std::vector<Protocol::RLPipelineNode> RLMicroBatchMegatronClassifier::MicroBatchClassifier(
    std::vector<RLPipelineNode> &nodes) {
    if (nodes.empty()) {
        return {};
    }
    Clear();
    std::vector<Protocol::RLPipelineNode> res;
    for (auto &node : nodes) {
        switch (state) {
        case Init: {
            // 0状态接收一个正向算子，进入状态1，且将当前算子填入结果中
            InitStateProcess(res, node);
            break;
        }
        case FP: {
            // 1状态接收一个正向算子，仍为状态1，且清空count
            FPStateProcess(res, node);
            break;
        }
        case BP: {
            BPStateProcess(res, node);
            break;
        }
        default:
            break;
        }
    }
    res.push_back(current);
    return res;
}

void RLMicroBatchMegatronClassifier::PushFPNode(std::vector<Protocol::RLPipelineNode> &res) {
    res.emplace_back(std::move(current));
    countQue.push(count);
    count = 0;
}

void RLMicroBatchMegatronClassifier::PushBPNode(std::vector<Protocol::RLPipelineNode> &res) {
    res.emplace_back(std::move(current));
    if (!countQue.empty()) {
        count = countQue.front();
        countQue.pop();
    } else {
        count = 0;
    }
}

void RLMicroBatchMegatronClassifier::SetStateAndNode(const RLPipelineNode &node, State newState) {
    current = node;
    this->state = newState;
}

void RLMicroBatchMegatronClassifier::Clear() {
    state = Init;
    count = 0;
    current = RLPipelineNode();
    countQue = std::queue<int>();
}

void RLMicroBatchMegatronClassifier::InitStateProcess(
    std::vector<Protocol::RLPipelineNode> &res, const Protocol::RLPipelineNode &node) {
    if (node.nodeType == "FP") {
        SetStateAndNode(node, FP);
        count = 0;
    } else {
        // 状态0 不能接收反向算子
        Server::ServerLog::Error("Receive a backward pass operator at init state, ignored");
    }
}

void RLMicroBatchMegatronClassifier::FPStateProcess(
    std::vector<Protocol::RLPipelineNode> &res, const RLPipelineNode &node) {
    if (node.nodeType == "FP") {
        if (node.startTime >= current.startTime &&
            node.startTime + node.duration <= current.startTime + current.duration) {
            return;
        }
        PushFPNode(res);
        SetStateAndNode(node, FP);
    } else {
        // 接收一个在正向算子时间范围内的反向算子
        if (node.startTime >= current.startTime &&
            node.duration + node.startTime <= current.duration + current.startTime) {
            count++;
        } else { // 接收一个正向时间范围外的反向算子, 状态为2
            PushFPNode(res);
            count = countQue.front();
            countQue.pop();
            SetStateAndNode(node, BP);
        }
    }
}

void RLMicroBatchMegatronClassifier::BPStateProcess(
    std::vector<Protocol::RLPipelineNode> &res, const RLPipelineNode &node) {
    // 状态2收到一个前向算子，状态变为1
    if (node.nodeType == "FP") {
        PushBPNode(res);
        SetStateAndNode(node, FP);
    } else {
        if (count <= 1) {
            PushBPNode(res);
            SetStateAndNode(node, BP);
            return;
        }
        current.duration = (node.startTime + node.duration) - current.startTime;
        count--;
    }
}

std::vector<Protocol::RLPipelineNode> RLMicroBatchMegatronClassifier::QueryMicroBatchSlices(
    const std::string &fileId, const RLMstxConfig &config, const Protocol::RLPipelineNode &taskNode) {
    // 不同的后台框架需要不同的查询和聚合逻辑
    if (config.taskConfigMap.find(taskNode.name) == config.taskConfigMap.end()) {
        ServerLog::Error("[RL] task config not found when query micro batch");
        return {};
    }
    /*
     * 1. 整理所有microBatch
     * 2. 在task的时间区间内过滤microBatch
     * 3. 状态机算法处理microBatch的时间掩盖问题
     */
    const auto &taskConfig = config.taskConfigMap.at(taskNode.name);
    std::vector<std::string> microBatchNames;
    microBatchNames.reserve(taskConfig.microBatchConfigs.size());
    std::transform(taskConfig.microBatchConfigs.begin(), taskConfig.microBatchConfigs.end(),
        std::back_inserter(microBatchNames), [](const MicroBatchConfig &item) { return item.batchName; });
    if (microBatchNames.empty()) {
        return {};
    }
    FullDb::DataType type = DataBaseManager::Instance().GetDataType(fileId);
    auto microBatchInDbs = RenderEngine::Instance()->QueryMstxRLDetail(
        fileId, type, microBatchNames, taskNode.startTime, NumberSafe::Add(taskNode.startTime, taskNode.duration));
    if (microBatchInDbs.empty()) {
        return {};
    }
    std::sort(microBatchInDbs.begin(), microBatchInDbs.end(),
        [](const CompeteSliceDomain &left, const CompeteSliceDomain &right) {
            if (left.timestamp != right.timestamp) {
                return left.timestamp < right.timestamp;
            } else {
                return left.duration > right.duration;
            }
        });
    std::vector<Protocol::RLPipelineNode> res;
    std::for_each(
        microBatchInDbs.begin(), microBatchInDbs.end(), [&res, &taskNode, &taskConfig](const auto &sliceItem) {
            RLPipelineNode microBatchNode;
            microBatchNode.stageType = taskNode.stageType;
            microBatchNode.name = sliceItem.name;
            microBatchNode.nodeType = taskConfig.microBatchConfigMap.at(microBatchNode.name).type;
            microBatchNode.startTime = sliceItem.timestamp;
            microBatchNode.duration = sliceItem.endTime - sliceItem.timestamp;
            res.emplace_back(std::move(microBatchNode));
        });
    return res;
}

}
