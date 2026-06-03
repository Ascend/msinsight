/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
#include "QueryMemScopeAllocationHandler.h"

namespace Dic {
namespace Module {
namespace MemScope {
bool QueryMemScopeAllocationHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemScopeMemoryAllocationRequest &>(*requestPtr.get());
    std::unique_ptr<MemScopeMemoryAllocationsResponse> responsePtr =
        std::make_unique<MemScopeMemoryAllocationsResponse>();
    MemScopeMemoryAllocationsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    auto memoryDatabase = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (memoryDatabase == nullptr) {
        errorMsg = "Get memscope memory database failed when querying allocations.";
        Server::ServerLog::Error(errorMsg);
        SendResponse(std::move(responsePtr), false, errorMsg);
        return false;
    }
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(request.params, allocations);
    PaddingAllocations(allocations, request.params);
    if (allocations.empty()) {
        Server::ServerLog::Warn("Query memory allocations: empty data.");
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    responsePtr->minTimestamp = allocations[0].timestamp;
    responsePtr->maxTimestamp = allocations.back().timestamp;
    responsePtr->allocations = std::move(allocations);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryMemScopeAllocationHandler::PaddingAllocations(
    std::vector<MemoryAllocation> &allocations, const MemScopeMemoryAllocationParams &queryParams) {
    auto memoryDatabase = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (memoryDatabase == nullptr) {
        Server::ServerLog::Error("Get memscope memory database failed when padding allocations.");
        return;
    }
    const uint64_t minTimestamp = queryParams.relativeTime ? memoryDatabase->GetGlobalMinTimestamp() : 0;
    const uint64_t globalMinTimestamp = memoryDatabase->GetGlobalMinTimestamp();
    const uint64_t globalMaxTimestamp = memoryDatabase->GetGlobalMaxTimestamp();
    const bool hasTimeRange = queryParams.endTimestamp > 0;
    const uint64_t startTimestamp =
        hasTimeRange ? queryParams.startTimestamp : (queryParams.relativeTime ? 0 : globalMinTimestamp);
    const uint64_t endTimestamp = hasTimeRange
        ? queryParams.endTimestamp
        : (queryParams.relativeTime ? globalMaxTimestamp - globalMinTimestamp : globalMaxTimestamp);
    auto beforeAllocation = memoryDatabase->QueryLatestAllocationWithinTimestamp(
        queryParams.deviceId, queryParams.eventType, startTimestamp + minTimestamp);
    if (!beforeAllocation.has_value()) {
        beforeAllocation = std::make_optional<MemoryAllocation>(
            startTimestamp, 0, queryParams.deviceId, queryParams.eventType, queryParams.optimized);
    } else {
        beforeAllocation.value().timestamp = startTimestamp;
    }
    if (allocations.empty() || allocations.front().timestamp != startTimestamp) {
        allocations.insert(allocations.begin(), beforeAllocation.value());
    }
    if (allocations.back().timestamp != endTimestamp) {
        allocations.emplace_back(endTimestamp, allocations.back().totalSize, queryParams.deviceId,
            queryParams.eventType, queryParams.optimized);
    }
}
} // Memory
} // Module
} // Dic
