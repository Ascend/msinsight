/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "CommunicationOperatorFluctuationHandler.h"
#include "CommunicationStatisticsHandlerUtils.h"
#include "CommunicationStatisticsProtocol.h"

namespace Dic::Module::Communication {
bool CommunicationOperatorFluctuationHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    if (requestPtr == nullptr) {
        SetCommunicationError(ErrorCode::PARAMS_ERROR);
        return false;
    }
    auto response = std::make_unique<Protocol::CommunicationOperatorFluctuationResponse>();
    SetBaseResponse(*requestPtr, *response);
    const auto *request = dynamic_cast<const Protocol::CommunicationOperatorFluctuationRequest *>(requestPtr.get());
    std::string error;
    if (request == nullptr || !request->params.CheckParams(error)) {
        return SendStatisticsError(std::move(response), ErrorCode::PARAMS_ERROR);
    }
    const auto &params = request->params;
    auto database = GetStatisticsDatabase(params.clusterPath);
    if (database == nullptr) {
        return SendStatisticsError(std::move(response), ErrorCode::CONNECT_DATABASE_FAILED);
    }
    auto result = database->QueryOperatorFluctuation(params.GetStep(), params.groupName);
    response->items = std::move(result.rows);
    return SendStatisticsResponse(std::move(response), result.status);
}
}
