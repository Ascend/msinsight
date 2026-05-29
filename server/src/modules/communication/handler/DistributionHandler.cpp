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
#include "pch.h"
#include "CommunicationProtocolRequest.h"
#include "CommunicationProtocolResponse.h"
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "DistributionHandler.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Dic;
using namespace Dic::Server;

bool DistributionHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::DistributionDataRequest &>(*requestPtr);
    std::unique_ptr<Protocol::DistributionResponse> responsePtr = std::make_unique<Protocol::DistributionResponse>();
    DistributionResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    // check request parameters
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetCommunicationError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    if (database == nullptr || !database->QueryDistributionData(request.params, response.body)) {
        SetCommunicationError(ErrorCode::QUERY_COMMUNICATION_DISTRIBUTION_FAILED);
        SetResponseResult(response, false);
        ServerLog::Error("Failed to get communication distribution data.");
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Communication
} // Module
} // Dic
