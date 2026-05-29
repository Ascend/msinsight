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
#include "QueryTableDataDetailHandler.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
bool QueryTableDataDetailHandler::HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<TableDataDetailRequest &>(*requestPtr);
    std::unique_ptr<TableDataDetailResponse> responsePtr = std::make_unique<TableDataDetailResponse>();
    TableDataDetailResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query table data detail failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (request.params.type.empty()) {
        ComputeTableDetail(request, response, database);
    } else if (request.params.type == "1") {
        ComputeLinkPageDetail(request, response, database);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryTableDataDetailHandler::ComputeLinkPageDetail(TableDataDetailRequest &request,
    TableDataDetailResponse &response, const std::shared_ptr<VirtualTraceDatabase> &database) const {
    if (request.params.equalConditions.empty()) {
        return;
    }
    std::shared_ptr<TextTraceDatabase> databasePtr =
        std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(database);
    if (databasePtr == nullptr) {
        ServerLog::Warn("Failed to get text connection when query table detail,ID: %", request.params.rankId);
        return;
    }
    auto linkInfos = databasePtr->QueryTableNameAndCol(request.params.equalConditions[0].col);
    if (linkInfos.empty()) {
        return;
    }
    const std::string tableName = linkInfos[0].tableName;
    auto colums = databasePtr->QueryTableInfoByName(tableName);
    PageQuery query;
    query.fileId = request.params.rankId;
    query.curPage = request.params.currentPage;
    query.size = request.params.pageSize;
    query.viewName = tableName;
    query.order = request.params.order;
    query.orderBy = request.params.orderBy;
    for (const auto &item : request.params.filterconditions) {
        query.pageFilters.push_back({item.col, item.content});
    }
    const std::string col = linkInfos[0].col;
    for (const auto &item : request.params.equalConditions) {
        query.equalFilters.push_back({col, item.content});
    }
    auto data = databasePtr->QueryDataByPage(query, colums);
    uint64_t count = databasePtr->QueryCountByTableName(query, colums);
    for (const auto &item : colums) {
        TableColumn column;
        column.name = item.name;
        column.type = item.type;
        column.key = item.key;
        response.body.columnAttr.emplace_back(column);
    }
    for (const auto &item : data) {
        response.body.columnData.emplace_back(item);
    }
    response.body.totalNum = count;
}

void QueryTableDataDetailHandler::ComputeTableDetail(const TableDataDetailRequest &request,
    TableDataDetailResponse &response, std::shared_ptr<VirtualTraceDatabase> database) {
    std::shared_ptr<TextTraceDatabase> databasePtr =
        std::dynamic_pointer_cast<TextTraceDatabase, VirtualTraceDatabase>(database);
    if (databasePtr == nullptr) {
        ServerLog::Warn("Failed to get text connection when query table detail,ID: %", request.params.rankId);
        return;
    }
    auto nameList = databasePtr->QueryTableDataNameList();
    if (nameList.size() <= request.params.tableIndex) {
        return;
    }
    const std::string tableName = nameList[request.params.tableIndex].second;
    auto colums = databasePtr->QueryTableInfoByName(tableName);
    PageQuery query;
    query.fileId = request.params.rankId;
    query.curPage = request.params.currentPage;
    query.size = request.params.pageSize;
    query.viewName = tableName;
    query.order = request.params.order;
    query.orderBy = request.params.orderBy;
    for (const auto &item : request.params.filterconditions) {
        query.pageFilters.push_back({item.col, item.content});
    }
    auto data = databasePtr->QueryDataByPage(query, colums);
    uint64_t count = databasePtr->QueryCountByTableName(query, colums);
    for (const auto &item : colums) {
        TableColumn column;
        column.name = item.name;
        column.type = item.type;
        column.key = item.key;
        response.body.columnAttr.emplace_back(column);
    }
    for (const auto &item : data) {
        response.body.columnData.emplace_back(item);
    }
    response.body.totalNum = count;
}
} // namespace Dic::Module::Timeline
