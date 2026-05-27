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
#include "QueryModelInfoHandler.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "ExpertHotspotManager.h"

namespace Dic::Module::Summary {
bool QueryModelInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<QueryModelInfoRequest &>(*requestPtr);
    std::unique_ptr<QueryModelInfoResponse> responsePtr = std::make_unique<QueryModelInfoResponse>();
    QueryModelInfoResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return true;
    }
    // 查询模型信息
    ModelInfo modelInfo = ExpertHotspotManager::GetModelInfo(request.params.clusterPath);
    response.body.expertNum = modelInfo.expertNumber;
    response.body.denseLayerList = modelInfo.denseLayerList;
    response.body.layerNum = modelInfo.modelLayer;
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
