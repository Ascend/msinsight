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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "QueryMemSnapshotLeakStatsHandler.h"

using namespace Dic::Module::MemSnapshot;

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotLeakStatsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemSnapshotLeakStatsRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<MemSnapshotLeakStatsResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errMsg;
    if (!request.params.CommonCheck(errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const auto database = GetMemSnapshotDatabaseByRequest(request);
    if (database == nullptr || !database->IsOpen()) {
        errMsg = LOG_TAG + "Failed to query leak stats: get database connection failed";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    MemSnapshotLeakStatsDTO stats;
    if (!database->QueryPotentialLeakStats(request.params, stats)) {
        errMsg = LOG_TAG + "Failed to query leak stats: query db failed.";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    response.totalSize = stats.totalSize;
    response.maxSize = stats.maxSize;
    response.minSize = stats.minSize;
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // namespace Dic::Module::MemSnapshot
