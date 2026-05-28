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
#ifndef PROFILER_SERVER_SERVER_SRC_MODULES_TIMELINE_CORE_PARSER_JSONPARSEMEMPOOL_H_
#define PROFILER_SERVER_SERVER_SRC_MODULES_TIMELINE_CORE_PARSER_JSONPARSEMEMPOOL_H_
#include "rapidjson.h"
#include "allocators.h"
#include <unordered_map>
#include <mutex>
#include <memory>
namespace Dic {
namespace Module {

class JsonParseMemPool {
  public:
    static JsonParseMemPool &Instance() {
        static JsonParseMemPool pool;
        return pool;
    }

    void Clear() {
        std::unique_lock lock(mutex);
        memPoolMap.clear();
    }

    std::shared_ptr<rapidjson::MemoryPoolAllocator<>> GetMemBuff(std::thread::id threadId) {
        constexpr size_t memoryPoolSize = 5 * 1024 * 1024;
        std::unique_lock lock(mutex);
        if (memPoolMap.find(threadId) == memPoolMap.end()) {
            memPoolMap[threadId] = std::make_shared<rapidjson::MemoryPoolAllocator<>>(memoryPoolSize);
        }
        return memPoolMap[threadId];
    }

  private:
    JsonParseMemPool() = default;
    ~JsonParseMemPool() = default;
    std::mutex mutex;
    std::unordered_map<std::thread::id, std::shared_ptr<rapidjson::MemoryPoolAllocator<>>> memPoolMap;
};

} // Module
} // Dic

#endif // PROFILER_SERVER_SERVER_SRC_MODULES_TIMELINE_CORE_PARSER_JSONPARSEMEMPOOL_H_
