/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "QueryMemSnapshotDetailHandler.h"
#include "MemSnapshotDefs.h"
#include "MemSnapshotService.h"
#include "MemSnapshotStateCache.h"
#include "SegmentSummaryCalculator.h"
#include "DataBaseManager.h"

#include <algorithm>

using namespace Dic::Module::MemSnapshot;

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotDetailRequest &>(*requestPtr);
    std::unique_ptr<MemSnapshotDetailResponse> responsePtr = std::make_unique<MemSnapshotDetailResponse>();
    auto &response = *responsePtr;
    response.type = request.params.type;
    SetBaseResponse(request, response);
    std::string errMsg;
    if (!request.params.CommonCheck(errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const auto database = GetMemSnapshotDatabaseByRequest(request);
    if (database == nullptr || !database->IsOpen()) {
        errMsg = LOG_TAG + "Failed to query detail: get database connection failed";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }

    if (request.params.type == DETAIL_TYPE_BLOCK) {
        auto block = database->QueryBlockById(request.params.id, request.params.deviceId);
        if (!block.has_value()) {
            errMsg =
                LOG_TAG + "Failed to query block detail: block not found with id " + std::to_string(request.params.id);
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        auto blockDetail = std::make_unique<BlockDetailDTO>(block.value());
        BuildBlockDetailDTO(*blockDetail, request.params.deviceId, database);
        response.detail = std::move(blockDetail);
    } else if (request.params.type == DETAIL_TYPE_EVENT) {
        auto event = database->QueryTraceEntryById(request.params.id, request.params.deviceId);
        if (!event.has_value()) {
            errMsg =
                LOG_TAG + "Failed to query event detail: event not found with id " + std::to_string(request.params.id);
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        response.detail = std::make_unique<TraceEntryDetailDTO>(event.value());
    } else if (request.params.type == DETAIL_TYPE_SEGMENT) {
        auto segmentDetail = BuildSegmentDetailDTO(request, database, errMsg);
        if (segmentDetail == nullptr) {
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        response.detail = std::move(segmentDetail);
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryMemSnapshotDetailHandler::BuildBlockDetailDTO(BlockDetailDTO &blockDetail, const std::string &deviceId,
    const std::shared_ptr<FullDb::MemSnapshotDatabase> &database) {
    if (blockDetail.allocEventId > 0) {
        if (auto allocEntry = database->QueryTraceEntryById(blockDetail.allocEventId, deviceId);
            allocEntry.has_value()) {
            blockDetail.allocEvent = std::make_optional<TraceEntryDetailDTO>(allocEntry.value());
        }
    }
    if (blockDetail.freeEventId > 0) {
        if (auto freeCompletedEntry = database->QueryTraceEntryById(blockDetail.freeEventId, deviceId);
            freeCompletedEntry.has_value()) {
            blockDetail.freeCompletedEvent = std::make_optional<TraceEntryDetailDTO>(freeCompletedEntry.value());
        }
    }
    if (auto freeRequestEntry = database->QueryFreeRequestedTraceEntryByBlock(blockDetail, deviceId);
        freeRequestEntry.has_value()) {
        blockDetail.freeRequestedEvent = std::make_optional<TraceEntryDetailDTO>(freeRequestEntry.value());
    }
}

std::unique_ptr<Protocol::SegmentDetailDTO> QueryMemSnapshotDetailHandler::BuildSegmentDetailDTO(
    const Protocol::MemSnapshotDetailRequest &request, const std::shared_ptr<FullDb::MemSnapshotDatabase> &database,
    std::string &errMsg) {
    uint64_t segmentAddress = 0;
    if (!ParseSegmentAddress(request.params.segmentAddress, segmentAddress)) {
        errMsg = "Failed to query segment detail: invalid segmentAddress " + request.params.segmentAddress;
        return nullptr;
    }

    const auto dataKey = GetMemSnapshotDataKey(request);
    auto cachedSegments = MemSnapshotStateCache::Get(dataKey, request.params.deviceId, request.params.eventId);
    std::vector<Segment> segments;
    if (cachedSegments.has_value()) {
        segments = cachedSegments.value();
    } else {
        segments = MemSnapshotService::GetSegmentsByEventId(request.params.eventId, request.params.deviceId, database);
        MemSnapshotStateCache::Put(dataKey, request.params.deviceId, request.params.eventId, segments);
    }

    auto segmentIter = std::find_if(segments.begin(), segments.end(), [&](const Segment &segment) {
        return segment.address == segmentAddress && segment.stream == request.params.stream;
    });
    if (segmentIter == segments.end()) {
        errMsg = LOG_TAG + "Failed to query segment detail: segment not found";
        return nullptr;
    }

    auto segmentDetail =
        std::make_unique<Protocol::SegmentDetailDTO>(*segmentIter, ComputeSegmentSummary(*segmentIter));
    if (segmentIter->allocOrMapEventId >= 0) {
        if (auto event = database->QueryTraceEntryById(segmentIter->allocOrMapEventId, request.params.deviceId);
            event.has_value()) {
            segmentDetail->allocOrMapEvent = std::make_optional<TraceEntryDetailDTO>(event.value());
        }
    }
    return segmentDetail;
}

bool QueryMemSnapshotDetailHandler::ParseSegmentAddress(const std::string &address, uint64_t &value) {
    try {
        size_t parsed = 0;
        value = std::stoull(address, &parsed, 0);
        return parsed == address.size();
    } catch (std::invalid_argument &) {
        return false;
    } catch (std::out_of_range &) {
        return false;
    }
}
} // namespace Dic::Module::MemSnapshot
