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

#include "ServerLog.h"
#include "../utils/ThreadPool.h"
#include "RequestFilter.h"
#include "BaseModule.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Dic::Protocol;
void BaseModule::OnRequest(std::unique_ptr<Protocol::Request> request) {
    static constexpr int MIN_SIZE = 20;
    static ThreadPool threadPool(MIN_SIZE);
    std::string command = request->command;
    if (requestHandlerMap.count(command) == 0) {
        ServerLog::Error("Failed to find request handler");
        return;
    }
    auto &requestHandler = requestHandlerMap.at(command);
    RequestFilter::Instance().SetRequestId(requestHandler->GetRequestKey(*request), request->id);
    if (requestHandler->IsAsync()) {
        threadPool.AddTask(
            [&requestHandler, requestPtr = request.release()]() {
                requestHandler->HandleRequestEntrance(std::unique_ptr<Protocol::Request>(requestPtr));
            },
            "");
    } else {
        TraceIdManager::SetTraceId(TraceIdManager::GenerateTraceId());
        requestHandler->HandleRequestEntrance(std::move(request));
    }
};
} // end of namespace Module
} // end of namespace Dic
