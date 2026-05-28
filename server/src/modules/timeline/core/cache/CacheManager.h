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
#ifndef PROFILER_SERVER_CACHEMANAGER_H
#define PROFILER_SERVER_CACHEMANAGER_H
#include "SimulationSliceCacheManager.h"
#include "SliceCacheManager.h"
#include "SpinLockGuard.h"
#include "SystemViewOverallCacheManager.h"
#include "DomainObject.h"
namespace Dic::Module::Timeline {
class CacheManager {
  public:
    static CacheManager &Instance() {
        static CacheManager cacheManager;
        return cacheManager;
    }
    CacheManager(const CacheManager &) = delete;
    CacheManager &operator=(const CacheManager &) = delete;
    CacheManager(CacheManager &&) = delete;
    CacheManager &operator=(CacheManager &&) = delete;
    /**
     * 清理所有缓存
     */
    void ClearAll() {
        simulationSliceCacheManager.ClearAll();
        sliceCacheManager.Clear();
        overallCacheManager.ClearAll();
    }

    /**
     * 清理单卡缓存
     * @param rankId
     */
    void ClearCacheByRankId(const std::string &rankId) { simulationSliceCacheManager.ClearCacheByFileId(rankId); }

  private:
    CacheManager() = default;
    ~CacheManager() = default;
    SimulationSliceCacheManager &simulationSliceCacheManager = SimulationSliceCacheManager::Instance();
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    SystemViewOverallCacheManager &overallCacheManager = SystemViewOverallCacheManager::Instance();
    SpinLock mutex;
};
}
#endif // PROFILER_SERVER_CACHEMANAGER_H
