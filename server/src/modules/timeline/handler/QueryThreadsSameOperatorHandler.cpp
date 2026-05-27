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
#include "QueryThreadsSameOperatorHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryThreadsSameOperatorHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitThreadsOperatorsRequest &request = dynamic_cast<UnitThreadsOperatorsRequest &>(*requestPtr.get());
    std::unique_ptr<UnitThreadsOperatorsResponse> responsePtr = std::make_unique<UnitThreadsOperatorsResponse>();
    UnitThreadsOperatorsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto db = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (db == nullptr) {
        ServerLog::Error("Query threads same operator failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    UnitThreadsOperatorsParams queryParams = request.params;
    PythonStackHelper::RestoreSameOperatorParams(queryParams);
    std::vector<uint64_t> trackIdList;
    auto &TrackInfoManagerIns = TrackInfoManager::Instance();
    for (const auto &process : queryParams.processes) {
        for (const auto &tid : process.tidList) {
            trackIdList.emplace_back(TrackInfoManagerIns.GetTrackId(queryParams.rankId, process.pid, tid));
        }
    }
    bool result = db->QueryThreadSameOperatorsDetails(queryParams, response.body, minTimestamp, trackIdList);
    if (!result) {
        SetTimelineError(ErrorCode::QUERY_THREAD_SAME_OPERATORS_DETAIL_FAILED);
    }
    SendResponse(std::move(responsePtr), result);
    return result;
}

} // Timeline
} // Module
} // Dic
