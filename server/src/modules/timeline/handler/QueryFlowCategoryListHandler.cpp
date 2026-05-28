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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "QueryFlowCategoryListHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryFlowCategoryListHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    FlowCategoryListRequest &request = dynamic_cast<FlowCategoryListRequest &>(*requestPtr.get());
    std::unique_ptr<FlowCategoryListResponse> responsePtr = std::make_unique<FlowCategoryListResponse>();
    FlowCategoryListResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query flow category list failed to get connection. ");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    // 状态校验
    if (!database->CheckValueFromStatusInfoTable(CONNECTION_UNIT, FINISH_STATUS)) {
        SetTimelineError(ErrorCode::CATEGORY_PARSE_NOT_FINISH);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    bool result = database->QueryFlowCategoryList(response.body.category, request.params.rankId);
    if (!result) {
        SetTimelineError(ErrorCode::QUERY_FLOW_CATEGORY_FAILED);
    }
    SendResponse(std::move(responsePtr), result);
    return result;
}

} // Timeline
} // Module
} // Dic
