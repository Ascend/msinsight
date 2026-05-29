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

#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
static const std::string unknownError = "Unknown error code";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::RESET_ERROR, ""},

    {ErrorCode::PARAMS_ERROR, "Request parameter exception"},

    {ErrorCode::GET_DEVICE_ID_FAILED, "Failed to get device id"},
    {ErrorCode::GET_ABSOLUTE_PATH_FAILED, "Failed to retrieve the absolute path"},

    {ErrorCode::CONNECT_DATABASE_FAILED, "Failed to connect to database"},
    {ErrorCode::QUERY_ACLNN_OPERATOR_COUNT_FAILED, "Failed to query aclnn operator count"},
    {ErrorCode::QUERY_AFFINITY_API_FAILED, "Failed to query affinity API"},
    {ErrorCode::DATA_ANOMALY_END_TIME_SMALLER_TIMESTAMP,
        "The original data seems to have an issue, as the end time is smaller than the timestamp. Please check the "
        "rationality of the data"},
    {ErrorCode::QUERY_AFFINITY_OPTIMIZER_FAILED, "Failed to query affinity optimizer"},
    {ErrorCode::QUERY_AI_CPU_OP_CAN_BE_OPTIMIZED_FAILED, "Failed to query can be optimized AI CPU Op"},
    {ErrorCode::QUERY_OPERATOR_DISPATCH_FAILED, "Failed to query operator dispatch data"},
    {ErrorCode::QUERY_FUSED_OP_DATABASE_FAILED, "Failed to query fused operator advice from database"},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetAdvisorError(ErrorCode code) {
    Module::ModuleRequestHandler::SetRequestContextError(
        {.code = static_cast<int>(code), .message = GetErrorMessage(code)});
}
} // namespace Dic::Module::Advisor
