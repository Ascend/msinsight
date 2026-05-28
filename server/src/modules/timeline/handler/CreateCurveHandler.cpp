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
#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "CreateCurveHandler.h"
namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool CreateCurveHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    CreateCurveRequest &request = dynamic_cast<CreateCurveRequest &>(*requestPtr.get());
    std::unique_ptr<CreateCurveResponse> responsePtr = std::make_unique<CreateCurveResponse>();
    CreateCurveResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    if (!request.params.x.empty() && !StringUtil::CheckSqlValid(request.params.x)) {
        ServerLog::Error("The current input does not support generating line graphs");
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string viewName;
    if (request.params.type == "1") {
        viewName = CreateSliceDurationCurve(request);
    } else if (request.params.type == "2") {
        viewName = CreateBubbleCurve(request);
    }
    response.body.curveName = viewName;
    SendResponse(std::move(responsePtr), true);
    return true;
}

std::string CreateCurveHandler::CreateBubbleCurve(const CreateCurveRequest &request) {
    std::string trackId = std::to_string(
        TrackInfoManager::Instance().GetTrackId(request.params.fileId, request.params.pid, request.params.tid));
    auto start = TraceTime::Instance().GetStartTime();
    std::string viewName = request.params.x + "_bubble_" + trackId + "_curve";
    std::string sql = "WITH gaps AS (SELECT timestamp,end_time,LEAD( timestamp ) OVER ( ORDER BY timestamp ) - "
                      "end_time AS duration FROM slice WHERE track_id = " +
        trackId + " AND name = '" + request.params.x +
        "' "
        "ORDER BY timestamp ) SELECT end_time - " +
        std::to_string(start) +
        " as startTime,duration,AVG( duration ) OVER ( ) AS avg_duration"
        " FROM gaps WHERE duration > 0;";
    std::string curveSql = "DROP VIEW IF EXISTS '" + viewName + "'; CREATE VIEW '" + viewName + "' AS " + sql;
    openApi->CreateCurve(request.params.fileId, curveSql);
    return viewName;
}

std::string CreateCurveHandler::CreateSliceDurationCurve(const CreateCurveRequest &request) {
    std::string trackId = std::to_string(
        TrackInfoManager::Instance().GetTrackId(request.params.fileId, request.params.pid, request.params.tid));
    std::string viewName = request.params.x + "_" + trackId + "_curve";
    auto start = Timeline::TraceTime::Instance().GetStartTime();
    std::string sql = "SELECT timestamp - " + std::to_string(start) +
        " as startTime,AVG( duration ) OVER ( ) AS avg_duration,duration FROM slice WHERE track_id = " + trackId +
        " AND name = '" + request.params.x + "' ORDER BY timestamp;";
    std::string curveSql = "DROP VIEW IF EXISTS '" + viewName + "'; CREATE VIEW '" + viewName + "' AS " + sql;
    openApi->CreateCurve(request.params.fileId, curveSql);
    return viewName;
}
} // namespace Timeline
} // namespace Module
} // namespace Dic
