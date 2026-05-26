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
#include "QueryMemoryTypeHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryTypeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryTypeRequest &request = dynamic_cast<MemoryTypeRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryTypeResponse> responsePtr = std::make_unique<MemoryTypeResponse>();
    MemoryTypeResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!CheckStrParamValid(request.rankId, errorMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(request.rankId);
    if (!database || !database->QueryMemoryType(response.type, response.graphId)) {
        SetMemoryError(ErrorCode::QUERY_MEMORY_TYPE_FAILED);
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
