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
#include "MemSnapshotAllocationDataProcessor.h"
#include "ProjectExplorerManager.h"
#include "QueryMemSnapshotAllocationHandler.h"

namespace Dic::Module::MemSnapshot {
namespace {
template <typename T> void Paginate(std::vector<T> &values, const MemSnapshotAllocationParams &params) {
    const size_t offset = static_cast<size_t>((params.currentPage - 1) * params.pageSize);
    if (offset >= values.size()) {
        values.clear();
        return;
    }
    const size_t end = std::min(values.size(), offset + static_cast<size_t>(params.pageSize));
    std::vector<T> page(
        values.begin() + static_cast<std::ptrdiff_t>(offset), values.begin() + static_cast<std::ptrdiff_t>(end));
    values = std::move(page);
}
} // namespace

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
    std::vector<Protocol::AllocationRecord> records;
    memoryDatabase->QueryMemoryAllocations(request.params.deviceId, records);
    if (records.empty()) {
        Server::ServerLog::Warn("Query memory records: empty data.");
        if (request.params.pageSize > 0) {
            response.paginated = true;
        }
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    response.allocations = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(records);
    response.reservedLine = MemSnapshotAllocationDataProcessor::CompressReservedLine(records);
    response.minEventId = 0;
    response.maxEventId = memoryDatabase->GetDeviceMaxEntryId(request.params.deviceId);
    if (request.params.pageSize > 0) {
        response.paginated = true;
        response.allocationsTotal = response.allocations.size();
        response.reservedLineTotal = response.reservedLine.size();
        Paginate(response.allocations, request.params);
        Paginate(response.reservedLine, request.params);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Dic::Module::MemSnapshot
