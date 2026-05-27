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
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceFileParser.h"
#include "QueryDetailsBaseInfoHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryDetailsBaseInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SourceDetailBaseInfoRequest &>(*requestPtr);
    std::unique_ptr<DetailsBaseInfoResponse> responsePtr = std::make_unique<DetailsBaseInfoResponse>();
    DetailsBaseInfoResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    bool compareDataRes = SourceFileParser::Instance().GetDetailsBaseInfo(response.body.compare, false);
    if (!compareDataRes) {
        SendResponse(std::move(responsePtr), false, "Get detail base info error", UNKNOW_ERROR);
        return false;
    }

    // 如果设置了对比状态，但是获取基线数据失败，则打印警告日志，但不影响前端返回（只返回了对比数据）
    if (request.params.isCompared && !SourceFileParser::Instance().GetDetailsBaseInfo(response.body.baseline, true)) {
        ServerLog::Warn("Get baseline details base info fail.");
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
}
}
