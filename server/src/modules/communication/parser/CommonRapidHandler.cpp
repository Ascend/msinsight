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

#include "CommonRapidHandler.h"
namespace Dic {
namespace Module {
namespace Timeline {
std::string CommonRapidHandler::GenerateAndGetGroupInfoId(const std::string &rankSet, const std::string &groupIdHash) {
    if (groupIdHash.empty()) {
        return "-1";
    }
    std::string key = rankSet == "p2p" ? "p2p" : rankSet + groupIdHash;
    // 已有key值，直接返回相关的内容
    auto it = groupIdsMap.find(key);
    if (it != groupIdsMap.end()) {
        return std::to_string(it->second.id);
    }
    // 兼容老场景，老场景中group文件解析后没有group id的hash值，因此从此处进行填充
    it = groupIdsMap.find(rankSet);
    if (it != groupIdsMap.end()) {
        it->second.groupIdHash = groupIdHash;
        // 原来数据中不存在group id的hash值，因此这里对map中的key进行了更新（按rank_set + group id hash重新插入，删除了原有的键）
        groupIdsMap[key] = it->second;
        uint64_t id = it->second.id;
        groupIdsMap.erase(it->first);
        return std::to_string(id);
    }

    // 不满足以上两个情况，原始数据中就是没有对应的rank_set + group id hash，因此生成一个
    CommGroupParallelInfo info;
    info.rankSetStr = rankSet;
    info.type = "unknown";
    info.groupIdHash = rankSet == "p2p" ? "p2p" : groupIdHash;
    info.id = ++groupIdIndex;
    groupIdsMap[key] = info;
    return std::to_string(info.id);
}

void CommonRapidHandler::InitGroupInfoMap() {
    std::vector<CommGroupParallelInfo> groupInfoList = database->GetAllGroupInfo();
    for (const auto &item : groupInfoList) {
        std::string key = (item.groupIdHash.empty() || item.rankSetStr == "p2p") ? item.rankSetStr
                                                                                 : item.rankSetStr + item.groupIdHash;
        groupIdsMap[key] = item;
        groupIdIndex = std::max(groupIdIndex, item.id);
    }
}

bool CommonRapidHandler::SaveGroupInfoMap() {
    // group数据更新落库
    std::vector<CommGroupParallelInfo> groupInfoList;
    for (const auto &item : groupIdsMap) {
        groupInfoList.push_back(item.second);
    }
    if (groupInfoList.empty()) {
        return false;
    }
    return database->BatchInsertDuplicateUpdateGroupInfo(groupInfoList);
}
}
}
}
