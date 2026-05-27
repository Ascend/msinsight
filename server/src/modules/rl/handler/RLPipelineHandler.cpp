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

#include "RLPipelineHandler.h"
#include "RLProtocolRequest.h"
#include "RLProtocolResponse.h"
#include "RLPipelineService.h"

namespace Dic::Module::RL {
bool RLPipelineHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::RLPipelineRequest &>(*requestPtr);
    std::unique_ptr<Protocol::RLPipelineResponse> responsePtr = std::make_unique<Protocol::RLPipelineResponse>();
    RLPipelineResponse &response = *responsePtr;
    bool res = RLPipelineService::Instance().GetPipelineInfo(response);
    SetBaseResponse(request, response);
    SendResponse(std::move(responsePtr), res);
    return true;
}
}
