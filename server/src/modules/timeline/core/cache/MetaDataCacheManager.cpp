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

#include "MetaDataCacheManager.h"

namespace Dic::Module::Timeline {
void MetaDataCacheManager::Clear() {
    std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
    hcclGroupInfoMap.clear();
    distributedArgsInfo.reset();
}

void MetaDataCacheManager::AddParallelGroupInfo(const std::vector<ParallelGroupInfo> &parallelGroupInfoList) {
    std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
    for (const auto &item : parallelGroupInfoList) {
        if (item.group.empty()) {
            continue;
        }
        hcclGroupInfoMap[item.group] = item;
    }
}

std::optional<ParallelGroupInfo> MetaDataCacheManager::GetParallelGroupInfo(const std::string &group) {
    std::shared_lock<std::shared_mutex> sharedLock(sharedMutex);
    auto res = hcclGroupInfoMap.find(group);
    if (res != hcclGroupInfoMap.end()) {
        return {res->second};
    }
    return {};
}

void MetaDataCacheManager::SetDistributedArgsInfo(const std::optional<DistributedArgs> &args) {
    std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
    distributedArgsInfo = args;
}

std::optional<DistributedArgs> MetaDataCacheManager::GetDistributedArgsInfo() {
    std::shared_lock<std::shared_mutex> sharedLock(sharedMutex);
    return distributedArgsInfo;
}
}
