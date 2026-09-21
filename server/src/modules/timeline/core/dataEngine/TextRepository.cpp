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
#include "ProcessTable.h"
#include "SliceTable.h"
#include "FlowTable.h"
#include "ThreadTable.h"
#include "KernelDetailTable.h"
#include "TrackInfoManager.h"
#include "TextRepository.h"
#include "TimeRangeUtils.h"

namespace Dic::Module::Timeline {
namespace {
const std::string GROUP_PREFIX = "Group ";
const std::string GROUP_SUFFIX = " Communication";

bool IsCommunicationGroupTrack(const SliceQuery &sliceQuery) {
    ThreadTable threadTable;
    std::vector<ThreadPO> threads;
    threadTable.Select(ThreadColumn::THREAD_NAME)
        .Eq(ThreadColumn::TRACK_ID, sliceQuery.trackId)
        .ExcuteQuery(sliceQuery.rankId, threads);
    if (threads.size() != 1) {
        return false;
    }
    const std::string &threadName = threads.front().threadName;
    return threadName.size() > GROUP_PREFIX.size() + GROUP_SUFFIX.size() &&
        threadName.compare(0, GROUP_PREFIX.size(), GROUP_PREFIX) == 0 &&
        threadName.compare(threadName.size() - GROUP_SUFFIX.size(), GROUP_SUFFIX.size(), GROUP_SUFFIX) == 0;
}
} // namespace

void TextRepository::QuerySimpleSliceWithOutNameByTrackId(
    const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    auto &instance = TrackInfoManager::Instance();
    const bool isSuccess = instance.GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    SliceTable sliceTable;
    std::vector<SlicePO> tempSlicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP, SliceColumn::ENDTIME)
        .Select(SliceColumn::GROUPID, SliceColumn::DEPTH)
        .Eq(SliceColumn::TRACKID, sliceQuery.trackId)
        .IsNullOrNotEq(SliceColumn::CAT, std::string("python_function"))
        .OrderBy(SliceColumn::TIMESTAMP, TableOrder::ASC)
        .OrderBy(SliceColumn::ID, TableOrder::ASC)
        .ExcuteQuery(trackInfo.cardId, tempSlicePOVec);
    for (const auto &item : tempSlicePOVec) {
        SliceDomain cachelice;
        cachelice.id = item.id;
        cachelice.timestamp = item.timestamp;
        cachelice.endTime = item.endTime;
        cachelice.depth = item.depth;
        cachelice.groupId = item.groupId;
        sliceVec.emplace_back(cachelice);
    }
}

void TextRepository::QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID)
        .Eq(SliceColumn::TRACKID, sliceQuery.trackId)
        .Eq(SliceColumn::CAT, sliceQuery.cat)
        .OrderBy(SliceColumn::ID, TableOrder::ASC)
        .ExcuteQuery(trackInfo.cardId, slicePOVec);
    for (const auto &item : slicePOVec) {
        sliceIds.emplace_back(item.id);
    }
}

bool TextRepository::QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return false;
    }
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP, SliceColumn::ENDTIME)
        .Select(SliceColumn::GROUPID, SliceColumn::DEPTH)
        .Eq(SliceColumn::TRACKID, sliceQuery.trackId)
        .Eq(SliceColumn::CAT, sliceQuery.cat)
        .LessEq(SliceColumn::TIMESTAMP, AddTimestampOffset(sliceQuery.endTime, sliceQuery.minTimestamp))
        .GreaterEq(SliceColumn::ENDTIME, AddTimestampOffset(sliceQuery.startTime, sliceQuery.minTimestamp))
        .OrderBy(SliceColumn::TIMESTAMP, TableOrder::ASC)
        .OrderBy(SliceColumn::ID, TableOrder::ASC)
        .ExcuteQuery(trackInfo.cardId, slicePOVec);
    for (const auto &item : slicePOVec) {
        SliceDomain cacheSlice;
        cacheSlice.id = item.id;
        cacheSlice.timestamp = item.timestamp;
        cacheSlice.endTime = item.endTime;
        cacheSlice.depth = item.depth;
        cacheSlice.groupId = item.groupId;
        sliceVec.emplace_back(cacheSlice);
    }
    return true;
}

uint64_t TextRepository::QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return 0;
    }
    SliceTable sliceTable;
    uint64_t count = sliceTable.Eq(SliceColumn::TRACKID, sliceQuery.trackId)
                         .Eq(SliceColumn::CAT, sliceQuery.cat)
                         .Count(trackInfo.cardId);
    return count;
}

void TextRepository::QueryCompeteSliceVecByTimeRangeAndTrackId(
    const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::DURATION, SliceColumn::ENDTIME, SliceColumn::NAME, SliceColumn::DEPTH)
        .Eq(SliceColumn::TRACKID, sliceQuery.trackId)
        .LessEq(SliceColumn::TIMESTAMP, AddTimestampOffset(sliceQuery.endTime, sliceQuery.minTimestamp))
        .Greater(SliceColumn::ENDTIME, AddTimestampOffset(sliceQuery.startTime, sliceQuery.minTimestamp))
        .ExcuteQuery(trackInfo.cardId, slicePOVec);
    for (const auto &item : slicePOVec) {
        CompeteSliceDomain temp;
        temp.id = item.id;
        temp.timestamp = item.timestamp;
        temp.duration = item.duration;
        temp.endTime = item.endTime;
        temp.depth = item.depth;
        temp.name = item.name;
        sliceVec.emplace_back(std::move(temp));
    }
}

void TextRepository::QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(flowQuery.trackId, trackInfo, flowQuery.fileId);
    if (!isSuccess) {
        return;
    }
    FlowTable flowTable;
    std::vector<FlowPO> flowPOVec;
    flowTable.Select(FlowColumn::TYPE, FlowColumn::TIMESTAMP, FlowColumn::FLOW_ID)
        .GreaterEq(FlowColumn::TIMESTAMP, AddTimestampOffset(flowQuery.startTime, flowQuery.minTimestamp))
        .LessEq(FlowColumn::TIMESTAMP, AddTimestampOffset(flowQuery.endTime, flowQuery.minTimestamp))
        .Eq(FlowColumn::TRACK_ID, flowQuery.trackId)
        .GroupBy(FlowColumn::FLOW_ID)
        .ExcuteQuery(trackInfo.cardId, flowPOVec);
    for (const auto &item : flowPOVec) {
        FlowPoint flowPoint;
        flowPoint.timestamp = item.timestamp;
        flowPoint.type = item.type;
        flowPoint.flowId = item.flowId;
        flowPointVec.emplace_back(std::move(flowPoint));
    }
}

void TextRepository::QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(flowQuery.trackId, trackInfo, flowQuery.fileId);
    if (!isSuccess) {
        return;
    }
    FlowTable flowTable;
    std::vector<FlowPO> flowPOVec;
    flowTable.Select(FlowColumn::NAME, FlowColumn::CAT, FlowColumn::FLOW_ID)
        .Select(FlowColumn::TIMESTAMP, FlowColumn::TYPE, FlowColumn::TRACK_ID)
        .In(FlowColumn::FLOW_ID, flowQuery.flowIds)
        .OrderBy(FlowColumn::TIMESTAMP, TableOrder::ASC)
        .OrderBy(FlowColumn::TRACK_ID, TableOrder::ASC)
        .OrderBy(FlowColumn::ID, TableOrder::ASC)
        .ExcuteQuery(trackInfo.cardId, flowPOVec);
    for (const auto &item : flowPOVec) {
        FlowPoint flowPoint;
        flowPoint.name = item.name;
        flowPoint.cat = item.cat;
        flowPoint.flowId = item.flowId;
        flowPoint.timestamp = item.timestamp;
        flowPoint.type = item.type;
        flowPoint.trackId = item.trackId;
        flowPointVec.emplace_back(std::move(flowPoint));
    }
}

void TextRepository::QueryAllThreadInfo(
    const ThreadQuery &threadQuery, std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) {
    ThreadTable threadTable;
    std::vector<ThreadPO> threadPOVec;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::PID, ThreadColumn::TID)
        .ExcuteQuery(threadQuery.fileId, threadPOVec);
    for (const auto &item : threadPOVec) {
        threadInfo[item.trackId] = {item.pid, item.tid};
    }
}

void TextRepository::QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
    std::vector<CompeteSliceDomain> &competeSliceVec) {
    if (std::empty(sliceIds)) {
        return;
    }
    TrackInfo trackInfo;
    const bool isSuccess = TrackInfoManager::Instance().GetTrackInfo(sliceQuery.trackId, trackInfo, sliceQuery.rankId);
    if (!isSuccess) {
        return;
    }
    SliceTable sliceTable;
    std::vector<SlicePO> tempSlicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP, SliceColumn::ENDTIME)
        .Select(SliceColumn::NAME, SliceColumn::CNAME, SliceColumn::DEPTH)
        .In(SliceColumn::ID, sliceIds)
        .ExcuteQuery(trackInfo.cardId, tempSlicePOVec);
    for (const auto &item : tempSlicePOVec) {
        CompeteSliceDomain cachelice;
        cachelice.id = item.id;
        cachelice.timestamp = item.timestamp;
        cachelice.endTime = item.endTime;
        cachelice.depth = item.depth;
        cachelice.name = item.name;
        cachelice.cname = item.cname;
        competeSliceVec.emplace_back(cachelice);
    }
}

void TextRepository::QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(flowQuery.fileId);
    if (database == nullptr) {
        ServerLog::Warn("Failed to get database when querying flow points by category.");
        return;
    }
    const std::string sql = "SELECT f.id, f.track_id, f.flow_id, f.type, f.timestamp, CASE "
                            "WHEN f.type = '" +
        Protocol::LINE_START +
        "' THEN COALESCE((SELECT s.depth FROM slice AS s "
        "WHERE s.track_id = f.track_id AND (s.cat IS NULL OR s.cat != 'python_function') "
        "AND s.timestamp = f.timestamp ORDER BY s.id ASC LIMIT 1), "
        "(SELECT s.depth FROM slice AS s "
        "WHERE s.track_id = f.track_id AND (s.cat IS NULL OR s.cat != 'python_function') "
        "AND s.timestamp < f.timestamp AND s.end_time >= f.timestamp "
        "AND EXISTS (SELECT 1 FROM slice AS boundary WHERE boundary.track_id = f.track_id "
        "AND (boundary.cat IS NULL OR boundary.cat != 'python_function') "
        "AND boundary.timestamp >= f.timestamp) "
        "ORDER BY s.timestamp DESC, s.id DESC LIMIT 1), "
        "CASE WHEN EXISTS (SELECT 1 FROM slice AS boundary WHERE boundary.track_id = f.track_id "
        "AND (boundary.cat IS NULL OR boundary.cat != 'python_function') "
        "AND boundary.timestamp >= f.timestamp) "
        "THEN (SELECT s.depth FROM slice AS s WHERE s.track_id = f.track_id "
        "AND (s.cat IS NULL OR s.cat != 'python_function') "
        "ORDER BY s.timestamp ASC, s.id ASC LIMIT 1) ELSE 0 END, 0) "
        "WHEN f.type IN ('" +
        Protocol::LINE_END + "', '" + Protocol::LINE_END_OPTIONAL +
        "') THEN COALESCE((SELECT s.depth FROM slice AS s "
        "WHERE s.track_id = f.track_id AND (s.cat IS NULL OR s.cat != 'python_function') "
        "AND s.timestamp >= f.timestamp ORDER BY s.timestamp ASC, s.id ASC LIMIT 1), 0) "
        "ELSE 0 END AS depth FROM flow AS f WHERE f.cat = ? "
        "ORDER BY f.track_id ASC, f.timestamp ASC";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Warn("Failed to prepare flow point depth query.");
        return;
    }
    stmt->BindParams(flowQuery.cat);
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Warn("Failed to execute flow point depth query.");
        return;
    }
    while (resultSet->Next()) {
        const uint64_t timestamp = resultSet->GetUint64("timestamp");
        if (timestamp < flowQuery.minTimestamp) {
            continue;
        }
        FlowPoint flowPoint;
        flowPoint.id = resultSet->GetUint64("id");
        flowPoint.trackId = resultSet->GetUint64("track_id");
        flowPoint.flowId = resultSet->GetString("flow_id");
        flowPoint.type = resultSet->GetString("type");
        flowPoint.timestamp = timestamp - flowQuery.minTimestamp;
        flowPoint.depth = resultSet->GetUint32("depth");
        flowPointVec.emplace_back(flowPoint);
    }
}

void TextRepository::QueryAllFlagSlice(
    const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &competeSliceDomainVec) {
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::FLAGID, SliceColumn::DEPTH)
        .NotEq(SliceColumn::FLAGID, "")
        .Eq(SliceColumn::TRACKID, sliceQuery.trackId)
        .ExcuteQuery(sliceQuery.rankId, slicePOVec);
    for (const auto &item : slicePOVec) {
        CompeteSliceDomain competeSliceDomain;
        competeSliceDomain.id = item.id;
        competeSliceDomain.flagId = item.flagId;
        competeSliceDomain.depth = item.depth;
        competeSliceDomainVec.emplace_back(competeSliceDomain);
    }
}

bool TextRepository::QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    bool success = QuerySliceDetailById(sliceQuery, competeSliceDomain);
    if (success) {
        QueryShapeInfoBySlice(sliceQuery, competeSliceDomain);
        competeSliceDomain.isCommunicationGroup = IsCommunicationGroupTrack(sliceQuery);
    }
    return success;
}

bool TextRepository::QuerySliceDetailInfoByNameList(
    const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) {
    // 从process表查询pid
    ProcessTable processTable;
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID);
    if (!params.processName.empty()) {
        processTable.Eq(ProcessColumn::PROCESS_NAME, params.processName);
    }
    if (!params.processNameExclusion.empty()) {
        processTable.NotIn(ProcessColumn::PROCESS_NAME, params.processNameExclusion);
    }
    if (!params.processLabel.empty()) {
        processTable.Eq(ProcessColumn::LABEL, params.processLabel);
    }
    processTable.ExcuteQuery(params.rankId, processPOS);
    if (processPOS.empty()) {
        return false;
    }
    std::vector<std::string> pidList;
    std::transform(processPOS.begin(), processPOS.end(), std::back_inserter(pidList),
        [](ProcessPO process) { return process.pid; });

    // 根据pid去查询track_id列表
    ThreadTable threadTable;
    std::vector<ThreadPO> threadPOVec;
    threadTable.Select(ThreadColumn::TRACK_ID).In(ThreadColumn::PID, pidList).ExcuteQuery(params.rankId, threadPOVec);
    if (threadPOVec.empty()) {
        return false;
    }
    std::vector<uint64_t> trackIdList;
    std::transform(threadPOVec.begin(), threadPOVec.end(), std::back_inserter(trackIdList),
        [](ThreadPO thread) { return thread.trackId; });

    // 根据track id和算子名查询结果数据
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable
        .Select(SliceColumn::TIMESTAMP, SliceColumn::DURATION, SliceColumn::NAME, SliceColumn::ENDTIME,
            SliceColumn::TRACKID, SliceColumn::ARGS, SliceColumn::DEPTH)
        .In(SliceColumn::TRACKID, trackIdList)
        .In(SliceColumn::NAME, params.nameList);
    if (params.startTime < params.endTime) {
        sliceTable.GreaterEq(SliceColumn::TIMESTAMP, params.startTime).LessEq(SliceColumn::ENDTIME, params.endTime);
    }
    sliceTable.OrderBy(SliceColumn::TIMESTAMP, TableOrder::ASC).ExcuteQuery(params.rankId, slicePOVec);
    for (const auto &item : slicePOVec) {
        CompeteSliceDomain domain;
        domain.timestamp = item.timestamp;
        domain.name = item.name;
        domain.duration = item.duration;
        domain.endTime = item.endTime;
        domain.args = item.args;
        domain.trackId = item.trackId;
        domain.depth = item.depth;
        res.push_back(domain);
    }
    return true;
}

bool TextRepository::QuerySliceDetailById(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) const {
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::ENDTIME, SliceColumn::NAME)
        .Select(SliceColumn::ARGS, SliceColumn::DEPTH)
        .Eq(SliceColumn::ID, sliceQuery.sliceId)
        .ExcuteQuery(sliceQuery.rankId, slicePOVec);
    if (std::empty(slicePOVec)) {
        ServerLog::Warn("Failed to query text slice by id in text scene!");
        return false;
    }
    const SlicePO &slicePo = slicePOVec[0];
    competeSliceDomain.id = slicePo.id;
    competeSliceDomain.name = slicePo.name;
    competeSliceDomain.endTime = slicePo.endTime;
    competeSliceDomain.timestamp = slicePo.timestamp;
    competeSliceDomain.depth = slicePo.depth;
    competeSliceDomain.args = slicePo.args;
    return true;
}

void TextRepository::QueryShapeInfoBySlice(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) const {
    KernelDetailTable kernelDetailTable;
    std::vector<KernelDetailPO> kernelDetailPOs;
    kernelDetailTable.Select(KernelDetailColumn::OUTPUT_FORMATS, KernelDetailColumn::INPUT_SHAPES)
        .Select(KernelDetailColumn::INPUT_DATA_TYPES, KernelDetailColumn::INPUT_FORMATS)
        .Select(KernelDetailColumn::OUTPUT_SHAPES, KernelDetailColumn::OUTPUT_DATA_TYPES)
        .Eq(KernelDetailColumn::START_TIME, competeSliceDomain.timestamp)
        .Eq(KernelDetailColumn::NAME, competeSliceDomain.name)
        .NotEq(KernelDetailColumn::ACCELERATOR_CORE, COMMUNICATION_LIST.at(0))
        .NotEq(KernelDetailColumn::ACCELERATOR_CORE, COMMUNICATION_LIST.at(1))
        .ExcuteQuery(sliceQuery.rankId, kernelDetailPOs);
    if (std::empty(kernelDetailPOs)) {
        return;
    }
    const KernelDetailPO &kernelDetailPo = kernelDetailPOs[0];
    competeSliceDomain.sliceShape.outputFormats = kernelDetailPo.outputFormats;
    competeSliceDomain.sliceShape.outputDataTypes = kernelDetailPo.outputDataTypes;
    competeSliceDomain.sliceShape.outputShapes = kernelDetailPo.outputShapes;
    competeSliceDomain.sliceShape.inputFormats = kernelDetailPo.inputFormats;
    competeSliceDomain.sliceShape.inputDataTypes = kernelDetailPo.inputDataTypes;
    competeSliceDomain.sliceShape.inputShapes = kernelDetailPo.inputShapes;
}

bool TextRepository::QuerySliceByTimepointAndName(
    const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) {
    SliceTable sliceTable;
    std::vector<SlicePO> slicePOVec;
    sliceTable.Select(SliceColumn::ID, SliceColumn::TIMESTAMP)
        .Select(SliceColumn::ENDTIME, SliceColumn::TRACKID, SliceColumn::DURATION, SliceColumn::DEPTH)
        .LessEq(SliceColumn::TIMESTAMP, sliceQuery.timePoint)
        .GreaterEq(SliceColumn::ENDTIME, sliceQuery.timePoint)
        .Eq(SliceColumn::NAME, sliceQuery.name)
        .ExcuteQuery(sliceQuery.rankId, slicePOVec);
    if (std::empty(slicePOVec)) {
        ServerLog::Warn("Failed to query text slice by time point in text scene!");
        return false;
    }
    const SlicePO &slicePo = slicePOVec[0];
    competeSliceDomain.id = slicePo.id;
    competeSliceDomain.endTime = slicePo.endTime;
    competeSliceDomain.timestamp = slicePo.timestamp;
    competeSliceDomain.depth = slicePo.depth;
    TrackInfo trackInfo;
    auto &instance = TrackInfoManager::Instance();
    instance.GetTrackInfo(slicePo.trackId, trackInfo, sliceQuery.rankId);
    competeSliceDomain.pid = trackInfo.processId;
    competeSliceDomain.tid = trackInfo.threadId;
    competeSliceDomain.trackId = slicePo.trackId;
    competeSliceDomain.duration = slicePo.duration;
    competeSliceDomain.cardId = sliceQuery.rankId;
    return true;
}
}
