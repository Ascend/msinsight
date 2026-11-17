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

#include "SummaryErrorManager.h"
#include "StringUtil.h"

namespace Dic::Module::Summary {
static const std::string unknownError = "Unable to complete this operation. Please try again.";

const std::map<ErrorCode, std::string> errorMessages = {
    {ErrorCode::PARAMS_ERROR, "Some settings are invalid. Please check your input and try again."},

    {ErrorCode::GET_ALGORITHM_FAILED,
        "The parallel strategy is not available. Please apply the parallel strategy settings and try "
        "again."},
    {ErrorCode::GET_ALGORITHM_CONNECTIONS_FAILED,
        "Device connections are not available yet. Please apply the parallel strategy settings first."},
    {ErrorCode::GET_MASK_FAILED, "Unable to group devices. Please check the parallel strategy settings."},
    {ErrorCode::GET_RANK_ID_FAILED,
        "Unable to identify the devices for this view. Please check the imported data and parallel strategy "
        "settings."},
    {ErrorCode::GET_REAL_PATH_FAILED,
        "Unable to access the selected file or folder. Please check that it exists and you have permission to "
        "read it."},
    {ErrorCode::GET_PARSED_FILES_FAILED,
        "No supported data files were found. Please select a file or folder containing supported "
        "data."},
    {ErrorCode::GENERATE_ORTHOGONAL_FAILED,
        "Unable to group devices. Please check that the parallel sizes match the number of "
        "devices."},

    {ErrorCode::CONNECT_DATABASE_FAILED,
        "Unable to access the imported data. Please re-import the data and try "
        "again."},
    {ErrorCode::QUERY_COMPUTE_STATISTICS_FAILED,
        "Unable to load computation statistics. Please try again or re-import the profiling "
        "data."},
    {ErrorCode::QUERY_COMMUNICATION_STATISTICS_FAILED,
        "Unable to load communication statistics. Please try again or re-import the profiling "
        "data."},
    {ErrorCode::QUERY_PARALLEL_STATISTICS_FAILED, "Unable to load the parallel strategy settings. Please try again."},
    {ErrorCode::QUERY_PARALLELISM_PERFORMANCE_FAILED,
        "No parallelism performance data is available. Please check the selected data and parallel strategy "
        "settings."},
    {ErrorCode::QUERY_COMPUTE_DETAIL_FAILED,
        "Unable to load computation details. Please try again or re-import the profiling "
        "data."},
    {ErrorCode::QUERY_COMMUNICATION_DETAIL_FAILED,
        "Unable to load communication details. Please try again or re-import the profiling "
        "data."},
    {ErrorCode::UPDATE_PARALLEL_STRATEGY_FAILED, "Unable to save the parallel strategy settings. Please try again."},
    {ErrorCode::UPDATE_PARALLEL_SHOW_MAP_FAILED,
        "Unable to display the selected parallel dimension. Please check the parallel strategy "
        "settings."},
    {ErrorCode::UPDATE_PARALLEL_VIEW_FAILED,
        "Unable to refresh the parallel view. Please check the parallel strategy settings and try "
        "again."},
    {ErrorCode::UPDATE_MODEL_INFO_MODIFY_FAILED,
        "The number of experts cannot be changed for the imported data. Please keep the original expert count."},
    {ErrorCode::UPDATE_MODEL_INFO_NOT_EQUAL_FAILED,
        "The total layer count must be non-negative and at least the sum of MoE and dense layers. "
        "Please check the total layer count and dense layer settings."},
    {ErrorCode::MERGE_AND_SAVE_MODEL_INFO_FAILED,
        "Unable to save the model information. Please check the model settings and try "
        "again."},
    {ErrorCode::ADD_ALGORITHM_FAILED,
        "The selected parallel strategy algorithm is not supported. Please select a supported "
        "algorithm."},
    {ErrorCode::CLEAR_EXPERT_HOTSPOT_FAILED,
        "Unable to replace the previous expert hotspot data. Please try importing the data "
        "again."},
    {ErrorCode::CLEAR_DEPLOYMENT_FAILED,
        "Unable to replace the previous expert deployment data. Please try importing the data "
        "again."},

    {ErrorCode::PARSER_MODEL_GEN_CONFIG_FILE_FAILED,
        "The model configuration format is not supported. Please check the file contents and import it "
        "again."},
    {ErrorCode::PARSER_META_DATA_FILE_CONTEXT_FAILED,
        "Unable to read the metadata contents. Please check the file format and import it "
        "again."},
    {ErrorCode::READ_MODEL_GEN_CONFIG_FILE_FAILED,
        "Unable to read the model configuration. Please check that the file is accessible and contains valid "
        "settings."},
};

const std::string &GetErrorMessage(ErrorCode code) {
    auto it = errorMessages.find(code);
    if (it != errorMessages.end()) {
        return it->second;
    } else {
        return unknownError;
    }
}

void SetSummaryError(ErrorCode code, const std::string &extendedMessage) {
    std::string message = extendedMessage.size() > 0 ? StringUtil::StrJoin(GetErrorMessage(code), ":", extendedMessage)
                                                     : GetErrorMessage(code);
    ModuleRequestHandler::SetRequestContextError({.code = static_cast<int>(code), .message = message});
}
} // namespace Dic::Module::Summary
