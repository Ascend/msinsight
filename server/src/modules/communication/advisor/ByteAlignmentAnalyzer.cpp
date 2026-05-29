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

#include "ByteAlignmentAnalyzer.h"
#include "DataBaseManager.h"
#include "ServerLog.h"

namespace Dic {
namespace Module {
namespace Communication {
bool ByteAlignmentAnalyzer::QueryAdvisorData(const std::string &clusterPath) {
    std::vector<IterationsOrRanksObject> rankList;
    auto communicationDatabase = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (!communicationDatabase || !communicationDatabase->QueryRanksHandler(rankList)) {
        Server::ServerLog::Error("Failed to get ranks data when query byte alignment data.");
        return false;
    }
    for (const auto &rank : rankList) {
        data.insert({rank.iterationOrRankId, {}});
        auto timelineDatabase = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(rank.iterationOrRankId);
        if (!timelineDatabase) {
            timelineDatabase =
                Timeline::DataBaseManager::Instance().GetTraceDatabaseInCluster(clusterPath, rank.iterationOrRankId);
            if (!timelineDatabase) {
                Server::ServerLog::Error("Failed to get connection to trace database when query byte alignment data.");
                continue;
            }
        }
        timelineDatabase->QueryByteAlignmentAnalyzerData(data[rank.iterationOrRankId]);
    }
    return true;
}

void ByteAlignmentAnalyzer::ComputeStatistics() {
    for (const auto &itemsForEachRank : data) {
        for (const auto &item : itemsForEachRank.second) {
            if (Check(item)) {
                ByteAlignmentAnalyzerStatistics op;
                op.rankId = itemsForEachRank.first;
                op.name = item.name;
                statistics.emplace_back(op);
            }
        }
    }
}

bool ByteAlignmentAnalyzer::Check(const Dic::Module::CommunicationLargeOperatorInfo &item) {
    bool flag = false;
    for (const auto &memcpyItem : item.memcpyTasks) {
        if (memcpyItem.transportType != "SDMA" || memcpyItem.linkType == "ON_CHIP" ||
            memcpyItem.size <= BYTE_ALIGNMENT_SMALL_SIZE) {
            continue;
        }
        if (memcpyItem.size % BYTE_ALIGNMENT_BASE_SIZE > 0) {
            flag = true;
        }
    }
    for (const auto &reduceItem : item.reduceInlineTasks) {
        if (reduceItem.transportType != "SDMA" || reduceItem.linkType == "ON_CHIP" ||
            reduceItem.size <= BYTE_ALIGNMENT_SMALL_SIZE) {
            continue;
        }
        if (reduceItem.size % BYTE_ALIGNMENT_BASE_SIZE > 0) {
            flag = true;
        }
    }
    return flag;
}

void ByteAlignmentAnalyzer::AssembleAdvisor(Dic::Protocol::CommunicationAdvisorInfo &info) {
    info.name = BYTEALIGNMENT_ANALYZER_TITLE;
    info.statistics.insert({"rankId", {}});
    info.statistics.insert({"name", {}});
    for (const auto &item : statistics) {
        info.statistics["rankId"].emplace_back(item.rankId);
        info.statistics["name"].emplace_back(item.name);
    }
}
}
}
}
