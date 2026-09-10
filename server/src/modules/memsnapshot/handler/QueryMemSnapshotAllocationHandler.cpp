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
    const auto resolved = ResolveMemSnapshotRequest(request, request.params.deviceId, request.params.sliceIndex);
    const auto memoryDatabase = resolved.database;
    if (memoryDatabase == nullptr || !memoryDatabase->IsOpen()) {
        errorMsg = "Get memsnapshot database failed when querying allocations.";
        Server::ServerLog::Error(errorMsg);
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    const auto slice = resolved.slice;
    if (slice.has_value()) {
        if (!memoryDatabase->QueryMemoryAllocationOverviewCache(request.params.deviceId, response.allocations) ||
            !memoryDatabase->QueryMemoryAllocationCacheMaxSize(request.params.deviceId, response.maxSize)) {
            errorMsg = "Failed to query precomputed memory allocation overview.";
            Server::ServerLog::Error(errorMsg);
            SendResponse(std::move(responsePtr), false, errorMsg);
            return false;
        }
    } else {
        // 兼容未分窗的旧版数据库；新分窗产物只读取解析阶段生成的抽样结果。
        std::vector<Protocol::AllocationRecord> records;
        memoryDatabase->QueryMemoryAllocations(request.params.deviceId, records);
        if (records.empty()) {
            Server::ServerLog::Warn("Query memory records: empty data.");
        } else {
            response.allocations = MemSnapshotAllocationDataProcessor::ExtractAllocationTurningPoints(records);
            for (const auto &record : records) {
                response.maxSize = std::max(response.maxSize, std::max(record.allocated, record.reserved));
            }
        }
    }
    response.minEventId = 0;
    const int64_t maxEventId = memoryDatabase->GetDeviceMaxEntryId(request.params.deviceId);
    response.maxEventId = ToNonNegativeEventId(maxEventId);
    if (request.params.pageSize > 0) {
        response.paginated = true;
        response.allocationsTotal = response.allocations.size();
        Paginate(response.allocations, request.params);
    }
    if (slice.has_value()) {
        response.minEventId = static_cast<uint64_t>(slice->startEventId);
        response.maxEventId = static_cast<uint64_t>(slice->endEventId);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Dic::Module::MemSnapshot
