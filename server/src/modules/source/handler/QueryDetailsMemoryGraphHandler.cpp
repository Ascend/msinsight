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
#include "DetailsService.h"
#include "SourceProtocolResponse.h"
#include "WsSessionManager.h"
#include "SourceProtocolRequest.h"
#include "QueryDetailsMemoryGraphHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryDetailsMemoryGraphHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<DetailsMemoryGraphRequest &>(*requestPtr);
    std::unique_ptr<DetailsMemoryGraphResponse> responsePtr = std::make_unique<DetailsMemoryGraphResponse>();
    DetailsMemoryGraphResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (auto [isValid, errMsg] = request.params.Valid(); isValid == false) {
        ServerLog::Error("Parameter of command ", request.command, " is invalid, error:", errMsg);
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    bool result = DetailsService::QueryMemoryGraph(request, response);
    SendResponse(std::move(responsePtr), result);
    return result;
}
}
}
}
