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
#include "TraceTime.h"
#include "TrackInfoManager.h"
#include "SearchAllSlicesHandler.h"
#include "SearchSliceCacheManager.h"

// clang-format off
namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

std::vector<TrackQuery> SearchAllSlicesHandler::BuildTrackQueryVec(
    const Protocol::SearchAllSliceParams& params, const std::string& dbPath, uint64_t minTimestamp)
{
    std::vector<TrackQuery> trackQueryVec;
    for (const auto& item : params.metadataList) {
        if ((params.rankId == item.rankId || item.rankId == dbPath) && !item.pid.empty() && !item.tid.empty()) {
            TrackQuery trackQuery;
            trackQuery.rankId = params.rankId;
            trackQuery.processId = item.pid;
            trackQuery.threadId = item.tid;
            trackQuery.trackId = TrackInfoManager::Instance().GetTrackId(params.rankId, item.pid, item.tid);
            trackQuery.startTime = item.lockStartTime + minTimestamp;
            trackQuery.endTime = item.lockEndTime + minTimestamp;
            trackQuery.metaType = item.metaType;
            trackQueryVec.emplace_back(trackQuery);
        }
    }
    return trackQueryVec;
}

bool SearchAllSlicesHandler::FallbackToSqlQuery(VirtualTraceDatabase* database,
    const Protocol::SearchAllSliceParams& params, Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp)
{
    std::vector<TrackQuery> emptyTrackQuery;
    if (!database->SearchAllSlicesDetails(params, body, minTimestamp, emptyTrackQuery)) {
        ServerLog::Error("Failed to search slice details.");
        SetTimelineError(ErrorCode::QUERY_SLICE_DETAIL_FAILED);
        return false;
    }
    return true;
}

bool SearchAllSlicesHandler::HandleWithLockRange(VirtualTraceDatabase* database,
    const Protocol::SearchAllSliceParams& params, Protocol::SearchAllSlicesBody& body,
    uint64_t minTimestamp, const std::vector<TrackQuery>& trackQueryVec)
{
    if (!database->SearchAllSlicesDetails(params, body, minTimestamp, trackQueryVec)) {
        ServerLog::Error("Failed to search slice details with lock range.");
        SetTimelineError(ErrorCode::QUERY_SLICE_DETAIL_FAILED);
        return false;
    }
    return true;
}

bool SearchAllSlicesHandler::LoadOrRefreshCache(CacheGuard& guard, SearchSliceCacheManager& cacheManager,
    const std::string& cacheKey, VirtualTraceDatabase* database,
    const Protocol::SearchAllSliceParams& params, uint64_t minTimestamp)
{
    bool needReload = false;
    if (!guard) {
        guard = cacheManager.getOrCreate(cacheKey);
        needReload = true;
    } else if (!guard->isDataValid(params.rankId, params.searchContent)) {
        guard->clear();
        needReload = true;
    }

    if (needReload) {
        if (!database->LoadSliceCache(*guard, params, minTimestamp)) {
            ServerLog::Warn("LoadSliceCache failed.");
            return false;
        }
    }
    return true;
}

bool SearchAllSlicesHandler::HandleWithSoACache(VirtualTraceDatabase* database,
    const Protocol::SearchAllSliceParams& params, Protocol::SearchAllSlicesBody& body, uint64_t minTimestamp)
{
    std::string cacheKey = SearchSliceCacheManager::makeKey(params.rankId, params.searchContent);
    auto& cacheManager = SearchSliceCacheManager::Instance();
    CacheGuard guard; // 空句柄，由 LoadOrRefreshCache 填充

    if (!LoadOrRefreshCache(guard, cacheManager, cacheKey, database, params, minTimestamp)) {
        return false;
    }

    SearchSliceCacheManager::SortCache(*guard, params.orderBy, params.order);
    PagedResult pagedResult = SearchSliceCacheManager::FilterAndPage(
        *guard, params.nameFilter, params.current, params.pageSize);

    body.currentPage = params.current;
    body.pageSize = params.pageSize;
    body.count = pagedResult.totalCount;

    if (!pagedResult.rows.empty()) {
        if (!database->FetchSliceDetails(*guard, pagedResult.rows, params, body, minTimestamp)) {
            ServerLog::Warn("FetchSliceDetails failed, returning partial results.");
        }
    }
    return true;
}

bool SearchAllSlicesHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr)
{
    SearchAllSlicesRequest& request = dynamic_cast<SearchAllSlicesRequest&>(*requestPtr.get());
    std::unique_ptr<SearchAllSlicesResponse> responsePtr = std::make_unique<SearchAllSlicesResponse>();
    SearchAllSlicesResponse& response = *responsePtr.get();
    SetBaseResponse(request, response);

    std::string warnMsg;
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        ServerLog::Warn(warnMsg);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get search all slices connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.fileId = request.fileId;

    std::vector<TrackQuery> trackQueryVec = BuildTrackQueryVec(request.params, database->GetDbPath(), minTimestamp);

    bool success = false;
    if (!trackQueryVec.empty()) {
        success = HandleWithLockRange(database.get(), request.params, response.body, minTimestamp, trackQueryVec);
    } else {
        success = HandleWithSoACache(database.get(), request.params, response.body, minTimestamp);
        if (!success) {
            success = FallbackToSqlQuery(database.get(), request.params, response.body, minTimestamp);
        }
    }

    if (success) {
        response.body.dbPath = database->GetDbPath();
    }
    SendResponse(std::move(responsePtr), success);
    return success;
}

} // Timeline
} // Module
} // Dic
// clang-format on
