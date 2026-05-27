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
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "SummaryStatisticsHandler.h"

namespace Dic {
namespace Module {
namespace Summary {
using namespace Dic;
using namespace Dic::Server;

bool SummaryStatisticsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    Protocol::SummaryStatisticRequest &request = dynamic_cast<Protocol::SummaryStatisticRequest &>(*requestPtr.get());
    std::unique_ptr<Protocol::SummaryStatisticsResponse> responsePtr =
        std::make_unique<Protocol::SummaryStatisticsResponse>();
    SummaryStatisticsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    // check request parameters
    std::string errorMsg;
    if (!request.params.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        database = Timeline::DataBaseManager::Instance().GetTraceDatabaseInCluster(
            request.params.clusterPath, request.params.rankId);
        if (database == nullptr) {
            // 若Timeline数据尚未加载，后端返回为空，此时前端不应弹出报错提示，因此此处SendResponse需返回true
            SendResponse(std::move(responsePtr), true);
            ServerLog::Warn("Failed to get trace database in cluster.");
            return false;
        }
    }
    if (!request.params.timeFlag.empty() && request.params.timeFlag.find("compute") != std::string::npos) {
        if (!database->QueryComputeStatisticsData(request.params, response.body)) {
            SetSummaryError(ErrorCode::QUERY_COMPUTE_STATISTICS_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else {
        if (!database->QueryCommunicationStatisticsData(request.params, response.body)) {
            SetSummaryError(ErrorCode::QUERY_COMMUNICATION_STATISTICS_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // Summary
} // Module
} // Dic
