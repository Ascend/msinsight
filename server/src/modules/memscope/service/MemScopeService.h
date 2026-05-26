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

#ifndef PROFILER_SERVER_MEM_SCOPE_SERVICE_H
#define PROFILER_SERVER_MEM_SCOPE_SERVICE_H

#include "MemScopeEventTree.h"
#include "MemScopeProtocolResponse.h"

namespace Dic {
namespace Module {
namespace MemScope {
class BuildDetailTreeContext {
  public:
    BuildDetailTreeContext(
        const std::string &deviceId, const uint64_t timestamp, const std::string &eventType, const bool relativeTime)
        : deviceId(deviceId), timestamp(timestamp), eventType(eventType), relativeTime(relativeTime) {}

    std::string deviceId;
    uint64_t timestamp{0};
    std::string eventType;
    bool relativeTime{false};
};

class MemScopeService {
  public:
    static bool BuildMemoryAllocDetailTreeByContext(
        std::unique_ptr<MemScopeMemoryDetailTreeNode> &detailTreePtr, BuildDetailTreeContext &buildCtx);

    // 判断eventType是否合法
    static bool IsValidMemoryEventType(const std::string &event, const std::string &eventType);
};
} // Memory
} // Module
} // Dic

#endif // PROFILER_SERVER_MEM_SCOPE_SERVICE_H
