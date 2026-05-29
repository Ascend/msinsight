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

#include "CommunicationAdvisorHandler.h"
#include "CommunicationProtocolRequest.h"
#include "CommunicationProtocolResponse.h"
#include "WsSessionManager.h"
#include "WsSender.h"
#include "CommunicationAdvisor.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Dic;
using namespace Dic::Server;

bool CommunicationAdvisorHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::CommunicationAdvisorRequest &>(*requestPtr);
    std::unique_ptr<Protocol::CommunicationAdvisorResponse> responsePtr =
        std::make_unique<Protocol::CommunicationAdvisorResponse>();
    CommunicationAdvisorResponse &response = *responsePtr;
    std::string errMsg;
    if (!request.params.CheckParams(errMsg)) {
        SetCommunicationError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    CommunicationAdvisor advisor;
    advisor.Register();
    advisor.GenerateAdvisor(response.body.items, request.params.clusterPath);
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Communication
} // Module
} // Dic
