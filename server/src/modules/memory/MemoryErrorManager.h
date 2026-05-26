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

#ifndef PROFILER_SERVER_MEMORYERRORMANAGER_H
#define PROFILER_SERVER_MEMORYERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Module::Memory {
// 错误码枚举
enum class ErrorCode {
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_DEVICE_ID_FAILED = 2001,
    GET_BASELINE_ID_FAILED = 2002,

    // 数据库错误
    CONNECT_DATABASE_FAILED = 3001,
    QUERY_SLICE_FAILED = 3101,
    QUERY_SLICE_IN_TIMELINE_FAILED = 3102,
    QUERY_MEMORY_COMPONENT_FAILED = 3103,
    QUERY_MEMORY_COMPONENT_COMPARE_FAILED = 3104,
    QUERY_MEMORY_COMPONENT_BASELINE_FAILED = 3105,
    QUERY_MEMORY_OPERATOR_FAILED = 3106,
    QUERY_MEMORY_OPERATOR_COMPARE_FAILED = 3107,
    QUERY_MEMORY_OPERATOR_BASELINE_FAILED = 3108,
    QUERY_OPERATOR_SIZE_FAILED = 3109,
    QUERY_OPERATOR_SIZE_COMPARE_FAILED = 3110,
    QUERY_OPERATOR_SIZE_BASELINE_FAILED = 3111,
    QUERY_MEMORY_RESOURCE_TYPE_FAILED = 3112,
    QUERY_MEMORY_STATIC_OPERATOR_GRAPH_FAILED = 3113,
    QUERY_MEMORY_STATIC_OPERATOR_GRAPH_COMPARE_FAILED = 3114,
    QUERY_MEMORY_STATIC_OPERATOR_GRAPH_BASELINE_FAILED = 3115,
    QUERY_MEMORY_STATIC_OPERATOR_FAILED = 3116,
    QUERY_MEMORY_STATIC_OPERATOR_COMPARE_FAILED = 3117,
    QUERY_MEMORY_STATIC_OPERATOR_BASELINE_FAILED = 3118,
    QUERY_MEMORY_STATIC_OPERATOR_SIZE_FAILED = 3119,
    QUERY_MEMORY_STATIC_OPERATOR_SIZE_COMPARE_FAILED = 3120,
    QUERY_MEMORY_STATIC_OPERATOR_SIZE_BASELINE_FAILED = 3121,
    QUERY_MEMORY_TYPE_FAILED = 3122,
    QUERY_MEMORY_VIEW_DATA_FAILED = 3123,
    QUERY_MEMORY_VIEW_DATA_COMPARE_FAILED = 3124,
    QUERY_MEMORY_VIEW_DATA_BASELINE_FAILED = 3125,
};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetMemoryError(ErrorCode code);
} // namespace Dic::Module::Memory
#endif // PROFILER_SERVER_MEMORYERRORMANAGER_H
