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
#include <set>
#include <unordered_set>
#include <algorithm>
#include "ServerLog.h"
#include "RLMstxConfigReader.h"
#include "RLMstxConfigManager.h"

namespace Dic::Module::RL {
RLMstxConfigManager &RLMstxConfigManager::Instance() {
    static RLMstxConfigManager instance;
    return instance;
}

std::vector<RLMstxConfig> RLMstxConfigManager::GetRLMstxConfig() { return config; }

std::vector<std::string> RLMstxConfigManager::GetMstxTaskNameList() {
    std::set<std::string> resSet;
    for (const auto &item : config) {
        for (const auto &task : item.taskConfigs) {
            resSet.insert(task.taskName);
        }
    }
    return std::vector<std::string>(resSet.begin(), resSet.end());
}

std::string RLMstxConfigManager::GetTaskTypeByName(const std::string &name) {
    std::string type = "";
    for (const auto &item : config) {
        for (const auto &task : item.taskConfigs) {
            if (name == task.taskName) {
                return task.roleName;
            }
        }
    }
    return type;
}

RLMstxConfig RLMstxConfigManager::GetMstxConfigByTaskName(const std::vector<std::string> &taskNames) {
    std::unordered_set<std::string> taskNameSet{taskNames.begin(), taskNames.end()};
    auto it = std::find_if(config.begin(), config.end(), [&taskNameSet](const RLMstxConfig &configItem) {
        for (const auto &taskConfig : configItem.taskConfigs) {
            if (taskNameSet.count(taskConfig.taskName) == 0) {
                return false;
            }
        }
        return true;
    });
    if (it == config.end()) {
        // can't find total match config
        Server::ServerLog::Warn("No total matching config could be found");
        for (const std::string &taskName : taskNameSet) {
            auto iterator = std::find_if(config.begin(), config.end(), [&taskName](const RLMstxConfig &configItem) {
                return configItem.taskConfigMap.find(taskName) != configItem.taskConfigMap.end();
            });
            if (iterator != config.end()) {
                return *iterator;
            }
        }
        return {};
    }
    return *it;
}

void RLMstxConfigManager::InitConfig() {
    RLMstxConfigReader reader;
    config = reader.ReadConfigFile();
    // config file not exist or empty, add default one
    if (config.empty()) {
        InitDefaultConf();
    }
}

RLMstxConfigManager::RLMstxConfigManager() { InitConfig(); }

void RLMstxConfigManager::InitDefaultConf() {
    InitVerlGrpoConf();
    InitMindspeedRlGrpoConf();
}
void RLMstxConfigManager::InitVerlGrpoConf() {
    RLMstxConfig defaultConf = {
        .framework = "verl",
        .algorithm = "GRPO",
    };
    TaskConfig gs{.roleName = "ActorRollout", .taskName = "generate_sequences"};
    TaskConfig reward{.roleName = "Reward", .taskName = "compute_log_prob"};
    MicroBatchConfig fp{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp{.batchName = "TransformerLayer", .type = "BP"};
    reward.AddMicroBatchConf(std::move(fp));
    reward.AddMicroBatchConf(std::move(bp));
    TaskConfig actor{.roleName = "Actor", .taskName = "update_actor"};
    MicroBatchConfig fp1{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp1{.batchName = "TransformerLayer", .type = "BP"};
    actor.AddMicroBatchConf(std::move(fp1));
    actor.AddMicroBatchConf(std::move(bp1));
    TaskConfig refLog{.roleName = "Reference", .taskName = "compute_ref_log_prob"};
    MicroBatchConfig fp2{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp2{.batchName = "TransformerLayer", .type = "BP"};
    refLog.AddMicroBatchConf(std::move(fp2));
    refLog.AddMicroBatchConf(std::move(bp2));
    defaultConf.AddTaskConfig(std::move(gs));
    defaultConf.AddTaskConfig(std::move(reward));
    defaultConf.AddTaskConfig(std::move(actor));
    defaultConf.AddTaskConfig(std::move(refLog));
    config.push_back(defaultConf);
}
void RLMstxConfigManager::InitMindspeedRlGrpoConf() {
    RLMstxConfig mindSpeedRlConf = {
        .framework = "MindSpeed-RL",
        .algorithm = "GRPO",
    };
    TaskConfig mindSpeedGS{.roleName = "ActorRollout", .taskName = "ActorHybridWorkerBase.generate_sequences"};
    MicroBatchConfig fp{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp{.batchName = "TransformerLayer", .type = "BP"};
    mindSpeedGS.AddMicroBatchConf(std::move(fp));
    mindSpeedGS.AddMicroBatchConf(std::move(bp));
    TaskConfig mindSpeedRef{.roleName = "Reference", .taskName = "IntegratedWorker.compute_ref_log_prob"};
    MicroBatchConfig fp2{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp2{.batchName = "TransformerLayer", .type = "BP"};
    mindSpeedRef.AddMicroBatchConf(std::move(fp2));
    mindSpeedRef.AddMicroBatchConf(std::move(bp2));
    TaskConfig mindSpeedActor{.roleName = "Actor", .taskName = "ActorRolloutHybrid.update_actor"};
    MicroBatchConfig fp3{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp3{.batchName = "TransformerLayer", .type = "BP"};
    mindSpeedActor.AddMicroBatchConf(std::move(fp3));
    mindSpeedActor.AddMicroBatchConf(std::move(bp3));
    TaskConfig mindSpeedReward{.roleName = "Reward", .taskName = "Reward.compute_rm_score"};
    MicroBatchConfig fp4{.batchName = "TransformerBlock", .type = "FP"};
    MicroBatchConfig bp4{.batchName = "TransformerLayer", .type = "BP"};
    mindSpeedReward.AddMicroBatchConf(std::move(fp4));
    mindSpeedReward.AddMicroBatchConf(std::move(bp4));
    mindSpeedRlConf.AddTaskConfig(std::move(mindSpeedGS));
    mindSpeedRlConf.AddTaskConfig(std::move(mindSpeedRef));
    mindSpeedRlConf.AddTaskConfig(std::move(mindSpeedActor));
    mindSpeedRlConf.AddTaskConfig(std::move(mindSpeedReward));
    config.push_back(mindSpeedRlConf);
}
}
