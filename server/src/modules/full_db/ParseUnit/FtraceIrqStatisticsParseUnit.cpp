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

#include "FtraceIrqStatisticsParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"
#include "RenderEngine.h"

namespace Dic::Module::FullDb {

std::string FtraceIrqStatisticsParseUnit::GetUnitName() const { return Dic::FTRACE_IRQ_STATISTICS_UNIT; }

bool FtraceIrqStatisticsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->CreateFtraceTable();
}

void FtraceIrqStatisticsParseUnit::AddIrqInfo(uint64_t trackId, const std::string &irqType, uint64_t duration,
    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> &trackIdMap) {
    auto initStatData = []() {
        return std::unordered_map<std::string, uint64_t>{
            {"soft_irq_count", 0}, {"soft_irq_duration", 0}, {"hard_irq_count", 0}, {"hard_irq_duration", 0}};
    };

    auto addIrqStat = [](std::unordered_map<std::string, uint64_t> &statData, const std::string &irqType,
                          uint64_t duration) {
        if (irqType == "softirq") {
            statData["soft_irq_count"]++;
            statData["soft_irq_duration"] += duration;
        } else if (irqType == "irq") {
            statData["hard_irq_count"]++;
            statData["hard_irq_duration"] += duration;
        }
    };

    if (trackIdMap.find(trackId) == trackIdMap.end()) {
        trackIdMap[trackId] = initStatData();
    }
    addIrqStat(trackIdMap[trackId], irqType, duration);
}

bool FtraceIrqStatisticsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    std::vector<std::string> nameList = {"irq", "softirq"};
    auto threadInfoMap = RenderEngine::Instance()->GetAllThreadInfo({params.dbId, PROCESS_TYPE::TEXT});
    auto allIrqSlice =
        RenderEngine::Instance()->QuerySliceDetailByNameList(params.dbId, DataType::TEXT, "CPU Scheduling", nameList);

    std::unordered_map<std::string, uint64_t> tidToTrackId;
    for (const auto &pair : threadInfoMap) {
        tidToTrackId[pair.second.second] = pair.first;
    }

    std::unordered_map<uint64_t, std::unordered_map<std::string, uint64_t>> trackIdMap;

    for (const auto &slice : allIrqSlice) {
        uint64_t trackId = slice.trackId;
        std::string irqType = slice.name;
        uint64_t duration = slice.duration;
        auto argsMap = JsonUtil::JsonStrToMap(slice.args);
        std::string processTid = argsMap["task"];

        AddIrqInfo(trackId, irqType, duration, trackIdMap);

        auto it = tidToTrackId.find(processTid);
        if (it != tidToTrackId.end()) {
            AddIrqInfo(it->second, irqType, duration, trackIdMap);
        }
    }

    for (auto &pair : trackIdMap) {
        FtraceStatisticsData statData = {pair.first, FtraceDataType::IRQ};
        for (auto &dataPair : pair.second) {
            statData.data[dataPair.first] = std::to_string(dataPair.second);
        }
        database->InsertFtraceStat(statData);
    }
    database->CommitData();
    return true;
}

ParseUnitRegistrar<FtraceIrqStatisticsParseUnit> unitRegFtraceIrq(Dic::FTRACE_IRQ_STATISTICS_UNIT);

}
