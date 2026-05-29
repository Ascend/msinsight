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

#ifndef PROFILER_SERVER_ADVISORERRORMANAGER_H
#define PROFILER_SERVER_ADVISORERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Module::Advisor {
// 错误码枚举（9000以下错误码，前端弹窗，需被用户感知；9000以上错误码，无需用户感知，仅供调试）
enum class ErrorCode {
    RESET_ERROR = 0,
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_DEVICE_ID_FAILED = 2001,
    GET_ABSOLUTE_PATH_FAILED = 2101,

    // 数据库错误(需被用户感知)
    CONNECT_DATABASE_FAILED = 3001,
    DATA_ANOMALY_END_TIME_SMALLER_TIMESTAMP = 3103,

    // 9000以上区间错误码，无需被用户感知，仅供调试
    QUERY_ACLNN_OPERATOR_COUNT_FAILED = 9101,
    QUERY_AFFINITY_API_FAILED = 9102,
    QUERY_AFFINITY_OPTIMIZER_FAILED = 9104,
    QUERY_AI_CPU_OP_CAN_BE_OPTIMIZED_FAILED = 9105,
    QUERY_OPERATOR_DISPATCH_FAILED = 9106,
    QUERY_FUSED_OP_DATABASE_FAILED = 9107,
};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetAdvisorError(ErrorCode code);
} // namespace Dic::Module::Advisor

#endif // PROFILER_SERVER_ADVISORERRORMANAGER_H
