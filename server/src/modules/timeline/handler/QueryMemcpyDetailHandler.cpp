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

#include "DataBaseManager.h"
#include "MemcpyOverallDatabaseAccesser.h"
#include "QueryMemcpyDetailHandler.h"

namespace Dic::Module::Timeline {

bool QueryMemcpyDetailHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    // 1. 验证请求格式
    auto &request = dynamic_cast<SystemViewOverallMoreDetailsRequest &>(*requestPtr);
    std::unique_ptr<MemcpyDetailResponse> responsePtr = std::make_unique<MemcpyDetailResponse>();
    MemcpyDetailResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string error;
    request.params.CheckParams(minTimestamp, error);
    if (!std::empty(error) || request.params.categoryList.size() <= 0) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    const std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;

    // 执行查询
    if (!QueryMemcpyDetails(request, response, error, database)) {
        SetTimelineError(ErrorCode::QUERY_MEMCPY_DETAIL_FAILED);
        Server::ServerLog::Error("Query memcpy detail list error: %", error);
        return false;
    }

    // 构建响应
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemcpyDetailHandler::QueryMemcpyDetails(const SystemViewOverallMoreDetailsRequest &request,
    MemcpyDetailResponse &response, std::string &error, const std::shared_ptr<VirtualTraceDatabase> &database) {
    try {
        // ===== 参数提取 =====
        uint64_t startTime = request.params.startTime;
        uint64_t endTime = request.params.endTime;

        // 解析 categoryList: [tid, memcpyType]
        const std::string tid = request.params.categoryList[0]; // 可能为空字符串（表示不过滤）
        std::optional<std::string> memcpyType;
        if (request.params.categoryList.size() > 1) {
            memcpyType = request.params.categoryList[1]; // 可能为空字符串
        }

        // 排序参数
        OrderParam orderParam = request.params.order;

        // 分页参数
        uint32_t currentPage = request.params.page.current;
        uint32_t pageSize = request.params.page.pageSize;

        // ===== 获取数据库访问器并执行查询 =====
        const MemcpyOverallDatabaseAccesser accesser(database, request.fileId);

        std::vector<MemcpyDetailRecord> records;
        uint64_t total = 0;

        // 调用分页查询（内部处理 startTime==endTime 的全量逻辑）
        if (!accesser.GetMemcpyDetailRecordsPaged(
                startTime, endTime, tid, memcpyType, currentPage, pageSize, orderParam, records, total)) {
            error = "Failed to query memcpy detail records";
            return false;
        }

        // ===== 类型转换 =====
        std::vector<SameMemcpyDetails> results;
        results.reserve(records.size());
        // 直接字段映射（两个结构体字段完全对齐）
        for (const auto &rec : records) {
            results.emplace_back(SameMemcpyDetails{rec.timestamp, rec.duration, rec.size, rec.id, rec.name});
        }

        response.body.sameOperatorsDetails = std::move(results); // 避免拷贝
        response.body.count = total;
        response.body.currentPage = currentPage;
        response.body.pageSize = pageSize;
        return true;
    } catch (const std::exception &e) {
        error = "Database error: " + std::string(e.what());
        return false;
    } catch (...) {
        error = "Unknown database exception";
        return false;
    }
}

} // namespace Dic::Module::Timeline
