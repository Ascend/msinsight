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

#include "TimelineErrorManager.h"

namespace Dic::Module::Timeline {
static const std::string unknownError = "Unknown error code";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::RESET_ERROR, ""},
    {ErrorCode::PARAMS_ERROR, "Request parameter exception"},

    {ErrorCode::GET_DEVICE_ID_FAILED, "Failed to get device id"},
    {ErrorCode::PROJECT_EXPLORER_NOT_EXISTED, "Project explorer info is not existed"},
    {ErrorCode::PROJECT_TYPE_INVALID, "Project type invalid"},
    {ErrorCode::PROJECT_IS_NOT_CLUSTER, "Project is not cluster data"},

    {ErrorCode::CONNECT_DATABASE_FAILED, "Failed to connect to database"},
    {ErrorCode::QUERY_COMMUNICATION_KERNEL_FAILED,
        "Failed to query communication kernel info"
        " Please ensure that timeline data has been fully parsed and then try again"},
    {ErrorCode::QUERY_EVENTS_VIEW_DATA_FAILED, "Failed to get events view table response data"},
    {ErrorCode::QUERY_AI_CORE_FREQ_FAILED, "Failed to get system view AI core freq table response data"},
    {ErrorCode::QUERY_FLOW_EVENTS_FAILED, "Failed to query flow events"},
    {ErrorCode::CATEGORY_PARSE_NOT_FINISH, "The connection category parse unit is not finish"},
    {ErrorCode::QUERY_FLOW_CATEGORY_FAILED, "Failed to query flow category"},
    {ErrorCode::QUERY_UNIT_FLOWS_FAILED, "Failed to query unit flows"},
    {ErrorCode::QUERY_KERNEL_DETAIL_FAILED, "Failed to query kernel detail data"},
    {ErrorCode::QUERY_KERNEL_DEPTH_AND_THREAD_FAILED, "Failed to query kernel depth and thread"},
    {ErrorCode::QUERY_OVERALL_METRICS_DETAIL_FAILED, "Failed to get any overall metrics details"},
    {ErrorCode::QUERY_SYSTEM_VIEW_FAILED, "Failed to query system view data"},
    {ErrorCode::OVERLAP_ANALYSIS_PARSE_NOT_FINISH, "The overlap analysis data is not parse finish"},
    {ErrorCode::QUERY_SYSTEM_VIEW_OVERALL_FAILED, "Failed to query system view overall"},
    {ErrorCode::QUERY_THREAD_DETAIL_FAILED, "Failed to query thread detail"},
    {ErrorCode::QUERY_THREAD_FAILED, "Failed to query thread"},
    {ErrorCode::QUERY_THREAD_SAME_OPERATORS_DETAIL_FAILED, "Failed to query thread same operators details"},
    {ErrorCode::QUERY_THREAD_TRACES_FAILED, "Failed to query thread traces"},
    {ErrorCode::QUERY_THREAD_TRACES_SUMMARY_FAILED, "Failed to query thread traces summary data"},
    {ErrorCode::QUERY_UNIT_COUNTER_FAILED, "Failed to query unit counter"},
    {ErrorCode::QUERY_SLICE_DETAIL_FAILED, "Failed to search slice details"},
    {ErrorCode::QUERY_SLICE_NAME_FAILED, "Failed to search slice name"},
    {ErrorCode::SET_CARD_ALIAS_FAILED, "Failed to set card alias"},
    {ErrorCode::QUERY_MEMCPY_OVERALL_FAILED, "Failed to query memcpy overall"},
    {ErrorCode::QUERY_MEMCPY_DETAIL_FAILED, "Failed to query memcpy detail"},
    {ErrorCode::QUERY_KERNEL_OVERALL_FAILED, "Failed to query kernel overall"},

    {ErrorCode::FILE_PATH_IS_EMPTY, "Import file path is empty"},
    {ErrorCode::FOLDER_IS_EMPTY, "Import path is empty folder"},
    {ErrorCode::OTHER_CAN_WRITE, "The path is writeable by other user"},
    {ErrorCode::FILE_NOT_EXIST, "File not exist"},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetTimelineError(ErrorCode code) {
    ModuleRequestHandler::SetRequestContextError({.code = static_cast<int>(code), .message = GetErrorMessage(code)});
}
} // namespace Dic::Module::Timeline
