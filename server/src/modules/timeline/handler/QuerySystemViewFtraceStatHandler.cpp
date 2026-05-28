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
#include "QuerySystemViewFtraceStatHandler.h"

#include "RenderEngine.h"

namespace Dic::Module::Timeline {
bool QuerySystemViewFtraceStatHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SystemViewFtraceStatRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<SystemViewFtraceStatResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);

    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 将Database转换为TextTraceDatabase
    auto textDb = std::dynamic_pointer_cast<TextTraceDatabase>(database);
    if (textDb == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 计算offset
    uint64_t offset = (request.params.current - 1) * request.params.pageSize;

    // 查询数据
    auto ftraceStat = textDb->QueryFtraceStatistics(request.params.dataType, offset, request.params.pageSize);
    auto threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({request.params.rankId, PROCESS_TYPE::TEXT});
    // 转换数据为map格式
    for (const auto &item : ftraceStat.data) {
        std::unordered_map<std::string, std::string> row;
        row["track_id"] = std::to_string(item.trackId);
        auto it = threadInfoMap.find(item.trackId);
        if (it == threadInfoMap.end()) {
            continue;
        }
        row["process"] = it->second.first;
        row["thread"] = it->second.second;
        for (const auto &kv : item.data) {
            row[kv.first] = kv.second;
        }
        response.data.push_back(row);
    }

    // 设置分页信息
    response.pageParam.current = request.params.current;
    response.pageParam.pageSize = request.params.pageSize;
    response.pageParam.total = ftraceStat.totalCount;

    SendResponse(std::move(responsePtr), true);
    return true;
}

} // namespace Dic::Module::Timeline
