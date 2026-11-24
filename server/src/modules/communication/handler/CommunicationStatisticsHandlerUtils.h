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
#ifndef PROFILER_SERVER_COMMUNICATION_STATISTICS_HANDLER_UTILS_H
#define PROFILER_SERVER_COMMUNICATION_STATISTICS_HANDLER_UTILS_H

#include "CommunicationRequestHandler.h"
#include "DataBaseManager.h"
#include "WsSender.h"

namespace Dic::Module::Communication {
inline std::shared_ptr<VirtualClusterDatabase> GetStatisticsDatabase(const std::string &clusterPath) {
    auto &manager = Timeline::DataBaseManager::Instance();
    return manager.HasClusterDatabase(clusterPath) ? manager.GetClusterDatabase(clusterPath) : nullptr;
}

inline bool SendStatisticsError(std::unique_ptr<Protocol::Response> response, ErrorCode code) {
    SetCommunicationError(code);
    ModuleRequestHandler::SetResponseResult(*response, false);
    SendResponse(std::move(response), false);
    return false;
}

inline bool SendStatisticsResponse(std::unique_ptr<Protocol::Response> response, StatisticsStatus status) {
    switch (status) {
    case StatisticsStatus::SUCCESS:
        ModuleRequestHandler::SetResponseResult(*response, true);
        SendResponse(std::move(response), true);
        return true;
    case StatisticsStatus::UNSUPPORTED:
        return SendStatisticsError(std::move(response), ErrorCode::COMMUNICATION_STATISTICS_UNSUPPORTED);
    case StatisticsStatus::INVALID_DATA:
        return SendStatisticsError(std::move(response), ErrorCode::COMMUNICATION_STATISTICS_INVALID_DATA);
    default:
        return SendStatisticsError(std::move(response), ErrorCode::QUERY_COMMUNICATION_STATISTICS_FAILED);
    }
}
}

#endif
