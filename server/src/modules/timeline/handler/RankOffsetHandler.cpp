/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can be obtained a copy of Mulan PSL v2 at:
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
#include "RankOffsetHandler.h"
#include "RankOffsetCalculator.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

struct FailedRank {
    std::string rankId;
    std::string reason;
};

struct SortedSliceQuery {
    std::vector<Protocol::SimpleSlice> slices;
    std::set<std::string> processIds;
};

namespace {

constexpr int32_t RANK_OFFSET_ERROR_CODE = 1;

void SetRankOffsetError(RankOffsetResponse &response, const std::string &message) {
    response.body.error.code = RANK_OFFSET_ERROR_CODE;
    response.body.error.message = message;
}

void SortSlicesByTimestamp(std::vector<Protocol::SimpleSlice> &slices) {
    std::sort(slices.begin(), slices.end(),
        [](const Protocol::SimpleSlice &a, const Protocol::SimpleSlice &b) { return a.timestamp < b.timestamp; });
}

int64_t FindSelectedSliceIndex(
    const std::vector<Protocol::SimpleSlice> &pidSlices, uint64_t startTime, uint64_t duration) {
    for (size_t i = 0; i < pidSlices.size(); ++i) {
        if (pidSlices[i].timestamp == startTime && pidSlices[i].duration == duration) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

std::vector<std::string> GetTargetRankIds(const std::string &baseRankId) {
    auto allRankIds = DataBaseManager::Instance().GetAllRankId();
    std::vector<std::string> targetRankIds;
    for (const auto &rankId : allRankIds) {
        if (rankId != baseRankId) {
            targetRankIds.push_back(rankId);
        }
    }
    return targetRankIds;
}

std::string BuildFailedRanksMessage(const std::vector<FailedRank> &failedRanks) {
    std::string details;
    for (size_t i = 0; i < failedRanks.size(); ++i) {
        if (i > 0) {
            details += "; ";
        }
        details += failedRanks[i].rankId + " (" + failedRanks[i].reason + ")";
    }
    return "Failed ranks: " + details;
}

bool QuerySlicesByName(VirtualTraceDatabase &db, const std::string &rankId, const std::string &sliceName,
    const std::string &metaType, RankOffsetSide side, SortedSliceQuery &query) {
    if (metaType == "TEXT") {
        return db.QueryTextSlicesByName(sliceName, metaType, query.slices, query.processIds);
    }
    if (side == RankOffsetSide::HOST) {
        return db.QueryHostSlicesByName(sliceName, metaType, query.slices, query.processIds);
    }
    if (side == RankOffsetSide::DEVICE) {
        return db.QueryDeviceSlicesByName(rankId, sliceName, metaType, query.slices, query.processIds);
    }
    return true;
}

SortedSliceQuery QueryAndSortSlices(
    const std::string &rankId, const std::string &sliceName, const std::string &metaType, RankOffsetSide side) {
    SortedSliceQuery query;
    auto db = DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
    if (db == nullptr) {
        return query;
    }

    QuerySlicesByName(*db, rankId, sliceName, metaType, side, query);
    SortSlicesByTimestamp(query.slices);
    return query;
}

const Protocol::SimpleSlice *PickSliceByIndex(const std::vector<Protocol::SimpleSlice> &slices, int64_t index) {
    if (index < 0 || static_cast<int64_t>(slices.size()) <= index) {
        return nullptr;
    }
    return &slices[index];
}

} // namespace

bool RankOffsetHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    RankOffsetRequest &request = dynamic_cast<RankOffsetRequest &>(*requestPtr.get());
    auto responsePtr = std::make_unique<RankOffsetResponse>();
    RankOffsetResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);

    // 1. Validate parameters
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        ServerLog::Warn(errorMsg);
        SetRankOffsetError(response, errorMsg);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    response.body.baseOffset = TraceTime::Instance().GetOffsetByFileId(request.params.rankId);

    // 2. Determine side (GPU/DEVICE or CPU/HOST)
    RankOffsetSide side = RankOffsetCalculator::GetSide(request.params.metaType);
    if (side == RankOffsetSide::UNSUPPORTED) {
        errorMsg = "Unsupported metaType: " + request.params.metaType;
        ServerLog::Warn(errorMsg);
        SetRankOffsetError(response, errorMsg);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    RankOffsetAlignType alignType = RankOffsetCalculator::ParseAlignType(request.params.alignType);
    if (alignType == RankOffsetAlignType::UNSUPPORTED) {
        errorMsg = "Unsupported alignType: " + request.params.alignType;
        ServerLog::Warn(errorMsg);
        SetRankOffsetError(response, errorMsg);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 3. Resolve base slice and its ordinal index
    int64_t baseIndex = -1;
    Protocol::SimpleSlice baseSlice;
    if (!ResolveBaseSlice(request.params, side, baseSlice, baseIndex, errorMsg)) {
        ServerLog::Warn(errorMsg);
        SetRankOffsetError(response, errorMsg);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 4. Get target ranks
    auto targetRankIds = GetTargetRankIds(request.params.rankId);
    if (targetRankIds.empty()) {
        errorMsg = "No target ranks found. Only base rank is loaded.";
        ServerLog::Warn(errorMsg);
        SetRankOffsetError(response, errorMsg);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 5. Calculate offsets for each target rank
    std::vector<FailedRank> failedRanks;
    for (const auto &targetRankId : targetRankIds) {
        ProcessTargetRank(targetRankId, request.params.sliceName, request.params.metaType, side, alignType, baseSlice,
            baseIndex, response.body.baseOffset, response.body.successList, failedRanks);
    }

    // 6. Report failures
    if (!failedRanks.empty()) {
        SetRankOffsetError(response, BuildFailedRanksMessage(failedRanks));
    }

    // 7. Send response
    bool hasSuccess = !response.body.successList.empty();
    SendResponse(std::move(responsePtr), hasSuccess);
    return hasSuccess;
}

bool RankOffsetHandler::ResolveBaseSlice(const RankOffsetParams &params, RankOffsetSide side,
    Protocol::SimpleSlice &outSlice, int64_t &outIndex, std::string &errorMsg) {
    auto baseDb = DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (baseDb == nullptr) {
        errorMsg = "Base rank database not found for rankId: " + params.rankId;
        return false;
    }

    // Query all slices on the base rank
    SortedSliceQuery baseQuery;
    if (!QuerySlicesByName(*baseDb, params.rankId, params.sliceName, params.metaType, side, baseQuery)) {
        errorMsg = "Failed to query base slices from database.";
        return false;
    }
    SortSlicesByTimestamp(baseQuery.slices);

    // Filter by pid
    std::vector<Protocol::SimpleSlice> &pidSlices = baseQuery.slices;
    if (pidSlices.empty()) {
        errorMsg = "Base slice not found for sliceName: " + params.sliceName + ", pid: " + params.pid;
        return false;
    }

    // Find selected slice by matching startTime and duration
    int64_t selectedIndex = FindSelectedSliceIndex(pidSlices, params.startTime, params.duration);
    if (selectedIndex < 0) {
        errorMsg = "Selected slice not found for sliceName: " + params.sliceName +
            ", startTime: " + std::to_string(params.startTime) + ", duration: " + std::to_string(params.duration);
        return false;
    }

    outSlice = pidSlices[selectedIndex];
    outIndex = selectedIndex;
    return true;
}

void RankOffsetHandler::ProcessTargetRank(const std::string &targetRankId, const std::string &sliceName,
    const std::string &metaType, RankOffsetSide side, RankOffsetAlignType alignType,
    const Protocol::SimpleSlice &baseSlice, int64_t baseIndex, uint64_t baseOffset,
    std::vector<RankOffsetItem> &successList, std::vector<FailedRank> &failedRanks) {
    // Query and sort slices on the target rank (reuses common logic)
    auto query = QueryAndSortSlices(targetRankId, sliceName, metaType, side);
    if (query.slices.empty()) {
        failedRanks.push_back({targetRankId, "Slice not found"});
        return;
    }

    const auto *targetSlice = PickSliceByIndex(query.slices, baseIndex);
    if (targetSlice == nullptr) {
        failedRanks.push_back({targetRankId,
            "Insufficient slices: has " + std::to_string(query.slices.size()) + ", need " +
                std::to_string(baseIndex + 1)});
        return;
    }

    int64_t offset =
        RankOffsetCalculator::CalculateOffset(baseSlice, *targetSlice, alignType) + static_cast<int64_t>(baseOffset);

    RankOffsetItem item;
    item.rankId = targetRankId;
    item.offset = offset;
    item.processId.assign(query.processIds.begin(), query.processIds.end());
    successList.push_back(item);
}

} // Timeline
} // Module
} // Dic
