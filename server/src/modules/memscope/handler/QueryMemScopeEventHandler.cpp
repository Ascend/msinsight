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
#include "QueryMemScopeEventHandler.h"

namespace Dic::Module::MemScope {
bool QueryMemScopeEventHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemScopeEventRequest &>(*requestPtr);
    std::unique_ptr<MemScopeEventResponse> responsePtr = std::make_unique<MemScopeEventResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (database == nullptr) {
        errorMsg = "Get memscope database failed when querying events table.";
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    int64_t total = database->QueryEventsByRequestParams(request.params, responsePtr->events);
    if (total < 0) {
        errorMsg = "Failed to query events table using params: Prepare sql failed.";
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    responsePtr->total = total;
    responsePtr->withCallStackC = database->withCallStackC;
    responsePtr->withCallStackPython = database->withCallStackPython;
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // namespace Dic::Module::MemScope
