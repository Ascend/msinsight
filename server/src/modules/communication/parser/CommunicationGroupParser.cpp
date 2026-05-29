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
#include "CommunicationGroupParser.h"
#include "ClusterDef.h"
#include "ServerLog.h"
#include "FileUtil.h"
#include "SafeFile.h"

namespace Dic {
namespace Module {
namespace Communication {
std::vector<CommGroupParallelInfo> CommunicationGroupParser::GetGroupFromParallelInfo(const json_t &json) {
    auto parallelInfoList = json["comm_group_parallel_info"].GetArray();
    std::vector<CommGroupParallelInfo> res;
    for (const auto &item : parallelInfoList) {
        CommGroupParallelInfo info;
        info.type = JsonUtil::GetString(item, "type");
        info.groupId = JsonUtil::GetString(item, "group_id");
        info.groupIdHash = JsonUtil::GetString(item, "group_name");
        info.pgName = JsonUtil::GetString(item, "pg_name");
        info.rankSet = JsonUtil::GetVector<std::string>(item, "rank_set");
        info.rankSetStr = StringUtil::JoinNumberStrWithParenthesesByOrder(info.rankSet);
        res.push_back(info);
    }
    return res;
}

bool SetCommGroupParallelInfoListByJsonKey(
    const json_t &json, std::string_view key, std::vector<CommGroupParallelInfo> &res) {
    if (JsonUtil::IsJsonKeyValid(json, key) && json[key.data()].IsArray()) {
        for (const auto &item : json[key.data()].GetArray()) {
            CommGroupParallelInfo info;
            info.type = key.data();
            if (!item.IsArray()) {
                Server::ServerLog::Warn("Skip item when parsing communication parallel info: not json array.");
                continue;
            }
            info.rankSet = JsonUtil::JsonToVector(item.GetArray());
            info.rankSetStr = StringUtil::JoinNumberStrWithParenthesesByOrder(info.rankSet);
            res.push_back(info);
        }
        return true;
    }
    return false;
}

std::vector<CommGroupParallelInfo> CommunicationGroupParser::GetGroupFromP2pAndCollective(const json_t &json) {
    // 分别从collective和p2p中读取数据
    std::vector<CommGroupParallelInfo> res;
    std::vector<std::string_view> keys = {"p2p", "collective"};
    for (auto key : keys) {
        if (!SetCommGroupParallelInfoListByJsonKey(json, key, res)) {
            Server::ServerLog::Warn("Failed to set communication group info list from json by \"%\".", key);
        }
    }
    if (res.empty()) {
        Server::ServerLog::Warn("Get group from p2p and collective return empty.");
    }
    return res;
}

std::vector<CommGroupParallelInfo> CommunicationGroupParser::ParseCommunicationGroupByText(
    const std::string &fileContent) {
    try {
        std::string errorStr;
        std::optional<document_t> jsonInfo = JsonUtil::TryParse<kParseNumbersAsStringsFlag>(fileContent, errorStr);
        if (!errorStr.empty() || !jsonInfo.has_value()) {
            Server::ServerLog::Error("Try to parse communication group failed, error is ", errorStr);
            return {};
        }
        // 如果数据中存在comm_group_parallel_info字段，则以该字段为准（该字段会囊括p2p和collective内容，且信息更全）
        if (JsonUtil::IsJsonArray(jsonInfo.value(), "comm_group_parallel_info")) {
            return GetGroupFromParallelInfo(jsonInfo.value());
        } else {
            // 数据中没有comm_group_parallel_info字段，则表示数据较老，只能从p2p和collective中获取内容
            return GetGroupFromP2pAndCollective(jsonInfo.value());
        }
    } catch (std::exception &e) {
        // 异常解析失败返回空列表
        Server::ServerLog::Error("Parse communication group failed by system exception (%s)", e.what());
        return {};
    }
}

std::vector<CommGroupParallelInfo> CommunicationGroupParser::ParseCommunicationGroup(const std::string &selectedPath) {
    const std::string &filePath = FileUtil::PathPreprocess(selectedPath);
    std::ifstream communicationGroup = OpenReadFileSafely(filePath, std::ios::binary);
    if (communicationGroup.good()) {
        auto start = std::chrono::high_resolution_clock::now();
        std::string fileContent;
        std::copy(std::istream_iterator<unsigned char>(communicationGroup), std::istream_iterator<unsigned char>(),
            back_inserter(fileContent));
        std::vector<CommGroupParallelInfo> res = ParseCommunicationGroupByText(fileContent);
        auto end = std::chrono::high_resolution_clock::now();
        Server::ServerLog::Info(
            "End parse communication group file data into db ,file:", filePath, ",cost time:", (end - start).count());
        communicationGroup.close();
        return res;
    } else {
        Server::ServerLog::Error("Parse communication group file fail, path:", filePath);
        communicationGroup.close();
        return {};
    }
}
}
}
}
