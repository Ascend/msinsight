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
#include "Paginator.h"
#include "QueryMemcpyOverallHandler.h"

namespace Dic::Module::Timeline {

bool QueryMemcpyOverallHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemcpyOverallRequest &>(*requestPtr);
    std::unique_ptr<MemcpyOverallResponse> responsePtr = std::make_unique<MemcpyOverallResponse>();
    MemcpyOverallResponse &response = *responsePtr;
    SetBaseResponse(request, response);

    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    std::string error;
    if (!request.params.CheckParams(minTimestamp, error)) {
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }

    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        // 部分数据会缺少deviceId，查询结果会为空，所以直接返回true，而不是报错，并加上日志提示
        ServerLog::Warn("DeviceId is empty for memcpy view overall statistics.");
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    request.params.deviceId = deviceId;

    if (!CalMemcpyData(request, response, error, database)) {
        SetTimelineError(ErrorCode::QUERY_MEMCPY_OVERALL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    SendResponse(std::move(responsePtr), true);
    return true;
}

void BuildMemcpyOverallResult(
    const std::vector<MemcpyRecord> &records, MemcpyOverallResponse &response, uint32_t current, uint32_t pageSize) {
    // std::map 自带排序
    std::map<std::string, StatsAccumulator> threadMap;
    std::map<std::string, std::map<std::string, StatsAccumulator>> typeMap;
    std::map<std::string, std::string> threadNameMap;

    for (const auto &rec : records) {
        threadMap[rec.threadId].Update(rec.size, rec.duration);
        threadNameMap[rec.threadId] = rec.threadName;
        typeMap[rec.threadId][rec.memcpyType].Update(rec.size, rec.duration);
    }

    std::vector<MemcpyOverallRes> result;
    result.reserve(threadMap.size());

    for (auto &[tid, tStat] : threadMap) {
        MemcpyOverallRes ts;
        ts.key = tid;
        ts.name = threadNameMap.at(tid);
        ts.level = 1;
        ts.totalSize = tStat.totalSize;
        ts.totalTime = tStat.totalTime;
        ts.number = tStat.count;
        ts.avgSize = tStat.GetAvgSize();
        ts.minSize = tStat.GetMinSize();
        ts.maxSize = tStat.GetMaxSize();
        ts.avgTime = tStat.GetAvgTime();
        ts.minTime = tStat.GetMinTime();
        ts.maxTime = tStat.GetMaxTime();

        if (auto it = typeMap.find(tid); it != typeMap.end()) {
            ts.children.reserve(it->second.size());
            for (auto &[mtype, mStat] : it->second) {
                MemcpyOverallRes tts;
                tts.key = mtype;
                tts.name = mtype;
                tts.level = 2;
                tts.totalSize = mStat.totalSize;
                tts.totalTime = mStat.totalTime;
                tts.number = mStat.count;
                tts.avgSize = mStat.GetAvgSize();
                tts.minSize = mStat.GetMinSize();
                tts.maxSize = mStat.GetMaxSize();
                tts.avgTime = mStat.GetAvgTime();
                tts.minTime = mStat.GetMinTime();
                tts.maxTime = mStat.GetMaxTime();
                ts.children.push_back(std::move(tts));
            }
        }
        result.push_back(std::move(ts));
    }
    Paginator<MemcpyOverallRes> paginator(result, pageSize);
    response.pageParam.total = paginator.GetTotal();
    response.details = paginator.GetPage(current);
}

bool QueryMemcpyOverallHandler::CalMemcpyData(MemcpyOverallRequest &request, MemcpyOverallResponse &response,
    std::string &error, const std::shared_ptr<VirtualTraceDatabase> &database) {
    const MemcpyOverallDatabaseAccesser accesser(database, request.fileId);

    std::vector<MemcpyRecord> records;
    if (!accesser.GetMemcpyRecords(request.params.startTime, request.params.endTime, records)) {
        error = "Failed to query memcpy statistics.";
        return false;
    }

    BuildMemcpyOverallResult(records, response, request.params.page.current, request.params.page.pageSize);
    response.pageParam.current = request.params.page.current;
    response.pageParam.pageSize = request.params.page.pageSize;
    return true;
}
}
