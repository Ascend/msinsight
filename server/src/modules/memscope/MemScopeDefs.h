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

#ifndef PROFILER_SERVER_MEM_SCOPE_DEFS_H
#define PROFILER_SERVER_MEM_SCOPE_DEFS_H

#include "pch.h"
#include "FileDef.h"

namespace Dic::Module::MemScope {
const char OWNER_STRING_DELIMITER = '@';
// 内存事件
struct MEM_SCOPE_DUMP_EVENT {
    inline const static std::string MALLOC = "MALLOC"; // 内存申请事件
    inline const static std::string FREE = "FREE"; // 内存释放事件
    inline const static std::string ACCESS = "ACCESS"; // 内存访问
    inline const static std::string OP_LAUNCH = "OP_LAUNCH"; // 算子launch
    inline const static std::string KERNEL_LAUNCH = "KERNEL_LAUNCH"; // kernel launch
    inline const static std::string SYSTEM = "SYSTEM"; // 系统事件
};
// 内存事件类型
struct MEM_SCOPE_DUMP_EVENT_TYPE {
    // 内存申请/释放事件 子类型
    inline const static std::string MALLOC_FREE_PTA = "PTA"; // PTA内存池分配(从PTA内存池申请)
    inline const static std::string MALLOC_FREE_MINDSPORE = "MINDSPORE"; // MINDSPORE内存池分配(从MINDSPORE内存池申请)
    inline const static std::string MALLOC_FREE_ATB = "ATB"; // ATB申请
    inline const static std::string MALLOC_FREE_HAL = "HAL"; // 从HAL申请
    inline const static std::string MALLOC_FREE_PTA_WORKSPACE = "PTA_WORKSPACE"; // 从workspace申请
    // 内存访问事件 子类型
    inline const static std::string ACCESS_READ = "READ"; // 内存读事件
    inline const static std::string ACCESS_WRITE = "WRITE"; // 内存写事件
    // 算子launch事件 子类型
    inline const static std::string OP_LAUNCH_ATEN_START = "ATEN_START"; // aten开始, name字段标识aten算子名
    inline const static std::string OP_LAUNCH_ATEN_END = "ATEN_END"; // aten结束
    // kernel launch事件 子类型
    inline const static std::string KERNEL_LAUNCH = "KERNEL_LAUNCH"; // kernel launch
    inline const static std::string KERNEL_LAUNCH_START = "KERNEL_EXECUTE_START"; // kernel launch 开始
    inline const static std::string KERNEL_LAUNCH_END = "KERNEL_EXECUTE_END"; // kernel launch 结束
    // 系统事件 子类型
    inline const static std::string SYSTEM_ACL_INIT = "ACL_INIT"; // ACL初始化
    inline const static std::string SYSTEM_ACL_FINI = "ACL_FINI"; // ACL结束
};
// 内存事件 - 类型映射表
const std::unordered_map<std::string, std::set<std::string>> EVENT_TYPE_MAP = {
    {MEM_SCOPE_DUMP_EVENT::MALLOC,
        {MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_MINDSPORE,
            MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_ATB, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HAL,
            MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA_WORKSPACE}},
    {MEM_SCOPE_DUMP_EVENT::FREE,
        {MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_MINDSPORE,
            MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_ATB, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HAL,
            MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA_WORKSPACE}},
    {MEM_SCOPE_DUMP_EVENT::ACCESS, {MEM_SCOPE_DUMP_EVENT_TYPE::ACCESS_READ, MEM_SCOPE_DUMP_EVENT_TYPE::ACCESS_WRITE}},
    {MEM_SCOPE_DUMP_EVENT::OP_LAUNCH,
        {MEM_SCOPE_DUMP_EVENT_TYPE::OP_LAUNCH_ATEN_START, MEM_SCOPE_DUMP_EVENT_TYPE::OP_LAUNCH_ATEN_END}},
    {MEM_SCOPE_DUMP_EVENT::KERNEL_LAUNCH,
        {MEM_SCOPE_DUMP_EVENT_TYPE::KERNEL_LAUNCH, MEM_SCOPE_DUMP_EVENT_TYPE::KERNEL_LAUNCH_START,
            MEM_SCOPE_DUMP_EVENT_TYPE::KERNEL_LAUNCH_END}},
    {MEM_SCOPE_DUMP_EVENT::SYSTEM,
        {MEM_SCOPE_DUMP_EVENT_TYPE::SYSTEM_ACL_INIT, MEM_SCOPE_DUMP_EVENT_TYPE::SYSTEM_ACL_FINI}}};

struct MemScopeEvent {
    uint64_t id{0};
    std::string event;
    std::string eventType;
    std::string name;
    uint64_t timestamp{0};
    uint64_t processId{0};
    uint64_t threadId{0};
    std::string deviceId;
    std::string ptr;
    std::string attr;
    int64_t size{0};
    std::string callStackC;
    std::string callStackPython;
};

struct MemoryAllocation {
    uint64_t id{0};
    uint64_t timestamp{0};
    uint64_t totalSize{0};
    std::string deviceId;
    std::string eventType;
    bool optimized{false};

    MemoryAllocation() = default;
    MemoryAllocation(
        uint64_t timestamp, uint64_t totalSize, std::string deviceId, std::string eventType, bool optimized)
        : id(0), timestamp(timestamp), totalSize(totalSize), deviceId(std::move(deviceId)),
          eventType(std::move(eventType)), optimized(optimized) {}
};

struct MemoryBlock {
    uint64_t id{0};
    std::string ptr;
    std::string deviceId;
    uint64_t size{0};
    uint64_t startTimestamp{0};
    uint64_t endTimestamp{0};
    std::string owner;
    std::string eventType;
    std::string attrJsonString;
    uint64_t processId{0};
    uint64_t threadId{0};
    int64_t firstAccessTimestamp{-1};
    int64_t lastAccessTimestamp{-1};
    uint64_t maxAccessInterval{0};
    // 低效显存标识
    bool lazyUsed{false};
    bool delayedFree{false};
    bool longIdle{false};

    MemoryBlock() = default;
    virtual ~MemoryBlock() = default;
    MemoryBlock(std::string ptr, std::string deviceId, uint64_t size, uint64_t startTs, uint64_t endTs,
        std::string owner, std::string eventType, std::string attrJsonString, uint64_t pid, uint64_t tid)
        : id(0), ptr(std::move(ptr)), deviceId(std::move(deviceId)), size(size), startTimestamp(startTs),
          endTimestamp(endTs), owner(std::move(owner)), eventType(std::move(eventType)),
          attrJsonString(std::move(attrJsonString)), processId(pid), threadId(tid) {}
};
} // namespace Dic::Module::MemScope
#endif // PROFILER_SERVER_MEM_SCOPE_DEFS_H
