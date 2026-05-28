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
#include "MetaDataParser.h"
#include "ServerLog.h"
#include "JsonUtil.h"
#include "FileReader.h"
#include "NumberUtil.h"

namespace Dic::Module::Timeline {
std::vector<ParallelGroupInfo> MetaDataParser::ParserParallelGroupInfoByText(const std::string &text) {
    std::vector<ParallelGroupInfo> res;
    if (text.empty()) {
        return res;
    }
    try {
        std::string error;
        auto groupInfoJson = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(text, error);
        if (!error.empty()) {
            Server::ServerLog::Error("Fail to convert parallel group info, error: ", error);
            return res;
        }
        return ConvertGroupInfoJsonToObject(groupInfoJson.value());
    } catch (const std::exception &e) {
        Server::ServerLog::Error("Fail to parser parallel group info. Error: ", e.what());
        return res;
    }
}

std::vector<ParallelGroupInfo> MetaDataParser::ParserParallelGroupInfoByFilePath(const std::string &filePath) {
    FileReader reader;
    std::string fileContext = reader.ReadJsonArray(filePath, 0, 0);
    std::vector<ParallelGroupInfo> res;
    if (fileContext.empty()) {
        Server::ServerLog::Error("Fail to read meta data file.");
        return res;
    }
    try {
        std::string error;
        auto metaDataJsonOpt = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(fileContext, error);
        if (!error.empty()) {
            Server::ServerLog::Error("Fail to parser meta data file, error: ", error);
            return res;
        }
        if (!metaDataJsonOpt.value().IsObject()) {
            Server::ServerLog::Error("Fail to parser meta data file, data in wrong format.");
            return res;
        }
        if (metaDataJsonOpt.value().HasMember("parallel_group_info")) {
            document_t groupInfo;
            groupInfo.CopyFrom(metaDataJsonOpt.value()["parallel_group_info"], groupInfo.GetAllocator());
            res = ConvertGroupInfoJsonToObject(groupInfo);
        }
        return res;
    } catch (const std::exception &e) {
        Server::ServerLog::Error("Fail to parser meta data file context. Error: ", e.what());
        return res;
    }
}

std::optional<DistributedArgs> MetaDataParser::ParserDistributedArgsByFilePath(const std::string &filePath) {
    FileReader reader;
    std::string fileContext = reader.ReadJsonArray(filePath, 0, 0);
    if (fileContext.empty()) {
        Server::ServerLog::Error("Fail to read meta data file.");
        return std::nullopt;
    }
    try {
        std::string error;
        auto metaDataJsonOpt = JsonUtil::TryParse(fileContext, error);
        if (!error.empty()) {
            Server::ServerLog::Error("Fail to parser meta data file, error: ", error);
            return std::nullopt;
        }
        if (!metaDataJsonOpt.value().IsObject()) {
            Server::ServerLog::Error("Fail to parser meta data file, data in wrong format.");
            return std::nullopt;
        }
        if (!metaDataJsonOpt.value().HasMember("distributed_args")) {
            return std::nullopt;
        }
        document_t distributedArgsInfo;
        distributedArgsInfo.CopyFrom(metaDataJsonOpt.value()["distributed_args"], distributedArgsInfo.GetAllocator());
        return ConvertDistributedArgsJsonToObject(distributedArgsInfo);
    } catch (const std::exception &e) {
        Server::ServerLog::Error("Fail to parser meta data file context. Error: ", e.what());
        return std::nullopt;
    }
}

std::vector<ParallelGroupInfo> MetaDataParser::ConvertGroupInfoJsonToObject(const document_t &json) {
    std::vector<ParallelGroupInfo> res;
    if (!json.IsObject()) {
        Server::ServerLog::Error("Fail to convert parallel group info, data in wrong format.");
        return res;
    }
    for (auto iter = json.MemberBegin(); iter != json.MemberEnd(); ++iter) {
        ParallelGroupInfo info;
        info.group = JsonUtil::GetStringWithoutKey(iter->name);
        info.globalRanks = JsonUtil::GetVector<std::string>(iter->value, GLOBAL_RANKS);
        info.groupName = JsonUtil::GetString(iter->value, GLOBAL_NAME);
        res.push_back(info);
    }
    return res;
}

std::optional<DistributedArgs> MetaDataParser::ConvertDistributedArgsJsonToObject(const Dic::document_t &json) {
    if (!json.IsObject()) {
        Server::ServerLog::Error("Value of key distributed_args in profiler_metadata.json is not valid json format.");
        return std::nullopt;
    }
    for (const auto &item : DISTRIBUTED_ARGS_INT_KEY) {
        if (!json.HasMember(item.c_str()) || !json[item.c_str()].IsInt64()) {
            Server::ServerLog::Error("Value of key distributed_args in profiler_metadata.json lacks ", item,
                " key or "
                "value of this key is not of int type.");
            return std::nullopt;
        }
    }
    for (const auto &item : DISTRIBUTED_ARGS_BOOL_KEY) {
        if (!json.HasMember(item.c_str()) || !json[item.c_str()].IsBool()) {
            Server::ServerLog::Error("Value of key distributed_args in profiler_metadata.json lacks ", item,
                " key or "
                "value of this key is not of bool type.");
            return std::nullopt;
        }
    }
    DistributedArgs args;
    args.config.tpSize = NumberUtil::IntToUint32(json["tensor_model_parallel_size"].GetInt());
    args.config.ppSize = NumberUtil::IntToUint32(json["pipeline_model_parallel_size"].GetInt());
    args.config.dpSize = NumberUtil::IntToUint32(json["data_parallel_size"].GetInt());
    args.config.cpSize = NumberUtil::IntToUint32(json["context_parallel_size"].GetInt());
    args.config.epSize = NumberUtil::IntToUint32(json["expert_model_parallel_size"].GetInt());
    args.worldSize = NumberUtil::IntToUint32(json["world_size"].GetInt());
    args.sequenceParallel = json["sequence_parallel"].GetBool();
    return std::optional<DistributedArgs>(args);
}
}
