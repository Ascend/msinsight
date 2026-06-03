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

#include "UtilErrorManager.h"
#if defined(_WIN32)
#include <windows.h>
#endif

namespace Dic::Common {
uint32_t GetFilePathLengthLimit() {
#ifdef _WIN32
    return MAX_PATH;
#else
    return PATH_MAX;
#endif
    return 0;
}

static const std::string unknownError = "Unknown error code";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::RESET_ERROR, ""},

    {ErrorCode::OUTDATED_REQUEST_ERROR, "Request is outdated, intercepted."},

    {ErrorCode::PARAMS_ERROR, "Request parameter exception"},

    {ErrorCode::GET_ABSOLUTE_PATH_FAILED, "Failed to retrieve the absolute path"},

    {ErrorCode::CONNECT_DATABASE_FAILED, "Failed to connect to database"},

    {ErrorCode::FILE_TOO_DEEP, "Too many levels of file nesting"},
    {ErrorCode::FILE_TOO_MANY, "Too many files matching the requirements"},
    {ErrorCode::FILE_NOT_EXIST, "File not exist"},
    {ErrorCode::FILE_PATH_IS_EMPTY, "The path is empty"},
    {ErrorCode::SUB_FILE_PATH_LENGTH_EXCEEDS,
        "The sub-file path length exceeds " + std::to_string(GetFilePathLengthLimit())},
    {ErrorCode::FILE_PATH_CONTAINS_INVALID_CHAR, "The path contains invalid character"},
    {ErrorCode::FILE_PATH_IS_SOFT_LINK, "The path is soft link"},
    {ErrorCode::FILE_NOT_READ_ACCESS, "The path has no read access"},
    {ErrorCode::PATH_OWNER_ERROR, "The path's owner is not current user"},
    {ErrorCode::OTHER_CAN_WRITE, "The path is writeable by other user"},
    {ErrorCode::OPEN_DIR_FAILED, "open dir failed"},
    {ErrorCode::IMPORT_FILE_OTHER_TYPE, "No parsable files found"},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetCommonError(ErrorCode code) {
    Module::ModuleRequestHandler::SetRequestContextError(
        {.code = static_cast<int>(code), .message = GetErrorMessage(code)});
}
} // namespace Dic::Common
