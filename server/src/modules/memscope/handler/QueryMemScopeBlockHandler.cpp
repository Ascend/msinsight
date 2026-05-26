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
#include "ServerLog.h"
#include "DataBaseManager.h"
#include "QueryMemScopeBlockHandler.h"

namespace Dic::Module::MemScope {
bool QueryMemScopeBlockHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemScopeMemoryBlockRequest &>(*requestPtr);
    std::unique_ptr<MemScopeMemoryBlocksResponse> responsePtr = std::make_unique<MemScopeMemoryBlocksResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    auto memoryDatabase = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (memoryDatabase == nullptr) {
        Server::ServerLog::Error("[MemScope] Failed to query memory blocks: get database connection failed.");
        SendResponse(std::move(responsePtr), false, REQUEST_ERROR_UNKNOWN);
        return false;
    }
    const int64_t total = memoryDatabase->QueryMemoryBlocks(request.params, request.isTable, response.blocks);
    if (total < 0) {
        Server::ServerLog::Error("[MemScope] Failed to query memory blocks: query db failed.");
        SendResponse(std::move(responsePtr), false, REQUEST_ERROR_UNKNOWN);
        return false;
    }
    response.total = static_cast<uint64_t>(total);
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Dic::Module::MemScope
