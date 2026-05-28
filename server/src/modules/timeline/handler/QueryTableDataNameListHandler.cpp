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
#include "QueryTableDataNameListHandler.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
bool QueryTableDataNameListHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<TableDataNameListRequest &>(*requestPtr);
    std::unique_ptr<TableDataNameListResponse> responsePtr = std::make_unique<TableDataNameListResponse>();
    TableDataNameListResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query table data name list failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::shared_ptr<TextTraceDatabase> databasePtr =
        std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(database);
    if (databasePtr == nullptr) {
        ServerLog::Warn("Failed to get text connection when query table names,ID: %", request.params.rankId);
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto nameList = databasePtr->QueryTableDataNameList();
    auto translate = databasePtr->QueryTranslate(request.params.isZh);
    for (const auto &item : nameList) {
        response.body.layers.emplace_back(item.first, translate[item.first]);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
