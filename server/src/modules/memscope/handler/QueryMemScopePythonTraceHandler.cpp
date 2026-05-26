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
#include "QueryMemScopePythonTraceHandler.h"

namespace Dic {
namespace Module {
namespace MemScope {
bool QueryMemScopePythonTraceHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemScopePythonTraceRequest &>(*requestPtr);
    std::unique_ptr<MemScopePythonTracesResponse> responsePtr = std::make_unique<MemScopePythonTracesResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (database == nullptr) {
        errorMsg = "Get memscope database failed when querying python traces.";
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    database->QueryPythonTrace(request.params, response.trace);
    if (response.trace.Empty()) {
        Server::ServerLog::Warn("Query memscope python traces: empty data.");
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    response.trace.threadId = request.params.threadId;
    if (request.allowTrim && response.trace.slices.size() > TRIM_THRESHOLD) {
        response.trace.Trim(DEFAULT_TRIM_STRATEGY);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Memory
} // Module
} // Dic
