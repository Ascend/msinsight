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
#include "QueryCommunicationDetailHandler.h"

namespace Dic {
namespace Module {
namespace Summary {
using namespace Dic::Server;
bool QueryCommunicationDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    CommunicationDetailRequest &request = dynamic_cast<CommunicationDetailRequest &>(*requestPtr.get());
    std::unique_ptr<CommunicationDetailResponse> responsePtr = std::make_unique<CommunicationDetailResponse>();
    CommunicationDetailResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    // check request parameters
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDataBaseByFileId(request.fileId);
    if (!database || !database->QueryCommunicationOpDetail(request.params, response.commDetails) or
        !database->QueryTotalNumByAcceleratorCore(request.params.timeFlag, response.totalNum)) {
        ServerLog::Warn("Failed to query communication detail or get total num.");
        SetSummaryError(ErrorCode::QUERY_COMMUNICATION_DETAIL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Summary
} // Module
} // Dic
