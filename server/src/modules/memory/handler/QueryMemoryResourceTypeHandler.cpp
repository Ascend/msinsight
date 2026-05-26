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
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "QueryMemoryResourceTypeHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryResourceTypeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryResourceTypeRequest &request = dynamic_cast<MemoryResourceTypeRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryResourceTypeResponse> responsePtr = std::make_unique<MemoryResourceTypeResponse>();
    MemoryResourceTypeResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!CheckStrParamValid(request.rankId, errorMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(request.rankId);
    if (!database || !database->QueryMemoryResourceType(response.type)) {
        SetMemoryError(ErrorCode::QUERY_MEMORY_RESOURCE_TYPE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    // add response to response queue in session
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
