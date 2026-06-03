/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "DataBaseManager.h"
#include "QueryKernelE2ETimeHandler.h"

namespace Dic::Module::Timeline {

bool QueryKernelE2ETimeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    std::unique_ptr<Protocol::KernelE2ETimeResponse> responsePtr = std::make_unique<Protocol::KernelE2ETimeResponse>();
    auto *request = dynamic_cast<Protocol::KernelE2ETimeRequest *>(requestPtr.get());
    if (request == nullptr) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, "Invalid kernel E2E request type.");
        return false;
    }
    auto &response = *responsePtr;
    SetBaseResponse(*request, response);

    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request->fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string error;
    if (!request->params.CheckParams(minTimestamp, error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }

    const KernelE2ETimeDatabaseAccesser accesser(database, request->fileId);
    if (!accesser.GetKernelE2ETimeRecords(request->params, response.body)) {
        SetTimelineError(ErrorCode::QUERY_SYSTEM_VIEW_FAILED);
        SendResponse(std::move(responsePtr), false, "Failed to query kernel E2E time records.");
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}

} // namespace Dic::Module::Timeline
