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
#include "AICpuOpAdvisor.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool AICpuOpAdvisor::Process(const Protocol::APITypeParams &params, Protocol::AICpuOperatorResBody &resBody) {
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection in AI CPU advice. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::vector<Protocol::KernelBaseInfo> data{};
    Protocol::KernelDetailsParams param = {.orderBy = params.orderBy,
        .order = params.orderType,
        .current = params.currentPage,
        .pageSize = params.pageSize,
        .startTime = params.startTime,
        .endTime = params.endTime};
    param.order = params.orderType == "ascend" ? "ASC" : "DESC";
    if (std::count(AICPU_OP_ORDER_BY_NAME_LIST.begin(), AICPU_OP_ORDER_BY_NAME_LIST.end(), params.orderBy) == 0) {
        param.orderBy = "duration";
    }
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(params.rankId);
    if (deviceId.empty()) {
        ServerLog::Error("Query AI CPU advice failed to get deviceId. deviceId:", deviceId);
        SetAdvisorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    param.deviceId = deviceId;
    if (!database->QueryAICpuOpCanBeOptimized(
            param, AICPU_OP_EQUIVALENT_REPLACE, AICPU_OP_DATATYPE_RULE, data, startTime)) {
        ServerLog::Error("Failed to Query Can Be Optimized AI CPU Op from database. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::QUERY_AI_CPU_OP_CAN_BE_OPTIMIZED_FAILED);
        return false;
    }
    uint64_t start = param.pageSize * (param.current - 1);
    for (uint64_t i = start; i < start + param.pageSize && i < data.size(); ++i) {
        auto item = data.at(i);
        Protocol::AICpuOperatorData one{};
        one.baseInfo.id = item.id;
        one.baseInfo.rankId = params.rankId;
        one.baseInfo.startTime = item.startTime;
        one.baseInfo.duration = item.duration;
        one.baseInfo.pid = item.pid;
        one.baseInfo.tid = item.tid;
        one.baseInfo.depth = item.depth;
        one.opName = item.name;
        one.note = GenerateAICpuOperatorNote(item);
        resBody.data.emplace_back(one);
    }
    resBody.dbPath = database->GetDbPath();
    resBody.size = data.size();
    return true;
}

std::string AICpuOpAdvisor::GenerateAICpuOperatorNote(const Protocol::KernelBaseInfo &info) {
    std::vector<std::string> replace = AICPU_OP_EQUIVALENT_REPLACE;
    if (std::find(replace.begin(), replace.end(), info.type) != replace.end()) {
        return "Try to replace " + info.type + " operator with equivalent operator.";
    }
    auto it = AICPU_OP_DATATYPE_RULE.find(info.type);
    Timeline::AICpuCheckDataType type;
    if (it != AICPU_OP_DATATYPE_RULE.end()) {
        type = it->second;
    } else {
        type = AICPU_OP_DATATYPE_RULE.at("other");
    }
    if (std::find(type.input.begin(), type.input.end(), info.inputType) == type.input.end() ||
        std::find(type.output.begin(), type.output.end(), info.outputType) == type.output.end()) {
        return "The input/output data type is not supported, "
               "and maybe you can convert its type to meet the requirements: "
               "input data type, " +
            StringUtil::join(type.input, ",") + "; output data type, " + StringUtil::join(type.output, ",");
    }
    uint32_t threshold = AICPU_OP_DURATION_THRESHOLD / THOUSAND;
    if (info.duration > threshold) {
        return "Duration exceeds " + std::to_string(threshold) + " us, and modify code to avoid AI CPU operator";
    }
    return "";
}
} // Dic::Module::Advisor
