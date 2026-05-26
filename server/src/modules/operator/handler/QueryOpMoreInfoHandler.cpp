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
#include "DataBaseManager.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"
#include "OperatorGroupConverter.h"
#include "WsSessionManager.h"
#include "OperatorProtocol.h"
#include "QueryOpMoreInfoHandler.h"

namespace Dic::Module::Operator {
using namespace Dic::Server;

bool QueryOpMoreInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    OperatorMoreInfoRequest &request = dynamic_cast<OperatorMoreInfoRequest &>(*requestPtr);
    std::unique_ptr<OperatorMoreInfoResponse> responsePtr = std::make_unique<OperatorMoreInfoResponse>();
    OperatorMoreInfoResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errMsg;
    if (!request.params.CommonCheck(errMsg)) {
        ServerLog::Error("[Operator]Failed to check request parameter in query op more info.%", errMsg);
        SetOperatorError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        ServerLog::Error("[Operator]Failed to query More Info by empty deviceId.%");
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database || !database->QueryOperatorMoreInfo(request.params, response)) {
        ServerLog::Error("[Operator]Failed to query More Info by rankId.");
        SetOperatorError(ErrorCode::QUERY_MORE_INFO_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
