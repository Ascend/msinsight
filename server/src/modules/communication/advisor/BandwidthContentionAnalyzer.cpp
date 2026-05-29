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

#include "BandwidthContentionAnalyzer.h"
#include "DataBaseManager.h"
#include "ServerLog.h"

namespace Dic {
namespace Module {
namespace Communication {
bool BandwidthContentionAnalyzer::QueryAdvisorData(const std::string &clusterPath) {
    std::vector<IterationsOrRanksObject> rankList;
    auto communicationDatabase = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (!communicationDatabase || !communicationDatabase->QueryRanksHandler(rankList)) {
        Server::ServerLog::Error("Failed to get ranks data when query bandwidth contention data.");
        return false;
    }

    for (const auto &rank : rankList) {
        data.matMulData.insert({rank.iterationOrRankId, {}});
        data.SDMAData.insert({rank.iterationOrRankId, {}});
        auto summaryDatabase = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rank.iterationOrRankId);
        if (!summaryDatabase) {
            summaryDatabase = Timeline::DataBaseManager::Instance().GetSummaryDatabaseWithCluster(
                clusterPath, rank.iterationOrRankId);
        }
        if (!summaryDatabase) {
            Server::ServerLog::Error("Failed to get summary database connection.");
            continue;
        }
        summaryDatabase->QueryBandwidthContentionMatMulData(data.matMulData[rank.iterationOrRankId]);
        communicationDatabase->QueryBandwidthContentionAnalyzerData(
            data.SDMAData[rank.iterationOrRankId], rank.iterationOrRankId);
    }
    return true;
}

void BandwidthContentionAnalyzer::ComputeStatistics() {
    for (const auto &item : data.matMulData) {
        size_t HCCLIndex = 0;
        size_t matMulIndex = 0;
        while (HCCLIndex < data.SDMAData[item.first].size() && matMulIndex < data.matMulData[item.first].size()) {
            if (data.SDMAData[item.first][HCCLIndex].startTime + data.SDMAData[item.first][HCCLIndex].duration <
                data.matMulData[item.first][matMulIndex].startTime) {
                ++HCCLIndex;
                continue;
            }
            if (data.matMulData[item.first][matMulIndex].startTime + data.matMulData[item.first][matMulIndex].duration <
                data.SDMAData[item.first][HCCLIndex].startTime) {
                ++matMulIndex;
                continue;
            }
            if (data.SDMAData[item.first][HCCLIndex].bandwidth < BANDWIDTH_CONTENTION_ANALYZER_THRESHOLD) {
                BandwidthContentionAnalyzerStatistics op;
                op.rankId = item.first;
                op.name = data.SDMAData[item.first][HCCLIndex].name;
                op.duration = data.SDMAData[item.first][HCCLIndex].duration;
                op.bandwidth = data.SDMAData[item.first][HCCLIndex].bandwidth;
                statistics.emplace_back(op);
            }
            ++HCCLIndex;
        }
    }
}

void BandwidthContentionAnalyzer::AssembleAdvisor(Dic::Protocol::CommunicationAdvisorInfo &info) {
    info.name = BANDWIDTHCONTENTION_ANALYZER_TITLE;
    info.statistics.insert({"rankId", {}});
    info.statistics.insert({"name", {}});
    info.statistics.insert({"duration(us)", {}});
    info.statistics.insert({"bandwidth(GB/s)", {}});
    for (const auto &item : statistics) {
        info.statistics["rankId"].emplace_back(item.rankId);
        info.statistics["name"].emplace_back(item.name);
        info.statistics["duration(us)"].emplace_back(std::to_string(item.duration));
        info.statistics["bandwidth(GB/s)"].emplace_back(std::to_string(item.bandwidth));
    }
}
}
}
}
