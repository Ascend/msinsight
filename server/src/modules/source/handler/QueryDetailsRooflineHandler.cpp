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
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceFileParser.h"
#include "QueryDetailsRooflineHandler.h"

namespace Dic::Module::Source {
using namespace Dic::Server;

bool QueryDetailsRooflineHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<DetailsRooflineRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<DetailsRooflineResponse>();
    DetailsRooflineResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    bool result = SourceFileParser::Instance().GetDetailsRoofline(response.body);
    // 解析失败时返回空数据
    if (!result) {
        response.body.soc = "";
        response.body.data.clear();
        response.body.advice.clear();
        result = true;
    }
    SendResponse(std::move(responsePtr), result);
    return result;
}
}
