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
#include "QueryMemSnapshotAllocationLinesHandler.h"

namespace Dic::Module::MemSnapshot {
namespace {
template <typename T> void Paginate(std::vector<T> &values, const MemSnapshotAllocationParams &params) {
    const size_t offset = static_cast<size_t>((params.currentPage - 1) * params.pageSize);
    if (offset >= values.size()) {
        values.clear();
        return;
    }
    const size_t end = std::min(values.size(), offset + static_cast<size_t>(params.pageSize));
    values = std::vector<T>(
        values.begin() + static_cast<std::ptrdiff_t>(offset), values.begin() + static_cast<std::ptrdiff_t>(end));
}
} // namespace

bool QueryMemSnapshotAllocationLinesHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotAllocationLinesRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<MemSnapshotAllocationLinesResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    const auto resolved = ResolveMemSnapshotRequest(request, request.params.deviceId, request.params.sliceIndex);
    const auto memoryDatabase = resolved.database;
    if (memoryDatabase == nullptr || !memoryDatabase->IsOpen()) {
        errorMsg = "Get memsnapshot database failed when querying allocation lines.";
        Server::ServerLog::Error(errorMsg);
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    const auto slice = resolved.slice;
    if (slice.has_value()) {
        if (!memoryDatabase->QueryMemoryAllocationLineCache(request.params.deviceId, response.reservedLine)) {
            errorMsg = "Failed to query precomputed memory allocation lines.";
            Server::ServerLog::Error(errorMsg);
            SendResponse(std::move(responsePtr), false, errorMsg);
            return false;
        }
        if (response.reservedLine.empty()) {
            std::vector<Protocol::AllocationRecord> records;
            memoryDatabase->QueryMemoryAllocations(request.params.deviceId, records);
            response.reservedLine = MemSnapshotAllocationDataProcessor::CompressReservedLine(records);
        }
    } else {
        std::vector<Protocol::AllocationRecord> records;
        memoryDatabase->QueryMemoryAllocations(request.params.deviceId, records);
        response.reservedLine = MemSnapshotAllocationDataProcessor::CompressReservedLine(records);
    }
    response.minEventId = 0;
    const int64_t maxEventId = memoryDatabase->GetDeviceMaxEntryId(request.params.deviceId);
    response.maxEventId = ToNonNegativeEventId(maxEventId);
    if (request.params.pageSize > 0) {
        response.paginated = true;
        response.reservedLineTotal = response.reservedLine.size();
        Paginate(response.reservedLine, request.params);
    }
    if (slice.has_value()) {
        response.minEventId = static_cast<uint64_t>(slice->startEventId);
        response.maxEventId = static_cast<uint64_t>(slice->endEventId);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // namespace Dic::Module::MemSnapshot
