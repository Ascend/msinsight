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
#include "QueryTritonMemoryBlocksHandler.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"
#include "TritonService.h"
#include "WsSender.h"
namespace Dic::Module::Triton {
bool QueryTritonMemoryBlocksHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::TritonMemoryBlocksRequest &>(*requestPtr);
    std::unique_ptr<Protocol::TritonMemoryBlocksResponse> responsePtr =
        std::make_unique<Protocol::TritonMemoryBlocksResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    if (request.startTimestamp > request.endTimestamp) {
        SendResponse(std::move(responsePtr), false, "Invalid range: start timestamp is greater than end time.");
        return false;
    }
    auto &service = TritonService::Instance();
    auto blocks = service.QueryBlocksContainRange(request.scopeType, request.startTimestamp, request.endTimestamp);
    response.blocks = std::move(blocks);
    response.status = service.GetCompileStatus(request.scopeType);
    response.errMsg = service.GetCompileStatus(request.scopeType);
    response.UpdateInfo();
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
