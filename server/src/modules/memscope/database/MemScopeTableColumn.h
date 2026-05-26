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

#ifndef PROFILER_SERVER_MEM_SCOPE_COLUMN_H
#define PROFILER_SERVER_MEM_SCOPE_COLUMN_H

#include "CommonRequests.h"

namespace Dic::Module::MemScope {
namespace EventTableColumn {
constexpr std::string_view ID = "ID";
constexpr std::string_view EVENT = "Event";
constexpr std::string_view EVENT_TYPE = "`Event Type`";
constexpr std::string_view NAME = "Name";
constexpr std::string_view TIMESTAMP = "`Timestamp(ns)`";
constexpr std::string_view PROCESS_ID = "`Process Id`";
constexpr std::string_view THREAD_ID = "`Thread Id`";
constexpr std::string_view DEVICE_ID = "`Device Id`";
constexpr std::string_view PTR = "Ptr";
constexpr std::string_view ATTR = "Attr";
// 可能存在的列
constexpr std::string_view CALL_STACK_PYTHON = "`Call Stack(Python)`";
constexpr std::string_view CALL_STACK_C = "`Call Stack(C)`";
inline const std::vector<Dic::Protocol::TableViewColumn> FIELD_FULL_COLUMNS = {
    {ID, "id", true, true, false, false}, // ID, 事件ID
    {EVENT, "event", true, false, true, false}, // Event, 事件类型
    {EVENT_TYPE, "eventType", true, true, true, false}, // Event Type, 事件子类型
    {NAME, "name", true, true, true, false}, // Name, 名称
    {TIMESTAMP, "_timestamp", true, true, false, true}, // Timestamp(ns), 发生时间, 可能涉及到计算, key与列名区分
    {PROCESS_ID, "processId", true, true, true, false}, // Process ID, 进程ID
    {THREAD_ID, "threadId", true, true, true, false}, // Thread ID, 线程ID
    {DEVICE_ID, "deviceId"}, {PTR, "ptr", true, true, true, false}, // Ptr, 内存地址
    {ATTR, "attr", true, false, true, false}, // Attr, 特有属性
    // 可选列
    {CALL_STACK_PYTHON, "callStackPython", true, false, true, false}, // Call Stack(Python), Python调用栈
    {CALL_STACK_C, "callStackC", true, false, true, false} // Call Stack(C), C调用栈
};
inline const std::set<std::string_view> TIMESTAMP_COLUMN_SET = {TIMESTAMP};
}
namespace MemoryAllocationTableColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view TIMESTAMP = "timestamp";
constexpr std::string_view TOTAL_SIZE = "totalSize";
constexpr std::string_view OPTIMIZED = "optimized";
constexpr std::string_view DEVICE_ID = "deviceId";
constexpr std::string_view EVENT_TYPE = "eventType";
constexpr std::string_view FULL_COLUMNS_WITHOUT_ID[] = {TIMESTAMP, TOTAL_SIZE, OPTIMIZED, DEVICE_ID, EVENT_TYPE};
constexpr std::string_view FULL_COLUMNS[] = {ID, TIMESTAMP, TOTAL_SIZE, OPTIMIZED, DEVICE_ID, EVENT_TYPE};
}
namespace MemoryBlockTableColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view DEVICE_ID = "deviceId";
constexpr std::string_view ADDR = "addr";
constexpr std::string_view SIZE = "size";
constexpr std::string_view START_TIMESTAMP = "startTimestamp";
constexpr std::string_view END_TIMESTAMP = "endTimestamp";
constexpr std::string_view EVENT_TYPE = "eventType";
constexpr std::string_view OWNER = "owner";
constexpr std::string_view ATTR = "attr";
constexpr std::string_view PROCESS_ID = "processId";
constexpr std::string_view THREAD_ID = "threadId";
constexpr std::string_view FIRST_ACCESS_TIMESTAMP = "firstAccessTimestamp";
constexpr std::string_view LAST_ACCESS_TIMESTAMP = "lastAccessTimestamp";
constexpr std::string_view MAX_ACCESS_INTERVAL = "maxAccessInterval";
constexpr std::string_view FULL_COLUMNS_WITHOUT_ID[] = {DEVICE_ID, ADDR, SIZE, START_TIMESTAMP, END_TIMESTAMP,
    EVENT_TYPE, OWNER, ATTR, PROCESS_ID, THREAD_ID, FIRST_ACCESS_TIMESTAMP, LAST_ACCESS_TIMESTAMP, MAX_ACCESS_INTERVAL};
inline const std::vector<Dic::Protocol::TableViewColumn> FIELD_FULL_COLUMNS = {
    {ID, "id", true, true, false, false}, // ID, 内存块ID
    {DEVICE_ID, "deviceId"}, {ADDR, "addr", true, true, true, false}, // Addr, 内存地址
    {SIZE, "size", true, true, false, true}, // Size, 内存块大小
    {START_TIMESTAMP, "_startTimestamp", true, true, false, true}, // Malloc Timestamp(ns), 申请时间
    {END_TIMESTAMP, "_endTimestamp", true, true, false, true}, // Free Timestamp(ns), 释放时间
    {EVENT_TYPE, "eventType", false, true, true, false}, // 事件子类型
    {OWNER, "owner", true, true, true, false}, // 内存块持有者(标签)
    {PROCESS_ID, "processId", true, true, true, false}, // Process Id, 进程ID
    {THREAD_ID, "threadId", true, true, true, false}, // Thread Id, 线程ID
    {ATTR, "attr", true, false, true, false}, // Attr, 特有属性
    {FIRST_ACCESS_TIMESTAMP, "_firstAccessTimestamp", true, false, false, true},
    {LAST_ACCESS_TIMESTAMP, "_lastAccessTimestamp", true, false, false, true},
    {MAX_ACCESS_INTERVAL, "maxAccessInterval", true, false, false, true}};
inline const std::set<std::string_view> TIMESTAMP_COLUMN_SET = {
    START_TIMESTAMP, END_TIMESTAMP, FIRST_ACCESS_TIMESTAMP, LAST_ACCESS_TIMESTAMP};
}
namespace PythonTraceTableColumn {
constexpr std::string_view ID = "ROWID"; // 表中无该列, 为sqlite表的隐藏列, 用于标识唯一一行
constexpr std::string_view FUNC_INFO = "FuncInfo";
constexpr std::string_view START_TIME = "`StartTime(ns)`";
constexpr std::string_view END_TIME = "`EndTime(ns)`";
constexpr std::string_view THREAD_ID = "`Thread Id`";
constexpr std::string_view PROCESS_ID = "`Process Id`";
constexpr std::string_view DEPTH = "Depth"; // 源数据中无该列, 为便于后续做查询抽样, 在解析时生成
}
}
#endif // PROFILER_SERVER_MEM_SCOPE_COLUMN_H
