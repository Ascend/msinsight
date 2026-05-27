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
#include "pch.h"
#include "DomainObject.h"
#include "SliceAnalyzer.h"
#include "FlowAnalyzer.h"
#include "TrackInfoManager.h"
#include "FullDbEnumUtil.h"
#include "PythonStackHelper.h"
#include "SliceCacheManager.h"
#include "RenderEngine.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;
void RenderEngine::SetDataEngineInterface(std::shared_ptr<DataEngineInterface> dataEngineInterface) {
    dataEngine = dataEngineInterface;
}

void RenderEngine::QueryThreadTraces(const Protocol::UnitThreadTracesParams &requestParams,
    Protocol::UnitThreadTracesBody &responseBody, uint64_t minTimestamp, uint64_t traceId) {
    SliceQuery sliceQuery;
    sliceQuery.startTime = requestParams.startTime;
    sliceQuery.endTime = requestParams.endTime;
    sliceQuery.minTimestamp = minTimestamp;
    sliceQuery.isFilterPythonFunction = requestParams.isFilterPythonFunction;
    sliceQuery.cat = "python_function";
    sliceQuery.trackId = traceId;
    sliceQuery.rankId = requestParams.cardId;
    sliceQuery.metaType = Protocol::STR_TO_ENUM<PROCESS_TYPE>(requestParams.metaType).value();

    std::unique_ptr<SliceAnalyzer> sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    sliceAnalyzerPtr->SetRepository(dataEngine);

    uint64_t maxDepth = 0;
    std::set<uint64_t> ids;
    std::map<uint64_t, uint32_t> depthMap;
    if (requestParams.isPythonStack) {
        sliceAnalyzerPtr->ComputePythonFunctionSliceIds(sliceQuery, ids, maxDepth, depthMap);
    } else {
        sliceAnalyzerPtr->ComputeScreenSliceIds(sliceQuery, ids, maxDepth, depthMap);
    }
    std::vector<CompeteSliceDomain> competeSliceVec;
    std::vector<uint64_t> sliceIds(ids.begin(), ids.end());
    dataEngine->QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    for (auto &item : competeSliceVec) {
        item.depth = depthMap[item.id];
    }
    std::sort(competeSliceVec.begin(), competeSliceVec.end(), std::less<CompeteSliceDomain>());
    for (auto &item : competeSliceVec) {
        bool isHide = requestParams.isHideFlagEvents && (hideAbleNameSet.find(item.name) != hideAbleNameSet.end());
        if (isHide) {
            continue;
        }
        Protocol::ThreadTraces threadTraces{};
        threadTraces.id = std::to_string(item.id);
        threadTraces.name = item.name;
        if (!(item.endTime >= item.timestamp && item.timestamp >= minTimestamp && item.endTime >= minTimestamp)) {
            continue;
        }
        threadTraces.duration = item.endTime - item.timestamp;
        threadTraces.startTime = item.timestamp - minTimestamp;
        threadTraces.endTime = item.endTime - minTimestamp;
        threadTraces.depth = depthMap[item.id];
        threadTraces.threadId = requestParams.threadId;
        threadTraces.cname = item.cname;
        while (responseBody.data.size() <= item.depth) {
            responseBody.data.emplace_back();
        }
        responseBody.data[item.depth].emplace_back(threadTraces);
    }
    responseBody.maxDepth = maxDepth;
    responseBody.currentMaxDepth = responseBody.data.size();
    responseBody.havePythonFunction =
        SliceCacheManager::Instance().GetPythonFunctionStatus(traceId) == PYTHON_FUNCTION_STATUS::EXIST;
}

bool RenderEngine::QueryFlowCategoryEvents(Protocol::FlowCategoryEventsParams &params, uint64_t minTimestamp,
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) {
    std::vector<FlowPoint> flowPointResult;
    std::vector<FlowPoint> flowEventsVec;
    FlowQuery flowQuery;
    flowQuery.cat = params.category;
    flowQuery.fileId = params.rankId;
    flowQuery.minTimestamp = minTimestamp;
    dataEngine->QueryFlowPointByCategory(flowQuery, flowEventsVec);
    flowEventsVec = ComputeLockRangePoints(params, flowEventsVec);
    std::unique_ptr<FlowAnalyzer> flowAnalyzerPtr = std::make_unique<FlowAnalyzer>();
    flowAnalyzerPtr->ComputeScreenFlowPoint(flowEventsVec, params.startTime, params.endTime, flowPointResult);
    std::unique_ptr<SliceAnalyzer> sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    flowAnalyzerPtr->SortByTrackIdASC(flowPointResult);
    ThreadQuery threadQuery;
    threadQuery.fileId = params.rankId;
    TrackInfo trackInfo;
    if (params.isSimulation) {
        ComputeSimulationFlows(params, flowDetailList, flowPointResult);
        return true;
    }
    uint64_t curTrackId = 0;
    std::vector<SliceDomain> cacheSlices;
    for (auto &item : flowPointResult) {
        if (item.trackId != curTrackId) {
            cacheSlices.clear();
            std::string sliceCacheKey = std::to_string(item.trackId);
            SliceQuery sliceQuery;
            sliceQuery.startTime = params.startTime;
            sliceQuery.endTime = params.endTime;
            cacheSlices = SliceCacheManager::Instance().GetSliceDomainVec(sliceCacheKey, params.rankId, sliceQuery);
            curTrackId = item.trackId;
            TrackInfoManager::Instance().GetTrackInfo(curTrackId, trackInfo, flowQuery.fileId);
            sliceAnalyzerPtr->SortByTimestampASC(cacheSlices);
        }
        // item.timestamp = timestamp - flowQuery.minTimestamp，timestamp 是从数据库中查出，一定有 timestamp <= INT64_MAX
        // 业务上 flowQuery.minTimestamp 的值能保证是数据库中的最小时间
        item.depth = sliceAnalyzerPtr->ComputeFlowPointDepth(cacheSlices, item.type, item.timestamp + minTimestamp);
        item.pid = trackInfo.processId;
        item.tid = trackInfo.threadId;
    }
    flowAnalyzerPtr->SortByFlowIdAndTimestampASC(flowPointResult);
    flowAnalyzerPtr->ComputeUintFlows(flowPointResult, params.category, flowDetailList);
    ServerLog::Info("Query flow category events. size:", flowDetailList.size());
    return true;
}

std::vector<FlowPoint> RenderEngine::ComputeLockRangePoints(
    FlowCategoryEventsParams &params, std::vector<FlowPoint> &flowEventsVec) const {
    ServerLog::Info("flowEventsVec size is: ", flowEventsVec.size());
    std::unordered_set<uint64_t> trackIdSet;
    for (const auto &metadata : params.metadataList) {
        if (std::empty(metadata.pid) || std::empty(metadata.tid)) {
            continue;
        }
        Protocol::Metadata queryMetadata = metadata;
        PythonStackHelper::RestoreMetadata(queryMetadata);
        uint64_t trackId = TrackInfoManager::Instance().GetTrackId(params.rankId, queryMetadata.pid, queryMetadata.tid);
        trackIdSet.emplace(trackId);
    }
    if (std::empty(trackIdSet)) {
        return flowEventsVec;
    }
    std::unordered_set<std::string> lockFlowIdSet;
    for (const auto &item : flowEventsVec) {
        if (trackIdSet.count(item.trackId) > 0 && item.timestamp >= params.lockStartTime &&
            item.timestamp <= params.lockEndTime) {
            lockFlowIdSet.emplace(item.flowId);
        }
    }
    std::vector<FlowPoint> lockFlowPointVec;
    for (const auto &item : flowEventsVec) {
        if (lockFlowIdSet.count(item.flowId) > 0) {
            lockFlowPointVec.emplace_back(item);
        }
    }
    ServerLog::Info("lockFlowPointVec size is: ", lockFlowPointVec.size());
    return lockFlowPointVec;
}

void RenderEngine::ComputeSimulationFlows(const FlowCategoryEventsParams &params,
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList, std::vector<FlowPoint> &flowPointResult) {
    TrackInfo trackInfo;
    std::unique_ptr<FlowAnalyzer> flowAnalyzerPtr = std::make_unique<FlowAnalyzer>();
    std::unique_ptr<SliceAnalyzer> sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    std::unordered_map<std::string, uint32_t> simpleSliceMap;
    SliceQuery sliceQuery;
    sliceQuery.rankId = params.rankId;
    sliceQuery.startTime = params.startTime;
    sliceQuery.endTime = params.endTime;
    sliceQuery.metaType = PROCESS_TYPE::TEXT;
    uint64_t curTrackId = 0;
    for (auto &item : flowPointResult) {
        if (curTrackId != item.trackId) {
            curTrackId = item.trackId;
            sliceQuery.trackId = curTrackId;
            TrackInfoManager::Instance().GetTrackInfo(curTrackId, trackInfo, sliceQuery.rankId);
            simpleSliceMap.clear();
            std::vector<CompeteSliceDomain> sliceVec;
            dataEngine->QueryAllFlagSlice(sliceQuery, sliceVec);
            std::unordered_map<uint64_t, uint32_t> depthCache;
            sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
            for (const auto &slice : sliceVec) {
                simpleSliceMap[slice.flagId] = depthCache[slice.id];
            }
        }
        std::unordered_map<uint64_t, uint32_t> depthCache;
        sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
        item.depth = simpleSliceMap[item.flowId];
        item.pid = trackInfo.processId;
        item.tid = trackInfo.threadId;
    }
    flowAnalyzerPtr->SortByFlowIdAndTimestampASC(flowPointResult);
    flowAnalyzerPtr->ComputeUintFlows(flowPointResult, params.category, flowDetailList);
    ServerLog::Info("Query Simulation flow category events. size:", flowDetailList.size());
}

std::vector<CompeteSliceDomain> RenderEngine::QuerySliceDetailByNameList(const std::string &fileId,
    const DataType &type, const std::string &processName, const std::vector<std::string> &nameList) {
    if (processName.empty() || nameList.empty()) {
        ServerLog::Warn("Fail to query slice detail by name list");
        return {};
    }
    PROCESS_TYPE processType = type == DataType::TEXT ? PROCESS_TYPE::TEXT : PROCESS_NAME_TO_TYPE(processName);
    SliceQueryByNameList sliceQuery{fileId, processName, nameList, processType};
    std::vector<CompeteSliceDomain> res;
    dataEngine->QuerySliceDetailInfoByNameList(sliceQuery, res);
    return res;
}

std::vector<CompeteSliceDomain> RenderEngine::QueryMstxRLDetail(const std::string &rankId, const DataType &type,
    const std::vector<std::string> &nameList, uint64_t startTime, uint64_t endTime) {
    if (nameList.empty()) {
        ServerLog::Warn("Fail to query mstx rl detail.");
        return {};
    }
    PROCESS_TYPE processType = type == DataType::TEXT ? PROCESS_TYPE::TEXT : PROCESS_TYPE::MS_TX;
    SliceQueryByNameList sliceQuery{rankId, "", nameList, processType, startTime, endTime, {"Python", "CANN"}, "CPU"};
    std::vector<CompeteSliceDomain> res;
    if (!dataEngine) {
        return {};
    }
    dataEngine->QuerySliceDetailInfoByNameList(sliceQuery, res);
    return res;
}

std::unordered_map<uint64_t, std::pair<std::string, std::string>> RenderEngine::GetAllThreadInfo(
    const ThreadQuery &query) {
    if (query.metaType != PROCESS_TYPE::TEXT) {
        ServerLog::Warn("GetAllThreadInfo only implemented for text process type");
        return {};
    }
    std::unordered_map<uint64_t, std::pair<std::string, std::string>> res;
    dataEngine->QueryAllThreadInfo(query, res);
    return res;
}

void RenderEngine::QueryThreadDetail(
    const ThreadDetailParams &requestParams, UnitThreadDetailBody &responseBody, uint64_t trackId) {
    CompeteSliceDomain competeSliceDomain;
    SliceQuery sliceQuery;
    sliceQuery.trackId = trackId;
    sliceQuery.rankId = requestParams.rankId;
    sliceQuery.sliceId = requestParams.id;
    sliceQuery.metaType = Protocol::STR_TO_ENUM<PROCESS_TYPE>(requestParams.metaType).value();
    dataEngine->QuerySliceDetailInfo(sliceQuery, competeSliceDomain);
    responseBody.data.selfTime = 0;
    responseBody.data.args = competeSliceDomain.args;
    responseBody.data.title = competeSliceDomain.name;
    responseBody.data.duration = competeSliceDomain.endTime - competeSliceDomain.timestamp; // 保证 endTime >= timestamp
    responseBody.data.rawTimestamp = competeSliceDomain.timestamp;
    responseBody.data.rawEndstamp = competeSliceDomain.endTime;
    responseBody.data.inputShapes = competeSliceDomain.sliceShape.inputShapes;
    responseBody.data.inputDataTypes = competeSliceDomain.sliceShape.inputDataTypes;
    responseBody.data.inputFormats = competeSliceDomain.sliceShape.inputFormats;
    responseBody.data.outputShapes = competeSliceDomain.sliceShape.outputShapes;
    responseBody.data.outputDataTypes = competeSliceDomain.sliceShape.outputDataTypes;
    responseBody.data.outputFormats = competeSliceDomain.sliceShape.outputFormats;
    sliceQuery.startTime = competeSliceDomain.timestamp;
    sliceQuery.endTime = competeSliceDomain.endTime;
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(dataEngine);
    std::vector<SliceDomain> sliceVec;
    uint32_t targetDepth = 0;
    auto targetIt = sliceVec.end();
    if (requestParams.isPythonStack) {
        std::vector<uint64_t> sliceIds;
        dataEngine->QuerySliceIdsByCat(sliceQuery, sliceIds);
        std::unordered_map<uint64_t, uint32_t> depthMap;
        sliceAnalyzer.ComputeDepthInfoByTrackId(sliceQuery, depthMap);
        std::vector<CompeteSliceDomain> competeSliceVec;
        dataEngine->QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
        sliceVec.reserve(competeSliceVec.size());
        for (const auto &item : competeSliceVec) {
            SliceDomain sliceDomain;
            sliceDomain.id = item.id;
            sliceDomain.timestamp = item.timestamp;
            sliceDomain.endTime = item.endTime;
            sliceDomain.depth = depthMap[item.id];
            sliceVec.emplace_back(sliceDomain);
        }
        std::sort(sliceVec.begin(), sliceVec.end(), SliceDomain::CompareTimestampASC);
        if (depthMap.count(competeSliceDomain.id) == 0) {
            return;
        }
        targetDepth = depthMap[competeSliceDomain.id] + 1;
        SliceDomain target;
        target.id = competeSliceDomain.id;
        target.timestamp = competeSliceDomain.timestamp;
        targetIt = std::lower_bound(sliceVec.begin(), sliceVec.end(), target, SliceDomain::CompareTimestampASC);
    } else {
        sliceAnalyzer.ComputeSliceDomainVecByTrackId(sliceQuery, sliceVec);
        SliceDomain target;
        target.id = competeSliceDomain.id;
        target.timestamp = competeSliceDomain.timestamp;
        auto it = std::lower_bound(sliceVec.begin(), sliceVec.end(), target, SliceDomain::CompareTimestampASC);
        if (it == sliceVec.end()) {
            return;
        }
        targetDepth = it->depth + 1;
        targetIt = it;
    }
    if (targetIt == sliceVec.end()) {
        return;
    }
    const uint64_t targetTimestamp = competeSliceDomain.timestamp;
    const uint64_t targetEndTime = competeSliceDomain.endTime;
    uint64_t nextDepthTime = 0;
    for (auto item = targetIt; item != sliceVec.end(); ++item) {
        if (item->timestamp >= targetTimestamp && item->endTime <= targetEndTime && item->depth == targetDepth) {
            nextDepthTime +=
                item->endTime - item->timestamp; // 从数据库查询得到。业务上保证 item->endTime >= item->timestamp
        }
    }
    if (nextDepthTime > 0 && nextDepthTime <= responseBody.data.duration) {
        responseBody.data.selfTime = responseBody.data.duration - nextDepthTime;
    }
}

CompeteSliceDomain RenderEngine::FindSliceByTimePoint(
    const std::string &fileId, const std::string &name, uint64_t timePoint, const std::string &metaType) {
    SliceQuery sliceQuery;
    CompeteSliceDomain slice;
    sliceQuery.rankId = fileId;
    sliceQuery.name = name;
    if (Protocol::STR_TO_ENUM<PROCESS_TYPE>(metaType) == std::nullopt) {
        return slice;
    }
    sliceQuery.metaType = Protocol::STR_TO_ENUM<PROCESS_TYPE>(metaType).value();
    sliceQuery.timePoint = timePoint;
    bool res = dataEngine->QuerySliceByTimepointAndName(sliceQuery, slice);
    if (!res) {
        ServerLog::Warn("Failed to find slice, name is: %", name);
        return slice;
    }
    std::unordered_map<uint64_t, uint32_t> depthCache;
    std::unique_ptr<SliceAnalyzer> sliceAnalyzerPtr = std::make_unique<SliceAnalyzer>();
    sliceQuery.trackId = slice.trackId;
    sliceQuery.startTime = slice.timestamp;
    sliceQuery.endTime = slice.endTime;
    sliceAnalyzerPtr->SetRepository(dataEngine);
    sliceAnalyzerPtr->ComputeDepthInfoByTrackId(sliceQuery, depthCache);
    slice.depth = depthCache[slice.id];
    return slice;
}
}
