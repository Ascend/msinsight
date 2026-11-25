/*
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "pch.h"
#include "DetailsService.h"
#include "WsSessionManager.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "QueryTopWarpStallReasonHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryTopWarpStallReasonHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SourceTopWarpStallReasonRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<TopWarpStallReasonResponse>();
    TopWarpStallReasonResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    bool result = DetailsService::QueryTopWarpStallReason(request, response);
    SendResponse(std::move(responsePtr), result);
    return result;
}
}
}
}
