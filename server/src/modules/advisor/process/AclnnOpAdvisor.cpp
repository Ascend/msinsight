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
#include "VirtualTraceDatabase.h"
#include "AclnnOpAdvisor.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool AclnnOpAdvisor::Process(const Protocol::APITypeParams &params, Protocol::AclnnOperatorResBody &resBody) {
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection in Aclnn op advice. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }

    Protocol::KernelDetailsParams param = {.orderBy = params.orderBy,
        .order = params.orderType,
        .current = params.currentPage,
        .pageSize = params.pageSize,
        .startTime = params.startTime,
        .endTime = params.endTime,
        .rankId = params.rankId,
        .deviceId = params.deviceId};
    param.order = params.orderType == "ascend" ? "ASC" : "DESC";
    if (std::count(SINGLE_OP_ORDER_BY_NAME_LIST.begin(), SINGLE_OP_ORDER_BY_NAME_LIST.end(), params.orderBy) == 0) {
        param.orderBy = "duration";
    }
    if (!AclnnOpProcess(database, param, resBody)) {
        return false;
    }
    resBody.dbPath = database->GetDbPath();
    return true;
}

bool AclnnOpAdvisor::AclnnOpProcess(const std::shared_ptr<Timeline::VirtualTraceDatabase> &database,
    Protocol::KernelDetailsParams &param, Protocol::AclnnOperatorResBody &resBody) {
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(param.rankId);
    if (deviceId.empty()) {
        ServerLog::Error("Query Aclnn op advice failed to get deviceId.");
        SetAdvisorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    param.deviceId = deviceId;
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::vector<Protocol::KernelBaseInfo> data{};
    if (!database->QueryAclnnOpCountExceedThreshold(param, ACLNN_OP_CNT_THRESHOLD, data, startTime)) {
        ServerLog::Error("Failed to Query Aclnn Op from database which count exceeds .", ACLNN_OP_CNT_THRESHOLD);
        SetAdvisorError(ErrorCode::QUERY_ACLNN_OPERATOR_COUNT_FAILED);
        return false;
    }
    uint64_t start = param.pageSize * (param.current - 1);
    for (uint64_t i = start; i < start + param.pageSize && i < data.size(); ++i) {
        auto item = data.at(i);
        Protocol::AclnnOperatorData one{};
        one.baseInfo.id = item.id;
        one.baseInfo.rankId = param.rankId;
        one.baseInfo.startTime = item.startTime;
        one.baseInfo.duration = item.duration;
        one.baseInfo.pid = item.pid;
        one.baseInfo.tid = item.tid;
        one.baseInfo.depth = item.depth;
        one.opName = item.name;
        one.note = "";
        resBody.data.emplace_back(one);
    }
    resBody.size += data.size();
    return true;
}
} // Dic::Module::Advisor
