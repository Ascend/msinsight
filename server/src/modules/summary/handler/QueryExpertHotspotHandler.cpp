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

#include "QueryExpertHotspotHandler.h"
#include "ExpertHotspotManager.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"

namespace Dic::Module::Summary {
bool QueryExpertHotspotHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<QueryExpertHotspotRequest &>(*requestPtr);
    std::unique_ptr<QueryExpertHotspotResponse> responsePtr = std::make_unique<QueryExpertHotspotResponse>();
    QueryExpertHotspotResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    ModelInfo modelInfo;
    modelInfo.denseLayerList = request.params.denseLayerList;
    modelInfo.expertNumber = request.params.expertNum;
    modelInfo.modelLayer = request.params.layerNum;
    if (!ExpertHotspotManager::UpdateModelInfo(request.params.clusterPath, modelInfo, errorMsg)) {
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    response.body.hotspotInfos = ExpertHotspotManager::QueryExpertHotspotData(
        request.params.clusterPath, request.params.modelStage, request.params.version);
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
