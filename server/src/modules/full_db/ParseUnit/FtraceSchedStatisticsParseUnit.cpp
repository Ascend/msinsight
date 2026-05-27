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

#include "FtraceSchedStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"

namespace Dic::Module::FullDb {

std::string FtraceSchedStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_SCHED_STATISTICS_UNIT; }

bool FtraceSchedStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateFtraceTable();
}

bool FtraceSchedStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"sched_switch"};
    auto allSchedSlice =
        RenderEngine::Instance()->QuerySliceDetailByNameList(params.dbId, DataType::TEXT, "CPU Scheduling", nameList);
    auto threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});

    std::unordered_map<std::string, uint64_t> tidToTrackId;
    for (const auto &pair : threadInfoMap) {
        tidToTrackId[pair.second.second] = pair.first;
    }
    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> trackIdMap;
    for (const auto &slice : allSchedSlice) {
        std::unordered_map<std::string, std::string> argsMap = JsonUtil::JsonStrToMap(slice.args);
        std::string prevComm = argsMap["prev_comm"];
        std::string prevPid = argsMap["prev_pid"];
        auto it = tidToTrackId.find(prevComm + ":" + prevPid);
        if (it == tidToTrackId.end()) {
            continue;
        }
        uint64_t trackId = it->second;
        if (trackIdMap.find(trackId) == trackIdMap.end()) {
            trackIdMap[trackId] = std::unordered_map<std::string, uint64_t>{{"context_switch_count", 0}};
        }
        trackIdMap[trackId]["context_switch_count"]++;
    }
    for (auto &pair : trackIdMap) {
        FtraceStatisticsData statData;
        statData.trackId = pair.first;
        statData.dataType = FtraceDataType::SCHED;
        for (auto &dataPair : pair.second) {
            statData.data[dataPair.first] = std::to_string(dataPair.second);
        }
        database->InsertFtraceStat(statData);
    }
    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceSchedStatisticsParseUnit> unitRegFtraceSched(Dic::FTRACE_SCHED_STATISTICS_UNIT);

}
