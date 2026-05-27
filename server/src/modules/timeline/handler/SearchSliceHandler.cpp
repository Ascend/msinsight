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
#include "TraceTime.h"
#include "TrackInfoManager.h"
#include "SearchSliceHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool SearchSliceHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    SearchSliceRequest &request = dynamic_cast<SearchSliceRequest &>(*requestPtr.get());
    std::unique_ptr<SearchSliceResponse> responsePtr = std::make_unique<SearchSliceResponse>();
    SearchSliceResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    response.body.rankId = request.params.rankId;
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Search slice can't find rankId.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::vector<TrackQuery> trackQueryVec;
    for (const auto &item : request.params.metadataList) {
        Protocol::Metadata metadata = item;
        bool isPythonStack = PythonStackHelper::RestoreMetadata(metadata);
        if (metadata.rankId == request.params.rankId && !metadata.tid.empty() && !metadata.pid.empty()) {
            TrackQuery trackQuery;
            trackQuery.fileId = request.fileId;
            trackQuery.rankId = metadata.rankId;
            trackQuery.processId = metadata.pid;
            trackQuery.threadId = metadata.tid;
            trackQuery.startTime = metadata.lockStartTime + minTimestamp; // 校验过，保证 lockStartTime < lockEndTime
            trackQuery.endTime = metadata.lockEndTime + minTimestamp; // 校验过，保证 lockEndTime + minTime < UINT64_MAX
            trackQuery.trackId = TrackInfoManager::Instance().GetTrackId(metadata.rankId, metadata.pid, metadata.tid);
            trackQuery.metaType = metadata.metaType;
            trackQuery.displayMetaType =
                isPythonStack ? PythonStackHelper::GetPythonStackDisplayMetaType() : metadata.metaType;
            trackQuery.isPythonStack = isPythonStack;
            trackQueryVec.emplace_back(trackQuery);
        }
    }
    if (!database->SearchSliceName(
            request.params, request.params.index - 1, minTimestamp, response.body, trackQueryVec)) {
        ServerLog::Error("Failed to search slice name.");
        SetTimelineError(ErrorCode::QUERY_SLICE_NAME_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    response.body.dbPath = database->GetDbPath();
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Timeline
} // Module
} // Dic
