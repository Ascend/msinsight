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

#include "SummaryService.h"
#include "TraceTime.h"
#include "DataBaseManager.h"
#include "CollectionUtil.h"
#include "BaselineManager.h"
#include "SummaryErrorManager.h"

using namespace Dic::Module::Summary;
using namespace Dic::Module::Global;
using namespace Dic::Protocol;
using namespace Dic::Module;

bool SummaryService::UpdateStartTimeAndDuration(
    SummaryBaseInfo &baseInfo, std::shared_ptr<VirtualClusterDatabase> &db) {
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    db->QueryExtremumTimestamp(min, max); // time unit of min, max: ns
    if (min > max) {
        ServerLog::Warn("Fail to get extremum timestamp when query summary base info.");
        return false;
    }
    // time unit of collectStartTime: ms, time unit of collectDuration: us
    baseInfo.collectStartTime = static_cast<int64_t>(
        NumberUtil::CeilingClamp(min / (numberThousands * numberThousands), static_cast<uint64_t>(INT64_MAX)));
    baseInfo.collectDuration =
        NumberUtil::CeilingClamp((max - min) / numberThousands, static_cast<uint64_t>(INT64_MAX));
    // time unit of step trace data: us
    std::unordered_map<std::uint32_t, StepStatistic> rankStepTraceData{}; // key: rankId, value: step trace data
    db->QueryAllPerformanceDataByStep("All", rankStepTraceData);
    for (const auto &stepTraceData : rankStepTraceData) {
        baseInfo.collectDuration = stepTraceData.second.npuTotalTime > baseInfo.collectDuration
            ? stepTraceData.second.npuTotalTime
            : baseInfo.collectDuration;
    }
    if (!db->UpdateCollectTimeInfo(baseInfo)) {
        ServerLog::Warn("Failed to update database for cluster base info.");
        return false;
    }
    return true;
}

bool SummaryService::QuerySummaryBaseInfo(SummaryBaseInfo &baseInfo, std::shared_ptr<VirtualClusterDatabase> &db) {
    // db在外层进行校验 必不为空
    if (!db->QueryBaseInfo(baseInfo)) {
        ServerLog::Warn("Fail to query summary base info.");
        return false;
    }
    if ((baseInfo.collectStartTime == 0 || baseInfo.collectDuration == 0.0) &&
        !UpdateStartTimeAndDuration(baseInfo, db)) {
        ServerLog::Warn("Fail to update start time and duration for summary base info.");
        return false;
    }
    return true;
}
void SummaryService::QueryCompareSummaryBaseInfo(
    const SummaryTopRankRequest &request, SummaryTopRankResponse &response) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    if (database == nullptr || !QuerySummaryBaseInfo(response.body.baseInfo.compare, database)) {
        ServerLog::Warn("Fail to query compare summary base info");
    }

    if (!request.params.isCompare) {
        return;
    }

    auto baselineDatabase =
        Timeline::DataBaseManager::Instance().GetClusterDatabase(BaselineManager::Instance().GetBaseLineClusterPath());
    if (baselineDatabase == nullptr || !QuerySummaryBaseInfo(response.body.baseInfo.baseline, baselineDatabase)) {
        ServerLog::Warn("Fail to query baseline summary base info");
    }
}

std::vector<IndicatorDataStruct> SummaryService::GetPerformanceDataByDimension(
    std::shared_ptr<VirtualClusterDatabase> &database, const GetPerformanceIndicatorParam &params) {
    std::vector<IndicatorDataStruct> indicatorData;
    if (database == nullptr) {
        ServerLog::Warn("Fail to query compare parallelism info");
        return indicatorData;
    }
    std::unordered_map<std::uint32_t, StepStatistic> stepStatisticData{}; // key: rankId, value: step trace data
    bool result = database->QueryAllPerformanceDataByStep(params.step, stepStatisticData);
    if (!result || stepStatisticData.empty()) {
        ServerLog::Warn("Failed to query original parallelism performance data.");
        return indicatorData;
    }
    // 前端入参已校验，无整数溢出风险
    uint32_t worldSize = params.config.dpSize * params.config.cpSize * params.config.tpSize * params.config.ppSize;
    // 允许在未设置并行策略时，直接返回计算通信概览, 最多允许展示64卡
    if (worldSize == 1 && stepStatisticData.size() > maxRankCountForSummaryWithoutConfig) {
        ServerLog::Warn("When no parallel strategy is configured, computation/communication overview is limited to "
                        "a maximum of " +
            std::to_string(maxRankCountForSummaryWithoutConfig) + " cards.");
        return indicatorData;
    }
    auto algPtr = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(database->GetDbPath());
    if (algPtr == nullptr) {
        ServerLog::Warn("Failed to get algorithm by project name for query parallelism performance.");
        return indicatorData;
    }
    std::string err;
    result = algPtr->GetPerformanceIndicatorByDimension(params, stepStatisticData, indicatorData, err);
    if (!result) {
        ServerLog::Warn(err);
        return indicatorData;
    }
    return indicatorData;
}

std::unordered_map<std::string, double> SummaryService::CalDiffIndicators(
    std::unordered_map<std::string, double> &compare, std::unordered_map<std::string, double> &baseline) {
    std::set<std::string> keySet;
    for (const auto &item : compare) {
        keySet.insert(item.first);
    }
    for (const auto &item : baseline) {
        keySet.insert(item.first);
    }
    std::unordered_map<std::string, double> diff;
    // 数据保留三位小数
    int precision = 3;
    for (const auto &item : keySet) {
        diff[item] = NumberUtil::DoubleReservedNDigits(compare[item] - baseline[item], precision);
    }
    return diff;
}

void SummaryService::MergeParallelismPerformance(std::vector<IndicatorDataStruct> &compare,
    std::vector<IndicatorDataStruct> &baseline, PerformanceIndicatorData &indicatorData) {
    std::set<uint32_t> indexList;
    std::map<uint32_t, IndicatorDataStruct> compareMap;
    for (const auto &item : compare) {
        indexList.insert(item.index);
        compareMap[item.index] = item;
    }

    std::map<uint32_t, IndicatorDataStruct> baselineMap;
    for (const auto &item : baseline) {
        indexList.insert(item.index);
        baselineMap[item.index] = item;
    }

    for (const auto &item : indexList) {
        IndicatorDataStructVo indicatorDataStructVo;
        indicatorDataStructVo.index = item;
        if (compareMap.count(item)) {
            indicatorDataStructVo.indicators.compare = compareMap[item].indicators;
        }
        if (baselineMap.count(item)) {
            indicatorDataStructVo.indicators.baseline = baselineMap[item].indicators;
        }
        indicatorDataStructVo.indicators.diff =
            CalDiffIndicators(indicatorDataStructVo.indicators.compare, indicatorDataStructVo.indicators.baseline);
        indicatorData.performanceData.push_back(indicatorDataStructVo);
    }
}

bool SummaryService::QueryParallelismPerformanceInfo(
    const ParallelismPerformance &params, PerformanceIndicatorData &indicatorData) {
    // 查询compare数据
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    GetPerformanceIndicatorParam indicatorParam{params.step, params.dimension, params.config};
    std::vector<IndicatorDataStruct> compareIndicatorData = GetPerformanceDataByDimension(database, indicatorParam);
    CommInfoMap compareCommInTpDimension;
    CommInfoMap compareCommInfo = QueryParallelismCommTime(database, indicatorParam, compareCommInTpDimension);
    // 查询baseline数据
    std::vector<IndicatorDataStruct> baselineIndicatorData;
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> baselineCommInfo;
    if (params.isCompare) {
        auto databaseBaseline = Timeline::DataBaseManager::Instance().GetClusterDatabase(
            BaselineManager::Instance().GetBaseLineClusterPath());
        GetPerformanceIndicatorParam baselineParams{params.baselineStep, params.dimension, params.config};
        baselineIndicatorData = GetPerformanceDataByDimension(databaseBaseline, baselineParams);
        CommInfoMap baseCommInTpDimension;
        baselineCommInfo = QueryParallelismCommTime(databaseBaseline, baselineParams, baseCommInTpDimension);
    }

    if (compareIndicatorData.empty() && baselineIndicatorData.empty()) {
        ServerLog::Error("Fail to query parallelism performance info.");
        SetSummaryError(ErrorCode::QUERY_PARALLELISM_PERFORMANCE_FAILED);
        return false;
    }
    MergeParallelismPerformance(compareIndicatorData, baselineIndicatorData, indicatorData);
    MergeCommDataPerformance(compareCommInfo, baselineCommInfo, indicatorData);
    // 非对比状态下，才计算专家建议信息
    if (!params.isCompare && database != nullptr) {
        auto algPtr = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(database->GetDbPath());
        if (algPtr != nullptr) {
            algPtr->CalAdviceInfo(params.dimension, indicatorData.advices, compareIndicatorData);
            if (!algPtr->CalAdviceInfoByCommInfo(compareCommInTpDimension)) {
                ServerLog::Warn("Failed to calculate slow rank advice by communication time. Current parallel "
                                "strategy config do not match the actual model training parameters.");
            }
        }
    }
    return true;
}

void SummaryService::MergeCommDataPerformance(std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &compare,
    std::unordered_map<std::string, std::vector<CommInfoUnderRank>> &baseline,
    PerformanceIndicatorData &indicatorData) {
    for (auto &item : indicatorData.performanceData) {
        std::string key = std::to_string(item.index);
        MergeCommInfo(compare[key], baseline[key], item.commTimeIndicator);
    }
}

void SummaryService::MergeCommInfo(std::vector<CommInfoUnderRank> compare, std::vector<CommInfoUnderRank> baseline,
    CompareData<std::unordered_map<std::string, double>> &commRes) {
    std::unordered_map<std::string, double> compareMap;
    for (const auto &item : compare) {
        compareMap[item.pgName] = item.commTime;
    }

    std::unordered_map<std::string, double> baselineMap;
    for (const auto &item : baseline) {
        baselineMap[item.pgName] = item.commTime;
    }

    commRes.compare = compareMap;
    commRes.baseline = baselineMap;
    commRes.diff = CalDiffIndicators(compareMap, baselineMap);
}

std::unordered_map<std::string, std::vector<CommInfoUnderRank>> SummaryService::QueryParallelismCommTime(
    const std::shared_ptr<VirtualClusterDatabase> &database, const GetPerformanceIndicatorParam &params,
    CommInfoMap &commInTpDimension) {
    if (database == nullptr) {
        ServerLog::Warn("Fail to query parallelism communication info, database not exist.");
        return {};
    }
    auto algPtr = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(database->GetDbPath());
    if (algPtr == nullptr) {
        ServerLog::Warn("Failed to get algorithm by project name for query parallelism communication info.");
        return {};
    }
    // 获取导入的rank数据信息
    std::vector<std::string> importRankList = database->GetAllRankFromStepStatisticInfo();
    if (importRankList.empty()) {
        ServerLog::Warn("Fail to get all rank from step statistic info.");
        return {};
    }

    // 获取要填充的数据
    std::vector<CommInfoUnderRank> commTimeForRankDim = database->GetCommTimeForRankDim(params.step);
    if (commTimeForRankDim.empty()) {
        ServerLog::Warn("Fail to get communication time data.");
        return {};
    }
    // 按rank划分数据
    for (auto &item : commTimeForRankDim) {
        // pgName不存在时不返回按通信域拆解通信时间
        if (item.pgName.empty()) {
            continue;
        }
        commInTpDimension[item.rankId].push_back(item);
    }
    // 数据根据维度进行折叠
    return algPtr->GetCommInfoByDimension(commInTpDimension, params.dimension);
}
