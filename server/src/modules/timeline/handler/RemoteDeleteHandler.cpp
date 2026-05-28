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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "TraceFileSimulationParser.h"
#include "TraceTime.h"
#include "ParserStatusManager.h"
#include "JsonFileParserManager.h"
#include "RemoteDeleteHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool RemoteDeleteHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    RemoteDeleteRequest &request = dynamic_cast<RemoteDeleteRequest &>(*requestPtr.get());
    std::unique_ptr<RemoteDeleteResponse> responsePtr = std::make_unique<RemoteDeleteResponse>();
    RemoteDeleteResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    Timeline::JsonFileParserManager::GetTraceFileParser().DeleteParseFiles(request.params.rankId);
    TraceFileSimulationParser::Instance().DeleteParseFiles(request.params.rankId);
    GetUpdateTime(response.body);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void RemoteDeleteHandler::GetUpdateTime(RemoteDeleteBody &body) {
    auto fileIdList = DataBaseManager::Instance().GetAllRankId();
    TraceTime::Instance().Reset();
    auto &parseStatusInstance = ParserStatusManager::Instance();
    for (const auto &fileId : fileIdList) {
        if (parseStatusInstance.GetParserStatus(fileId) == ParserStatus::FINISH) {
            uint64_t min = UINT64_MAX;
            uint64_t max = 0;
            auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
            if (database == nullptr) {
                ServerLog::Error("Remote delete failed to get connection.");
                return;
            }
            if (!database->QueryExtremumTimestamp(min, max)) {
                return;
            }
            if (min != max || max != 0) {
                body.startTimeUpdated = true;
                TraceTime::Instance().UpdateTime(min, max);
            }
            ServerLog::Info("update time. rankId:", fileId, ", min:", min, ", max:", max);
        }
    }
    if (body.startTimeUpdated) {
        body.maxTimeStamp = TraceTime::Instance().GetDuration();
    }
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
