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
#include "DataBaseManager.h"

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
} // namespace Dic::Module::MemSnapshot
