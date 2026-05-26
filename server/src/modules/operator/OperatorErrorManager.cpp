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

#include "OperatorErrorManager.h"

namespace Dic::Module::Operator {
static const std::string unknownError = "Unknown error code";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::PARAMS_ERROR, "Request parameter exception"},

    {ErrorCode::GET_DEVICE_ID_FAILED, "Failed to get device id"},
    {ErrorCode::GET_CSV_HANDLE_FAILED, "Failed to get csvHandle"},
    {ErrorCode::GET_BASELINE_ID_FAILED, "Failed to get baseline id"},

    {ErrorCode::QUERY_DURATION_FAILED, "Failed to query operator duration info"},
    {ErrorCode::QUERY_STATISTIC_FAILED, "Failed to query operator statistic info"},
    {ErrorCode::QUERY_DETAIL_FAILED, "Failed to query operator detail info"},
    {ErrorCode::QUERY_MORE_INFO_FAILED, "Failed to query operator more info"},
    {ErrorCode::QUERY_ALL_STATISTIC_FAILED, "Failed to query all operator statistic info"},
    {ErrorCode::QUERY_ALL_DETAIL_FAILED, "Failed to query all operator detail info"},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetOperatorError(ErrorCode code) {
    ModuleRequestHandler::SetRequestContextError({.code = static_cast<int>(code), .message = GetErrorMessage(code)});
}
} // namespace Dic::Module::Operator
