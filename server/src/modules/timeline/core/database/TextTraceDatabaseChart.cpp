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

#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "TraceDatabaseSqlConst.h"
#include "ServerLog.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;

void TextTraceDatabase::AssembleUnitFlowsBody(Protocol::UnitFlowsBody &responseBody, uint64_t minTimestamp,
    std::unordered_map<std::string, std::vector<FlowPoint>> &flowPointMap) {
    std::map<std::string, std::vector<Protocol::UnitSingleFlow>> flowMap;
    for (auto &item : flowPointMap) {
        const static int FLOW_COUNT = 2; // from + to
        if (item.second.size() < FLOW_COUNT) {
            continue;
        }
        std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> flowDetailList;
        FlowAnalyzer::SortByFlowIdAndTimestampASC(item.second);
        FlowAnalyzer::ComputeUintFlows(item.second, item.second[0].cat, flowDetailList);
        std::vector<Protocol::UnitCatFlows> unitAllFlow;
        for (const auto &singleFlow : flowDetailList) {
            if (singleFlow->from.timestamp < minTimestamp || singleFlow->to.timestamp < minTimestamp) {
                continue;
            }
            singleFlow->from.timestamp -= minTimestamp;
            singleFlow->to.timestamp -= minTimestamp;
            flowMap[singleFlow->cat].emplace_back(*singleFlow);
        }
    }
    std::vector<Protocol::UnitCatFlows> unitAllFlow;
    for (const auto &item : flowMap) {
        Protocol::UnitCatFlows unitCatFlows;
        unitCatFlows.cat = item.first;
        unitCatFlows.flows = item.second;
        unitAllFlow.emplace_back(unitCatFlows);
    }
    responseBody.unitAllFlows = unitAllFlow;
}
}
