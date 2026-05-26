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

#include "QueryMemSnapshotEventHandler.h"
#include "DataBaseManager.h"

using namespace Dic::Module::MemSnapshot;

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotEventHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotEventsRequest &>(*requestPtr);
    std::unique_ptr<MemSnapshotEventsResponse> responsePtr = std::make_unique<MemSnapshotEventsResponse>();
    auto &response = *responsePtr;
    response.isTable = request.isTable;
    SetBaseResponse(request, response);
    std::string errMsg;
    // 表格参数校验
    if (request.isTable && !request.params.CommonCheck(errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    // 非表格请求，仅校验分页参数是否可能溢出
    if (!request.isTable && !CheckEventsPaginationParamsOnListRequest(request.params, errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const auto database = GetMemSnapshotDatabaseByRequest(request);
    if (database == nullptr || !database->IsOpen()) {
        errMsg = LOG_TAG + "Failed to query events: get database connection failed";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const int64_t total = request.isTable
        ? database->QueryTraceEntriesTable(request.params, response.tableEntries)
        : database->QueryTraceEntriesList(request.params, request.params.deviceId, response.listEntries);
    if (total < 0) {
        errMsg = LOG_TAG + "Failed to query events: query db failed.";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    response.total = static_cast<uint64_t>(total);
    response.maxTimestamp = database->GetDeviceMaxEntryId(request.params.deviceId);
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemSnapshotEventHandler::CheckEventsPaginationParamsOnListRequest(
    const PaginationParam &params, std::string &errorMsg) const {
    if (params.currentPage < 0 || params.pageSize <= 0) {
        errorMsg = LOG_TAG + "Invalid params: pageSize and currentPage must be greater than 0";
        return false;
    }
    if (params.pageSize > 100000) {
        errorMsg = LOG_TAG + "Invalid params: pageSize must be less than 100000";
        return false;
    }
    if (INT64_MAX / params.pageSize < params.currentPage) {
        errorMsg = LOG_TAG + "Invalid params: currentPage exceeds the maximum value";
        return false;
    }
    return true;
}
} // namespace Dic::Module::MemSnapshot
