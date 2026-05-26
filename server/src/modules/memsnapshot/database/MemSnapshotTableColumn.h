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

#ifndef PROFILER_SERVER_MEMSNAPSHOTTABLECOLUMN_H
#define PROFILER_SERVER_MEMSNAPSHOTTABLECOLUMN_H
#include "CommonRequests.h"

namespace Dic::Module::MemSnapshot {
namespace TraceEntryTableColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view ACTION = "action";
constexpr std::string_view ADDRESS = "address";
constexpr std::string_view SIZE = "size";
constexpr std::string_view STREAM = "stream";
constexpr std::string_view ALLOCATED = "allocated";
constexpr std::string_view ACTIVE = "active";
constexpr std::string_view RESERVED = "reserved";
constexpr std::string_view CALLSTACK = "callstack";

inline const std::vector<Dic::Protocol::TableViewColumn> FIELD_FULL_COLUMNS = {
    {"ID", ID, true, true, false, true}, // ID, 事件索引
    {"Action", ACTION, true, true, true, false}, // 事件类型
    {"Address", ADDRESS, true, true, true, false}, // 事件地址
    {"Size(KBytes)", SIZE, true, true, false, true}, // 事件关联内存大小，单位KBytes
    {"Stream", STREAM, true, true, true, false}, // 事件所属流ptr
    {"Allocated(KBytes)", ALLOCATED, true, true, false, true}, // 事件发生时刻allocated内存大小，单位KBytes
    {"Active(KBytes)", ACTIVE, true, true, false, true}, // 事件发生时刻active内存大小，单位KBytes
    {"Reserved(KBytes)", RESERVED, true, true, false, true}, // 事件发生时刻预留内存大小，单位KBytes
    {"CallStack", CALLSTACK, true, false, true, false} // 事件发生的调用栈
};
}

namespace BlockTableColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view ADDRESS = "address";
constexpr std::string_view SIZE = "size";
constexpr std::string_view REQUESTED_SIZE = "requestedSize";
constexpr std::string_view STATE = "state";
constexpr std::string_view ALLOC_EVENT_ID = "allocEventId";
constexpr std::string_view FREE_EVENT_ID = "freeEventId";
inline const std::vector<Dic::Protocol::TableViewColumn> FIELD_FULL_COLUMNS = {{"ID", ID, true, true, false, true},
    {"Address", ADDRESS, true, true, true, false},
    {"Size(KBytes)", SIZE, true, true, false, true}, // 实际内存块大小，单位KBytes
    {"Requested Size(KBytes)", REQUESTED_SIZE, true, true, false, true}, // 分配事件请求内存大小，单位KBytes
    {"State", STATE, true, true, true, false}, {"Alloc Event ID", ALLOC_EVENT_ID, true, true, false, true},
    {"Free Event ID", FREE_EVENT_ID, true, true, false, true}};
}
}
#endif //PROFILER_SERVER_MEMSNAPSHOTTABLECOLUMN_H
