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

#include "RLMicroBatchFSDPClassifier.h"
#include "NumberSafeUtil.h"

using namespace Dic;
using namespace Dic::Module::Timeline;

std::vector<Protocol::RLPipelineNode> Dic::Module::RL::RLMicroBatchFSDPClassifier::QueryMicroBatchSlices(
    const std::string &fileId, const RLMstxConfig &config, const Protocol::RLPipelineNode &taskNode) {
    if (config.taskConfigMap.find(taskNode.name) == config.taskConfigMap.end()) {
        return {};
    }
    const auto &taskConfig = config.taskConfigMap.at(taskNode.name);
    if (taskConfig.microBatchConfigs.empty()) {
        return {};
    }
    std::string rankId = DataBaseManager::Instance().GetRankIdByFileId(fileId);
    std::vector<RLPipelineNode> fpNodes = QueryFPSlices(rankId, taskNode);
    std::vector<RLPipelineNode> bpNodes = QueryBPSlices(rankId, taskNode);
    return NodeSortMerge(fpNodes, bpNodes);
}

std::vector<Protocol::RLPipelineNode> Module::RL::RLMicroBatchFSDPClassifier::MicroBatchClassifier(
    std::vector<Protocol::RLPipelineNode> &nodes) {
    if (nodes.empty()) {
        return {};
    }
    std::vector<Protocol::RLPipelineNode> res;
    for (const auto &node : nodes) {
        switch (state) {
        case State::Init: {
            ProcessInitState(node, res);
            break;
        }
        case State::FP: {
            ProcessFPState(node, res);
            break;
        }
        case State::BP: {
            ProcessBPState(node, res);
            break;
        }
        case State::Communication: {
            ProcessCommunicationState(node, res);
            break;
        }
        }
    }
    if (state != State::Communication) {
        res.push_back(current);
    }
    return res;
}

void Module::RL::RLMicroBatchFSDPClassifier::ProcessInitState(
    const RLPipelineNode &node, std::vector<RLPipelineNode> &res) {
    if (node.nodeType == "FP") {
        current = node;
        state = State::FP;
    } else {
        ServerLog::Warn("Receive BP at init");
    }
}

void Module::RL::RLMicroBatchFSDPClassifier::ProcessFPState(
    const RLPipelineNode &node, std::vector<RLPipelineNode> &res) {
    if (node.nodeType == "FP") { // 正向算子存在时间掩盖
        if (node.startTime >= current.startTime + current.duration) {
            res.push_back(current);
            current = node;
        }
    } else {
        if (node.name == BP_MICRO_BATCH_END_NAME) {
            return;
        }
        res.push_back(current);
        current = node;
        state = State::BP;
    }
}

void Module::RL::RLMicroBatchFSDPClassifier::ProcessBPState(
    const RLPipelineNode &node, std::vector<RLPipelineNode> &res) {
    if (node.nodeType == "FP") {
        res.push_back(current);
        current = node;
        state = State::FP;
    } else {
        if (node.name == BP_MICRO_BATCH_END_NAME && node.startTime > current.startTime + current.duration) {
            current.duration = node.startTime + node.duration - current.startTime;
            res.push_back(current);
            current = node;
            state = State::Communication;
            return;
        }
        current.duration = node.startTime + node.duration - current.startTime;
    }
}

std::vector<RLPipelineNode> Module::RL::RLMicroBatchFSDPClassifier::QueryFPSlices(
    const std::string &rankId, const RLPipelineNode &taskNode) {
    PythonApiRepo pythonApiRepo;
    SliceQuery query;
    query.rankId = rankId;
    query.name = FP_MICRO_BATCH_NAME;
    query.startTime = taskNode.startTime;
    query.endTime = NumberSafe::Add(taskNode.startTime, taskNode.duration);
    std::vector<CompeteSliceDomain> fpSlices;
    pythonApiRepo.QuerySliceByVagueNameAndTime(query, fpSlices);
    return TransSliceToNodes(fpSlices, taskNode, FP_MICRO_BATCH_NAME, "FP");
}

std::vector<RLPipelineNode> Module::RL::RLMicroBatchFSDPClassifier::QueryBPSlices(
    const std::string &rankId, const RLPipelineNode &taskNode) {
    // 反向算子需要找出autograd::engine,并以 event_record作为结束
    PythonApiRepo pythonApiRepo;
    SliceQuery query;
    query.rankId = rankId;
    query.name = BP_MICRO_BATCH_NAME + "%";
    query.startTime = taskNode.startTime;
    query.endTime = NumberSafe::Add(taskNode.startTime, taskNode.duration);
    query.depth = 0;
    std::vector<CompeteSliceDomain> bpSlices;
    pythonApiRepo.QuerySliceByVagueNameAndTime(query, bpSlices);
    std::vector<RLPipelineNode> bpNodes = TransSliceToNodes(bpSlices, taskNode, BP_MICRO_BATCH_NAME, "BP");
    if (bpNodes.empty()) {
        return {};
    }
    std::vector<CompeteSliceDomain> eventSlices;
    query.name = BP_MICRO_BATCH_END_NAME;
    pythonApiRepo.QuerySliceByVagueNameAndTime(query, eventSlices);
    std::vector<RLPipelineNode> eventNodes = TransSliceToNodes(eventSlices, taskNode, BP_MICRO_BATCH_END_NAME, "BP");
    return NodeSortMerge(bpNodes, eventNodes);
}

std::vector<RLPipelineNode> Module::RL::RLMicroBatchFSDPClassifier::TransSliceToNodes(
    const std::vector<CompeteSliceDomain> &slices, const RLPipelineNode &task, const std::string &name,
    const std::string &nodeType) {
    std::vector<RLPipelineNode> res;
    std::transform(slices.begin(), slices.end(), std::back_inserter(res),
        [&task, &name, &nodeType](const CompeteSliceDomain &slice) {
            RLPipelineNode node;
            node.name = name;
            node.nodeType = nodeType;
            node.stageType = task.stageType;
            node.startTime = slice.timestamp;
            node.duration = slice.endTime - slice.timestamp;
            return node;
        });
    return res;
}

std::vector<RLPipelineNode> Module::RL::RLMicroBatchFSDPClassifier::NodeSortMerge(
    const std::vector<RLPipelineNode> &left, const std::vector<RLPipelineNode> &right) {
    auto itLeft = left.begin();
    auto itRight = right.begin();
    std::vector<RLPipelineNode> res;
    while (itLeft != left.end() && itRight != right.end()) {
        if (IsHappenBefore(itLeft, itRight)) {
            res.push_back(*itLeft);
            itLeft++;
        } else {
            res.push_back(*itRight);
            itRight++;
        }
    }
    while (itLeft != left.end()) {
        res.push_back(*itLeft);
        itLeft++;
    }
    while (itRight != right.end()) {
        res.push_back(*itRight);
        itRight++;
    }
    return res;
}

bool Module::RL::RLMicroBatchFSDPClassifier::IsHappenBefore(
    std::vector<RLPipelineNode>::const_iterator left, std::vector<RLPipelineNode>::const_iterator right) {
    if (left->startTime != right->startTime) {
        return left->startTime < right->startTime;
    } else {
        return left->duration > right->duration;
    }
}

void Module::RL::RLMicroBatchFSDPClassifier::ProcessCommunicationState(
    const RLPipelineNode &node, std::vector<RLPipelineNode> &res) {
    if (node.nodeType == "FP") {
        current = node;
        state = State::FP;
    } else {
        if (node.name == BP_MICRO_BATCH_END_NAME) {
            return;
        }
        current = node;
        state = State::BP;
    }
}
