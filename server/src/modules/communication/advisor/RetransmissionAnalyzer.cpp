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

#include "RetransmissionAnalyzer.h"
#include "DataBaseManager.h"
#include "ServerLog.h"

namespace Dic {
namespace Module {
namespace Communication {
bool RetransmissionAnalyzer::QueryAdvisorData(const std::string &clusterPath) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (!database || !database->QueryRetransmissionAnalyzerData(data)) {
        Server::ServerLog::Error("Failed to query retransmission analyzer data.");
        return false;
    }
    return true;
}

void RetransmissionAnalyzer::ComputeStatistics() {
    for (const auto &element : data) {
        if (element.minElapseTime < MIN_RETRANSMISSION_TIME) {
            continue;
        }
        if (element.maxRDMATransitTime > MIN_RETRANSMISSION_TIME) {
            RetransmissionAnalyzerStatistics info{element.groupId, element.opName};
            statistics.emplace_back(info);
        }
    }
}

void RetransmissionAnalyzer::AssembleAdvisor(Dic::Protocol::CommunicationAdvisorInfo &info) {
    info.name = RETRANSMISSION_ANALYZER_TITLE;
    info.statistics.insert({"name", {}});
    info.statistics.insert({"group name", {}});
    for (const auto &item : statistics) {
        info.statistics["group name"].emplace_back(item.groupName);
        info.statistics["name"].emplace_back(item.opName);
    }
}
}
}
}
