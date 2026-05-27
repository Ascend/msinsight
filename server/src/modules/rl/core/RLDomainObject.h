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

#ifndef PROFILER_SERVER_RLDOMAINOBJECT_H
#define PROFILER_SERVER_RLDOMAINOBJECT_H
#include <string>
#include <vector>
#include <unordered_map>

namespace Dic::Module::RL {
enum class RLBackEndType { Megatron, FSDP, Unknown };

inline std::string RLBackendToStr(enum RLBackEndType type) {
    switch (type) {
    case RLBackEndType::Megatron:
        return "Megatron";
    case RLBackEndType::FSDP:
        return "fsdp";
    case RLBackEndType::Unknown:
        return "unknown";
    }
    return "";
}

struct MicroBatchConfig {
    std::string batchName;
    std::string type;
};

struct TaskConfig {
    std::string roleName;
    std::string taskName;
    std::vector<MicroBatchConfig> microBatchConfigs;
    std::unordered_map<std::string, MicroBatchConfig> microBatchConfigMap;
    void AddMicroBatchConf(MicroBatchConfig &&config) {
        microBatchConfigs.push_back(config);
        microBatchConfigMap[config.batchName] = config;
    }
};

struct RLMstxConfig {
    std::string framework;
    std::string algorithm;
    std::vector<TaskConfig> taskConfigs;
    std::unordered_map<std::string, TaskConfig> taskConfigMap;
    void AddTaskConfig(TaskConfig &&config) {
        taskConfigs.push_back(config);
        taskConfigMap[config.taskName] = config;
    }
};
}
#endif // PROFILER_SERVER_RLDOMAINOBJECT_H
