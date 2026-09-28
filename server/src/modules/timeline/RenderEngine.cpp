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
#include <mutex>
#include <optional>
#include "DomainObject.h"
#include "SliceAnalyzer.h"
#include "FlowAnalyzer.h"
#include "TrackInfoManager.h"
#include "FullDbEnumUtil.h"
#include "PythonStackHelper.h"
#include "SliceCacheManager.h"
#include "DataBaseManager.h"
#include "SingleRankCommunicationJsonParser.h"
#include "TimeRangeUtils.h"
#include "RenderEngine.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;
namespace {
std::mutex singleRankCommunicationDatabaseMutex;

uint64_t ComputeCoveredDurationByChildren(
    const std::vector<SliceDomain> &childSlices, uint64_t parentStartTime, uint64_t parentEndTime) {
    uint64_t coveredDuration = 0;
    uint64_t mergedEndTime = parentStartTime;
    for (const auto &child : childSlices) {
        if (child.endTime <= parentStartTime || child.timestamp >= parentEndTime || child.endTime <= child.timestamp) {
            continue;
        }
        uint64_t childStartTime = std::max(child.timestamp, parentStartTime);
        uint64_t childEndTime = std::min(child.endTime, parentEndTime);
        if (childEndTime <= childStartTime) {
            continue;
        }
        if (childStartTime >= mergedEndTime) {
            coveredDuration += childEndTime - childStartTime;
            mergedEndTime = childEndTime;
            continue;
        }
        if (childEndTime > mergedEndTime) {
            coveredDuration += childEndTime - mergedEndTime;
            mergedEndTime = childEndTime;
        }
    }
    return coveredDuration;
}

std::string RemoveHostFromRankId(const std::string &rankId) {
    const std::vector<std::string> rankParts = StringUtil::Split(rankId, " ");
    return rankParts.size() > 1 ? rankParts[1] : rankId;
}

struct CommunicationRankContext {
    std::string rawRankId;
    std::string traceHost;
    bool requiresDeviceMapping = false;
};

std::optional<CommunicationRankContext> PrepareCommunicationRankContext(
    const std::string &timelineRankId, const std::string &traceDbPath) {
    const std::vector<RankInfo> rankInfos =
        TrackInfoManager::Instance().GetRankListByFileId(traceDbPath, timelineRankId);
    if (rankInfos.size() != 1) {
        return std::nullopt;
    }
    const RankInfo &rankInfo = rankInfos.front();
    const std::string rankWithoutHost = RemoveHostFromRankId(rankInfo.rankId);
    const std::string clusterPrefix = rankInfo.cluster + "_";
    std::string rawRankId = rankWithoutHost;
    if (!rankInfo.cluster.empty() && rankWithoutHost.compare(0, clusterPrefix.size(), clusterPrefix) == 0) {
        rawRankId = rankWithoutHost.substr(clusterPrefix.size());
    }
    if (rawRankId.empty() || rawRankId.find_first_not_of("0123456789") != std::string::npos) {
        // A suffix such as 0_2 identifies a duplicated Timeline rank and cannot be mapped reliably.
        return std::nullopt;
    }
    auto &databaseManager = DataBaseManager::Instance();
    if (databaseManager.GetDataType(traceDbPath) != DataType::DB ||
        databaseManager.GetFileType(traceDbPath) != FileType::MS_PROF) {
        return CommunicationRankContext{rawRankId, "", false};
    }
    const auto traceDatabase = databaseManager.GetTraceDatabaseByFileId(traceDbPath);
    if (traceDatabase == nullptr) {
        return std::nullopt;
    }
    return CommunicationRankContext{rawRankId, traceDatabase->QueryRawHostInfo(), true};
}

std::optional<std::string> ResolveCommunicationRankId(
    const CommunicationRankContext &rankContext, const std::shared_ptr<VirtualClusterDatabase> &clusterDatabase) {
    if (!rankContext.requiresDeviceMapping) {
        return rankContext.rawRankId;
    }
    if (clusterDatabase == nullptr) {
        return std::nullopt;
    }
    // MS_PROF Timeline ranks come from NPU_INFO.id, which is the local device id.
    return clusterDatabase->QueryCommunicationRankId(rankContext.traceHost, rankContext.rawRankId);
}

std::optional<CommunicationDetailDatabaseHandle> GetSingleRankCommunicationDatabase(const std::string &traceDbPath) {
    auto &databaseManager = DataBaseManager::Instance();
    auto databaseHandle = databaseManager.GetCommunicationDetailDatabaseHandleByFileId(traceDbPath);
    if (databaseHandle.has_value()) {
        return databaseHandle;
    }
    // A detail request can be issued concurrently for the same trace. Build and publish the optional JSON cache once.
    std::lock_guard<std::mutex> lock(singleRankCommunicationDatabaseMutex);
    databaseHandle = databaseManager.GetCommunicationDetailDatabaseHandleByFileId(traceDbPath);
    if (databaseHandle.has_value()) {
        return databaseHandle;
    }
    // Text Timeline data is materialized as mindstudio_insight_data.db and can also have a sibling analysis.db.
    // The communication analysis database schema is independent of the Timeline trace source format.
    const std::string analysisDbPath = FileUtil::SplicePath(FileUtil::GetParentPath(traceDbPath), "analysis.db");
    if (FileUtil::IsRegularFile(analysisDbPath)) {
        if (!databaseManager.CreateCommunicationDetailConnectionPool(
                traceDbPath, analysisDbPath, CommunicationDetailSourceMode::RANK_LOCAL, true)) {
            return std::nullopt;
        }
        return databaseManager.GetCommunicationDetailDatabaseHandleByFileId(traceDbPath);
    }
    // DB-only profiler data does not guarantee communication.json. Keep analysis.db as its authoritative source and
    // use the raw JSON only for a Text single-rank trace that has no analysis database.
    if (databaseManager.GetDataType(traceDbPath) != DataType::TEXT) {
        return std::nullopt;
    }
    const std::string communicationJsonPath =
        FileUtil::SplicePath(FileUtil::GetParentPath(traceDbPath), "communication.json");
    if (!FileUtil::IsRegularFile(communicationJsonPath)) {
        return std::nullopt;
    }
    const auto communicationCachePath = PrepareSingleRankCommunicationJsonCache(traceDbPath, communicationJsonPath);
    if (!communicationCachePath.has_value()) {
        return std::nullopt;
    }
    if (!databaseManager.CreateCommunicationDetailConnectionPool(
            traceDbPath, communicationCachePath.value(), CommunicationDetailSourceMode::RANK_LOCAL, true)) {
        return std::nullopt;
    }
    return databaseManager.GetCommunicationDetailDatabaseHandleByFileId(traceDbPath);
}

void AppendCommunicationDetail(
    const CompeteSliceDomain &slice, const ThreadDetailParams &requestParams, UnitThreadDetailBody &responseBody) {
    if (!slice.isCommunicationGroup) {
        return;
    }
    auto &databaseManager = DataBaseManager::Instance();
    const std::string traceDbPath =
        requestParams.dbPath.empty() ? databaseManager.GetDbPathByRankId(requestParams.rankId) : requestParams.dbPath;
    if (traceDbPath.empty()) {
        return;
    }
    auto communicationDatabaseHandle = databaseManager.GetCommunicationDetailDatabaseHandleByFileId(traceDbPath);
    const std::string clusterProjectPath = TrackInfoManager::Instance().GetClusterProjectPathByFileId(traceDbPath);
    const bool hasClusterDatabase =
        !clusterProjectPath.empty() && databaseManager.HasClusterDatabase(clusterProjectPath);
    const bool useCachedClusterDatabase = communicationDatabaseHandle.has_value() &&
        communicationDatabaseHandle->sourceMode == CommunicationDetailSourceMode::CLUSTER;
    std::optional<CommunicationRankContext> communicationRankContext;
    if (useCachedClusterDatabase || hasClusterDatabase) {
        communicationRankContext = PrepareCommunicationRankContext(requestParams.rankId, traceDbPath);
        if (!communicationRankContext.has_value()) {
            return;
        }
    }
    const auto clusterDatabase = hasClusterDatabase ? databaseManager.GetClusterDatabase(clusterProjectPath) : nullptr;
    std::shared_ptr<VirtualClusterDatabase> communicationDatabase =
        useCachedClusterDatabase ? communicationDatabaseHandle->GetConnection() : clusterDatabase;
    CommunicationDetailSourceMode sourceMode =
        useCachedClusterDatabase ? communicationDatabaseHandle->sourceMode : CommunicationDetailSourceMode::CLUSTER;
    std::string communicationRankId;
    if (communicationDatabase == nullptr && (useCachedClusterDatabase || hasClusterDatabase)) {
        return;
    }
    if (communicationDatabase == nullptr) {
        if (!communicationDatabaseHandle.has_value()) {
            communicationDatabaseHandle = GetSingleRankCommunicationDatabase(traceDbPath);
        }
        if (!communicationDatabaseHandle.has_value()) {
            return;
        }
        sourceMode = communicationDatabaseHandle->sourceMode;
        communicationDatabase = communicationDatabaseHandle->GetConnection();
        if (communicationDatabase == nullptr) {
            return;
        }
    }
    if (sourceMode == CommunicationDetailSourceMode::CLUSTER) {
        if (!communicationRankContext.has_value()) {
            return;
        }
        const auto resolvedRankId = ResolveCommunicationRankId(communicationRankContext.value(), communicationDatabase);
        if (!resolvedRankId.has_value()) {
            return;
        }
        communicationRankId = resolvedRankId.value();
    }
    CommunicationDetailDo detail;
    if (!communicationDatabase->QueryCommunicationDetail(communicationRankId, slice.name, detail, sourceMode)) {
        return;
    }
    responseBody.data.transitTime = detail.transitTime;
    responseBody.data.waitTime = detail.waitTime;
    responseBody.data.communicationBandwidthInfo.reserve(detail.bandwidthInfo.size());
    for (const auto &bandwidth : detail.bandwidthInfo) {
        responseBody.data.communicationBandwidthInfo.emplace_back(Protocol::CommunicationBandwidthInfo{
            bandwidth.transportType, bandwidth.transitSize, bandwidth.transitTime, bandwidth.bandwidth});
    }
}

class TextFlowDepthResolver {
  public:
    explicit TextFlowDepthResolver(const std::vector<SliceDomain> &slices) : slices(slices) {}

    /**
     * 匹配语义边界：FlowPoint 未携带对应 Slice 的 ID。没有同时间 Slice 且多个历史 Slice
     * 同时包裹 FlowPoint 时，选择开始时间最晚的 Slice，而不能识别业务上可能对应的中间层。
     * 例如 A=[100, 300]、B=[120, 250]、C=[140, 180]，FlowPoint 时间为 150 时会选择 C。
     * 这与修改前 ORDER BY timestamp DESC, id DESC LIMIT 1 的 SQL 规则一致；本次修改仅将
     * 该规则改为采样后的内存计算，没有改变关联语义。
     */
    uint32_t Resolve(const std::string &type, uint64_t timestamp) {
        AdvanceTo(timestamp);
        const bool hasFutureSlice = nextSliceIndex < slices.size();
        if (type == Protocol::LINE_START) {
            if (hasFutureSlice && slices[nextSliceIndex].timestamp == timestamp) {
                return slices[nextSliceIndex].depth;
            }
            if (!hasFutureSlice) {
                return 0;
            }
            return activeSliceIndexes.empty() ? slices.front().depth : slices[activeSliceIndexes.back()].depth;
        }
        if ((type == Protocol::LINE_END || type == Protocol::LINE_END_OPTIONAL) && hasFutureSlice) {
            return slices[nextSliceIndex].depth;
        }
        return 0;
    }

  private:
    // 将内部状态推进到当前 FlowPoint 的时间，并准备好计算该点 depth 所需的 Slice。
    void AdvanceTo(uint64_t timestamp) {
        // Flow 点和 Slice 均已按时间戳排序。仅保留可能包裹当前或后续 Flow 点的较早 Slice，
        // 并确保能够找到其中开始时间最晚的 Slice。
        while (nextSliceIndex < slices.size() && slices[nextSliceIndex].timestamp < timestamp) {
            // A later slice whose end time is not earlier permanently supersedes the previous candidate.
            while (!activeSliceIndexes.empty() &&
                slices[activeSliceIndexes.back()].endTime <= slices[nextSliceIndex].endTime) {
                activeSliceIndexes.pop_back();
            }
            activeSliceIndexes.emplace_back(nextSliceIndex);
            ++nextSliceIndex;
        }
        while (!activeSliceIndexes.empty() && slices[activeSliceIndexes.back()].endTime < timestamp) {
            activeSliceIndexes.pop_back();
        }
    }

    const std::vector<SliceDomain> &slices;
    size_t nextSliceIndex = 0;
    // 保存仍可能成为当前或后续 FlowPoint“包裹 Slice”的候选下标子集。
    // 从栈底到栈顶，开始时间越来越晚，结束时间越来越早。
    // 例如：
    // A: 100 ├──────────────────────────┤ 300
    // C:      150 ├──────────────┤ 220
    // D:           170 ├────┤ 180
    // 栈顶 D 是开始时间最晚的候选，所以只要 D 仍然包裹当前时间，就应该优先选择 D。
    // D 结束后将其弹出，C 重新成为栈顶；C 结束后，A 重新成为栈顶。
    std::vector<size_t> activeSliceIndexes;
};
}

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
    sliceQuery.dbPath = requestParams.dbPath;
    sliceQuery.metaType = Protocol::STR_TO_ENUM<PROCESS_TYPE>(requestParams.metaType).value();
    sliceQuery.isPythonStack = requestParams.isPythonStack;

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
        threadTraces.threadId = PythonStackHelper::BuildDisplayThreadId(
            requestParams.processId, requestParams.threadId, requestParams.metaType, requestParams.isPythonStack);
        threadTraces.cname = item.cname;
        while (responseBody.data.size() <= item.depth) {
            responseBody.data.emplace_back();
        }
        responseBody.data[item.depth].emplace_back(threadTraces);
    }
    responseBody.maxDepth = maxDepth;
    responseBody.currentMaxDepth = responseBody.data.size();
    const std::string pythonFunctionCacheKey =
        SliceCacheManager::BuildPythonFunctionCacheKey(sliceQuery.GetDataSourceId(), traceId);
    responseBody.havePythonFunction =
        SliceCacheManager::Instance().GetPythonFunctionStatus(pythonFunctionCacheKey) == PYTHON_FUNCTION_STATUS::EXIST;
}

bool RenderEngine::QueryFlowCategoryEvents(Protocol::FlowCategoryEventsParams &params, uint64_t minTimestamp,
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) {
    std::vector<FlowPoint> flowPointResult;
    std::vector<FlowPoint> flowEventsVec;
    FlowQuery flowQuery;
    flowQuery.cat = params.category;
    const std::string queryFileId = params.dbPath.empty() ? params.rankId : params.dbPath;
    flowQuery.fileId = queryFileId;
    flowQuery.minTimestamp = minTimestamp;
    dataEngine->QueryFlowPointByCategory(flowQuery, flowEventsVec);
    flowEventsVec = ComputeLockRangePoints(params, flowEventsVec);
    std::unique_ptr<FlowAnalyzer> flowAnalyzerPtr = std::make_unique<FlowAnalyzer>();
    flowAnalyzerPtr->ComputeScreenFlowPoint(flowEventsVec, params.startTime, params.endTime, flowPointResult);
    flowAnalyzerPtr->SortByTrackIdASC(flowPointResult);
    ThreadQuery threadQuery;
    threadQuery.fileId = queryFileId;
    TrackInfo trackInfo;
    if (params.isSimulation) {
        ComputeSimulationFlows(params, flowDetailList, flowPointResult);
        return true;
    }
    uint64_t curTrackId = 0;
    std::vector<SliceDomain> textSlices;
    std::unique_ptr<TextFlowDepthResolver> textDepthResolver;
    for (auto &item : flowPointResult) {
        if (item.trackId != curTrackId) {
            curTrackId = item.trackId;
            const std::string endpointSourceId = item.rankId.empty() ? queryFileId : item.rankId;
            TrackInfo endpointTrackInfo;
            bool trackInfoFound =
                TrackInfoManager::Instance().GetTrackInfo(curTrackId, endpointTrackInfo, endpointSourceId);
            if (!trackInfoFound && endpointSourceId != queryFileId) {
                trackInfoFound = TrackInfoManager::Instance().GetTrackInfo(curTrackId, endpointTrackInfo, queryFileId);
            }
            trackInfo = trackInfoFound ? endpointTrackInfo : TrackInfo{};
            textDepthResolver.reset();
            textSlices.clear();
            if (item.resolveDepthAfterSampling) {
                SliceQuery sliceQuery;
                sliceQuery.trackId = curTrackId;
                sliceQuery.rankId = endpointSourceId;
                sliceQuery.dbPath = params.dbPath;
                sliceQuery.metaType = PROCESS_TYPE::TEXT;
                const SliceQuery pagedQuery = SliceCacheManager::GetSlicePagedQuery(sliceQuery);
                dataEngine->QuerySimpleSliceWithOutNameByTrackId(pagedQuery, textSlices);
                SliceAnalyzer::SortByTimestampASC(textSlices);
                textDepthResolver = std::make_unique<TextFlowDepthResolver>(textSlices);
            }
        }
        if (item.resolveDepthAfterSampling && textDepthResolver != nullptr) {
            item.depth = textDepthResolver->Resolve(item.type, AddTimestampOffset(item.timestamp, minTimestamp));
        }
        item.pid = trackInfo.processId;
        item.tid = trackInfo.threadId;
    }
    flowAnalyzerPtr->SortByFlowIdAndTimestampASC(flowPointResult);
    flowAnalyzerPtr->ComputeUintFlows(flowPointResult, params.category, flowDetailList);
    const bool includeSourceIdentity =
        !params.dbPath.empty() && DataBaseManager::Instance().GetSourceFileIdsByRankId(params.rankId).size() > 1;
    for (auto &flow : flowDetailList) {
        if (flow->from.rankId == queryFileId) {
            flow->from.rankId = params.rankId;
        }
        if (flow->to.rankId == queryFileId) {
            flow->to.rankId = params.rankId;
        }
        if (includeSourceIdentity) {
            flow->from.dbPath = params.dbPath;
            flow->to.dbPath = params.dbPath;
        }
    }
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
        const std::string queryFileId = params.dbPath.empty() ? params.rankId : params.dbPath;
        uint64_t trackId = TrackInfoManager::Instance().GetTrackId(queryFileId, queryMetadata.pid, queryMetadata.tid);
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
            for (const auto &slice : sliceVec) {
                simpleSliceMap[slice.flagId] = slice.depth;
            }
        }
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
    sliceQuery.dbPath = requestParams.dbPath;
    sliceQuery.sliceId = requestParams.id;
    sliceQuery.metaType = Protocol::STR_TO_ENUM<PROCESS_TYPE>(requestParams.metaType).value();
    sliceQuery.isPythonStack = requestParams.isPythonStack;
    dataEngine->QuerySliceDetailInfo(sliceQuery, competeSliceDomain);
    responseBody.data.selfTime = 0;
    responseBody.data.args = competeSliceDomain.args;
    responseBody.data.modelStreamIds = competeSliceDomain.modelStreamIds;
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
    AppendCommunicationDetail(competeSliceDomain, requestParams, responseBody);
    sliceQuery.startTime = competeSliceDomain.timestamp;
    sliceQuery.endTime = competeSliceDomain.endTime;
    SliceAnalyzer sliceAnalyzer;
    sliceAnalyzer.SetRepository(dataEngine);
    std::vector<SliceDomain> sliceVec;
    if (requestParams.isPythonStack) {
        sliceAnalyzer.ComputePythonFunctionSliceVecByTimeRange(sliceQuery, sliceVec);
    } else {
        sliceAnalyzer.ComputeSliceDomainVecByTrackId(sliceQuery, sliceVec);
    }
    std::vector<SliceDomain> childSlices;
    const uint32_t childDepth = competeSliceDomain.depth + 1;
    for (const auto &slice : sliceVec) {
        if (slice.depth == childDepth) {
            childSlices.emplace_back(slice);
        }
    }
    const uint64_t childCoveredDuration =
        ComputeCoveredDurationByChildren(childSlices, competeSliceDomain.timestamp, competeSliceDomain.endTime);
    if (childCoveredDuration > 0 && childCoveredDuration <= responseBody.data.duration) {
        responseBody.data.selfTime = responseBody.data.duration - childCoveredDuration;
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
    return slice;
}
}
