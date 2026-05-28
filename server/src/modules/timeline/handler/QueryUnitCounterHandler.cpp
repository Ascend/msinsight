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
#include "TraceTime.h"
#include "DataBaseManager.h"
#include "QueryUnitCounterHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryUnitCounterHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    UnitCounterRequest &request = dynamic_cast<UnitCounterRequest &>(*requestPtr.get());
    std::unique_ptr<UnitCounterResponse> responsePtr = std::make_unique<UnitCounterResponse>();
    UnitCounterResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    const std::string hostString = "Host";
    if (StringUtil::EndWith(request.params.rankId, hostString)) {
        request.params.rankId = DataBaseManager::Instance().GetAnyTraceDatabaseId();
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query unit counter failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    bool result = database->QueryUnitCounter(request.params, minTimestamp, response.body.data);
    if (!result) {
        SetTimelineError(ErrorCode::QUERY_UNIT_COUNTER_FAILED);
    }
    SendResponse(std::move(responsePtr), true);
    return result;
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
