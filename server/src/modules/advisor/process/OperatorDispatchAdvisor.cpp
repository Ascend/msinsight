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

#include "pch.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "AdvisorProcessUtil.h"
#include "OperatorDispatchAdvisor.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool OperatorDispatchAdvisor::Process(
    const Protocol::APITypeParams &params, Protocol::OperatorDispatchResBody &resBody) {
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection for Operator Dispatch advice. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    Protocol::KernelDetailsParams param = {.orderBy = params.orderBy,
        .order = params.orderType,
        .current = params.currentPage,
        .pageSize = params.pageSize,
        .startTime = params.startTime,
        .endTime = params.endTime,
        .rankId = params.rankId};
    param.order = params.orderType == "ascend" ? "ASC" : "DESC";
    if (std::count(OPERATOR_DISPATCH_ORDER_BY_NAME_LIST.begin(), OPERATOR_DISPATCH_ORDER_BY_NAME_LIST.end(),
            params.orderBy) == 0) {
        param.orderBy = "duration";
    }
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::string filePath = Timeline::DataBaseManager::Instance().GetDbPathByRankId(params.rankId);
    std::vector<Protocol::KernelBaseInfo> data{};
    if (!database->QueryOperatorDispatchData(param, data, startTime, OPERATOR_COMPILE_CNT_THRESHOLD)) {
        ServerLog::Error("Failed to Query Operator Dispatch data.");
        SetAdvisorError(ErrorCode::QUERY_OPERATOR_DISPATCH_FAILED);
        return false;
    }
    uint64_t start = param.pageSize * (param.current - 1);
    for (uint64_t i = start; i < start + param.pageSize && i < data.size(); ++i) {
        auto item = data.at(i);
        Protocol::OperatorDispatchData one{};
        one.baseInfo.id = item.id;
        one.baseInfo.rankId = params.rankId;
        one.baseInfo.startTime = item.startTime;
        one.baseInfo.duration = item.duration;
        one.baseInfo.pid = item.pid;
        one.baseInfo.tid = item.tid;
        one.baseInfo.depth = item.depth;
        one.opName = item.name;
        one.note = SUGGESTION_NOTE;
        resBody.data.emplace_back(one);
    }
    resBody.size = data.size();
    resBody.dbPath = database->GetDbPath();
    return true;
}
} // Dic::Module::Advisor
