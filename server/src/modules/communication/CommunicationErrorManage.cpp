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

#include "CommunicationErrorManage.h"

namespace Dic::Module::Communication {

static const std::string unknownError = "Unknown error code";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::RESET_ERROR, ""},
    {ErrorCode::PARAMS_ERROR, "Request parameter exception"},

    {ErrorCode::GET_DEVICE_ID_FAILED, "Failed to get device id"},

    {ErrorCode::CONNECT_DATABASE_FAILED, "Failed to connect to database"},
    {ErrorCode::CLUSTER_ANALYSIS_FAILED, "Cluster analysis failed, unable to retrieve cluster data"},
    {ErrorCode::QUERY_COMMUNICATION_BANDWIDTH_FAILED, "Failed to query communication bandwidth data"},
    {ErrorCode::QUERY_COMMUNICATION_OPERATOR_FAILED, "Failed to query communication operator data"},
    {ErrorCode::QUERY_SLOW_OPERATOR_FAILED, "Failed to query slow operator data"},
    {ErrorCode::QUERY_COMMUNICATION_DISTRIBUTION_FAILED, "Failed to query communication distribution data"},
    {ErrorCode::QUERY_MATRIX_SORT_OPERATOR_NAMES_FAILED, "Failed to query matrix sort operator names"},
    {ErrorCode::QUERY_OPERATOR_NAMES_FAILED, "Failed to query operator names"},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetCommunicationError(ErrorCode code) {
    ModuleRequestHandler::SetRequestContextError({.code = static_cast<int>(code), .message = GetErrorMessage(code)});
}
} // namespace Dic::Module::Communication
