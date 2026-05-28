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

#include "JsonFileParserManager.h"

namespace Dic::Module::Timeline {
JsonFileParserManager::JsonFileParserManager() { InitThreadPool(); }
JsonFileParserManager::~JsonFileParserManager() { globalThreadPool_->ShutDown(); }

JsonFileParserManager &JsonFileParserManager::Instance() {
    static JsonFileParserManager instance;
    return instance;
}

void JsonFileParserManager::ResetAll() {
    std::shared_lock lock(Instance().mutex_);
    for (auto &[_, parser] : Instance().parsers) {
        parser->Reset(); // 调用各解析器的Reset()
    }
}

void JsonFileParserManager::InitThreadPool() {
    // 设置解析内存池的大小，根据32卡数据测试，性能最好的线程数大约为64
    const uint32_t hardwareLimit = std::thread::hardware_concurrency();
    const uint32_t threadCount = std::min(hardwareLimit, maxThreadNum);
    globalThreadPool_ = std::make_shared<ThreadPool>(threadCount);
}
}
