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

#ifndef PROFILER_SERVER_SIMULATIONSLICECACHEMANAGER_H
#define PROFILER_SERVER_SIMULATIONSLICECACHEMANAGER_H
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <vector>
#include <mutex>
#include "EventDef.h"
#include "SpinLockGuard.h"

namespace Dic::Module::Timeline {
class SimulationSliceCacheManager {
  public:
    static SimulationSliceCacheManager &Instance() {
        static SimulationSliceCacheManager simulationSliceCacheManager;
        return simulationSliceCacheManager;
    }
    SimulationSliceCacheManager(const SimulationSliceCacheManager &) = delete;
    SimulationSliceCacheManager &operator=(const SimulationSliceCacheManager &) = delete;
    SimulationSliceCacheManager(SimulationSliceCacheManager &&) = delete;
    SimulationSliceCacheManager &operator=(SimulationSliceCacheManager &&) = delete;
    std::vector<Trace::Slice> GetCompeteSlice(const std::map<std::string, Trace::Slice> setFlagSliceMap,
        std::map<std::string, Trace::Slice> waitFlagSliceMap, std::string field) {
        SpinLockGuard lock(mutex);
        std::vector<Trace::Slice> result;
        std::map<std::string, Trace::Slice> filedSetFlagSliceMap = allSetFlagSliceMap[field];
        std::map<std::string, Trace::Slice> filedWaitFlagSliceMap = allWaitFlagSliceMap[field];
        ProcessFlagMap(setFlagSliceMap, result, filedSetFlagSliceMap);
        ProcessFlagMap(waitFlagSliceMap, result, filedWaitFlagSliceMap);
        allSetFlagSliceMap[field] = filedSetFlagSliceMap;
        allWaitFlagSliceMap[field] = filedWaitFlagSliceMap;
        return result;
    }

    void ClearAll() {
        SpinLockGuard lock(mutex);
        allSetFlagSliceMap.clear();
        allWaitFlagSliceMap.clear();
    }

    void ClearCacheByFileId(const std::string &fileId) {
        SpinLockGuard lock(mutex);
        allSetFlagSliceMap.erase(fileId);
        allWaitFlagSliceMap.erase(fileId);
    }

  protected:
    void ProcessFlagMap(const std::map<std::string, Trace::Slice> &flagSliceMap, std::vector<Trace::Slice> &result,
        std::map<std::string, Trace::Slice> &filedFlagSliceMap) const {
        for (const auto &item : flagSliceMap) {
            if (filedFlagSliceMap.count(item.first) == 0) {
                filedFlagSliceMap[item.first] = item.second;
                return;
            }
            if (item.second.type == "SB") {
                Trace::Slice slice = item.second;
                slice.dur =
                    filedFlagSliceMap[item.first].ts > slice.ts ? filedFlagSliceMap[item.first].ts - slice.ts : 0;
                result.emplace_back(slice);
                filedFlagSliceMap.erase(item.first);
            }
            if (item.second.type == "SE") {
                Trace::Slice slice = filedFlagSliceMap[item.first];
                slice.dur =
                    slice.ts > filedFlagSliceMap[item.first].ts ? slice.ts - filedFlagSliceMap[item.first].ts : 0;
                result.emplace_back(slice);
                filedFlagSliceMap.erase(item.first);
            }
        }
    }

  private:
    SimulationSliceCacheManager() = default;
    ~SimulationSliceCacheManager() = default;

    std::map<std::string, std::map<std::string, Trace::Slice>> allSetFlagSliceMap;
    std::map<std::string, std::map<std::string, Trace::Slice>> allWaitFlagSliceMap;
    SpinLock mutex;
};
}

#endif // PROFILER_SERVER_SIMULATIONSLICECACHEMANAGER_H
