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

#include "SummaryClusterTimeHandler.h"
#include "DataBaseManager.h"
#include "DbClusterDataBase.h"
#include "SummaryClusterStatisticsProtocol.h"
#include "WsSender.h"

namespace Dic::Module::Summary {
bool SummaryClusterTimeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<const Protocol::SummaryClusterStatisticsRequest &>(*requestPtr);
    auto response = std::make_unique<Protocol::SummaryClusterTimeResponse>();
    SetBaseResponse(request, *response);

    std::string errorMsg;
    if (!request.CheckParams(errorMsg)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR, errorMsg);
        SendResponse(std::move(response), false);
        return false;
    }
    auto database = std::dynamic_pointer_cast<FullDb::DbClusterDataBase>(
        Timeline::DataBaseManager::Instance().GetClusterDatabase(request.clusterPath));
    if (database == nullptr) {
        SetSummaryError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(response), false);
        return false;
    }
    const bool success = database->QuerySummaryClusterTime(request.step, response->body);
    if (!success) {
        response->body = {};
        SetSummaryError(ErrorCode::QUERY_CLUSTER_STATISTICS_FAILED);
    }
    SendResponse(std::move(response), success);
    return success;
}
} // namespace Dic::Module::Summary
