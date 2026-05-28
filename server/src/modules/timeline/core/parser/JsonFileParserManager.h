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

#ifndef PROFILER_SERVER_JSON_FILE_PARSER_MANAGER_H
#define PROFILER_SERVER_JSON_FILE_PARSER_MANAGER_H
#include <memory>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>

#include "ACLGraphDebugJsonFileParser.h"
#include "FileParser.h"

namespace Dic::Module::Timeline {
/**
     * @brief JSON 统一解析器生命周期管理器（线程安全）
     *
     * 设计原则：
     * 1. Meyers' Singleton 实现
     * 2. 统一管理线程池
     */
class JsonFileParserManager {
  private:
    JsonFileParserManager();
    ~JsonFileParserManager();

    // C++17 读写锁：高频读（GetParser）/低频写（Reset）
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::type_index, std::unique_ptr<FileParser>> parsers;

    const uint32_t maxThreadNum = 64;
    // ✅ JSON 文件解析器全局唯一共享线程池（所有解析器共用）
    std::shared_ptr<ThreadPool> globalThreadPool_;

    template <typename T> T &GetOrCreateParser() {
        // 1. 先尝试无锁读（快速路径）
        {
            std::shared_lock lock(mutex_);
            if (const auto it = parsers.find(std::type_index(typeid(T))); it != parsers.end()) {
                return static_cast<T &>(*it->second);
            }
        }

        // 2. 未命中：加写锁创建
        std::unique_lock lock(mutex_);
        // 双重检查（防止其他线程已创建）
        const auto it = parsers.find(std::type_index(typeid(T)));
        if (it == parsers.end()) {
            auto parser = std::make_unique<T>(globalThreadPool_);
            T &ref = static_cast<T &>(*parser);
            parsers.emplace(std::type_index(typeid(T)), std::move(parser));
            return ref;
        }
        return static_cast<T &>(*it->second);
    }

    void InitThreadPool();

  public:
    // 禁止拷贝/移动
    JsonFileParserManager(const JsonFileParserManager &) = delete;
    JsonFileParserManager &operator=(const JsonFileParserManager &) = delete;
    JsonFileParserManager(JsonFileParserManager &&) = delete;
    JsonFileParserManager &operator=(JsonFileParserManager &&) = delete;

    static JsonFileParserManager &Instance();

    // 需调用模板函数，定义在头文件
    static ACLGraphDebugJsonFileParser &GetACLGraphDebugParser() {
        return Instance().GetOrCreateParser<ACLGraphDebugJsonFileParser>();
    }

    // 需调用模板函数，定义在头文件
    static TraceFileParser &GetTraceFileParser() { return Instance().GetOrCreateParser<TraceFileParser>(); }

    static void ResetAll();
};
} // namespace Dic::Module::Timeline

#endif //PROFILER_SERVER_JSON_FILE_PARSER_MANAGER_H
