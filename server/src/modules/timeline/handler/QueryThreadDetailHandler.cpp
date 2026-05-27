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
#include "PythonStackHelper.h"
#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "QueryThreadDetailHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryThreadDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    ThreadDetailRequest &request = dynamic_cast<ThreadDetailRequest &>(*requestPtr.get());
    std::unique_ptr<UnitThreadDetailResponse> responsePtr = std::make_unique<UnitThreadDetailResponse>();
    UnitThreadDetailResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    if (std::string errMsg; !request.params.CheckParams(errMsg)) {
        ServerLog::Error("Query thread detail failed: " + errMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (renderEngine == nullptr) {
        ServerLog::Error("Query thread detail failed to get connection.");
        SetTimelineError(ErrorCode::QUERY_THREAD_DETAIL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    ThreadDetailParams queryParams = request.params;
    PythonStackHelper::RestoreThreadDetailParams(queryParams);
    uint64_t trackId = TrackInfoManager::Instance().GetTrackId(queryParams.rankId, queryParams.pid, queryParams.tid);
    renderEngine->QueryThreadDetail(queryParams, response.body, trackId);
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Timeline
} // Module
} // Dic
