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

#include <vector>
#include <set>
#include <map>
#include "DataBaseManager.h"
#include "ServerLog.h"
#include "CollectionUtil.h"
#include "ClusterCovert.h"
#include "NumberUtil.h"
#include "BaselineManager.h"
#include "TrackInfoManager.h"
#include "ClusterService.h"

namespace Dic {
namespace Module {
namespace Communication {
using namespace Dic::Server;
using namespace Dic::Module::Global;
void ClusterService::QueryIterations(
    const Protocol::IterationsRequest &request, Protocol::IterationsOrRanksResponse &response) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    if (database == nullptr || !database->QueryIterations(response.body.compare)) {
        ServerLog::Warn("Fail to query compare iterations info.");
    }

    if (!request.params.isCompare) {
        return;
    }

    auto baselineDatabase =
        Timeline::DataBaseManager::Instance().GetClusterDatabase(BaselineManager::Instance().GetBaseLineClusterPath());
    if (baselineDatabase == nullptr || !baselineDatabase->QueryIterations(response.body.baseline)) {
        ServerLog::Warn("Fail to query baseline iterations info.");
    }
}

void ClusterService::QueryGroupInfo(
    const Protocol::MatrixGroupRequest &request, Protocol::MatrixGroupResponse &response) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    std::vector<GroupInfoDo> compareGroupList;
    if (database == nullptr || !database->GetGroups(compareGroupList)) {
        ServerLog::Warn("Fail to query compare group info.");
    }

    auto baselineDatabase =
        Timeline::DataBaseManager::Instance().GetClusterDatabase(BaselineManager::Instance().GetBaseLineClusterPath());
    std::vector<GroupInfoDo> baselineGroupList;
    if (request.params.isCompare) {
        if (baselineDatabase == nullptr || !baselineDatabase->GetGroups(baselineGroupList)) {
            ServerLog::Warn("Fail to query baseline group info.");
        }
    }
    // 合并通信域列表
    response.body.groupList = MergeGroupInfo(request, compareGroupList, baselineGroupList);
}

std::vector<OpTypeStatistics> ClusterService::GetOpTypeStatByStepId(
    const std::string &stepId, const std::string &clusterPath) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    if (database == nullptr) {
        return {};
    }
    return database->GetOpStatByStepId(stepId);
}

std::vector<Protocol::GroupInfo> ClusterService::MergeGroupInfo(const Protocol::MatrixGroupRequest &request,
    std::vector<GroupInfoDo> &compareGroupList, std::vector<GroupInfoDo> &baselineGroupList) {
    // 对compare数据和baseline通信域数据进行匹配
    // 1.如果两份数据都具备pgName，则直接根据pgName和rankSet进行直接匹配
    // 2.如果存在一份数据没有pgName，则需要通过算子类型进行匹配
    std::map<std::string, GroupInfoDo> baselineGroupInfoMap;
    std::map<std::string, GroupInfoDo> compareGroupInfoMap;
    if (IsHavePgName(compareGroupList) && IsHavePgName(baselineGroupList)) {
        for (const auto &item : compareGroupList) {
            std::string key = item.pgName + underline + item.rankSet;
            compareGroupInfoMap[key] = item;
        }
        for (const auto &item : baselineGroupList) {
            std::string key = item.pgName + underline + item.rankSet;
            baselineGroupInfoMap[key] = item;
        }
    } else {
        // 获取所有的每个通信域下有哪些类型的通信算子
        std::vector<OpTypeStatistics> compareStats =
            GetOpTypeStatByStepId(request.params.iterationId, request.params.clusterPath);
        std::vector<OpTypeStatistics> baselineStats = GetOpTypeStatByStepId(
            request.params.baselineIterationId, BaselineManager::Instance().GetBaseLineClusterPath());
        // 使用rankset和该通信域下算子的类型作为唯一key，用于后续合并操作
        baselineGroupInfoMap = GetRankSetAndOpTypeToGroupInfoMap(baselineStats, baselineGroupList);
        compareGroupInfoMap = GetRankSetAndOpTypeToGroupInfoMap(compareStats, compareGroupList);
    }
    std::vector<Protocol::GroupInfo> res = MergeGroupInfoWithPgName(compareGroupInfoMap, baselineGroupInfoMap);
    return res;
}

std::map<std::string, GroupInfoDo> ClusterService::GetRankSetAndOpTypeToGroupInfoMap(
    const std::vector<OpTypeStatistics> &StatsList, const std::vector<GroupInfoDo> &groupList) {
    std::map<std::string, GroupInfoDo> groupIdHashToRankSetMap;
    for (const auto &item : groupList) {
        // 正常解析数据 group id hash必不为空，为空则说明缺失了矩阵和通信耗时的数据
        if (item.groupIdHash.empty()) {
            continue;
        }
        groupIdHashToRankSetMap[item.groupIdHash] = item;
    }
    std::map<std::string, std::vector<std::string>> compareOpTypeMap;
    for (const auto &item : StatsList) {
        compareOpTypeMap[item.groupIdHash].push_back(item.opType);
    }
    std::map<std::string, GroupInfoDo> rankSetAndOpTypeToGroupInfoMap;
    for (auto &item : groupIdHashToRankSetMap) {
        std::string key;
        auto it = compareOpTypeMap.find(item.first);
        if (it == compareOpTypeMap.end()) {
            // 没有对应的算子信息
            key = item.second.rankSet;
        } else {
            std::sort(it->second.begin(), it->second.end());
            std::string opTypeListStr = StringUtil::join(it->second, "_");
            key = item.second.rankSet + "#" + opTypeListStr;
        }
        rankSetAndOpTypeToGroupInfoMap[key] = item.second;
    }
    return rankSetAndOpTypeToGroupInfoMap;
}

std::vector<Protocol::GroupInfo> ClusterService::MergeGroupInfoWithPgName(
    std::map<std::string, GroupInfoDo> &compareGroupMap, std::map<std::string, GroupInfoDo> &baselineGroupMap) {
    std::set<std::string> keyList;
    for (const auto &item : compareGroupMap) {
        keyList.insert(item.first);
    }
    for (const auto &item : baselineGroupMap) {
        keyList.insert(item.first);
    }

    std::vector<Protocol::GroupInfo> res;
    for (const auto &item : keyList) {
        Protocol::GroupInfo groupInfo;
        auto compareIt = compareGroupMap.find(item);
        if (compareIt != compareGroupMap.end()) {
            groupInfo.groupIdHash.compare = compareIt->second.groupIdHash;
            groupInfo.parallelStrategy = compareIt->second.pgName;
            groupInfo.group = compareIt->second.rankSet;
        }
        auto baselineIt = baselineGroupMap.find(item);
        if (baselineIt != baselineGroupMap.end()) {
            groupInfo.groupIdHash.baseline = baselineIt->second.groupIdHash;
            groupInfo.parallelStrategy = baselineIt->second.pgName;
            groupInfo.group = baselineIt->second.rankSet;
        }
        res.push_back(groupInfo);
    }
    return res;
}

bool ClusterService::IsHavePgName(const std::vector<GroupInfoDo> &groupList) {
    // 如果无数据，则返回true，此时与对比的另一份为准
    if (groupList.empty()) {
        return true;
    }
    bool res = false;
    for (const auto &item : groupList) {
        // p2p没有pgName，因此不能依据p2p来
        if (item.rankSet != "p2p") {
            res = !item.pgName.empty();
            break;
        }
    }
    return res;
}

void ClusterService::MergeMatrixInfo(Protocol::MatrixListResponseBody &body, const std::vector<MatrixInfoDo> &compare,
    const std::vector<MatrixInfoDo> &baseline) {
    std::set<std::string> keySet;
    std::map<std::string, MatrixInfoDo> compareMap;
    for (const auto &item : compare) {
        std::string key = std::to_string(item.srcRank) + underline + std::to_string(item.dstRank);
        keySet.insert(key);
        compareMap[key] = item;
    }
    std::map<std::string, MatrixInfoDo> baselineMap;
    for (const auto &item : baseline) {
        std::string key = std::to_string(item.srcRank) + underline + std::to_string(item.dstRank);
        keySet.insert(key);
        baselineMap[key] = item;
    }

    for (const auto &key : keySet) {
        Protocol::MatrixList matrix;
        std::vector<std::string> srcAndDst = StringUtil::Split(key, underline);
        if (srcAndDst.size() != matrixPointNumber) {
            continue;
        }
        matrix.srcRank = StringUtil::StringToInt(srcAndDst[0]);
        matrix.dstRank = StringUtil::StringToInt(srcAndDst[1]);
        if (compareMap.count(key) != 0) {
            matrix.matrixData.compare = ClusterCovert::CovertMatrixDoToInfo(compareMap[key]);
        }

        if (baselineMap.count(key) != 0) {
            matrix.matrixData.baseline = ClusterCovert::CovertMatrixDoToInfo(baselineMap[key]);
        }
        matrix.matrixData.diff = matrix.matrixData.compare - matrix.matrixData.baseline;

        body.matrixList.push_back(matrix);
    }
}

void ClusterService::QueryMatrixInfo(Protocol::MatrixBandwidthParam &params, Protocol::MatrixListResponseBody &body) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    std::vector<MatrixInfoDo> compareMatrixList;
    std::vector<MatrixInfoDo> baselineMatrixList;
    Protocol::MatrixBandwidthParam compareParams{
        params.stage, params.operatorName, params.iterationId, params.pgName, params.groupIdHash};
    if (database == nullptr || !database->QueryMatrixList(compareParams, compareMatrixList)) {
        ServerLog::Error("Failed to get compare matrix response data.");
    }

    if (params.isCompare) {
        Protocol::MatrixBandwidthParam baselineParams{
            params.stage, params.operatorName, params.baselineIterationId, params.pgName, params.baselineGroupIdHash};
        auto baselineDatabase = Timeline::DataBaseManager::Instance().GetClusterDatabase(
            BaselineManager::Instance().GetBaseLineClusterPath());
        if (baselineDatabase == nullptr || !baselineDatabase->QueryMatrixList(baselineParams, baselineMatrixList)) {
            ServerLog::Error("Failed to get baseline matrix response data.");
        }
    }

    MergeMatrixInfo(body, compareMatrixList, baselineMatrixList);
}

void ClusterService::MergeOperatorList(Protocol::OperatorListsResponseBody &body,
    const std::vector<OperatorTimeDo> &compare, const std::vector<OperatorTimeDo> &baseline,
    const std::string &operatorName) {
    auto numericStringCompare = [](const std::string &num1, const std::string &num2) {
        return StringUtil::StringToInt(num1) < StringUtil::StringToInt(num2);
    };
    std::set<std::string, decltype(numericStringCompare)> rankList(numericStringCompare);
    std::map<std::string, std::vector<Protocol::OperatorTimeItem>> compareRankToOperator;
    for (const auto &item : compare) {
        Protocol::OperatorTimeItem operatorTime = ClusterCovert::CovertDoToOperatorTime(item);
        compareRankToOperator[item.rankId].push_back(operatorTime);
        body.minTime = std::min(body.minTime, operatorTime.startTime);
        body.maxTime = std::max(body.maxTime, operatorTime.startTime + operatorTime.elapseTime);
        rankList.insert(item.rankId);
    }
    std::map<std::string, std::vector<Protocol::OperatorTimeItem>> baselineRankToOperator;
    for (const auto &item : baseline) {
        Protocol::OperatorTimeItem operatorTime = ClusterCovert::CovertDoToOperatorTime(item);
        baselineRankToOperator[item.rankId].push_back(operatorTime);
        body.minTime = std::min(body.minTime, operatorTime.startTime);
        body.maxTime = std::max(body.maxTime, operatorTime.startTime + operatorTime.elapseTime);
        rankList.insert(item.rankId);
    }

    for (const auto &item : rankList) {
        body.rankLists.push_back(item);
        Dic::Protocol::CompareData<std::vector<Protocol::OperatorTimeItem>> data;
        if (compareRankToOperator.count(item) != 0) {
            data.compare = compareRankToOperator[item];
            std::sort(data.compare.begin(), data.compare.end(), Protocol::OperatorTimeItem::SortByTime);
        }
        if (baselineRankToOperator.count(item) != 0) {
            data.baseline = baselineRankToOperator[item];
            std::sort(data.baseline.begin(), data.baseline.end(), Protocol::OperatorTimeItem::SortByTime);
        }
        body.opLists.push_back(data);
    }
    body.AdjustTime(operatorName);
}

void ClusterService::QueryOperatorList(
    Protocol::DurationListParams &params, Protocol::OperatorListsResponseBody &body) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    std::vector<OperatorTimeDo> compareOperatorTimeList;
    std::vector<OperatorTimeDo> baselineOperatorTimeList;
    Protocol::DurationListParams compareParams(params);
    if (database == nullptr || !database->QueryOperatorList(compareParams, compareOperatorTimeList)) {
        ServerLog::Error("Failed to get compare operator list response data.");
    }

    if (params.isCompare) {
        Protocol::DurationListParams baselineParams(params);
        baselineParams.iterationId = params.baselineIterationId;
        baselineParams.groupIdHash = params.baselineGroupIdHash;
        auto baselineDatabase = Timeline::DataBaseManager::Instance().GetClusterDatabase(
            BaselineManager::Instance().GetBaseLineClusterPath());
        if (baselineDatabase == nullptr ||
            !baselineDatabase->QueryOperatorList(baselineParams, baselineOperatorTimeList)) {
            ServerLog::Error("Failed to get baseline operator response data.");
        }
    }

    MergeOperatorList(body, compareOperatorTimeList, baselineOperatorTimeList, params.targetOperatorName);
    for (const auto &item : body.rankLists) {
        std::string traceDb =
            FullDb::TrackInfoManager::Instance().GetFileIdByClusterDbAndRankId(params.clusterPath, item);
        body.dbPathList.push_back(traceDb);
    }
}

void ClusterService::MergeDurationData(Protocol::DurationListsResponseBody &body, std::vector<DurationDo> &compare,
    std::vector<DurationDo> &baseline, const std::string &clusterPath) {
    std::set<std::string> rankIdSet;
    std::map<std::string, Protocol::DurationData> compareMap;
    for (const auto &item : compare) {
        compareMap[item.rankId] = ClusterCovert::CovertDoToDuration(item);
        rankIdSet.insert(item.rankId);
    }
    std::map<std::string, Protocol::DurationData> baselineMap;
    for (const auto &item : baseline) {
        baselineMap[item.rankId] = ClusterCovert::CovertDoToDuration(item);
        rankIdSet.insert(item.rankId);
    }

    for (const auto &item : rankIdSet) {
        Protocol::Duration duration;
        duration.rankId = item;
        duration.dbPath = FullDb::TrackInfoManager::Instance().GetFileIdByClusterDbAndRankId(clusterPath, item);
        if (compareMap.count(item) != 0) {
            duration.durationData.compare = compareMap[item];
        }
        if (baselineMap.count(item) != 0) {
            duration.durationData.baseline = baselineMap[item];
        }
        duration.durationData.diff = duration.durationData.compare - duration.durationData.baseline;
        body.durationList.push_back(duration);
    }
}

void ClusterService::StatisticBandwidthData(const DurationDo &item, std::vector<Protocol::BandwidthStatistic> &bwStat) {
    for (auto &one : bwStat) {
        if (one.type == "SDMA") {
            one.maxBw = std::max(one.maxBw, item.sdmaBw);
            one.minBw = std::min(one.minBw, item.sdmaBw);
            one.avgBw += item.sdmaBw;
            one.allTime += item.sdmaTime;
        } else {
            one.maxBw = std::max(one.maxBw, item.rdmaBw);
            one.minBw = std::min(one.minBw, item.rdmaBw);
            one.avgBw += item.rdmaBw;
            one.allTime += item.rdmaTime;
        }
    }
}

void ClusterService::GetBandwidthStatisticResult(
    std::vector<Protocol::BandwidthStatistic> &bwStat, Protocol::DurationListsResponseBody &responseBody) {
    if (responseBody.durationList.empty()) {
        return;
    }
    int digit = 4;
    for (auto &item : bwStat) {
        if (item.avgBw == 0) {
            continue;
        }
        item.avgBw = NumberUtil::DoubleReservedNDigits(item.avgBw / responseBody.durationList.size(), digit);
        if (item.minBw != DBL_MAX) {
            item.diffBw = NumberUtil::DoubleReservedNDigits(item.maxBw - item.minBw, digit);
        }
        item.maxBw = NumberUtil::DoubleReservedNDigits(item.maxBw, digit);
        item.minBw = NumberUtil::DoubleReservedNDigits(item.minBw, digit);
        item.allTime = NumberUtil::DoubleReservedNDigits(item.allTime, digit);
        responseBody.bwStatistics.emplace_back(item);
    }
}

void ClusterService::CalBandwidthData(
    Protocol::DurationListsResponseBody &body, const std::vector<DurationDo> &durationDoList) {
    std::vector<Protocol::BandwidthStatistic> bwStat = {{"SDMA", 0, 0, DBL_MAX, 0, 0}, {"RDMA", 0, 0, DBL_MAX, 0, 0}};
    for (const auto &item : durationDoList) {
        StatisticBandwidthData(item, bwStat);
    }
    GetBandwidthStatisticResult(bwStat, body);
}

void ClusterService::QueryDurationList(
    Protocol::DurationListParams &params, Protocol::DurationListsResponseBody &body) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    std::vector<DurationDo> compareDurationDoList;
    std::vector<DurationDo> baselineDurationDoList;
    Protocol::DurationListParams compareParams(params);
    if (database == nullptr || !database->QueryDurationList(compareParams, compareDurationDoList)) {
        ServerLog::Error("Failed to get compare during list response data.");
    }

    if (params.isCompare) {
        Protocol::DurationListParams baselineParams(params);
        baselineParams.iterationId = params.baselineIterationId;
        baselineParams.groupIdHash = params.baselineGroupIdHash;
        auto baselineDatabase = Timeline::DataBaseManager::Instance().GetClusterDatabase(
            BaselineManager::Instance().GetBaseLineClusterPath());
        if (baselineDatabase == nullptr ||
            !baselineDatabase->QueryDurationList(baselineParams, baselineDurationDoList)) {
            ServerLog::Error("Failed to get baseline during response data.");
        }
    }

    MergeDurationData(body, compareDurationDoList, baselineDurationDoList, params.clusterPath);
    CalBandwidthData(body, compareDurationDoList);
}

bool ClusterService::AnalyzeCommunicationSlowRanks(
    const Protocol::DurationListParams &params, CommunicationSlowRankAnalysisResponseBody &body) {
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(params.clusterPath);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection for analyze communication slow rank list.");
        SetCommunicationError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    if (!CheckOpNameList(params, database)) {
        body.hasAdvice = false;
        return true;
    }
    body.hasAdvice = true;

    // 异常卡定位
    RankDetailsForSlowRank fastestRank;
    FindSlowRankByCommDuration(database, params, fastestRank, body);
    if (body.slowRankList.empty()) {
        return true;
    }

    // 异常算子定位
    for (auto &slowRank : body.slowRankList) {
        if (!database->QuerySlowOpByCommDuration(params, fastestRank.rankId, slowRank)) {
            SetCommunicationError(ErrorCode::QUERY_SLOW_OPERATOR_FAILED);
            return false;
        }
    }
    return true;
}

void ClusterService::FindSlowRankByCommDuration(const std::shared_ptr<VirtualClusterDatabase> &database,
    const Protocol::DurationListParams &params, RankDetailsForSlowRank &fastestRank,
    CommunicationSlowRankAnalysisResponseBody &body) {
    std::vector<CommInfoUnderRank> commTimeForRankDim = database->GetCommTimeForRankDim(params.iterationId);
    std::set<RankDetailsForSlowRank> rankDetails;
    for (const auto &commInfo : commTimeForRankDim) {
        if (commInfo.groupIdHash == params.groupIdHash) {
            rankDetails.insert({commInfo.rankId, 0.0, commInfo.commTime, {}});
        }
    }
    if (rankDetails.size() <= 1) {
        // 当前通信域内不足2张卡，不可能存在快慢卡
        ServerLog::Warn("Not enough communication time info for analyze communication slow rank list.");
        return;
    }
    // get fastest rank and top N slow rank
    fastestRank = *rankDetails.begin();
    body.fastRankId = fastestRank.rankId;
    body.fastTotalElapseTime = fastestRank.totalElapseTime;
    int cnt = 0;
    double thresholdComm = thresholdForSlowRank * fastestRank.totalElapseTime;
    for (auto it = rankDetails.rbegin(); it != rankDetails.rend() && cnt < slowRankCnt; ++it, ++cnt) {
        // check threshold for slow rank
        double commTimeDiff = fastestRank.totalElapseTime - it->totalElapseTime;
        if (commTimeDiff >= thresholdComm) {
            RankDetailsForSlowRank rankDetail;
            rankDetail.rankId = it->rankId;
            rankDetail.totalDiffTime = NumberUtil::DoubleReservedNDigits(commTimeDiff, doubleReservedNum);
            rankDetail.totalElapseTime = NumberUtil::DoubleReservedNDigits(it->totalElapseTime, doubleReservedNum);
            body.slowRankList.emplace_back(rankDetail);
        }
    }
}

// 若校验失败结果为false，则不返回慢卡专家建议
bool ClusterService::CheckOpNameList(
    const Protocol::DurationListParams &params, const std::shared_ptr<VirtualClusterDatabase> &database) {
    if (params.operatorName != totalOpInfo || params.pgName == ppPgName) {
        return false;
    }
    Protocol::OperatorNamesParams queryOpNameParams;
    queryOpNameParams.iterationId = params.iterationId;
    queryOpNameParams.stage = params.stage;
    queryOpNameParams.pgName = params.pgName;
    queryOpNameParams.groupIdHash = params.groupIdHash;
    std::vector<OperatorNamesObject> opNameList;
    if (!database->QueryOperatorNames(queryOpNameParams, opNameList)) {
        ServerLog::Error("Failed to query operator names for analyze communication slow rank list.");
        return false;
    }
    // p2p, all2allv
    const std::vector<std::string> opKey = {"send", "receive", "recv", "all2allv", "alltoallv"};
    for (const auto &name : opNameList) {
        std::string opNameLower = StringUtil::ToLower(name.operatorName);
        for (const auto &key : opKey) {
            if (opNameLower.find(key) != std::string::npos) {
                return false;
            }
        }
    }
    return true;
}
}
}
}
