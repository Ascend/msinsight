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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "SummaryService.h"
#include "QueryParallelismPerformanceHandler.h"

namespace Dic::Module::Summary {
bool QueryParallelismPerformanceHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<QueryParallelismPerformanceRequest &>(*requestPtr);
    std::unique_ptr<ParallelismPerformanceResponse> responsePtr = std::make_unique<ParallelismPerformanceResponse>();
    ParallelismPerformanceResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    // check request parameter
    std::string err;
    if (!request.params.CheckParams(err)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    bool res = SummaryService::QueryParallelismPerformanceInfo(request.params, response.indicatorData);
    SendResponse(std::move(responsePtr), res);
    return true;
}
}
