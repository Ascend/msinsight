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
//
#ifndef PROFILER_SERVER_SLICECACHEMANAGER_H
#define PROFILER_SERVER_SLICECACHEMANAGER_H
#include <unordered_map>
#include <set>
#include <list>
#include <vector>
#include <mutex>
#include "DomainObject.h"
#include "SpinLockGuard.h"

namespace Dic::Module::Timeline {

static constexpr uint64_t MINUTE_NS = 60ULL * 1000ULL * 1000ULL * 1000ULL;

class SliceCacheManager {
  public:
    static SliceCacheManager &Instance() {
        static SliceCacheManager sliceCacheManager;
        return sliceCacheManager;
    }
    SliceCacheManager(const SliceCacheManager &) = delete;
    SliceCacheManager &operator=(const SliceCacheManager &) = delete;
    SliceCacheManager(SliceCacheManager &&) = delete;
    SliceCacheManager &operator=(SliceCacheManager &&) = delete;
    /* *
     * 全量DB场景下, 获取对应泳道的[start, end]时间区间内的算子
     * @param trackId
     * @param fileId
     * @return 返回的vector是先按timestamp升序，再按照id升序
     */
    std::vector<SliceDomain> GetSliceDomainVec(
        const std::string &trackId, const std::string &rankId, const SliceQuery &sliceQuery) {
        SpinLockGuard lock(mutex);
        std::string key = rankId + "@" + trackId;
        std::vector<SliceDomain> emptyValue;
        auto it = cache.find(key);
        if (it == cache.end()) {
            return emptyValue;
        }
        // 全量DB场景下, 若前端传入区间未命中缓存区间范围，同样需要返回空值，更新缓存
        auto durIt = cacheDuration.find(key);
        if (durIt == cacheDuration.end()) {
            return emptyValue;
        }
        // Text场景下，默认startTime与endTime都为0，不进入该if分支，直接返回全量缓存
        if (sliceQuery.startTime != sliceQuery.endTime) {
            auto [start, end] = durIt->second;
            if (start > sliceQuery.startTime || end < sliceQuery.endTime) {
                return emptyValue;
            }
        }
        Touch(it);
        return it->second.first;
    }

    /* *
     * 获取当前泳道算子深度信息
     * @param trackId
     * @param depthInfo
     * @return
     */
    bool QueryDepthInfoWithoutTimeRange(
        const std::string &trackId, const std::string &fileId, std::unordered_map<uint64_t, uint32_t> &depthInfo) {
        SpinLockGuard lock(mutex);
        std::string key = fileId + "@" + trackId;
        auto it = cache.find(key);
        if (it == cache.end()) {
            return false;
        }
        Touch(it);
        for (const auto &item : it->second.first) {
            depthInfo[item.id] = item.depth;
        }
        return true;
    }

    /* *
     * 全量DB场景下, 获取当前泳道的[start, end]时间区间内的算子的深度信息
     * @param trackId, sliceQuery.trackId
     * @param fileId, sliceQuery.rankId 用于支持服务化分布式db导入, key: filedId @ trackId
     * @param depthInfo
     * @return
     */
    bool QueryDepthInfo(std::unordered_map<uint64_t, uint32_t> &depthInfo, const SliceQuery &sliceQuery) {
        SpinLockGuard lock(mutex);
        // key: filedId @ trackId
        std::string key = sliceQuery.rankId + "@" + std::to_string(sliceQuery.trackId);
        auto it = cache.find(key);
        if (it == cache.end()) {
            return false;
        }
        // 全量DB场景下, 若前端传入区间未命中缓存区间范围，同样需要返回false，更新缓存
        auto durIt = cacheDuration.find(key);
        if (durIt == cacheDuration.end()) {
            return false;
        }
        // Text场景下，默认startTime与endTime都为0，不进入该if分支，直接返回全量缓存
        if (sliceQuery.startTime != sliceQuery.endTime) {
            auto [start, end] = durIt->second;
            if (start > sliceQuery.startTime || end < sliceQuery.endTime) {
                return false;
            }
        }
        Touch(it);
        for (const auto &item : it->second.first) {
            depthInfo[item.id] = item.depth;
        }
        return true;
    }

    /* *
     * 更新缓存，调用前需要对value按照timestamp排序，再按照id排序
     * @param trackId 对应泳道的trackId
     * @param value 对应泳道所有的简单算子信息，该vector先按照timestamp排序，再按照id排序
     */
    void UpdateSliceCache(
        const std::string &trackId, const std::vector<SliceDomain> &value, const SliceQuery &slicePagedQuery) {
        SpinLockGuard lock(mutex);
        if (std::empty(value)) {
            return;
        }
        std::string key = slicePagedQuery.rankId + "@" + trackId;
        auto it = cache.find(key);
        if (it == cache.end()) {
            while (curCapacity >= allCapacity) {
                // 此处上下文逻辑可以保证curCapacity大于cache[used.back()].first.size()
                curCapacity -= cache[used.back()].first.size();
                cache.erase(used.back());
                used.pop_back();
            }
            used.push_front(key);
            cache[key] = {value, used.begin()};
            curCapacity += value.size();
            // 添加算子缓存时间区间
            cacheDuration[key] = {slicePagedQuery.startTime, slicePagedQuery.endTime};
            return;
        }
        // 对于key命中cache的情况，仅当slicePagedQuery中有内容时，更新cacheDuration[key]
        if (slicePagedQuery.endTime != 0) {
            cacheDuration[key] = {slicePagedQuery.startTime, slicePagedQuery.endTime};
        }
        Touch(it);
        cache[key] = {value, used.begin()};
    }

    static SliceQuery GetSlicePagedQuery(const SliceQuery &sliceQuery) {
        if (sliceQuery.metaType == PROCESS_TYPE::TEXT) {
            SliceQuery result = sliceQuery;
            result.startTime = 0;
            result.endTime = UINT64_MAX;
            return result;
        } else {
            return GetSlicePagedQueryForDb(sliceQuery);
        }
    }

    /**
     * 由sliceQuery获取算子缓存分页参数slicePagedQuery, 区别主要是[startTime, endTime]所对应时间区间不同
     * @param sliceQuery 前端传回起止区间，用于从缓存中查询算子信息, 其起止时间区间通常小于slicePagedQuery
     * @return slicePagedQuery 用于从数据库中查询算子信息，更新分页缓存, 时长5min
     */
    static SliceQuery GetSlicePagedQueryForDb(const SliceQuery &sliceQuery) {
        const uint64_t threshold = 5 * MINUTE_NS; // 5 minutes
        const uint64_t halfThreshold = threshold / 2; // 2.5 minutes
        SliceQuery result = sliceQuery;

        // 取离区间中点最近且合法的3min
        uint64_t mid = (sliceQuery.startTime + sliceQuery.endTime) / 2;
        if (mid < halfThreshold) {
            result.startTime = 0;
            result.endTime = std::max(threshold, sliceQuery.endTime);
            return result;
        }
        result.startTime = std::min(mid - halfThreshold, sliceQuery.startTime);
        result.endTime = std::max(mid + halfThreshold, sliceQuery.endTime);
        return result;
    }

    std::vector<uint64_t> GetPythonFunctionIdVec(const std::string &key, const SliceQuery &sliceQuery) {
        SpinLockGuard lock(mutex);
        auto it = pythonFunctionIDCache.find(key);
        std::vector<uint64_t> emptyValue;
        if (it == pythonFunctionIDCache.end()) {
            return emptyValue;
        }
        // 若前端传入区间未命中缓存区间范围，同样需要返回空值，更新缓存
        auto durIt = pythonCacheDuration.find(key);
        if (durIt == pythonCacheDuration.end()) {
            return emptyValue;
        }
        auto [start, end] = durIt->second;
        if (start > sliceQuery.startTime || end < sliceQuery.endTime) {
            return emptyValue;
        }
        Touch(it);
        return it->second.first;
    }

    void PutPythonFunctionIdVec(
        const std::string &key, const std::vector<uint64_t> &value, const SliceQuery &slicePagedQuery = SliceQuery()) {
        SpinLockGuard lock(mutex);
        auto it = pythonFunctionIDCache.find(key);
        if (it != pythonFunctionIDCache.end()) {
            Touch(it);
        } else {
            if (pythonFunctionIDCache.size() == pythonCapacity) {
                pythonFunctionIDCache.erase(pythonFunctionIdUsed.back());
                pythonFunctionIdUsed.pop_back();
            }
            pythonFunctionIdUsed.push_front(key);
        }
        pythonFunctionIDCache[key] = {value, pythonFunctionIdUsed.begin()};
        // 仅当slicePagedQuery中有内容时，更新pythonCacheDuration[key]
        if (slicePagedQuery.endTime != 0) {
            pythonCacheDuration[key] = {slicePagedQuery.startTime, slicePagedQuery.endTime};
        }
    }

    PYTHON_FUNCTION_STATUS GetPythonFunctionStatus(const uint64_t trackId) {
        SpinLockGuard lock(mutex);
        if (trackIdAndPythonFunctionMap.count(trackId) > 0) {
            return trackIdAndPythonFunctionMap[trackId];
        }
        return PYTHON_FUNCTION_STATUS::UNKNOWN;
    }

    void SetPythonFunctionStatus(const uint64_t trackId, PYTHON_FUNCTION_STATUS status) {
        SpinLockGuard lock(mutex);
        trackIdAndPythonFunctionMap[trackId] = status;
    }

    void Clear() {
        SpinLockGuard lock(mutex);
        cache.clear();
        cacheDuration.clear();
        pythonCacheDuration.clear();
        used.clear();
        trackIdAndPythonFunctionMap.clear();
        pythonFunctionIDCache.clear();
        pythonFunctionIdUsed.clear();
    }

  private:
    SliceCacheManager() = default;
    ~SliceCacheManager() = default;
    using VisitOrderList = std::list<std::string>;
    using CacheValue = std::pair<std::vector<SliceDomain>, VisitOrderList::iterator>;
    using PythonFunctionIDCache = std::pair<std::vector<uint64_t>, VisitOrderList::iterator>;
    using CacheMap = std::unordered_map<std::string, CacheValue>;
    using PythonFunctionMap = std::unordered_map<std::string, PythonFunctionIDCache>;
    using CacheDurationMap = std::unordered_map<std::string, std::pair<uint64_t, uint64_t>>;

    // 算子缓存
    CacheMap cache;
    // 算子缓存使用记录
    VisitOrderList used;
    // 算子缓存时间区间, <filedId@trackId, <startTime, endTime>>
    CacheDurationMap cacheDuration;
    CacheDurationMap pythonCacheDuration;
    SpinLock mutex;
    // 算子缓存大小上限
    const uint64_t allCapacity = 100000000;
    // 当前缓存大小
    uint64_t curCapacity = 0;

    // trackId 是否存在python function
    std::unordered_map<uint64_t, PYTHON_FUNCTION_STATUS> trackIdAndPythonFunctionMap;

    // 算子调用栈id缓存
    PythonFunctionMap pythonFunctionIDCache;
    // 算子调用栈id缓存使用记录
    VisitOrderList pythonFunctionIdUsed;
    // 算子调用栈id缓存大小
    const size_t pythonCapacity = 3;

    // 更新算子缓存使用记录
    void Touch(CacheMap::iterator it) {
        std::string key = it->first;
        used.erase(it->second.second);
        used.push_front(key);
        it->second.second = used.begin();
    }

    // 更新算子缓存使用记录
    void Touch(PythonFunctionMap::iterator it) {
        std::string key = it->first;
        pythonFunctionIdUsed.erase(it->second.second);
        pythonFunctionIdUsed.push_front(key);
        it->second.second = pythonFunctionIdUsed.begin();
    }
};
}
#endif // PROFILER_SERVER_SLICECACHEMANAGER_H
