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
#include "QueryThreadsHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryThreadsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitThreadsRequest &request = dynamic_cast<UnitThreadsRequest &>(*requestPtr.get());
    std::unique_ptr<UnitThreadsResponse> responsePtr = std::make_unique<UnitThreadsResponse>();
    UnitThreadsResponse &response = *responsePtr.get();
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
        ServerLog::Error("Query threads failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    UnitThreadsParams queryParams = request.params;
    std::vector<uint64_t> trackIdList;
    trackIdList.reserve(queryParams.metadataList.size());
    for (auto &metadata : queryParams.metadataList) {
        PythonStackHelper::RestoreMetadata(metadata);
        if (!metadata.isPythonStack && metadata.tid == Protocol::PYTHON_API_THREAD_ID &&
            metadata.metaType == ENUM_TO_STR(PROCESS_TYPE::API).value_or("")) {
            metadata.hidePythonFunction = true;
        }
        uint64_t trackId = TrackInfoManager::Instance().GetTrackId(queryParams.rankId, metadata.pid, metadata.tid);
        trackIdList.push_back(trackId);
    }
    bool result = database->QueryThreads(queryParams, response.body, minTimestamp, trackIdList);
    if (!result) {
        SetTimelineError(ErrorCode::QUERY_THREAD_FAILED);
    }
    SendResponse(std::move(responsePtr), result);
    return result;
}
} // Timeline
} // Module
} // Dic
