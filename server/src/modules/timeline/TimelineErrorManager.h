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

#ifndef PROFILER_SERVER_TIMELINEERRORMANAGER_H
#define PROFILER_SERVER_TIMELINEERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Module::Timeline {
// 错误码枚举（9000以下错误码，前端弹窗，需被用户感知；9000以上错误码，无需用户感知，仅供调试）
enum class ErrorCode {
    RESET_ERROR = 0,
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_DEVICE_ID_FAILED = 2001,
    PROJECT_EXPLORER_NOT_EXISTED = 2101,
    PROJECT_TYPE_INVALID = 2102,
    PROJECT_IS_NOT_CLUSTER = 2103,

    // 数据库错误（需被用户感知）
    CONNECT_DATABASE_FAILED = 3001,
    QUERY_COMMUNICATION_KERNEL_FAILED = 3101,
    QUERY_FLOW_EVENTS_FAILED = 3104,
    CATEGORY_PARSE_NOT_FINISH = 3105,
    QUERY_FLOW_CATEGORY_FAILED = 3106,
    QUERY_UNIT_FLOWS_FAILED = 3107,
    QUERY_KERNEL_DEPTH_AND_THREAD_FAILED = 3109,
    OVERLAP_ANALYSIS_PARSE_NOT_FINISH = 3112,
    QUERY_THREAD_DETAIL_FAILED = 3114,
    QUERY_THREAD_FAILED = 3115,
    QUERY_THREAD_SAME_OPERATORS_DETAIL_FAILED = 3116,
    QUERY_THREAD_TRACES_FAILED = 3117,
    QUERY_THREAD_TRACES_SUMMARY_FAILED = 3118,
    QUERY_UNIT_COUNTER_FAILED = 3118,
    QUERY_SLICE_DETAIL_FAILED = 3119,
    QUERY_SLICE_NAME_FAILED = 3120,
    SET_CARD_ALIAS_FAILED = 3301,
    QUERY_MEMCPY_OVERALL_FAILED = 3302,
    QUERY_MEMCPY_DETAIL_FAILED = 3303,
    QUERY_KERNEL_OVERALL_FAILED = 3304,

    // 文件错误
    FILE_PATH_IS_EMPTY = 4101,
    FOLDER_IS_EMPTY = 4102,
    FILE_NOT_EXIST = 4103,
    OTHER_CAN_WRITE = 4110,

    // 9000以上区间错误码，无需被用户感知，仅供调试
    QUERY_EVENTS_VIEW_DATA_FAILED = 9102,
    QUERY_AI_CORE_FREQ_FAILED = 9103,
    QUERY_KERNEL_DETAIL_FAILED = 9108,
    QUERY_OVERALL_METRICS_DETAIL_FAILED = 9110,
    QUERY_SYSTEM_VIEW_FAILED = 9111,
    QUERY_SYSTEM_VIEW_OVERALL_FAILED = 9113,

};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetTimelineError(ErrorCode code);
} // namespace Dic::Module::Timeline
#endif // PROFILER_SERVER_TIMELINEERRORMANAGER_H
