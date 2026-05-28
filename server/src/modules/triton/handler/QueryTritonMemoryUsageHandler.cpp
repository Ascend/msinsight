// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *
#include "QueryTritonMemoryUsageHandler.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"
#include "TritonService.h"
#include "WsSender.h"

namespace Dic::Module::Triton {
bool QueryTritonMemoryUsageHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<Protocol::TritonMemoryUsageRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<Protocol::TritonMemoryUsageResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    const auto &service = TritonService::Instance();
    auto segments = service.QuerySegmentsContainRange(request.scopeType, request.timestamp);
    response.segments = std::move(segments);
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
