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
    // 表格请求走通用分页校验；图视图请求走宽松分页校验（支持前端分片拉取，0/0 表示全量，与 events 场景一致）
    const bool paramsValid =
        request.isTable ? request.params.CommonCheck(errMsg) : request.params.CommonCheckForView(errMsg);
    if (!paramsValid) {
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
        int64_t total = -1;
        if (request.params.pageSize > 0) {
            // 分片拉取：total 为 COUNT 总行数，前端 do-while 以累计条数达到 total 终止
            total = database->QueryBlocksByPage<Protocol::BlockViewItemDTO>(
                request.params, request.params.deviceId, response.viewBlocks);
        } else {
            // 全量兼容路径（不带分页参数），行为与历史版本一致
            if (database->QueryAllBlocks<Protocol::BlockViewItemDTO>(response.viewBlocks, request.params.deviceId)) {
                total = static_cast<int64_t>(response.viewBlocks.size());
            }
        }
        if (total < 0) {
            errMsg = LOG_TAG + "Failed to query blocks: query db failed.";
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
        response.total = static_cast<uint64_t>(total);
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
