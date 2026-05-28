// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *
#include "QueryTritonBasicInfoHandler.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"
#include "TritonService.h"
#include "WsSender.h"
namespace Dic::Module::Triton {
bool QueryTritonBasicInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<Protocol::TritonBasicInfoRequest &>(*requestPtr);
    std::unique_ptr<Protocol::TritonBasicInfoResponse> responsePtr =
        std::make_unique<Protocol::TritonBasicInfoResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    auto [kernel, scopeTypes] = TritonService::Instance().GetHeader();
    response.kernelName = std::move(kernel);
    response.scopeTypes = std::move(scopeTypes);
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
