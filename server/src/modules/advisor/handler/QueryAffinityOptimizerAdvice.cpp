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
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "AffinityOptimizerAdvisor.h"
#include "WsSessionManager.h"
#include "TraceTime.h"
#include "QueryAffinityOptimizerAdvice.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool QueryAffinityOptimizerAdvice::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<AffinityOptimizerRequest &>(*requestPtr);
    std::unique_ptr<AffinityOptimizerResponse> responsePtr = std::make_unique<AffinityOptimizerResponse>();
    AffinityOptimizerResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    uint64_t minTimestamp = Timeline::TraceTime::Instance().GetStartTime();
    std::string error;
    request.params.Check(minTimestamp, error);
    if (!std::empty(error)) {
        ServerLog::Error(error);
        SetAdvisorError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (!AffinityOptimizerAdvisor::Process(request.params, response.body)) {
        ServerLog::Error("Failed to Query Affinity Optimizer Advice");
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Dic::Module::Advisor
