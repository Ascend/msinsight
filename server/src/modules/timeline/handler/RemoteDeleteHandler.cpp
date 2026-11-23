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
#include "DbTraceDataBase.h"
#include "TraceFileSimulationParser.h"
#include "TraceTime.h"
#include "ParserStatusManager.h"
#include "JsonFileParserManager.h"
#include "RemoteDeleteHandler.h"
#include "RankLaneMergeCoordinator.h"
#include "MsprofFtraceImportService.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
namespace {
bool RestorePmuSourceTimeRanges(const std::string &rankId, RemoteDeleteBody &body) {
    auto &manager = DataBaseManager::Instance();
    auto sources = RankLaneMergeCoordinator::Instance().GetSuccessfulSources(rankId);
    if (sources.empty()) {
        sources = manager.GetSourceFileIdsByRankId(rankId);
    }
    std::vector<std::shared_ptr<VirtualTraceDatabase>> databases;
    bool hasPmuSource = false;
    for (const auto &source : sources) {
        auto database = manager.GetTraceDatabaseByFileId(source);
        auto fullDb = std::dynamic_pointer_cast<FullDb::DbTraceDataBase>(database);
        hasPmuSource = hasPmuSource || (fullDb != nullptr && fullDb->CheckTableExist("HOST_PMU_COUNTER_DATA"));
        databases.emplace_back(std::move(database));
    }
    if (!hasPmuSource) {
        return false;
    }
    for (const auto &database : databases) {
        auto fullDb = std::dynamic_pointer_cast<FullDb::DbTraceDataBase>(database);
        if (fullDb != nullptr) {
            fullDb->UpdateStartTime(rankId);
            body.startTimeUpdated = body.startTimeUpdated || TraceTime::Instance().GetStartTime() != UINT64_MAX;
            continue;
        }
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;
        if (database == nullptr || !database->QueryExtremumTimestamp(min, max) || min > max || (min == 0 && max == 0)) {
            continue;
        }
        body.startTimeUpdated = true;
        TraceTime::Instance().UpdateTime(min, max);
        TraceTime::Instance().UpdateCardTimeDuration(rankId, min, max);
        TraceTime::Instance().UpdateCardMinTimestamp(rankId, min);
    }
    return true;
}
}

bool RemoteDeleteHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    RemoteDeleteRequest &request = dynamic_cast<RemoteDeleteRequest &>(*requestPtr.get());
    std::unique_ptr<RemoteDeleteResponse> responsePtr = std::make_unique<RemoteDeleteResponse>();
    RemoteDeleteResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::vector<std::string> legacyRankIds;
    for (const auto &rankId : request.params.rankId) {
        if (!FullDb::MsprofFtraceImportService::Instance().Remove(rankId)) {
            legacyRankIds.push_back(rankId);
        }
    }
    if (!legacyRankIds.empty()) {
        Timeline::JsonFileParserManager::GetTraceFileParser().DeleteParseFiles(legacyRankIds);
        TraceFileSimulationParser::Instance().DeleteParseFiles(legacyRankIds);
    }
    GetUpdateTime(response.body);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void RemoteDeleteHandler::GetUpdateTime(RemoteDeleteBody &body) {
    auto fileIdList = DataBaseManager::Instance().GetAllRankId();
    TraceTime::Instance().Reset();
    auto &parseStatusInstance = ParserStatusManager::Instance();
    for (const auto &fileId : fileIdList) {
        const auto status = parseStatusInstance.GetParserStatus(fileId);
        if (status == ParserStatus::FINISH || status == ParserStatus::FINISH_ALL) {
            if (RestorePmuSourceTimeRanges(fileId, body)) {
                continue;
            }
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
                TraceTime::Instance().UpdateCardTimeDuration(fileId, min, max);
                TraceTime::Instance().UpdateCardMinTimestamp(fileId, min);
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
