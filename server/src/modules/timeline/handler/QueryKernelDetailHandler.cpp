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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "QueryKernelDetailHandler.h"
namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryKernelDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    KernelDetailsRequest &request = dynamic_cast<KernelDetailsRequest &>(*requestPtr.get());
    std::unique_ptr<KernelDetailsResponse> responsePtr = std::make_unique<KernelDetailsResponse>();
    KernelDetailsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string error;
    request.params.Check(minTimestamp, error);
    if (!std::empty(error)) {
        ServerLog::Warn(error);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        ServerLog::Error("Query kernel detail failed to get deviceId. ");
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QueryKernelDetailData(request.params, response.body, minTimestamp)) {
        ServerLog::Error("Failed to get kernel detail response data.");
        SetTimelineError(ErrorCode::QUERY_KERNEL_DETAIL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Timeline
} // Module
} // Dic
