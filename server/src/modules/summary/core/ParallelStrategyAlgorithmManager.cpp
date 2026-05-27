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
#include "MegatronParallelStrategyAlgorithm.h"
#include "MindSpeedParallelStrategyAlgorithm.h"
#include "MindIELLMParallelStrategyAlgorithm.h"
#include "VLLMParallelStrategyAlgorithm.h"
#include "ParallelStrategyAlgorithmManager.h"
#include "SummaryErrorManager.h"
using namespace Dic::Server;
namespace Dic::Module::Summary {
ParallelStrategyAlgorithmManager &ParallelStrategyAlgorithmManager::Instance() {
    static ParallelStrategyAlgorithmManager instance;
    return instance;
}

void ParallelStrategyAlgorithmManager::Reset() {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    algorithmMap.clear();
}

bool ParallelStrategyAlgorithmManager::IsSameAlgorithm(const std::string &algorithm1, const std::string &algorithm2) {
    if (algorithm1 == algorithm2 ||
        (StringUtil::Contains(algorithm1, MEGATRON_ALG) && StringUtil::Contains(algorithm2, MEGATRON_ALG))) {
        return true;
    }
    return false;
}

// algorithmFactoryTable算法映射表，用于按照算法名称创建对应类型算法实例 key: matcher, value: creator
using AlgorithmCreator = std::function<std::shared_ptr<BaseParallelStrategyAlgorithm>()>;
const std::vector<std::pair<std::function<bool(const std::string &)>, AlgorithmCreator>> algorithmFactoryTable = {
    {[](const std::string &alg) {
         return alg == MEGATRON_LM_TP_CP_EP_DP_PP_ALG || alg == MEGATRON_LM_TP_CP_PP_EP_DP_ALG;
     },
        []() { return std::make_shared<MegatronParallelStrategyAlgorithm>(); }},

    {[](const std::string &alg) { return alg == MINDSPEED_TP_CP_EP_DP_PP_ALG; },
        []() { return std::make_shared<MindSpeedParallelStrategyAlgorithm>(); }},

    {[](const std::string &alg) { return alg == MINDIE_LLM_TP_DP_EP_PP_MOETP_ALG; },
        []() { return std::make_shared<MindIELLMParallelStrategyAlgorithm>(); }},

    {[](const std::string &alg) { return alg == VLLM_TP_PP_DP_EP_ALG; },
        []() { return std::make_shared<VLLMParallelStrategyAlgorithm>(); }}};

std::shared_ptr<BaseParallelStrategyAlgorithm> ParallelStrategyAlgorithmManager::CreateAlgorithm(
    const std::string &algorithm) {
    std::string lowerAlg = StringUtil::ToLower(algorithm);
    for (const auto &[matcher, creator] : algorithmFactoryTable) {
        if (matcher(lowerAlg)) {
            return creator();
        }
    }
    return nullptr;
}

bool ParallelStrategyAlgorithmManager::AddOrUpdateAlgorithm(
    const std::string &projectName, const ParallelStrategyConfig &config, std::string &errMsg) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    auto it = algorithmMap.find(projectName);
    if (it != algorithmMap.end()) {
        ParallelStrategyConfig oldConfig = algorithmMap.at(projectName)->GetStrategyConfig();
        // 若已存在该project，且算法类别相同, 则更新config
        if (IsSameAlgorithm(oldConfig.algorithm, config.algorithm)) {
            algorithmMap.at(projectName)->ClearStrategyConfigCache();
            algorithmMap.at(projectName)->SetStrategyConfig(config);
            Server::ServerLog::Info("Algorithm already exist. Update parallel strategy config for this program.");
            return true;
        }
        DeleteAlgorithm(projectName);
    }
    // 若不存在, 或算法类不同，则添加相应算法类
    auto algorithm = CreateAlgorithm(config.algorithm);
    if (!algorithm) {
        errMsg = "Failed to add algorithm to manager. Unexpected algorithm.";
        SetSummaryError(ErrorCode::ADD_ALGORITHM_FAILED);
        return false;
    }
    algorithmMap.emplace(projectName, algorithm);
    algorithmMap.at(projectName)->SetStrategyConfig(config);
    return true;
}

ParallelStrategyConfig ParallelStrategyAlgorithmManager::GetParallelStrategyConfig(const std::string &key) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    ParallelStrategyConfig config;
    auto it = algorithmMap.find(key);
    if (it != algorithmMap.end()) {
        config = algorithmMap.at(key)->GetStrategyConfig();
    }
    Server::ServerLog::Warn("Fail to get parallel strategy config.");
    return config;
}

bool ParallelStrategyAlgorithmManager::DeleteAlgorithm(const std::string &projectName) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    // 若不存在该project，则删除失败
    auto it = algorithmMap.find(projectName);
    if (it == algorithmMap.end()) {
        return false;
    }
    algorithmMap.erase(projectName);
    return true;
}

std::shared_ptr<BaseParallelStrategyAlgorithm> ParallelStrategyAlgorithmManager::GetAlgorithmByProjectName(
    const std::string &projectName) {
    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (algorithmMap.count(projectName) == 0) {
        SetSummaryError(ErrorCode::GET_ALGORITHM_FAILED);
        return nullptr;
    }
    ServerLog::Info("Success to get algorithm by project name.");
    return algorithmMap.at(projectName);
}
}
