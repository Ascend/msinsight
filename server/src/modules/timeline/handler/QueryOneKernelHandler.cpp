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
#include "QueryOneKernelHandler.h"
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "ProjectExplorerManager.h"
#include "TraceTime.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryOneKernelHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    KernelRequest &request = dynamic_cast<KernelRequest &>(*requestPtr.get());
    std::unique_ptr<OneKernelResponse> responsePtr = std::make_unique<OneKernelResponse>();
    OneKernelResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string warnMsg;
    if (!request.params.CheckParams(minTimestamp, warnMsg)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        database = Timeline::DataBaseManager::Instance().GetTraceDatabaseWithOutHost(request.params.rankId);
        if (database == nullptr) {
            SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    }
    if (!database->QueryKernelDepthAndThread(request.params, response.body, TraceTime::Instance().GetStartTime())) {
        SetTimelineError(ErrorCode::QUERY_KERNEL_DEPTH_AND_THREAD_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Timeline
} // Module
} // Dic
