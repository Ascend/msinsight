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

#ifndef PROFILER_SERVER_OPERATORERRORMANAGER_H
#define PROFILER_SERVER_OPERATORERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Module::Operator {
// 错误码枚举
enum class ErrorCode {
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_DEVICE_ID_FAILED = 2001,
    GET_CSV_HANDLE_FAILED = 2102,
    GET_BASELINE_ID_FAILED = 2103,

    // 数据库错误
    QUERY_DURATION_FAILED = 3101,
    QUERY_STATISTIC_FAILED = 3102,
    QUERY_DETAIL_FAILED = 3103,
    QUERY_MORE_INFO_FAILED = 3104,
    QUERY_ALL_STATISTIC_FAILED = 3105,
    QUERY_ALL_DETAIL_FAILED = 3106,
};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetOperatorError(ErrorCode code);
} // namespace Dic::Module::Operator
#endif // PROFILER_SERVER_OPERATORERRORMANAGER_H
