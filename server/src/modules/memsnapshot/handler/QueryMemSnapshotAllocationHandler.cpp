/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "DataBaseManager.h"
#include "ProjectExplorerManager.h"
#include "QueryMemSnapshotAllocationHandler.h"

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotAllocationHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotAllocationsRequest &>(*requestPtr);
    std::unique_ptr<MemSnapshotAllocationsResponse> responsePtr = std::make_unique<MemSnapshotAllocationsResponse>();
    MemSnapshotAllocationsResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    const auto memoryDatabase = GetMemSnapshotDatabaseByRequest(request);
    if (memoryDatabase == nullptr) {
        errorMsg = "Get memsnapshot database failed when querying allocations.";
        Server::ServerLog::Error(errorMsg);
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    memoryDatabase->QueryMemoryAllocations(request.params.deviceId, response.records);
    if (response.records.empty()) {
        Server::ServerLog::Warn("Query memory records: empty data.");
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    responsePtr->minEventId = 0;
    responsePtr->maxEventId = memoryDatabase->GetDeviceMaxEntryId(request.params.deviceId);
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Dic::Module::MemSnapshot
