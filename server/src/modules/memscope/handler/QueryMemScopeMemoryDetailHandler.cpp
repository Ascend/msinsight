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

#include "DataBaseManager.h"
#include "QueryMemScopeMemoryDetailHandler.h"

namespace Dic {
namespace Module {
namespace MemScope {
bool QueryMemScopeMemoryDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemScopeMemoryDetailRequest &>(*requestPtr);
    std::unique_ptr<MemScopeMemoryDetailsResponse> responsePtr = std::make_unique<MemScopeMemoryDetailsResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    if (!MemScopeService::IsValidMemoryEventType(MEM_SCOPE_DUMP_EVENT::MALLOC, request.params.eventType)) {
        errorMsg = "The eventType is invalid.";
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    BuildDetailTreeContext buildCtx(
        request.params.deviceId, request.params.timestamp, request.params.eventType, request.params.relativeTime);
    if (!MemScopeService::BuildMemoryAllocDetailTreeByContext(response.detail, buildCtx)) {
        errorMsg = "Failed to query memory allocation detail.";
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
}
}
