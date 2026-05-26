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
#ifndef PROFILER_SERVER_MEMORYTABLEVIEW_H
#define PROFILER_SERVER_MEMORYTABLEVIEW_H

#include "StringUtil.h"
#include "CommonRequests.h"
#include "MemoryTableColum.h"

namespace Dic::Module::Memory {
const TableViewColumn TABLE_VIEW_COMPARE_COLUMN{"Source", "source", true, false, false, false};
namespace OperatorMemoryTableView {
using namespace Dic::Protocol;
constexpr int64_t DEFAULT_MAX_SIZE = std::numeric_limits<int32_t>::max();
constexpr int64_t DEFAULT_MIN_SIZE = std::numeric_limits<int32_t>::min();
inline const std::vector<TableViewColumn> FIELD_FULL_COLUMNS = {
    {"ID", "id"}, // 不可见
    {"Name", OpMemoryColumn::NAME, true, true, true, false}, // 可搜索 可排序
    {"Size(KB)", OpMemoryColumn::SIZE, true, true, false, true}, // 可范围筛选 可排序
    {"Allocation Time(ms)", OpMemoryColumn::ALLOCATION_TIME, true, true, false, true}, // 可范围筛选 可排序
    {"Release Time(ms)", OpMemoryColumn::RELEASE_TIME, true, true, false, true}, // 可范围筛选 可排序
    {"Active Release Time(ms)", OpMemoryColumn::ACTIVE_RELEASE_TIME, true, true, false, true}, // 可范围筛选 可排序
    {"Duration(ms)", OpMemoryColumn::DURATION, true, true, false, true}, // 可范围筛选 可排序
    {"Active Duration(ms)", OpMemoryColumn::ACTIVE_DURATION, true, true, false, true}, // 可范围筛选 可排序
    {"Allocation Total Allocated(MB)", OpMemoryColumn::ALLOCATION_ALLOCATED, true, true, false,
        true}, // 可范围筛选 可排序
    {"Allocation Total Reserved(MB)", OpMemoryColumn::ALLOCATION_RESERVE, true, true, false, true}, // 可范围筛选 可排序
    {"Allocation Total Active(MB)", OpMemoryColumn::ALLOCATION_ACTIVE, true, true, false, true}, // 可范围筛选 可排序
    {"Release Total Allocated(MB)", OpMemoryColumn::RELEASE_ALLOCATED, true, true, false, true}, // 可范围筛选 可排序
    {"Release Total Reserved(MB)", OpMemoryColumn::RELEASE_RESERVE, true, true, false, true}, // 可范围筛选 可排序
    {"Release Total Active(MB)", OpMemoryColumn::RELEASE_ACTIVE, true, true, false, true}, // 可范围筛选 可排序
    {"Stream", OpMemoryColumn::STREAM, true, true, false, false}, // 可排序
    {"deviceId", OpMemoryColumn::DEVICE_ID} // 不可见
};
}
namespace StaticOpTableView {
inline const std::vector<TableViewColumn> FIELD_FULL_COLUMNS = {
    {"Device ID", StaticOpColumn::DEVICE_ID, true, true, false, false},
    {"Name", StaticOpColumn::OP_NAME, true, true, false, false},
    {"Node Index Start", StaticOpColumn::NODE_INDEX_START, true, true, false, false},
    {"Node Index End", StaticOpColumn::NODE_INDEX_END, true, true, false, false},
    {"Size(MB)", StaticOpColumn::SIZE, true, true, false, false}};
}
}
#endif // PROFILER_SERVER_MEMORYTABLEVIEW_H
