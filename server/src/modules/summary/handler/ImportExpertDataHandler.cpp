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

#include "ImportExpertDataHandler.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "ExpertHotspotManager.h"
namespace Dic {
namespace Module {
namespace Summary {
bool ImportExpertDataHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<ImportExpertDataRequest &>(*requestPtr);
    std::unique_ptr<ImportExpertDataResponse> responsePtr = std::make_unique<ImportExpertDataResponse>();
    ImportExpertDataResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    if (!ExpertHotspotManager::InitExpertHotspotData(
            request.params.filePath, request.params.version, errorMsg, request.params.clusterPath)) {
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
}
}
