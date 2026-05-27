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

#include "pch.h"
#include "DetailsService.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "WsSessionManager.h"
#include "QueryInterCoreLoadAnalysisGraphHandler.h"

namespace Dic::Module::Source {
using namespace Dic::Server;

bool QueryInterCoreLoadAnalysisGraphHandler::HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<DetailsInterCoreLoadGraphRequest &>(*requestPtr);
    std::unique_ptr<DetailsInterCoreLoadGraphResponse> responsePtr =
        std::make_unique<DetailsInterCoreLoadGraphResponse>();
    DetailsInterCoreLoadGraphResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    // 获取核间负载数据
    bool result = DetailsService::QueryCoreLoadAnalysisGraph(request, response);
    SendResponse(std::move(responsePtr), result);
    return result;
}
}
