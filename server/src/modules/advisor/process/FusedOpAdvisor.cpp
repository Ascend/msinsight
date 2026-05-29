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
#include "FusedOpAdvisor.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool FusedOpAdvisor::Process(const Protocol::APITypeParams &params, Protocol::OperatorFusionResBody &resBody) {
    auto database = GetDatabaseConnection(params.rankId);
    if (database == nullptr) {
        return false;
    }
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    Protocol::KernelDetailsParams param = {.orderBy = params.orderBy,
        .order = params.orderType,
        .current = params.currentPage,
        .pageSize = params.pageSize,
        .startTime = params.startTime,
        .endTime = params.endTime,
        .rankId = params.rankId};
    param.order = params.orderType == "ascend" ? "ASC" : "DESC";
    if (std::count(FUSED_OP_ORDER_BY_NAME_LIST.begin(), FUSED_OP_ORDER_BY_NAME_LIST.end(), params.orderBy) == 0) {
        param.orderBy = "duration";
    }
    param.deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(params.rankId);
    if (param.deviceId.empty()) {
        ServerLog::Error("Failed to get deviceId when query fused operator advice.");
        SetAdvisorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    if (!database->QueryFusibleOpData(param, FUSEABLE_OPERATER_RULE_LIST, resBody, startTime)) {
        ServerLog::Error("Failed to query fused operator advice from database.");
        SetAdvisorError(ErrorCode::QUERY_FUSED_OP_DATABASE_FAILED);
        return false;
    }
    resBody.dbPath = database->GetDbPath();
    return true;
}

std::shared_ptr<Timeline::VirtualTraceDatabase> FusedOpAdvisor::GetDatabaseConnection(const std::string &rankId) {
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection for Fused Operator advice. fileId:", rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
    }
    return database;
}
} // Dic::Module::Advisor
