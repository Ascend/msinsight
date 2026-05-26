/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "QueryMemSnapshotBlockHandler.h"
#include "DataBaseManager.h"

using namespace Dic::Module::MemSnapshot;

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotBlockHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotBlocksRequest &>(*requestPtr);
    std::unique_ptr<MemSnapshotBlocksResponse> responsePtr = std::make_unique<MemSnapshotBlocksResponse>();
    auto &response = *responsePtr;
    response.isTable = request.isTable;
    SetBaseResponse(request, response);
    std::string errMsg;
    if (!request.params.CommonCheck(errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const auto database = GetMemSnapshotDatabaseByRequest(request);
    if (database == nullptr || !database->IsOpen()) {
        errMsg = LOG_TAG + "Failed to query blocks: get database connection failed";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    if (request.isTable) {
        const int64_t total = database->QueryBlocksTable(request.params, response.tableBlocks);
        if (total < 0) {
            errMsg = LOG_TAG + "Failed to query blocks: query db failed.";
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        response.total = static_cast<uint64_t>(total);
        response.maxTimestamp = database->GetDeviceMaxEntryId(request.params.deviceId);
        BuildBlockTableResponseColumnsBounds(request.params.deviceId, database, response.rangeFiltersBoundsMap);
    } else {
        if (!database->QueryAllBlocks<Protocol::BlockViewItemDTO>(response.viewBlocks, request.params.deviceId)) {
            errMsg = LOG_TAG + "Failed to query blocks: query db failed.";
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        response.total = response.viewBlocks.size();
        response.maxTimestamp = database->GetDeviceMaxEntryId(request.params.deviceId);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryMemSnapshotBlockHandler::BuildBlockTableResponseColumnsBounds(const std::string &deviceId,
    const std::shared_ptr<MemSnapshotDatabase> &database, Dic::Protocol::ColumnBounds &colBounds) {
    if (database == nullptr || !database->IsOpen()) {
        return;
    }
    auto minBlockId = INT64_MIN;
    auto maxBlockId = INT64_MAX;
    database->QueryBlockIdRangeByDeviceIdLazy(deviceId, minBlockId, maxBlockId);
    auto maxDeviceEntryId = database->GetDeviceMaxEntryId(deviceId);
    colBounds[BlockTableColumn::ID] = {minBlockId, maxBlockId};
    colBounds[BlockTableColumn::ALLOC_EVENT_ID] = {-1, maxDeviceEntryId};
    colBounds[BlockTableColumn::FREE_EVENT_ID] = {-1, maxDeviceEntryId};
}
} // namespace Dic::Module::MemSnapshot
