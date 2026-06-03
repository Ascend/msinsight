/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * -------------------------------------------------------------------------
 */

#include "MemSnapshotStateCache.h"

#include <algorithm>

namespace Dic::Module::MemSnapshot {
std::mutex MemSnapshotStateCache::mutex_;
std::unordered_map<std::string, std::vector<Segment>> MemSnapshotStateCache::cache_;
std::deque<std::string> MemSnapshotStateCache::cacheOrder_;

std::optional<std::vector<Segment>> MemSnapshotStateCache::Get(
    const std::string &dataKey, const std::string &deviceId, uint64_t eventId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = cache_.find(BuildKey(dataKey, deviceId, eventId));
    if (iter == cache_.end()) {
        return std::nullopt;
    }
    return iter->second;
}

void MemSnapshotStateCache::Put(
    const std::string &dataKey, const std::string &deviceId, uint64_t eventId, const std::vector<Segment> &segments) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto key = BuildKey(dataKey, deviceId, eventId);
    if (cache_.find(key) == cache_.end()) {
        cacheOrder_.push_back(key);
    }
    cache_[key] = segments;
    TrimCache();
}

void MemSnapshotStateCache::ClearData(const std::string &dataKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto iter = cache_.begin(); iter != cache_.end();) {
        if (iter->first.rfind(dataKey + "|", 0) == 0) {
            cacheOrder_.erase(std::remove(cacheOrder_.begin(), cacheOrder_.end(), iter->first), cacheOrder_.end());
            iter = cache_.erase(iter);
        } else {
            ++iter;
        }
    }
}

void MemSnapshotStateCache::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    cacheOrder_.clear();
}

void MemSnapshotStateCache::TrimCache() {
    while (cache_.size() > MAX_CACHE_SIZE && !cacheOrder_.empty()) {
        cache_.erase(cacheOrder_.front());
        cacheOrder_.pop_front();
    }
}

std::string MemSnapshotStateCache::BuildKey(const std::string &dataKey, const std::string &deviceId, uint64_t eventId) {
    return dataKey + "|" + deviceId + "|" + std::to_string(eventId);
}
}
