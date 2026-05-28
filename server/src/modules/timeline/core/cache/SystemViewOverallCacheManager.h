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

#ifndef PROFILER_SERVER_SYSTEMVIEWOVERALLCACHEMANAGER_H
#define PROFILER_SERVER_SYSTEMVIEWOVERALLCACHEMANAGER_H
#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include "TimelineProtocolResponse.h"

namespace Dic::Module::Timeline {
class SystemViewOverallCacheManager {
  public:
    static SystemViewOverallCacheManager &Instance() {
        static SystemViewOverallCacheManager overallCacheManager;
        return overallCacheManager;
    }
    SystemViewOverallCacheManager(const SystemViewOverallCacheManager &) = delete;
    SystemViewOverallCacheManager &operator=(const SystemViewOverallCacheManager &) = delete;
    SystemViewOverallCacheManager(SystemViewOverallCacheManager &&) = delete;
    SystemViewOverallCacheManager &operator=(SystemViewOverallCacheManager &&) = delete;

    void ClearAll() {
        std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
        systemViewOverallCacheMap.clear();
    }

    void SetOverallData(const std::string &key, const std::vector<Dic::Protocol::SystemViewOverallRes> &overallList) {
        std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
        systemViewOverallCacheMap[key] = overallList;
    }

    std::vector<Dic::Protocol::SystemViewOverallRes> GetOverallData(const std::string &key) {
        std::unique_lock<std::shared_mutex> uniqueLock(sharedMutex);
        auto it = systemViewOverallCacheMap.find(key);
        if (it == systemViewOverallCacheMap.end()) {
            return {};
        }
        return it->second;
    }

  private:
    SystemViewOverallCacheManager() = default;
    ~SystemViewOverallCacheManager() = default;
    std::map<std::string, std::vector<Dic::Protocol::SystemViewOverallRes>> systemViewOverallCacheMap;
    std::shared_mutex sharedMutex;
};
}
#endif // PROFILER_SERVER_SYSTEMVIEWOVERALLCACHEMANAGER_H
