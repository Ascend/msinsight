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

#include "pch.h"
#include "ConstantDefs.h"
#include "ParserStatusManager.h"

namespace Dic {
namespace Module {
namespace Timeline {

ParserStatusManager &ParserStatusManager::Instance() {
    static ParserStatusManager instance;
    return instance;
}

void ParserStatusManager::SetParserStatus(const std::string &fileId, ParserStatus status) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.erase(fileId);
    statusMap[fileId] = status;
}

void ParserStatusManager::ClearParserStatus(const std::string &fileId) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.erase(fileId);
    statusMap.erase(fileId);
}

void ParserStatusManager::ClearAllParserStatus() {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.clear();
    statusMap.clear();
    clusterStatusMap.clear();
}

ParserStatus ParserStatusManager::GetParserStatus(const std::string &fileId) {
    std::shared_lock lock(mutex);
    if (statusMap.count(fileId) == 0) {
        return ParserStatus::UN_KNOW;
    }
    return statusMap[fileId];
}

void ParserStatusManager::EraseClusterParserStatusByKeyWord(const std::string &keyWord) {
    std::unique_lock lock(mutex);
    for (auto it = clusterStatusMap.begin(); it != clusterStatusMap.end();) {
        if (it->first.find(keyWord) != std::string::npos) {
            // erase返回下一个迭代器，因此直接赋值给it
            it = clusterStatusMap.erase(it);
        } else {
            it++;
        }
    }
}

bool ParserStatusManager::IsClusterParserFinalState(const std::string &uniqueKey) {
    std::shared_lock lock(mutex);
    // 如果没有状态记录视为结束
    if (clusterStatusMap.count(uniqueKey) == 0) {
        return true;
    }
    if (std::find(finalStateList.begin(), finalStateList.end(), clusterStatusMap[uniqueKey]) != finalStateList.end()) {
        return true;
    }
    return false;
}

bool ParserStatusManager::SetRunningStatus(const std::string &fileId) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.erase(fileId);
    if (statusMap.count(fileId) == 0) {
        return false;
    }
    if (statusMap[fileId] != ParserStatus::INIT) {
        return false;
    }
    statusMap[fileId] = ParserStatus::RUNNING;
    return true;
}

bool ParserStatusManager::SetFinishStatus(const std::string &fileId) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.erase(fileId);
    if (statusMap.count(fileId) == 0) {
        return false;
    }
    if (statusMap[fileId] != ParserStatus::RUNNING && statusMap[fileId] != ParserStatus::TERMINATE) {
        return false;
    }
    statusMap[fileId] = ParserStatus::FINISH;
    parseCv.notify_all();
    return true;
}

ParserStatus ParserStatusManager::SetTerminateStatus(const std::string &fileId) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.erase(fileId);
    if (statusMap.count(fileId) == 0) {
        return ParserStatus::UN_KNOW;
    }
    auto oldStatus = statusMap[fileId];
    if (oldStatus != ParserStatus::FINISH && oldStatus != ParserStatus::FINISH_ALL) {
        statusMap[fileId] = ParserStatus::TERMINATE;
    }
    return oldStatus;
}

void ParserStatusManager::SetAllTerminateStatus() {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap.clear();
    for (auto &statu : statusMap) {
        if (statu.second != ParserStatus::FINISH && statu.second != ParserStatus::FINISH_ALL) {
            statu.second = ParserStatus::TERMINATE;
        }
    }
}

bool ParserStatusManager::SetStatusToInit(
    const std::string &uniqueKey, std::map<std::string, ParserStatus> &statusMap) {
    if (statusMap.count(uniqueKey) == 0) {
        statusMap[uniqueKey] = ParserStatus::INIT;
        return true;
    }
    return false;
}

bool ParserStatusManager::SetStatusToRunning(
    const std::string &uniqueKey, std::map<std::string, ParserStatus> &statusMap) {
    if (statusMap.count(uniqueKey) == 0) {
        return false;
    }
    if (statusMap[uniqueKey] != ParserStatus::INIT) {
        return false;
    }
    statusMap[uniqueKey] = ParserStatus::RUNNING;
    return true;
}

bool ParserStatusManager::SetStatusToFinalState(
    const std::string &uniqueKey, ParserStatus parserStatus, std::map<std::string, ParserStatus> &statusMap) {
    if (statusMap.count(uniqueKey) == 0) {
        return false;
    }
    if (statusMap[uniqueKey] != ParserStatus::INIT && statusMap[uniqueKey] != ParserStatus::RUNNING) {
        return false;
    }
    statusMap[uniqueKey] = parserStatus;
    return true;
}

bool ParserStatusManager::SetClusterParseStatus(const std::string &uniqueKey, ParserStatus parserStatus) {
    std::unique_lock lock(mutex);
    switch (parserStatus) {
    case ParserStatus::INIT:
        return SetStatusToInit(uniqueKey, clusterStatusMap);
    case ParserStatus::RUNNING:
        return SetStatusToRunning(uniqueKey, clusterStatusMap);
    case ParserStatus::FINISH:
    case ParserStatus::FINISH_ALL:
    case ParserStatus::TERMINATE:
        return SetStatusToFinalState(uniqueKey, parserStatus, clusterStatusMap);
    default:
        return false;
    }
}

bool ParserStatusManager::IsAllFinished(std::string &notFinishTask) {
    std::shared_lock lock(mutex);
    for (const auto &item : pendingRankAndFilePathMap) {
        if (!std::empty(item.second.second)) {
            notFinishTask = item.second.second[0];
            return false;
        }
    }
    for (const auto &item : statusMap) {
        if (item.second == ParserStatus::INIT || item.second == ParserStatus::RUNNING) {
            notFinishTask = item.first;
            return false;
        }
    }
    for (const auto &item : clusterStatusMap) {
        if (item.second == ParserStatus::INIT || item.second == ParserStatus::RUNNING) {
            notFinishTask = item.first;
            return false;
        }
    }
    return true;
}

bool ParserStatusManager::IsFinished(const std::string &fileId) {
    return statusMap.count(fileId) == 0 || statusMap[fileId] == ParserStatus::FINISH ||
        statusMap[fileId] == ParserStatus::FINISH_ALL;
}

bool ParserStatusManager::IsKernelAndMemoryFinished(const std::string &fileId) {
    return IsFinished(fileId) && IsFinished(KERNEL_PREFIX + fileId) && IsFinished(MEMORY_PREFIX + fileId);
}

void ParserStatusManager::WaitAllFinished(const std::vector<std::string> &fileIds) {
    std::unique_lock lock(mutex);
    auto func = [this](const std::string &fileId) {
        return !IsFinished(fileId) || !IsFinished(KERNEL_PREFIX + fileId) || !IsFinished(MEMORY_PREFIX + fileId);
    };
    while (std::any_of(fileIds.begin(), fileIds.end(), func)) {
        parseCv.wait_for(lock, std::chrono::seconds(2)); // 最长等待时间2秒
    }
}

void ParserStatusManager::SetPendingStatus(
    const std::string &fileId, const std::pair<ProjectTypeEnum, std::vector<std::string>> &filePathPair) {
    std::unique_lock lock(mutex);
    pendingRankAndFilePathMap[fileId] = filePathPair;
}

std::pair<ProjectTypeEnum, std::vector<std::string>> ParserStatusManager::QueryPendingFilePath(
    const std::string &fileId) {
    std::unique_lock lock(mutex);
    return pendingRankAndFilePathMap[fileId];
}
void ParserStatusManager::WaitStartParse() {
    std::unique_lock lock(importMutex);
    importResCv.wait(lock, [this] { return importRes.load(); });
}
void ParserStatusManager::NotifyStartParse() {
    std::lock_guard lock(importMutex);
    importResCv.notify_all();
    importRes.store(true);
}
void ParserStatusManager::ResetParse() {
    std::lock_guard lock(importMutex);
    importRes.store(false);
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
