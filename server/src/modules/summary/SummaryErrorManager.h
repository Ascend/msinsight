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

#ifndef PROFILER_SERVER_SUMMARYERRORMANAGER_H
#define PROFILER_SERVER_SUMMARYERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Module::Summary {
// 错误码枚举
enum class ErrorCode {
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_ALGORITHM_FAILED = 2101,
    GET_ALGORITHM_CONNECTIONS_FAILED = 2102,
    GET_MASK_FAILED = 2103,
    GET_RANK_ID_FAILED = 2104,
    GET_REAL_PATH_FAILED = 2105,
    GET_PARSED_FILES_FAILED = 2106,
    GENERATE_ORTHOGONAL_FAILED = 2301,

    // 数据库错误
    CONNECT_DATABASE_FAILED = 3001,
    QUERY_COMPUTE_STATISTICS_FAILED = 3101,
    QUERY_COMMUNICATION_STATISTICS_FAILED = 3102,
    QUERY_PARALLEL_STATISTICS_FAILED = 3103,
    QUERY_PARALLELISM_PERFORMANCE_FAILED = 3104,
    QUERY_COMPUTE_DETAIL_FAILED = 3105,
    QUERY_COMMUNICATION_DETAIL_FAILED = 3106,
    UPDATE_PARALLEL_STRATEGY_FAILED = 3301,
    UPDATE_PARALLEL_SHOW_MAP_FAILED = 3302,
    UPDATE_PARALLEL_VIEW_FAILED = 3303,
    UPDATE_MODEL_INFO_MODIFY_FAILED = 3304,
    UPDATE_MODEL_INFO_NOT_EQUAL_FAILED = 3305,
    MERGE_AND_SAVE_MODEL_INFO_FAILED = 3305,
    ADD_ALGORITHM_FAILED = 3501,
    CLEAR_EXPERT_HOTSPOT_FAILED = 3701,
    CLEAR_DEPLOYMENT_FAILED = 3702,

    // 文件错误
    PARSER_MODEL_GEN_CONFIG_FILE_FAILED = 4101,
    PARSER_META_DATA_FILE_CONTEXT_FAILED = 4102,
    READ_MODEL_GEN_CONFIG_FILE_FAILED = 4301,
};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetSummaryError(ErrorCode code, const std::string &extendedMessage = "");
} // namespace Dic::Module::Summary
#endif // PROFILER_SERVER_SUMMARYERRORMANAGER_H
