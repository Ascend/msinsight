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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "QueryComputeDetailInfoHandler.h"

namespace Dic {
namespace Module {
namespace Summary {
using namespace Dic::Server;
bool QueryComputeDetailInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<ComputeDetailRequest &>(*requestPtr);
    std::unique_ptr<ComputeDetailResponse> responsePtr = std::make_unique<ComputeDetailResponse>();
    ComputeDetailResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    // check request parameters
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDataBaseByFileId(request.fileId);
    if (!database || !database->QueryComputeOpDetail(request.params, response.computeDetails) or
        !database->QueryTotalNumByAcceleratorCore(request.params.timeFlag, response.totalNum)) {
        ServerLog::Warn("Failed to query compute detail or query total num.");
        SetSummaryError(ErrorCode::QUERY_COMPUTE_DETAIL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Summary
} // Module
} // Dic
