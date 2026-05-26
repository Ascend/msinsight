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
#include "QueryOpCategoryInfoHandler.h"

namespace Dic::Module::Operator {
using namespace Dic::Server;

bool QueryOpCategoryInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    OperatorCategoryInfoRequest &request = dynamic_cast<OperatorCategoryInfoRequest &>(*requestPtr);
    std::unique_ptr<OperatorCategoryInfoResponse> responsePtr = std::make_unique<OperatorCategoryInfoResponse>();
    OperatorCategoryInfoResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (!CheckRequestParam(request.params)) {
        ServerLog::Warn("[Operator]Failed to check request parameter in Query Category Info.");
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    if (!database) {
        ServerLog::Warn("[Operator]Not exist operator database. Fail to get op category info.");
        return true;
    }
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        ServerLog::Error("[Operator]Failed to query Category Info by empty deviceId.");
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QueryOperatorDurationInfo(request.params, QueryType::CATEGORY, response.data)) {
        ServerLog::Error("[Operator]Failed to query Category Info by rankId.");
        SetOperatorError(ErrorCode::QUERY_DURATION_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryOpCategoryInfoHandler::CheckRequestParam(OperatorDurationReqParams params) {
    std::string errMsg;
    if (!params.CommonCheck(errMsg)) {
        ServerLog::Warn(errMsg);
        SetOperatorError(ErrorCode::PARAMS_ERROR);
        return false;
    }
    return true;
}
}
