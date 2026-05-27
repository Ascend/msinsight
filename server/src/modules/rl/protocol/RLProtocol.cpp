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

#include "RLProtocol.h"
#include "ProtocolDefs.h"
#include "RLProtocolRequest.h"
#include "RLProtocolResponse.h"
#include "RLProtocolUtil.h"

namespace Dic::Protocol {
void RLProtocol::RegisterJsonToRequestFuncs() { jsonToReqFactory.emplace(REQ_REQ_RL_PIPELINE, ToRLPipelineRequest); }

void RLProtocol::RegisterResponseToJsonFuncs() { resToJsonFactory.emplace(REQ_REQ_RL_PIPELINE, ToRLPipelineResponse); }

void RLProtocol::RegisterEventToJsonFuncs() {}

#pragma region <<Json To Request>>
std::unique_ptr<Request> RLProtocol::ToRLPipelineRequest(const json_t &json, std::string &error) {
    auto reqPtr = std::make_unique<RLPipelineRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

#pragma endregion

#pragma region <<Reponse To Json>>

std::optional<document_t> RLProtocol::ToRLPipelineResponse(const Response &response) {
    return ToResponseJson<RLPipelineResponse>(dynamic_cast<const RLPipelineResponse &>(response));
}

#pragma endregion
}
