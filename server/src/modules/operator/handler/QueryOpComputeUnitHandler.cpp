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
#include "WsSessionManager.h"
#include "QueryOpComputeUnitHandler.h"

namespace Dic::Module::Operator {
using namespace Dic::Server;

bool QueryOpComputeUnitHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    OperatorComputeUnitInfoRequest &request = dynamic_cast<OperatorComputeUnitInfoRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<OperatorComputeUnitInfoResponse>();
    OperatorComputeUnitInfoResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (!CheckRequestParam(request.params)) {
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto &databaseManager = Timeline::DataBaseManager::Instance();
    auto database = databaseManager.GetSummaryDatabaseByRankId(rankId, false);
    if (!database) {
        LogDatabaseUnavailable("QueryComputeUnit", GetLogContext(request.params.rankId, request.params.group));
        return true;
    }
    std::string deviceId = databaseManager.GetDeviceIdFromRankId(rankId, false);
    if (deviceId.empty()) {
        LogDeviceUnavailable("QueryComputeUnit", GetLogContext(request.params.rankId, request.params.group));
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QueryOperatorDurationInfo(request.params, QueryType::COMPUTE_UNIT, response.data)) {
        SetOperatorError(ErrorCode::QUERY_DURATION_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryOpComputeUnitHandler::CheckRequestParam(OperatorDurationReqParams params) {
    std::string errMsg;
    if (!params.CommonCheck(errMsg)) {
        LogInvalidRequest("QueryComputeUnit", errMsg);
        SetOperatorError(ErrorCode::PARAMS_ERROR);
        return false;
    }
    return true;
}
}
