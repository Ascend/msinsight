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

#ifndef PROFILER_SERVER_UTILERRORMANAGER_H
#define PROFILER_SERVER_UTILERRORMANAGER_H

#include <map>
#include <string>
#include "ModuleRequestHandler.h"

namespace Dic::Common {
// 错误码枚举
enum class ErrorCode {
    RESET_ERROR = 0,
    // 过滤请求
    OUTDATED_REQUEST_ERROR = 1000,
    // 接口入参错误
    PARAMS_ERROR = 1101,

    // 中间变量
    GET_ABSOLUTE_PATH_FAILED = 2101,

    // 数据库错误
    CONNECT_DATABASE_FAILED = 3001,

    // 文件错误
    FILE_TOO_DEEP = 4101,
    FILE_TOO_MANY = 4102,
    FILE_NOT_EXIST = 4103,
    FILE_PATH_IS_EMPTY = 4104,
    SUB_FILE_PATH_LENGTH_EXCEEDS = 4105,
    FILE_PATH_CONTAINS_INVALID_CHAR = 4106,
    FILE_PATH_IS_SOFT_LINK = 4107,
    FILE_NOT_READ_ACCESS = 4108,
    PATH_OWNER_ERROR = 4109,
    OTHER_CAN_WRITE = 4110,
    OPEN_DIR_FAILED = 4111,
    IMPORT_FILE_OTHER_TYPE = 4112,
};

// 错误信息映射
extern const std::map<ErrorCode, std::string> errorMessages;

const std::string &GetErrorMessage(ErrorCode code);
void SetCommonError(ErrorCode code);
uint32_t GetFilePathLengthLimit();
} // namespace Dic::Common

#endif // PROFILER_SERVER_UTILERRORMANAGER_H
