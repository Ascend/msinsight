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

#ifndef PROFILER_SERVER_MEMSNAPSHOTSTATECACHE_H
#define PROFILER_SERVER_MEMSNAPSHOTSTATECACHE_H

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "MemSnapshotDefs.h"

namespace Dic::Module::MemSnapshot {
class MemSnapshotStateCache {
  public:
    static std::optional<std::vector<Segment>> Get(
        const std::string &dataKey, const std::string &deviceId, uint64_t eventId);
    static void Put(const std::string &dataKey, const std::string &deviceId, uint64_t eventId,
        const std::vector<Segment> &segments);
    static void ClearData(const std::string &dataKey);
    static void ClearAll();

  private:
    static std::string BuildKey(const std::string &dataKey, const std::string &deviceId, uint64_t eventId);
    static void TrimCache();
    static constexpr size_t MAX_CACHE_SIZE = 32;
    static std::mutex mutex_;
    static std::unordered_map<std::string, std::vector<Segment>> cache_;
    static std::deque<std::string> cacheOrder_;
};
}

#endif // PROFILER_SERVER_MEMSNAPSHOTSTATECACHE_H
