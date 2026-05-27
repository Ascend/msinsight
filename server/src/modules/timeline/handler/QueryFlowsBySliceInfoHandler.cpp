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
#include "QueryFlowsBySliceInfoHandler.h"
namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryFlowsBySliceInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitFlowsRequest &request = dynamic_cast<UnitFlowsRequest &>(*requestPtr.get());
    std::unique_ptr<UnitFlowsResponse> responsePtr = std::make_unique<UnitFlowsResponse>();
    UnitFlowsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query flows by slice info failed to get connection. ");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    UnitFlowsParams queryParams = request.params;
    PythonStackHelper::RestoreUnitFlowsParams(queryParams);
    uint64_t trackId = TrackInfoManager::Instance().GetTrackId(queryParams.rankId, queryParams.pid, queryParams.tid);
    try {
        database->QueryUnitFlows(queryParams, response.body, minTimestamp, trackId);
    } catch (DatabaseException &e) {
        e.Log("Query flows by slice info Fail, ");
        SetTimelineError(ErrorCode::QUERY_UNIT_FLOWS_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Timeline
} // Module
} // Dic
