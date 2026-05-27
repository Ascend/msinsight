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

#include <algorithm>
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "TraceDatabaseHelper.h"
#include "QueryFwdBwdTimelineHandler.h"

namespace Dic::Module::Summary {
using namespace Dic::Server;
using namespace Dic::Module::Timeline;
std::map<std::string, PipelineFwdBwdTimelineByRank> QueryFwdBwdTimelineHandler::dataMap;
bool QueryFwdBwdTimelineHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<PipelineFwdBwdTimelineRequest &>(*requestPtr.get());
    std::unique_ptr<PipelineFwdBwdTimelineResponse> responsePtr = std::make_unique<PipelineFwdBwdTimelineResponse>();
    PipelineFwdBwdTimelineResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);

    // check request parameter
    std::string err;
    if (!request.params.CheckParams(err)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::vector<std::string> rankIds = StringUtil::SplitStringWithParenthesesByComma(request.params.stageId);
    if (rankIds.empty()) {
        SetSummaryError(ErrorCode::GET_RANK_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    dataMap.clear();
    static ThreadPool threadPool = ThreadPool(4); // thread pool count 4
    for (auto const &rankId : rankIds) {
        response.body.rankLists.push_back(rankId);
        PipelineFwdBwdTimelineByRank rank = {rankId, {}, {}};
        dataMap.emplace(rankId, rank);
        threadPool.AddTask(QueryFwdBwdTimelineByRank, TraceIdManager::GetTraceId(), rankId, request.params.stepId,
            request.params.clusterPath);
    }

    threadPool.WaitForAllTasks();

    CalFlowInfo(response.body.flowList, rankIds);
    // collect all data
    for (auto const &rankId : rankIds) {
        response.body.rankDataList.push_back(dataMap[rankId]);
    }

    // calculate max/min timestamp, < 100 ranks, controllable time consumption
    for (auto &rank : response.body.rankDataList) {
        if (rank.componentDataList.empty()) {
            continue; // empty rank
        }
        for (auto &component : rank.componentDataList) {
            if (component.traceList.empty()) {
                continue; // empty component
            }
            auto first = component.traceList.at(0);
            auto last = component.traceList.at(component.traceList.size() - 1);
            response.body.maxTime = std::max(response.body.maxTime, last.startTime + last.duration);
            response.body.minTime = std::min(response.body.minTime, first.startTime);
        }
    }
    SendResponse(std::move(responsePtr), true);
    dataMap.clear();
    return true;
}

bool QueryFwdBwdTimelineHandler::QueryFwdBwdTimelineByRank(
    const std::string &rankId, const std::string &stepId, const std::string &clusterPath) {
    if (dataMap.find(rankId) == dataMap.end()) {
        return false;
    }
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(rankId);
    if (database == nullptr) {
        // DB场景下无法根据rankId获取database
        database = DataBaseManager::Instance().GetTraceDatabaseInCluster(clusterPath, rankId);
        if (database == nullptr) {
            ServerLog::Error("Failed to query fwd/bwd timeline data by rank due to null connection for database.");
            return false;
        }
    }
    if (QueryFwdBwdTimelineFromMstx(rankId, stepId, database)) {
        return true;
    }
    // 如果mstx没有获取到想要的数据，就从手动从timeline计算
    return QueryFwdBwdTimelineFromFlow(rankId, stepId, database);
}

bool QueryFwdBwdTimelineHandler::QueryFwdBwdTimelineFromMstx(const std::string &rankId, const std::string &stepId,
    const std::shared_ptr<Dic::Module::Timeline::VirtualTraceDatabase> &database) {
    // 校验表格是否存在，表格不存在，则直接返回，该表不存在属于正常现象
    if (!database->CheckTableExist(TABLE_STEP_TASK_INFO)) {
        ServerLog::Warn("The table of step task is not exist, skip to query fwd/bwd info from mstx.");
        return false;
    }
    auto rank = &dataMap.at(rankId);
    rank->rankId = rankId;
    // 前反向内容
    PipelineFwdBwdTimelineByComponent fwdBwdData = {LANE_FP_BP, {}};
    if (!database->QueryFwdBwdFromMstx(fwdBwdData.traceList) || fwdBwdData.traceList.empty()) {
        ServerLog::Error("Fail to query fwd/bwd info from mstx.");
        return false;
    }
    rank->componentDataList.push_back(fwdBwdData);
    // 从COMMUNICATION_OP表查询算子内容
    PipelineFwdBwdTimelineByComponent p2pOpData = {LANE_P2P_OP, {}};
    if (!database->QueryP2PCommunicationOpHaveConnectionId(p2pOpData.traceList)) {
        ServerLog::Warn("Query Fwd/Bwd timeline from mstx without p2p communication op info.");
        return true;
    }
    rank->componentDataList.push_back(p2pOpData);
    return true;
}

void QueryFwdBwdTimelineHandler::CalFlowInfo(std::vector<FlowInfo> &flowList, const std::vector<std::string> &rankIds) {
    std::map<std::string, std::vector<FlowPointInfo>> pointMap;
    for (const auto &rank : rankIds) {
        auto pipeline = dataMap[rank];
        std::vector<Protocol::ThreadTraces> p2pTraceList;
        for (auto &item : pipeline.componentDataList) {
            if (item.component == LANE_P2P_OP) {
                p2pTraceList = item.traceList;
                break;
            }
        }
        for (const auto &item : p2pTraceList) {
            if (item.opConnectionId.empty()) {
                continue;
            }
            FlowPointInfo point{rank, item.startTime, item.name};
            pointMap[item.opConnectionId].push_back(point);
        }
    }
    const size_t flowPointNumber = 2;
    for (auto &item : pointMap) {
        if (item.second.size() != flowPointNumber) {
            continue;
        }
        FlowInfo flowInfo;
        flowInfo.flowPointList = item.second;
        // 连线节点排序，保证send算子在前，receive算子在后
        std::sort(flowInfo.flowPointList.begin(), flowInfo.flowPointList.end(),
            [](const FlowPointInfo &pointA, const FlowPointInfo &pointB) {
                if (StringUtil::Contains(StringUtil::ToLower(pointA.opName), "send")) {
                    return true;
                }
                if (StringUtil::Contains(StringUtil::ToLower(pointB.opName), "receive")) {
                    return true;
                }
                return false;
            });
        flowList.push_back(flowInfo);
    }
}

bool QueryFwdBwdTimelineHandler::QueryFwdBwdTimelineFromFlow(const std::string &rankId, const std::string &stepId,
    const std::shared_ptr<Dic::Module::Timeline::VirtualTraceDatabase> &database) {
    uint64_t offset = Timeline::TraceTime::Instance().GetStartTime();
    auto rank = &dataMap.at(rankId);
    Protocol::ExtremumTimestamp range = {offset, (uint64_t)INT64_MAX};
    // 此处不检查返回值，原因为如果查询失败，则以offset为最小值，INT64_MAX为最大值，对应ALL场景
    database->QueryStepDuration(stepId, range.minTimestamp, range.maxTimestamp);
    rank->rankId = rankId;

    // 组装前反向数据
    PipelineFwdBwdTimelineByComponent fwdBwdData = {LANE_FP_BP, {}};
    if (!database->QueryFwdBwdDataByFlow(rankId, offset, range, fwdBwdData.traceList)) {
        ServerLog::Warn("Failed to query fwd/bwd detail trace data for rank ", rankId);
    }
    rank->componentDataList.push_back(fwdBwdData);

    // 组装P2P算子数据，再另一个MR里，带合并后补充此部分
    PipelineFwdBwdTimelineByComponent p2pOpData = {LANE_P2P_OP, {}};
    if (!database->QueryP2PCommunicationOpData(rankId, offset, range, p2pOpData.traceList)) {
        ServerLog::Warn("Failed to query p2p operator detail for rank ", rankId);
    }
    rank->componentDataList.push_back(p2pOpData);
    return true;
}
} // Dic::Module::Summary
