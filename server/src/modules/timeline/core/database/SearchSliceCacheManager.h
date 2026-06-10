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

#ifndef PROFILER_SERVER_SEARCHSLICECACHEMANAGER_H
#define PROFILER_SERVER_SEARCHSLICECACHEMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <functional>
#include <numeric>
#include <algorithm>
#include <list>
#include "TimelineParamStrcut.h"

// clang-format off
namespace Dic {
namespace Module {
namespace Timeline {

// 表类型枚举
enum class SliceTableType : int8_t {
    TASK = 0,
    CANN_API = 1,
    PYTORCH_API = 2,
    COMMUNICATION_OP = 3,
    MSTX = 4,
    CCU = 5,
    UNKNOWN = -1
};

// 分页结果中的行标识
struct TargetRow {
    SliceTableType tableType;
    uint64_t rowId;
};

// 分页结果
struct PagedResult {
    std::vector<TargetRow> rows;
    uint64_t totalCount;
};

// 紧凑的 SoA 缓存结构
struct LightSliceCache {
    // 缓存标识
    std::string rankId; // 设备/卡 ID
    std::string primarySearchContent; // 一级筛选关键字

    // 字典缓存：id → name 映射（用于按名称排序和二级筛选）
    std::unordered_map<int32_t, std::string> dictMap;

    // SoA 主键数组（连续内存，CPU Cache 友好）
    std::vector<int8_t> tableTypes; // 数据来源表类型
    std::vector<int32_t> stringIds; // 字符串字典 ID（用于二级筛选）
    std::vector<uint64_t> rowIds; // 表内主键 ID
    std::vector<uint64_t> timestamps; // 开始时间戳
    std::vector<uint64_t> durations; // 持续时间

    // 排序索引数组
    std::vector<uint32_t> sortedIndices;

    // 当前排序状态
    std::string orderBy; // 排序字段
    std::string order; // 排序方向

    void clear() {
        rankId.clear();
        primarySearchContent.clear();
        dictMap.clear();
        tableTypes.clear();
        stringIds.clear();
        rowIds.clear();
        timestamps.clear();
        durations.clear();
        sortedIndices.clear();
        orderBy.clear();
        order.clear();
    }

    bool isDataValid(const std::string &rId, const std::string &content) const {
        return rankId == rId && content == primarySearchContent;
    }

    bool isOrderValid(const std::string &orderField, const std::string &orderDirection) const {
        return orderBy == orderField && order == orderDirection;
    }

    size_t size() const { return tableTypes.size(); }
};

// RAII 句柄：持有共享锁，确保调用方使用缓存期间不会被驱逐或删除
class CacheGuard {
  public:
    CacheGuard() : cache_(nullptr) {}

    // 持锁查找：构造时获取共享锁，确保 cache_ 在锁保护下被找到
    CacheGuard(std::shared_mutex &mutex,
               std::unordered_map<std::string, std::pair<LightSliceCache, std::list<std::string>::iterator>> &caches,
               const std::string &key)
        : lock_(mutex), cache_(nullptr)
    {
        auto it = caches.find(key);
        if (it != caches.end()) {
            cache_ = &it->second.first;
        }
    }

    CacheGuard(const CacheGuard &) = delete;
    CacheGuard &operator=(const CacheGuard &) = delete;
    CacheGuard(CacheGuard &&) = default;
    CacheGuard &operator=(CacheGuard &&) = default;

    LightSliceCache *get() const { return cache_; }
    LightSliceCache &operator*() const { return *cache_; }
    LightSliceCache *operator->() const { return cache_; }
    explicit operator bool() const { return cache_ != nullptr; }

  private:
    std::shared_lock<std::shared_mutex> lock_;
    LightSliceCache *cache_;
};

// 全局搜索缓存管理器（单例模式，带 LRU 驱逐机制）
class SearchSliceCacheManager {
  private:
    static constexpr size_t MAX_CACHE_SIZE = 5; // 最大缓存数量
    mutable std::shared_mutex cacheMutex_;
    std::list<std::string> lruList_; // LRU 链表（头部为最近使用）
    std::unordered_map<std::string, std::pair<LightSliceCache, std::list<std::string>::iterator>> caches_;

    SearchSliceCacheManager() = default;
    SearchSliceCacheManager(const SearchSliceCacheManager &) = delete;
    SearchSliceCacheManager &operator=(const SearchSliceCacheManager &) = delete;

  public:
    static SearchSliceCacheManager &Instance() {
        static SearchSliceCacheManager instance;
        return instance;
    }

    // 获取缓存（返回 RAII 句柄，持有共享锁保护缓存不被驱逐）
    CacheGuard get(const std::string &key) {
        return CacheGuard(cacheMutex_, caches_, key);
    }

    // 创建或获取缓存（写锁完成创建/LRU 更新，返回持有共享锁的 RAII 句柄）
    CacheGuard getOrCreate(const std::string &key) {
        // 写锁路径：创建或更新 LRU
        {
            std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
            auto it = caches_.find(key);
            if (it != caches_.end()) {
                auto listIt = it->second.second;
                lruList_.splice(lruList_.begin(), lruList_, listIt);
            } else {
                // 检查容量，驱逐最久未使用的缓存
                while (caches_.size() >= MAX_CACHE_SIZE) {
                    std::string oldestKey = lruList_.back();
                    lruList_.pop_back();
                    caches_.erase(oldestKey);
                }

                // 创建新缓存
                lruList_.push_front(key);
                caches_.emplace(key, std::make_pair(LightSliceCache(), lruList_.begin()));
            }
        }
        // 写锁已释放，构造 CacheGuard（共享锁 + 查找）
        return CacheGuard(cacheMutex_, caches_, key);
    }

    // 清理指定缓存
    void clear(const std::string &key) {
        std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
        auto it = caches_.find(key);
        if (it != caches_.end()) {
            lruList_.erase(it->second.second);
            caches_.erase(it);
        }
    }

    // 清理所有缓存
    void clearAll() {
        std::unique_lock<std::shared_mutex> writeLock(cacheMutex_);
        caches_.clear();
        lruList_.clear();
    }

    // 检查缓存是否存在
    bool exists(const std::string &key) const {
        std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
        return caches_.find(key) != caches_.end();
    }

    // 获取当前缓存数量
    size_t size() const {
        std::shared_lock<std::shared_mutex> readLock(cacheMutex_);
        return caches_.size();
    }

    // 生成缓存 key
    static std::string makeKey(const std::string &rankId, const std::string &searchContent) {
        return rankId + "|" + searchContent;
    }

    // 初始化排序索引
    static void InitializeSortedIndices(LightSliceCache &cache) {
        cache.sortedIndices.resize(cache.size());
        std::iota(cache.sortedIndices.begin(), cache.sortedIndices.end(), 0);
    }

    // 排序缓存
    static void SortCache(LightSliceCache &cache, const std::string &orderBy, const std::string &order) {
        if (cache.isOrderValid(orderBy, order)) {
            return; // 排序条件未变，无需重排
        }

        // 初始化索引数组
        InitializeSortedIndices(cache);

        // 根据排序字段选择比较函数
        std::function<bool(size_t, size_t)> comparator;

        if (orderBy == "name") {
            comparator = [&](size_t a, size_t b) {
                const std::string &nameA = cache.dictMap[cache.stringIds[a]];
                const std::string &nameB = cache.dictMap[cache.stringIds[b]];
                return order == "ascend" ? nameA < nameB : nameA > nameB;
            };
        } else if (orderBy == "timestamp") {
            comparator = [&](size_t a, size_t b) {
                return order == "ascend" ? cache.timestamps[a] < cache.timestamps[b]
                                         : cache.timestamps[a] > cache.timestamps[b];
            };
        } else if (orderBy == "duration") {
            comparator = [&](size_t a, size_t b) {
                return order == "ascend" ? cache.durations[a] < cache.durations[b]
                                         : cache.durations[a] > cache.durations[b];
            };
        } else {
            return; // 未知排序字段，使用默认顺序
        }

        // 执行排序
        std::sort(cache.sortedIndices.begin(), cache.sortedIndices.end(), comparator);

        // 更新排序状态
        cache.orderBy = orderBy;
        cache.order = order;
    }

    // 获取二级筛选匹配的 stringId 集合
    static std::unordered_set<int32_t> GetMatchedStringIds(
        const LightSliceCache &cache, const std::string &nameFilter) {
        std::unordered_set<int32_t> result;
        if (nameFilter.empty()) {
            return result;
        }

        std::string lowerFilter = nameFilter;
        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

        for (const auto &[id, name] : cache.dictMap) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find(lowerFilter) != std::string::npos) {
                result.insert(id);
            }
        }
        return result;
    }

    // 二级筛选与分页
    static PagedResult FilterAndPage(
        LightSliceCache &cache, const std::string &nameFilter, uint64_t current, uint64_t pageSize) {
        PagedResult result;
        result.totalCount = 0;

        // 防御性校验（入口 CheckParams 已保证合法，此处为兜底保护）
        if (current == 0 || pageSize == 0) {
            return result;
        }

        // Step 1: 二级筛选 - 获取匹配的 stringId 集合
        std::unordered_set<int32_t> matchedStringIds;
        if (!nameFilter.empty()) {
            matchedStringIds = GetMatchedStringIds(cache, nameFilter);
            if (matchedStringIds.empty()) {
                return result; // 无匹配结果
            }
        }

        // Step 2: 分页扫描
        uint64_t offset = (current - 1) * pageSize;
        uint64_t limit = pageSize;

        for (size_t i = 0; i < cache.sortedIndices.size(); ++i) {
            uint32_t realIdx = cache.sortedIndices[i];

            // 判断是否匹配二级筛选
            bool matches = nameFilter.empty() || matchedStringIds.count(cache.stringIds[realIdx]) > 0;

            if (matches) {
                // 判断是否在当前页范围内
                if (result.totalCount >= offset && result.totalCount < offset + limit) {
                    TargetRow row;
                    row.tableType = static_cast<SliceTableType>(cache.tableTypes[realIdx]);
                    row.rowId = cache.rowIds[realIdx];
                    result.rows.push_back(row);
                }
                result.totalCount++;
            }
        }

        return result;
    }
};

} // namespace Timeline
} // namespace Module
} // namespace Dic
// clang-format on

#endif // PROFILER_SERVER_SEARCHSLICECACHEMANAGER_H
