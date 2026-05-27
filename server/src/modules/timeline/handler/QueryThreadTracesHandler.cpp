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
#include "DataBaseManager.h"
#include "PythonStackHelper.h"
#include "QueryThreadTracesHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryThreadTracesHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitThreadTracesRequest &request = dynamic_cast<UnitThreadTracesRequest &>(*requestPtr.get());
    std::unique_ptr<UnitThreadTracesResponse> responsePtr = std::make_unique<UnitThreadTracesResponse>();
    UnitThreadTracesResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.cardId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (request.params.metaType == "OVERLAP_ANALYSIS" &&
        !database->CheckValueFromStatusInfoTable(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS)) {
        response.body.isLoading = true;
        SetTimelineError(ErrorCode::OVERLAP_ANALYSIS_PARSE_NOT_FINISH);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (renderEngine == nullptr) {
        ServerLog::Error("Query thread traces Failed to render.");
        SetTimelineError(ErrorCode::QUERY_THREAD_TRACES_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (std::empty(request.params.threadIdList)) {
        UnitThreadTracesParams queryParams = request.params;
        PythonStackHelper::RestoreThreadTracesParams(queryParams);
        uint64_t trackId =
            TrackInfoManager::Instance().GetTrackId(queryParams.cardId, queryParams.processId, queryParams.threadId);
        renderEngine->QueryThreadTraces(queryParams, response.body, minTimestamp, trackId);
    } else {
        QueryTracesByTrackIds(request, response, minTimestamp);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

std::string QueryThreadTracesHandler::GetRequestKey(Request &requestPtr) {
    UnitThreadTracesRequest &request = dynamic_cast<UnitThreadTracesRequest &>(requestPtr);
    std::vector<std::string> keyContentList = {request.command, request.params.processId, request.params.threadId};
    return StringUtil::join(keyContentList, "_");
}

void QueryThreadTracesHandler::QueryTracesByTrackIds(
    UnitThreadTracesRequest &request, UnitThreadTracesResponse &response, uint64_t minTimestamp) {
    for (const auto &threadId : request.params.threadIdList) {
        UnitThreadTracesBody tempBody;
        UnitThreadTracesParams queryParams = request.params;
        queryParams.threadId = threadId;
        PythonStackHelper::RestoreThreadTracesParams(queryParams);
        uint64_t trackId =
            TrackInfoManager::Instance().GetTrackId(queryParams.cardId, queryParams.processId, queryParams.threadId);
        renderEngine->QueryThreadTraces(queryParams, tempBody, minTimestamp, trackId);
        for (size_t i = 0; i < tempBody.data.size(); ++i) {
            while (response.body.data.size() <= i) {
                response.body.data.emplace_back();
            }
            response.body.data[i].insert(response.body.data[i].end(), tempBody.data[i].begin(), tempBody.data[i].end());
        }
        response.body.maxDepth = std::max(tempBody.maxDepth, response.body.maxDepth);
        response.body.currentMaxDepth = std::max(tempBody.currentMaxDepth, response.body.currentMaxDepth);
    }
    for (auto &item : response.body.data) {
        std::sort(item.begin(), item.end(), [](const ThreadTraces &first, const ThreadTraces &second) {
            if (first.startTime != second.startTime) {
                return first.startTime < second.startTime;
            }
            if (first.endTime != second.endTime) {
                return first.endTime < second.endTime;
            }
            return first.id < second.id;
        });
    }
}
} // namespace Timeline
} // namespace Module
} // namespace Dic
