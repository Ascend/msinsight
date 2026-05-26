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

#ifndef PROFILER_SERVER_MEMORY_TABLECOLUMN_H
#define PROFILER_SERVER_MEMORY_TABLECOLUMN_H
#include <string>
namespace Dic::Module::Memory {
namespace OpMemoryColumn {
constexpr std::string_view ID = "rowid";
constexpr std::string_view NAME = "name";
constexpr std::string_view SIZE = "size";
constexpr std::string_view ALLOCATION_TIME = "allocationTime";
constexpr std::string_view RELEASE_TIME = "releaseTime";
constexpr std::string_view ACTIVE_RELEASE_TIME = "activeReleaseTime";
constexpr std::string_view DURATION = "duration";
constexpr std::string_view ACTIVE_DURATION = "activeDuration";
constexpr std::string_view ALLOCATION_ALLOCATED = "allocationTotalAllocated";
constexpr std::string_view ALLOCATION_RESERVE = "allocationTotalReserved";
constexpr std::string_view ALLOCATION_ACTIVE = "allocationTotalActive";
constexpr std::string_view RELEASE_ALLOCATED = "releaseTotalAllocated";
constexpr std::string_view RELEASE_RESERVE = "releaseTotalReserved";
constexpr std::string_view RELEASE_ACTIVE = "releaseTotalActive";
constexpr std::string_view STREAM = "streamPtr";
constexpr std::string_view DEVICE_ID = "deviceId";

constexpr std::string_view FULL_COLUMNS_WITHOUT_ID[] = {NAME, SIZE, ALLOCATION_TIME, RELEASE_TIME, ACTIVE_RELEASE_TIME,
    DURATION, ACTIVE_DURATION, ALLOCATION_ALLOCATED, ALLOCATION_RESERVE, ALLOCATION_ACTIVE, RELEASE_ALLOCATED,
    RELEASE_RESERVE, RELEASE_ACTIVE, STREAM, DEVICE_ID};
} // namespace OpMemoryColumn

namespace StaticOpColumn {
constexpr std::string_view ID = "id";
constexpr std::string_view DEVICE_ID = "deviceId";
constexpr std::string_view OP_NAME = "opName";
constexpr std::string_view MODEL_NAME = "modelName";
constexpr std::string_view GRAPH_ID = "graphId";
constexpr std::string_view NODE_INDEX_START = "nodeIndexStart";
constexpr std::string_view NODE_INDEX_END = "nodeIndexEnd";
constexpr std::string_view SIZE = "size";

constexpr std::string_view FULL_COLUMNS_WITHOUT_ID[] = {
    DEVICE_ID, OP_NAME, MODEL_NAME, GRAPH_ID, NODE_INDEX_START, NODE_INDEX_END, SIZE};
} // namespace StaticOpColumn
} // namespace Dic::Module::Memory
#endif // PROFILER_SERVER_MEMORY_TABLECOLUMN_H
