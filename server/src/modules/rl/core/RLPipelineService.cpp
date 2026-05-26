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
#include "RLPipelineService.h"
#include "DataBaseManager.h"
#include "RenderEngine.h"
#include "NumberSafeUtil.h"
#include "TrackInfoManager.h"
#include "RLMstxConfigManager.h"
#include "ParserStatusManager.h"
#include "RLMicroBatchClassifierFactory.h"

namespace Dic::Module::RL {
using namespace Dic::Module::Timeline;
void RLPipelineService::Clear() {
    std::lock_guard<std::mutex> lock(mtx);
    minTime = UINT64_MAX;
    maxTime = 0;
    stageTypeList.clear();
    taskPipelineMap.clear();
    microBatchPipelineMap.clear();
    rlBackEndType = RLBackEndType::Unknown;
    framework.clear();
}

std::vector<Protocol::RLPipelineNode> RLPipelineService::SearchNode(const std::string &rankId) {
    // 查找mstx数据
    FullDb::DataType dataType = Timeline::DataBaseManager::Instance().GetDataTypeByRank(rankId);
    std::vector<std::string> taskNameList = RLMstxConfigManager::Instance().GetMstxTaskNameList();
    std::vector<FullDb::CompeteSliceDomain> mstxSliceList =
        FullDb::RenderEngine::Instance()->QueryMstxRLDetail(rankId, dataType, taskNameList);
    if (mstxSliceList.empty()) {
        return {};
    }
    std::vector<Protocol::RLPipelineNode> res;
    std::set<std::string> stageTypeListTemp;
    uint64_t minTimeTemp = UINT64_MAX;
    uint64_t maxTimeTemp = 0;
    for (const auto &item : mstxSliceList) {
        uint64_t duration = NumberSafe::Sub(item.endTime, item.timestamp);
        std::string stageType = RLMstxConfigManager::Instance().GetTaskTypeByName(item.name);
        Protocol::RLPipelineNode node{rankId, "Task", item.timestamp, duration, item.name, stageType};
        res.push_back(node);

        stageTypeListTemp.insert(stageType);
        minTimeTemp = std::min(minTimeTemp, item.timestamp);
        maxTimeTemp = std::max(maxTimeTemp, item.endTime);
    }

    std::lock_guard<std::mutex> lock(mtx);
    stageTypeList.insert(stageTypeListTemp.begin(), stageTypeListTemp.end());
    minTime = std::min(minTimeTemp, minTime);
    maxTime = std::max(maxTimeTemp, maxTime);
    return res;
}

void RLPipelineService::FillPipelineMap(const std::string &originHostName, const std::string &rankId,
    const std::vector<Protocol::RLPipelineNode> &pipeline, std::unordered_map<std::string, RLPipelineItem> &targetMap) {
    std::string key = originHostName + " " + rankId;
    std::lock_guard<std::mutex> lock(mtx);
    auto taskIt = targetMap.find(key);
    if (taskIt == targetMap.end()) {
        RLPipelineItem taskPipelineItem{pipeline, rankId, originHostName};
        targetMap[key] = taskPipelineItem;
    } else {
        targetMap[key].lists.insert(targetMap[key].lists.end(), pipeline.begin(), pipeline.end());
    }
}

void RLPipelineService::QueryPipelineByRankId(const std::string &rankIdWithHost) {
    std::string fileId = FullDb::DataBaseManager::Instance().GetFileIdByRankId(rankIdWithHost);
    std::vector<std::string> rankIdWithHostAfterSplit = StringUtil::Split(rankIdWithHost, " ");
    // 获取host，如果rankIdWithHost按空格分割后长度大于1，则倒数第二个是host，否则赋值为空
    std::string hostName =
        rankIdWithHostAfterSplit.size() > 1 ? rankIdWithHostAfterSplit[rankIdWithHostAfterSplit.size() - 2] : "";
    std::string originHostName = StringUtil::GetOriginHostName(hostName);
    // 获取rankId，如果rankIdWithHost按空格分割后长度大于1，则最后一个是rankId，否则就是rankIdWithHost本身
    std::string rankId = rankIdWithHostAfterSplit.size() > 1
        ? rankIdWithHostAfterSplit[rankIdWithHostAfterSplit.size() - 1]
        : rankIdWithHost;

    // 查询任务维度的数据
    ServerLog::Warn("Start query the rankId:", rankId);
    std::vector<Protocol::RLPipelineNode> taskPipelineNodeList = SearchNode(rankIdWithHost);
    FillPipelineMap(originHostName, rankId, taskPipelineNodeList, taskPipelineMap);

    // 查询microBatch维度数据
    std::vector<std::string> taskNames;
    std::transform(taskPipelineNodeList.begin(), taskPipelineNodeList.end(), std::back_inserter(taskNames),
        [](const RLPipelineNode &node) { return node.name; });
    auto rlMstxConfig = RLMstxConfigManager::Instance().GetMstxConfigByTaskName(taskNames);
    framework = rlMstxConfig.framework;
    ServerLog::Warn("Start query microbatch, framework:", framework, ", size of task:", taskNames.size());
    std::vector<Protocol::RLPipelineNode> microBatchNodeList =
        QueryMicroBatchByTask(fileId, taskPipelineNodeList, rlMstxConfig);
    FillPipelineMap(originHostName, rankId, microBatchNodeList, microBatchPipelineMap);
}

bool RLPipelineService::GetPipelineInfo(Protocol::RLPipelineResponse &response) {
    Clear();
    static ThreadPool threadPool = ThreadPool(std::thread::hardware_concurrency());
    for (const auto &rankIdWithHost : FullDb::DataBaseManager::Instance().GetAllRankId()) {
        if (rlBackEndType == RLBackEndType::Unknown) {
            rlBackEndType = GetBackendType(rankIdWithHost);
        }
        threadPool.AddTask([this](std::string rankIdWithHost) { QueryPipelineByRankId(rankIdWithHost); },
            TraceIdManager::GetTraceId(), rankIdWithHost);
    }
    threadPool.WaitForAllTasks();

    std::lock_guard<std::mutex> lock(mtx);
    FillAndProcessPipelineData(taskPipelineMap, response.body.taskData);
    FillAndProcessPipelineData(microBatchPipelineMap, response.body.microBatchData);
    response.body.minTime = minTime;
    response.body.maxTime = maxTime;
    response.body.stageTypeList.insert(response.body.stageTypeList.end(), stageTypeList.begin(), stageTypeList.end());
    response.body.backendType = RLBackendToStr(rlBackEndType);
    response.body.framework = framework;
    return true;
}

std::vector<Protocol::RLPipelineNode> RLPipelineService::QueryMicroBatchByTask(
    const std::string &fileId, const std::vector<Protocol::RLPipelineNode> &taskList, const RLMstxConfig &taskConfig) {
    std::vector<Protocol::RLPipelineNode> microBatchNodeList;
    for (const auto &item : taskList) {
        auto microBatchUnderTask = QueryMicroBatch(fileId, taskConfig, item);
        std::transform(microBatchUnderTask.begin(), microBatchUnderTask.end(), std::back_inserter(microBatchNodeList),
            [](const auto &item) { return item; });
    }
    return microBatchNodeList;
}

void RLPipelineService::FillAndProcessPipelineData(
    std::unordered_map<std::string, RLPipelineItem> &pipelineMap, std::vector<RLPipelineItem> &pipelineData) {
    for (const auto &item : pipelineMap) {
        if (item.second.lists.empty()) {
            continue;
        }
        pipelineData.push_back(item.second);
    }
    std::sort(
        pipelineData.begin(), pipelineData.end(), [](const RLPipelineItem &pipelineA, const RLPipelineItem &pipelineB) {
            if (pipelineA.hostName != pipelineB.hostName) {
                return pipelineA.hostName > pipelineB.hostName;
            }
            if (StringUtil::IsAllDigits(pipelineA.rankId) && StringUtil::IsAllDigits(pipelineB.rankId)) {
                return StringUtil::StringToInt(pipelineA.rankId) > StringUtil::StringToInt(pipelineB.rankId);
            } else {
                return pipelineA.rankId > pipelineB.rankId;
            }
        });
}

std::vector<Protocol::RLPipelineNode> RLPipelineService::QueryMicroBatch(
    const std::string &fileId, const RLMstxConfig &config, const RLPipelineNode &node) {
    auto classifier = RLMicroBatchClassifierFactory::GetClassifier(rlBackEndType);
    if (classifier == nullptr) {
        return {};
    }
    return classifier->GetClassifiedMicroBatch(fileId, config, node);
}

RLPipelineService &RLPipelineService::Instance() {
    static RLPipelineService service;
    return service;
}

RLBackEndType RLPipelineService::GetBackendType(const std::string &rankId) {
    SliceQuery query;
    query.rankId = rankId;
    query.name = "FullyShardedDataParallel.forward";
    query.startTime = 0;
    query.endTime = std::numeric_limits<uint64_t>::max();
    PythonApiRepo apiRepo;
    std::vector<CompeteSliceDomain> res;
    apiRepo.QuerySliceByVagueNameAndTime(query, res);
    if (!res.empty()) {
        return RLBackEndType::FSDP;
    }
    return RLBackEndType::Megatron;
}
}
