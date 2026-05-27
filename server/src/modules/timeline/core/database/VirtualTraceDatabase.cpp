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

#include <cmath>
#include "CommonDefs.h"
#include "ServerLog.h"
#include "MetaDataCacheManager.h"
#include "VirtualTraceDatabase.h"
#include "TrackInfoManager.h"
#include "RepositoryFactory.h"
#include "FullDbEnumUtil.h"
#include "TraceTime.h"
#include "SliceAnalyzer.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;
bool VirtualTraceDatabase::QueryGroupedAscendHardwareThreadsByModelId(std::vector<ThreadGroup> &threadGroupList) {
    std::map<std::string, std::string> tId2ModelIdMap = QueryAllModelIdOfAscendHardwareThreads();
    // 检查是否有数据
    if (tId2ModelIdMap.empty()) {
        threadGroupList.clear();
        return false; // 没有数据，返回失败
    }

    // 创建一个临时映射：ModelID -> 线程ID列表
    std::map<std::string, ThreadGroup> modelIdToThreadsMap;
    // 定义需要跳过的特殊值
    const std::string emptyModelId = "";
    const std::string uintMaxModelId = std::to_string(UINT_MAX); // 通常为"4294967295"

    // 遍历所有线程，按ModelID分组
    for (const auto &[threadId, modelId] : tId2ModelIdMap) {
        if (modelId == emptyModelId || modelId == uintMaxModelId) {
            ServerLog::Warn("Invalid ModelId when querying grouped ascend hardware threads.");
            continue;
        }
        // 如果这个modelId还没有对应的ThreadGroup，创建一个新的
        if (modelIdToThreadsMap.find(modelId) == modelIdToThreadsMap.end()) {
            const ThreadGroup group;
            modelIdToThreadsMap[modelId] = group;
        }

        // 将线程ID转换为字符串并添加到对应ModelID的组中
        modelIdToThreadsMap[modelId].push(threadId);
    }

    // 清空输出参数
    threadGroupList.clear();
    threadGroupList.reserve(modelIdToThreadsMap.size());

    // 将map中的所有ThreadGroup添加到输出列表
    for (const auto &[modelId, threadGroup] : modelIdToThreadsMap) {
        threadGroupList.push_back(threadGroup);
    }

    return true; // 操作成功
}

uint64_t VirtualTraceDatabase::CalculateUncoveredTime(
    const std::vector<Protocol::ThreadTraces> &uncovered, size_t &index, const ThreadTraces &element) {
    uint64_t totalUncoveredTime = 0;
    if (uncovered.empty() || index >= uncovered.size()) {
        return totalUncoveredTime;
    }
    // sql语句能够保证uncovered按start_time升序排列
    while (index < uncovered.size()) {
        Protocol::ThreadTraces uncoveredEle = uncovered.at(index);
        // 未掩盖部分的分片小于通信Op或Task时，二者无交集，需要跳到下一个未掩盖部分的分片
        if (element.startTime >= uncoveredEle.endTime) {
            index++;
            continue;
        }
        // 未掩盖部分的分片大于通信Op或Task时，二者也无交集，退出循环
        if (element.endTime <= uncoveredEle.startTime) {
            break;
        }
        // 二者有交集时，取其交集部分，就是通信Op或Task真实的未掩盖部分
        uint64_t startMax = uncoveredEle.startTime > element.startTime ? uncoveredEle.startTime : element.startTime;
        uint64_t endMin = uncoveredEle.endTime > element.endTime ? element.endTime : uncoveredEle.endTime;
        uint64_t uncoveredTime = endMin >= startMax ? endMin - startMax : 0;

        if (UINT64_MAX - totalUncoveredTime > uncoveredTime) {
            totalUncoveredTime += uncoveredTime;
        } else {
            // 实际数据很小，正常情况下不会溢出
            ServerLog::Error("Accumulation overflow occurred when calculating total uncovered time: ", uncoveredTime);
            totalUncoveredTime += 0;
        }
        if (element.endTime > uncoveredEle.endTime) {
            index++;
        } else {
            break;
        }
    }
    return totalUncoveredTime;
}

void VirtualTraceDatabase::ExecuteQueryCommunicationSummaryData(
    std::map<std::string, Protocol::CommunicationSummaryInfoByGroup> &summaryInfoMap,
    const std::unique_ptr<SqliteResultSet> &resultSet, const std::map<std::string, std::string> &groupInfoMap,
    const std::vector<Protocol::ThreadTraces> &uncovered) {
    size_t index = 0;
    while (resultSet->Next()) {
        Protocol::ThreadTraces ele = {.name = resultSet->GetString("name"),
            .duration = resultSet->GetUint64("duration"),
            .startTime = resultSet->GetUint64("startTime"),
            .endTime = resultSet->GetUint64("endTime"),
            .depth = resultSet->GetUint32("type"), // Use to save "type" temporarily
            .threadId = std::to_string(resultSet->GetInt64("plane")),
            .pid = std::to_string(resultSet->GetUint64("groupName")),
            .id = ele.pid + "@" + ele.threadId,
            .cname = resultSet->GetString("threadName")};
        uint64_t flag = resultSet->GetUint64("flag");
        if (groupInfoMap.find(ele.id) == groupInfoMap.end()) {
            continue;
        }
        const std::string &group = groupInfoMap.at(ele.id);
        // SQL查询保证数据按通信组排序，不同通信组检查是否通信未掩盖,从0开始
        if (summaryInfoMap.count(group) == 0) {
            CommunicationSummaryInfoByGroup tmp = {group, {group, "", "", 0, 0, 0, 0}, {}};
            summaryInfoMap.emplace(group, tmp);
            index = 0;
        }
        CommunicationSummaryInfoByGroup *groupInfo = &summaryInfoMap.at(group);
        // 此处depth临时用来标识通信类型，1为通信算子，0为通信task
        if (ele.depth == 0 && groupInfo->taskMap.find(ele.threadId) == groupInfo->taskMap.end()) {
            CommunicationSummaryInfoByThread newPlane = {ele.cname, ele.pid, ele.threadId, 0, 0, 0, 0};
            groupInfo->taskMap.emplace(ele.threadId, newPlane);
            index = 0;
        } else if (ele.depth == 1 && summaryInfoMap.at(group).op.completeTransmitTime == 0) {
            groupInfo->op.group = ele.pid;
            groupInfo->op.plane = ele.threadId;
            index = 0;
        }
        uint64_t uncoveredTime = CalculateUncoveredTime(uncovered, index, ele);
        // 此处depth临时用来标识通信类型，1为通信算子，0为通信task
        if (ele.depth == 0) {
            // 上文逻辑能够保证，一定能找的到
            groupInfo->taskMap.at(ele.threadId).UpdateData(flag == 1, ele.duration, uncoveredTime);
        } else {
            groupInfo->op.group = ele.pid;
            groupInfo->op.plane = ele.threadId;
            groupInfo->op.UpdateData(false, ele.duration, uncoveredTime);
        }
    }
}

/**
 * 兼容Text场景和DB场景的Group Name，其中Text场景为"Group {groupNameValue} Communication"，DB场景为{groupNameValue}
 * 解开 "Group {groupNameValue} Communication" 的形式，获取 {groupNameValue}
 * @param str "Group {groupNameValue} Communication"
 * @return {groupNameValue}
 */
std::string VirtualTraceDatabase::ExtractGroupNameValue(const std::string &str) {
    // 静态初始化正则表达式，确保只编译一次
    static const std::regex expr(R"(Group ([\S]+(\s\w*)?) Communication)");

    std::smatch match;
    if (std::regex_match(str, match, expr) && match.size() > 1) {
        // 获取第一个匹配项（即 groupNameValue）
        return match.str(1);
    }
    return "";
}

SystemViewOverallRes VirtualTraceDatabase::CollectCommunicationGroupMetrics(
    const CommunicationSummaryInfoByGroup &data, SystemViewOverallHelper &overallHelper) {
    Protocol::SystemViewOverallRes group = {.totalTime = 0,
        .ratio = 0,
        .nums = 0,
        .avg = 0,
        .max = 0,
        .min = 0,
        .name = data.groupName,
        .children = {},
        .level = 2, // level 2
        .id = std::to_string(overallHelper.idCounter++)};
    // 获取 {groupNameValue}, 因为MetaDataCacheManager以groupNameValue作为键值
    std::string groupNameValue = ExtractGroupNameValue(group.name);

    // 此处进行了一个拆分判断，是由于数据中可能存在将一个通信甬道拆分成两个的情况（mc2算子）
    // 这个场景会在mc2算子的aicpu通信辅流加上“Aicpu”进行区分，需要将该字段去掉才能与metadata中通信域的数据相匹配
    std::vector<std::string> groupNameSplit = StringUtil::Split(groupNameValue, " ");
    std::string normalizedGroupNameValue = groupNameSplit.size() > 1 ? groupNameSplit[0] : groupNameValue;
    auto groupInfoOpt = MetaDataCacheManager::Instance().GetParallelGroupInfo(normalizedGroupNameValue);
    if (groupInfoOpt.has_value() && !groupInfoOpt.value().groupName.empty()) {
        group.name = groupInfoOpt.value().groupName + ":" + data.groupName;
    }
    group.totalTime = NumberUtil::DoubleReservedNDigits(data.op.uncoveredTransmitTime * NS_TO_US, TWO);
    group.ratio =
        NumberUtil::DoubleReservedNDigits(group.totalTime / overallHelper.e2eTime * PERCENTAGE_RATIO_SCALE, TWO);
    return group;
}

void VirtualTraceDatabase::ComputeCommunicationWaitAndTransmitTimeByGroup(
    const std::map<std::string, CommunicationSummaryInfoByGroup> &summaryData, SystemViewOverallHelper &overallHelper,
    Protocol::SystemViewOverallRes &result) {
    if (summaryData.empty() || overallHelper.e2eTime <= 0) {
        return;
    }
    for (auto &item : summaryData) {
        CommunicationSummaryInfoByGroup data = item.second;
        SystemViewOverallRes group = CollectCommunicationGroupMetrics(data, overallHelper);
        Protocol::SystemViewOverallRes wait = {.totalTime = 0,
            .ratio = 0,
            .nums = 0,
            .avg = 0,
            .max = 0,
            .min = 0,
            .name = WAIT_TIME,
            .children = {},
            .level = 3, // level 3
            .id = std::to_string(overallHelper.idCounter++)};
        uint64_t minWait = UINT64_MAX;
        for (auto &tmpItem : data.taskMap) {
            minWait = std::min(minWait, tmpItem.second.uncoveredWaitTime);
        }
        wait.totalTime = NumberUtil::DoubleReservedNDigits(minWait * NS_TO_US, TWO);
        wait.ratio =
            NumberUtil::DoubleReservedNDigits(wait.totalTime / overallHelper.e2eTime * PERCENTAGE_RATIO_SCALE, TWO);
        Protocol::SystemViewOverallRes transmit = {.totalTime = 0,
            .ratio = 0,
            .nums = 0,
            .avg = 0,
            .max = 0,
            .min = 0,
            .name = TRANSMIT_TIME,
            .children = {},
            .level = 3, // level 3
            .id = std::to_string(overallHelper.idCounter++)};
        if (data.op.uncoveredTransmitTime > minWait) {
            transmit.totalTime =
                NumberUtil::DoubleReservedNDigits((data.op.uncoveredTransmitTime - minWait) * NS_TO_US, TWO);
        }
        transmit.ratio =
            NumberUtil::DoubleReservedNDigits(transmit.totalTime / overallHelper.e2eTime * PERCENTAGE_RATIO_SCALE, TWO);
        group.children.emplace_back(wait);
        group.children.emplace_back(transmit);
        result.children.emplace_back(group);
    }
}

std::vector<UnitCounterData> VirtualTraceDatabase::DownSampleUnitCounterData(
    const std::vector<UnitCounterData> &dataList, size_t targetSize) {
    if (targetSize == 0) {
        return dataList;
    }

    std::vector<UnitCounterData> sampledData;
    if (dataList.empty()) {
        return sampledData;
    }

    size_t totalSize = dataList.size();
    if (totalSize <= targetSize) {
        // 数据量本来就小，不用采样
        return dataList;
    }

    // 每个桶的步长
    double step = static_cast<double>(totalSize) / targetSize;

    sampledData.reserve(targetSize);

    for (size_t i = 0; i < targetSize; ++i) {
        size_t index = static_cast<size_t>(i * step);
        if (index >= totalSize) {
            index = totalSize - 1;
        }
        sampledData.push_back(dataList[index]);
    }

    return sampledData;
}

SliceQuery VirtualTraceDatabase::CreateSliceQueryWithTimeRange(const SliceBaseInfo &sliceInfo) {
    auto curTrackId = TrackInfoManager::Instance().GetTrackId(sliceInfo.rankId, sliceInfo.pid, sliceInfo.tid);
    SliceQuery sliceQuery;
    sliceQuery.trackId = curTrackId;
    sliceQuery.pid = sliceInfo.pid;
    sliceQuery.tid = sliceInfo.tid;
    sliceQuery.rankId = sliceInfo.rankId;
    auto metaTypeEnum = STR_TO_ENUM<PROCESS_TYPE>(sliceInfo.metaType);
    if (metaTypeEnum.has_value()) {
        sliceQuery.metaType = metaTypeEnum.value();
    }
    sliceQuery.minTimestamp = TraceTime::Instance().GetStartTime();
    sliceQuery.startTime = sliceInfo.startTime;
    sliceQuery.endTime = sliceInfo.startTime + sliceInfo.duration;
    SliceQuery newSliceQuery = SliceCacheManager::GetSlicePagedQueryForDb(sliceQuery);
    return newSliceQuery;
}

uint64_t VirtualTraceDatabase::GetSliceDepthForJump(const SliceQuery &params, uint64_t sliceId) {
    SliceAnalyzer sliceAnalyzer;
    auto repositoryFactory = RepositoryFactory::Instance();
    auto repo = repositoryFactory->GetSliceRespo(params.metaType);
    if (repo == nullptr) {
        return 0;
    }
    sliceAnalyzer.SetRepository(repo);
    std::unordered_map<uint64_t, uint32_t> depthCache;
    sliceAnalyzer.ComputeDepthInfoByTrackId(params, depthCache);
    return depthCache[sliceId];
}
}
