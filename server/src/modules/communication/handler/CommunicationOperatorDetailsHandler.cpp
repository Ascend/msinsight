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
#include "BaselineManager.h"
#include "CommunicationOperatorDetailsHandler.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Dic;
using namespace Dic::Server;
using namespace Dic::Module::Global;

bool CommunicationOperatorDetailsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::OperatorDetailsRequest &>(*requestPtr);
    std::unique_ptr<Protocol::OperatorDetailsResponse> responsePtr =
        std::make_unique<Protocol::OperatorDetailsResponse>();
    // check request parameters
    std::string errorMsg;
    OperatorDetailsResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    if (!request.params.CheckParams(errorMsg)) {
        // 集群比对状态下，若基线数据为空，入参校验失败后，前端不应弹出报错提示，SendResponse需返回true，非集群比对时，需返回false
        if (!BaselineManager::Instance().GetBaseLineClusterPath().empty()) {
            SendResponse(std::move(responsePtr), true);
            return true;
        }
        SetCommunicationError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        ServerLog::Error(errorMsg);
        return false;
    }
    // query data
    std::string type = request.params.queryType == "Comparison" ? request.params.clusterPath
                                                                : BaselineManager::Instance().GetBaseLineClusterPath();
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(type);
    if (database == nullptr || !database->QueryOperatorsCount(request.params, response.body) ||
        !database->QueryAllOperators(request.params, response.body)) {
        SetCommunicationError(ErrorCode::QUERY_COMMUNICATION_OPERATOR_FAILED);
        SetResponseResult(response, false);
        ServerLog::Error("Failed to get communication operator data.");
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Communication
} // Module
} // Dic
