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
#include "RLMstxConfigReader.h"
#include "ServerLog.h"
#include "FileUtil.h"

namespace Dic::Module::RL {
RLMstxConfigReader::RLMstxConfigReader() {
    configPath = FileUtil::SplicePath(FileUtil::GetCurrPath(), "configs", "RLMstxConfig.json");
    Server::ServerLog::Info("RL config reader init, config path:", configPath);
}

std::vector<RLMstxConfig> RLMstxConfigReader::ReadConfigFile() {
    // check path
    if (configPath.empty() || !FileUtil::CheckPathSecurity(configPath, CHECK_FILE_READ)) {
        Server::ServerLog::Info("No rl config file load into");
        return {};
    }
    document_t configJson = JsonUtil::ReadJsonFromFile(configPath);
    if (configJson.IsNull()) {
        Server::ServerLog::Error("Rl config file parse error");
        return {};
    }
    if (!ConfigFormatValid(configJson)) {
        return {};
    }
    json_t &configs = configJson["config"];
    std::vector<RLMstxConfig> res;
    std::for_each(
        configs.Begin(), configs.End(), [&res, this](const json_t &config) { res.push_back(ParseMstxConfig(config)); });
    return res;
}

/**
 * @param doc
 * @return
 */
bool RLMstxConfigReader::ConfigFormatValid(document_t &doc) {
    if (doc.IsNull()) {
        return false;
    }
    if (!doc.IsObject()) {
        Server::ServerLog::Error("RL config suppose to be a object, but array found");
        return false;
    }
    if (!doc.HasMember("config") || !doc["config"].IsArray()) {
        Server::ServerLog::Error("Not found \"config\" field which should be array in config file");
        return false;
    }
    auto &configs = doc["config"];
    // 这里使用all_of， 在出现错误配置时及时停止，不必再检查后面的配置
    return std::all_of(configs.Begin(), configs.End(), RLMstxConfigReader::MstxConfigCheck);
}

bool RLMstxConfigReader::TaskConfigCheck(const json_t &taskConfJson) {
    if (!taskConfJson.HasMember("roleName") || !taskConfJson["roleName"].IsString()) {
        Server::ServerLog::Error("RL config parse failed, stage lost roleName field");
        return false;
    }
    if (!taskConfJson.HasMember("taskName") || !taskConfJson["taskName"].IsString()) {
        Server::ServerLog::Error("RL config parse failed, stage lost stageName");
        return false;
    }
    if (!taskConfJson.HasMember("microBatches") || !taskConfJson["microBatches"].IsArray()) {
        Server::ServerLog::Error("RL config parse failed, stage lost microBatchName");
        return false;
    }
    return std::all_of(taskConfJson["microBatches"].Begin(), taskConfJson["microBatches"].End(),
        RLMstxConfigReader::MicroBatchConfigCheck);
}

RLMstxConfig RLMstxConfigReader::ParseMstxConfig(const json_t &config) {
    RLMstxConfig mstxConf;
    mstxConf.framework = config["framework"].GetString();
    mstxConf.algorithm = config["algorithm"].GetString();
    const json_t &tasks = config["tasks"];
    std::for_each(tasks.Begin(), tasks.End(), [this, &mstxConf](const json_t &taskJson) {
        TaskConfig task = ParseTaskConfig(taskJson);
        mstxConf.AddTaskConfig(std::move(task));
    });
    return mstxConf;
}

void RLMstxConfigReader::SetConfigPath(const std::string &path) { configPath = path; }

bool RLMstxConfigReader::MicroBatchConfigCheck(const json_t &microBatchJson) {
    if (!microBatchJson.HasMember("name") || !microBatchJson["name"].IsString()) {
        Server::ServerLog::Error("field microBatchName needs, and it should be string type");
        return false;
    }
    if (!microBatchJson.HasMember("type") || !microBatchJson["type"].IsString()) {
        Server::ServerLog::Error("field type needs, and it should be string type");
        return false;
    }
    std::string type = microBatchJson["type"].GetString();
    if (type.compare("FP") != 0 && type.compare("BP") != 0) {
        Server::ServerLog::Error(R"(filed type should be "FP" or "BP")");
        return false;
    }
    return true;
}

bool RLMstxConfigReader::MstxConfigCheck(const json_t &mstxConfigJson) {
    if (!mstxConfigJson.HasMember("framework") || !mstxConfigJson["framework"].IsString()) {
        Server::ServerLog::Error("RL config parse failed, framework field not found");
        return false;
    }
    if (!mstxConfigJson.HasMember("algorithm") || !mstxConfigJson["algorithm"].IsString()) {
        Server::ServerLog::Error("RL config parse failed, algorithm field not found");
        return false;
    }
    if (!mstxConfigJson.HasMember("tasks") || !mstxConfigJson["tasks"].IsArray()) {
        Server::ServerLog::Error("RL config parse failed, taskStage field not found");
        return false;
    }
    return std::all_of(
        mstxConfigJson["tasks"].Begin(), mstxConfigJson["tasks"].End(), RLMstxConfigReader::TaskConfigCheck);
}

TaskConfig RLMstxConfigReader::ParseTaskConfig(const json_t &taskConfJson) {
    TaskConfig taskConf;
    taskConf.taskName = taskConfJson["taskName"].GetString();
    taskConf.roleName = taskConfJson["roleName"].GetString();
    std::for_each(taskConfJson["microBatches"].Begin(), taskConfJson["microBatches"].End(),
        [this, &taskConf](const json_t &microBatchConfJson) {
            taskConf.AddMicroBatchConf(ParseMicroBatchConfig(microBatchConfJson));
        });
    return taskConf;
}

MicroBatchConfig RLMstxConfigReader::ParseMicroBatchConfig(const json_t &microBatchConfJson) {
    MicroBatchConfig microBatchConf;
    microBatchConf.batchName = microBatchConfJson["name"].GetString();
    microBatchConf.type = microBatchConfJson["type"].GetString();
    return microBatchConf;
}
} // Dic
