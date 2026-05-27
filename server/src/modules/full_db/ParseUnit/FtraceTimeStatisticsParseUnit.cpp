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

#include "FtraceTimeStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"

namespace Dic::Module::FullDb {

std::string FtraceTimeStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_TIME_STATISTICS_UNIT; }

bool FtraceTimeStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateFtraceTable();
}

bool FtraceTimeStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"Sleeping", "Runnable", "Running"};
    auto allTimeSlice = RenderEngine::Instance()->QuerySliceDetailByNameList(
        params.dbId, DataType::TEXT, "Process Scheduling", nameList);

    auto initStatData = []() {
        return std::unordered_map<std::string, uint64_t>{{"runnable", 0}, {"running", 0}, {"sleeping", 0}};
    };

    auto addTimeStat = [](std::unordered_map<std::string, uint64_t> &statData, const std::string &timeType,
                           uint64_t duration) {
        if (timeType == "Runnable") {
            statData["runnable"] += duration;
        } else if (timeType == "Running") {
            statData["running"] += duration;
        } else if (timeType == "Sleeping") {
            statData["sleeping"] += duration;
        }
    };

    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> statsMap;

    for (const auto &slice : allTimeSlice) {
        uint64_t trackId = slice.trackId;
        std::string timeType = slice.name;
        uint64_t duration = slice.duration;

        if (statsMap.find(trackId) == statsMap.end()) {
            statsMap[trackId] = initStatData();
        }
        addTimeStat(statsMap[trackId], timeType, duration);
    }

    for (auto &pair : statsMap) {
        FtraceStatisticsData statData;
        statData.trackId = pair.first;
        statData.dataType = FtraceDataType::TIME;
        for (auto &dataPair : pair.second) {
            statData.data[dataPair.first] = std::to_string(dataPair.second);
        }
        database->InsertFtraceStat(statData);
    }
    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceTimeStatisticsParseUnit> unitRegFtraceTime(Dic::FTRACE_TIME_STATISTICS_UNIT);

}
