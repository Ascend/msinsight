/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_GENERICCACHEMANAGER_H
#define PROFILER_SERVER_GENERICCACHEMANAGER_H

#pragma once

#include <unordered_map>
#include <list>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <memory>
#include <future>
#include "ServerLog.h"
#include "SystemUtil.h"

// 通用缓存管理器（支持任意类型）
// KeyType 必须是数字或者字符串
template <typename KeyType, typename ValueType> class GenericCacheManager {
  public:
    struct Config {
        size_t maxSize = 10; // 最大缓存条目数
        std::chrono::minutes ttl = std::chrono::hours(24); // 缓存有效期
        bool enableLRU = true; // 是否启用LRU策略
        bool enableMemoryCheck = false; // 是否启用内存检查
        uint64_t reservedMemoryBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 保留内存（2GB），可用内存低于此值时不缓存
        uint64_t memoryBudgetDenominator = 3; // 可用内存的 1/N 作为缓存预算
        std::function<uint64_t(const ValueType &)> estimateBytes; // 估算缓存数据占用的内存字节数
    };

    struct CacheStats {
        size_t hits = 0;
        size_t misses = 0;
        size_t evictions = 0;
        size_t totalQueries = 0;
    };

    explicit GenericCacheManager(const Config &config = Config()) : config_(config) {}

    // 通用查询方法（带缓存，支持 single-flight 防止缓存击穿）
    template <typename FetchFunc> ValueType GetOrFetch(const KeyType &key, FetchFunc fetchFunc) {
        // 1. 检查缓存
        auto cachedValue = CheckCache(key);
        if (cachedValue) {
            // Note: 此处只希望 key 是数字或者字符串。
            Dic::Server::ServerLog::Info("GenericCache hit by key: %.", key);
            return *cachedValue;
        }

        // 2. Single-flight：检查是否有其他线程正在获取同一 Key
        std::shared_ptr<std::promise<ValueType>> promise;
        std::shared_future<ValueType> sharedFuture;
        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto it = pendingFutures_.find(key);
            if (it != pendingFutures_.end()) {
                sharedFuture = it->second;
            } else {
                promise = std::make_shared<std::promise<ValueType>>();
                sharedFuture = promise->get_future().share();
                pendingFutures_[key] = sharedFuture;
            }
        }

        if (!promise) {
            // 等待其他线程获取完成并返回结果
            return sharedFuture.get();
        }

        // 3. 当前线程负责执行 fetchFunc，异常时先记录日志，再统一通知等待者
        try {
            ValueType value = fetchFunc();
            InsertCache(key, value);
            promise->set_value(std::move(value));
        } catch (const std::exception &e) {
            Dic::Server::ServerLog::Error("GenericCache fetch failed for key: %, error: %", key, e.what());
            promise->set_exception(std::current_exception());
        } catch (...) {
            Dic::Server::ServerLog::Error("GenericCache fetch failed for key: %, unknown error", key);
            promise->set_exception(std::current_exception());
        }

        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pendingFutures_.erase(key);
        }

        return sharedFuture.get();
    }

    // 检查缓存（内部方法）
    std::optional<ValueType> CheckCache(const KeyType &key) {
        std::lock_guard<std::mutex> lock(mutex_);

        stats_.totalQueries++;

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // 检查TTL
            auto now = std::chrono::steady_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.timestamp);

            if (age < config_.ttl) {
                // 更新LRU顺序
                if (config_.enableLRU) {
                    lruList_.erase(it->second.lruIter);
                    lruList_.push_front(key);
                    it->second.lruIter = lruList_.begin();
                }

                stats_.hits++;
                return it->second.data;
            }

            // 缓存过期，删除
            if (config_.enableLRU) {
                lruList_.erase(it->second.lruIter);
            }
            cache_.erase(it);
        }

        stats_.misses++;
        return std::nullopt;
    }

    // 写入缓存（内部方法）
    void InsertCache(const KeyType &key, const ValueType &value) {
        if (config_.enableMemoryCheck && !CheckMemoryBudget(value)) {
            Dic::Server::ServerLog::Info("GenericCache: skip caching key % due to insufficient memory.", key);
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // 如果缓存已满，执行淘汰
        if (cache_.size() >= config_.maxSize && config_.maxSize > 0) {
            Evict();
        }

        // 插入新缓存
        CacheEntry entry;
        entry.data = value;
        entry.timestamp = std::chrono::steady_clock::now();

        if (config_.enableLRU) {
            lruList_.push_front(key);
            entry.lruIter = lruList_.begin();
        }

        cache_[key] = std::move(entry);
    }

    // 清除缓存
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        lruList_.clear();
    }

    // 清理过期缓存
    void CleanupExpired() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        std::vector<KeyType> toErase;

        for (const auto &[key, entry] : cache_) {
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - entry.timestamp);
            if (age >= config_.ttl) {
                toErase.push_back(key);
            }
        }

        for (const auto &key : toErase) {
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                if (config_.enableLRU) {
                    lruList_.erase(it->second.lruIter);
                }
                cache_.erase(it);
            }
        }
    }

    // 获取缓存统计
    CacheStats GetStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    // 获取缓存大小
    size_t GetSize() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

  private:
    struct CacheEntry {
        ValueType data;
        std::chrono::steady_clock::time_point timestamp;
        typename std::list<KeyType>::iterator lruIter;
    };

    // LRU淘汰
    void Evict() {
        if (!config_.enableLRU || lruList_.empty()) {
            // 简单策略：删除任意一个
            cache_.erase(cache_.begin());
            stats_.evictions++;
            return;
        }

        // LRU策略：删除最久未使用的
        KeyType lruKey = lruList_.back();
        cache_.erase(lruKey);
        lruList_.pop_back();
        stats_.evictions++;
    }

    // 检查可用内存是否满足缓存预算
    bool CheckMemoryBudget(const ValueType &value) const {
        const uint64_t availableBytes = Dic::SystemUtil::GetAvailablePhysicalMemoryBytes();
        if (availableBytes <= config_.reservedMemoryBytes) {
            return false;
        }
        const uint64_t ratioBudget = availableBytes / config_.memoryBudgetDenominator;
        const uint64_t reserveBudget = availableBytes - config_.reservedMemoryBytes;
        const uint64_t budget = std::min(ratioBudget, reserveBudget);
        if (config_.estimateBytes) {
            const uint64_t estimatedBytes = config_.estimateBytes(value);
            Dic::Server::ServerLog::Info("GenericCache: estimated bytes %, memory budget %.", estimatedBytes, budget);
            return estimatedBytes <= budget;
        }
        return budget > 0;
    }

  private:
    Config config_;
    mutable std::mutex mutex_;
    std::unordered_map<KeyType, CacheEntry> cache_;
    std::list<KeyType> lruList_;
    mutable CacheStats stats_;

    // Single-flight：防止同一 Key 并发击穿
    std::mutex pendingMutex_;
    std::unordered_map<KeyType, std::shared_future<ValueType>> pendingFutures_;
};

#endif //PROFILER_SERVER_GENERICCACHEMANAGER_H
