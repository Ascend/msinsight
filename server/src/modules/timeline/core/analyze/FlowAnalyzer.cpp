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
#include "TraceDatabaseDef.h"
#include "DomainObject.h"
#include "DominQuery.h"
#include "SliceCacheManager.h"
#include "TextRepository.h"
#include "FlowAnalyzer.h"

using namespace Dic::Server;
namespace Dic::Module::Timeline {
FlowAnalyzer::FlowAnalyzer() {
    if (repository == nullptr) {
        repository = std::make_unique<TextRepository>();
    }
}

void FlowAnalyzer::SetRepository(std::unique_ptr<FlowRepoInterface> repositoryDependency) {
    repository = std::move(repositoryDependency);
}

std::vector<FlowPoint> FlowAnalyzer::ComputeAllFlowPointBySliceId(FlowQuery &flowQuery, const std::string &sliceId) {
    std::unordered_set<std::string> onSliceFlowPointSet = ComputeOnSliceFlowPointBySliceId(flowQuery, sliceId);
    for (const auto &item : onSliceFlowPointSet) {
        flowQuery.flowIds.emplace_back(item);
    }
    std::vector<FlowPoint> allPoints;
    repository->QueryFlowPointByFlowId(flowQuery, allPoints);
    const uint64_t targetTime = flowQuery.minTimestamp + flowQuery.startTime;
    const uint64_t targetTrackId = flowQuery.trackId;
    FlowPoint targetPoint;
    targetPoint.timestamp = targetTime;
    targetPoint.trackId = targetTrackId;
    auto it = std::lower_bound(allPoints.begin(), allPoints.end(), targetPoint, FlowPoint::CompareTimestampASC);
    if (it == allPoints.end()) {
        return {};
    }
    std::vector<FlowPoint> res;
    res.emplace_back(*it);
    // 从该元素开始往前遍历，直到遍历到同泳道数据
    for (auto i = it - 1; i >= allPoints.begin(); --i) {
        res.emplace_back(*i);
        if (i->timestamp != targetTime && i->trackId == targetTrackId) {
            break;
        }
    }
    // 从该元素开始往后遍历，直到遍历到同泳道数据
    for (auto i = it + 1; i < allPoints.end(); ++i) {
        res.emplace_back(*i);
        if (i->timestamp != targetTime && i->trackId == targetTrackId) {
            break;
        }
    }
    return res;
}

std::unordered_set<std::string> FlowAnalyzer::ComputeOnSliceFlowPointBySliceId(
    const FlowQuery &flowQuery, const std::string &sliceId) {
    std::unordered_set<std::string> res;
    std::vector<FlowPoint> flowPointVec;
    repository->QueryFlowPointByTimeRange(flowQuery, flowPointVec);
    ServerLog::Info("flowPointVec is: ", flowPointVec.size());
    auto &instance = SliceCacheManager::Instance();
    SliceQuery sliceQuery;
    sliceQuery.startTime = flowQuery.startTime;
    sliceQuery.endTime = flowQuery.endTime;
    std::vector<SliceDomain> sliceVec =
        instance.GetSliceDomainVec(std::to_string(flowQuery.trackId), flowQuery.fileId, sliceQuery);
    // 此时是前端打开泳道点击算子，缓存中必定存在泳道下[startTime, endTime]内的算子数据,若不存在说明是异常情况
    if (std::empty(sliceVec)) {
        return res;
    }
    for (const auto &item : flowPointVec) {
        auto it = ComputeSliceByFlowPoint(item, sliceVec);
        if (it != sliceVec.end() && std::to_string(it->id) == sliceId) {
            res.emplace(item.flowId);
        }
    }
    return res;
}

std::vector<SliceDomain>::const_iterator FlowAnalyzer::ComputeSliceByFlowPoint(
    const FlowPoint &flowPoint, const std::vector<SliceDomain> &sliceVec) const {
    SliceDomain slice;
    slice.timestamp = flowPoint.timestamp;
    slice.id = 0;
    auto it = sliceVec.begin();
    if (flowPoint.type == Protocol::LINE_START) {
        it = std::lower_bound(sliceVec.begin(), sliceVec.end(), slice, SliceDomain::CompareTimestampASC);
        if (it != sliceVec.end() && it->timestamp == flowPoint.timestamp) {
            return it;
        }

        while (it != sliceVec.end() && it > sliceVec.begin()) {
            it--;
            if (it->timestamp <= flowPoint.timestamp && it->endTime >= flowPoint.timestamp) {
                break;
            }
        }
        return it;
    }
    if (flowPoint.type == Protocol::LINE_END || flowPoint.type == Protocol::LINE_END_OPTIONAL) {
        it = std::lower_bound(sliceVec.begin(), sliceVec.end(), slice, SliceDomain::CompareTimestampASC);
        if (it != sliceVec.end()) {
            return it;
        }
    }
    return it;
}

void FlowAnalyzer::ComputeCategoryAndFlowMap(const std::vector<FlowDetailDto> &flowDetailVec,
    std::map<std::string, std::vector<Protocol::UnitSingleFlow>> &flowMap, uint64_t minTimestamp) {
    const static int FLOW_COUNT = 2; // from + to
    if (flowDetailVec.size() != FLOW_COUNT) {
        return;
    }
    Protocol::UnitSingleFlow unitSingleFlow;
    unitSingleFlow.title = flowDetailVec[0].name;
    unitSingleFlow.cat = flowDetailVec[0].cat;
    unitSingleFlow.id = flowDetailVec[0].flowId;
    FlowDetailDto from(flowDetailVec[0]);
    FlowDetailDto to(flowDetailVec[1]);
    if (from.type != to.type && to.type == Protocol::LINE_START) {
        from = flowDetailVec[1];
        to = flowDetailVec[0];
    }
    unitSingleFlow.from.id = from.id;
    unitSingleFlow.from.pid = from.pid;
    unitSingleFlow.from.tid = from.tid;
    if (from.timestamp < minTimestamp || to.timestamp < minTimestamp) {
        return;
    }
    unitSingleFlow.from.timestamp = from.timestamp - minTimestamp;
    unitSingleFlow.from.depth = from.depth;
    unitSingleFlow.to.id = to.id;
    unitSingleFlow.to.pid = to.pid;
    unitSingleFlow.to.tid = to.tid;
    unitSingleFlow.to.timestamp = to.timestamp - minTimestamp;
    unitSingleFlow.to.depth = to.depth;
    flowMap[unitSingleFlow.cat].emplace_back(unitSingleFlow);
}

void FlowAnalyzer::SortByTrackIdASC(std::vector<FlowPoint> &flowCategoryEventsDtoVec) {
    std::sort(flowCategoryEventsDtoVec.begin(), flowCategoryEventsDtoVec.end(), CompareTrackIdASC);
}

void FlowAnalyzer::SortByFlowIdAndTimestampASC(std::vector<FlowPoint> &flowCategoryEventsDtoVec) {
    std::sort(flowCategoryEventsDtoVec.begin(), flowCategoryEventsDtoVec.end(), CompareFlowIdAndTimestampASC);
}

/**
 * 计算屏幕上需要呈现的连线点
 * @param flowEventsVec 数据集
 * @param startTime 屏幕开始时间
 * @param endTime 屏幕结束时间
 * @param flowIdResult 计算结果
 */
void FlowAnalyzer::ComputeScreenFlowPoint(const std::vector<FlowPoint> &flowEventsVec, uint64_t startTime,
    uint64_t endTime, std::vector<FlowPoint> &flowIdResult) {
    FlowPointSampleStruct flowPointSampleStruct;
    GroupSampleFlowPoint(flowEventsVec, startTime, endTime, flowPointSampleStruct);
    // 单位时间片，入参已根据业务规则校验开始时间结束时间
    uint64_t unitTime = (endTime - startTime) / 1000; // params.CheckParams 保证 endTime > startTime
    for (const auto &item : flowPointSampleStruct.startPointResultSet) {
        OfferFlowPointPair(flowEventsVec, flowIdResult, flowPointSampleStruct, item, unitTime);
    }
    for (const auto &item : flowPointSampleStruct.endPointResultSet) {
        OfferFlowPointPair(flowEventsVec, flowIdResult, flowPointSampleStruct, item, unitTime);
    }
}

/**
 * 根据连线点组装连线
 * 连线存在四种类型：1 - S-F; 2 - S-T; 3 - S-T-T-...T-F; 4 - S-F-F-...-F
 * S - Protocol::LINE_START
 * T - Protocol::LINE_END_OPTIONAL
 * F - Protocol::LINE_END
 * @param flowEventsVec 连线点
 * @param category 连线类别
 * @param flowDetailList 结果集
 */
void FlowAnalyzer::ComputeUintFlows(const std::vector<FlowPoint> &flowEventsVec, const std::string &category,
    std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) {
    std::string curFlowId;
    Protocol::FlowLocation location;
    Protocol::FlowLocation onePointer;
    Protocol::FlowLocation twoPointer;
    for (const auto &flow : flowEventsVec) {
        std::string type = flow.type;
        std::string flowId = flow.flowId;
        if (type == Protocol::LINE_START || flowId != curFlowId) {
            location = ComputeLocation(location, flow, type);
            // 连线中 起点对终点是一对多关系，因此每一个新的起点都会对上一个起点进行覆盖
            onePointer = location;
        } else if ((type == Protocol::LINE_END_OPTIONAL || type == Protocol::LINE_END) && flowId == curFlowId) {
            auto flowEvent = std::make_unique<Protocol::UnitSingleFlow>();
            flowEvent->cat = category;
            Protocol::FlowLocation fromPointer;
            if (onePointer.type != Protocol::LINE_END) {
                fromPointer = onePointer;
                // onePointer 将更新为当前的 flow，更新 twoPointer 为现在的 onePointer
                twoPointer = onePointer;
            } else {
                fromPointer = twoPointer;
            }
            // 更新 onePointer
            onePointer.pid = flow.pid;
            onePointer.tid = flow.tid;
            onePointer.depth = flow.depth;
            onePointer.type = flow.type;
            onePointer.timestamp = flow.timestamp;
            onePointer.rankId = flow.rankId;
            onePointer.id = std::to_string(flow.id);
            onePointer.duration = flow.duration;
            flowEvent->from.pid = fromPointer.pid;
            flowEvent->from.tid = fromPointer.tid;
            flowEvent->from.depth = fromPointer.depth;
            flowEvent->from.timestamp = fromPointer.timestamp;
            flowEvent->from.rankId = fromPointer.rankId;
            flowEvent->from.id = fromPointer.id;
            flowEvent->from.duration = fromPointer.duration;
            flowEvent->to.pid = flow.pid;
            flowEvent->to.tid = flow.tid;
            flowEvent->to.depth = flow.depth;
            flowEvent->to.timestamp = flow.timestamp;
            flowEvent->to.rankId = flow.rankId;
            flowEvent->to.id = std::to_string(flow.id);
            flowEvent->to.duration = flow.duration;
            flowDetailList.emplace_back(std::move(flowEvent));
        }
        curFlowId = flowId;
    }
}

Protocol::FlowLocation &FlowAnalyzer::ComputeLocation(
    Protocol::FlowLocation &location, const FlowPoint &flow, const std::string &type) {
    location.pid = flow.pid;
    location.tid = flow.tid;
    location.depth = flow.depth;
    location.timestamp = flow.timestamp;
    location.type = type;
    location.rankId = flow.rankId;
    location.id = std::to_string(flow.id);
    location.duration = flow.duration;
    return location;
}

void FlowAnalyzer::OfferFlowPointPair(const std::vector<FlowPoint> &flowEventsVec, std::vector<FlowPoint> &flowIdResult,
    FlowPointSampleStruct &flowPointSampleStruct, const std::string &flowId, uint64_t unitTime) const {
    // 过滤重复连线
    if (flowPointSampleStruct.resultFlowIdSet.count(flowId) > 0) {
        return;
    }
    // 过滤屏幕外右边的连线
    if (flowPointSampleStruct.endPointMap.count(flowId) == 0) {
        return;
    }
    // 过滤屏幕外左边的连线
    if (flowPointSampleStruct.startPointMap.count(flowId) == 0) {
        return;
    }
    // 收集在屏幕中间可展示的连线
    std::vector<FlowPoint> tempPoint;
    for (const auto &item : flowPointSampleStruct.endPointMap[flowId]) {
        tempPoint.emplace_back(flowEventsVec[item]);
    }
    std::sort(tempPoint.begin(), tempPoint.end());
    uint64_t curTime = 0;
    uint64_t curTrackId = 0;
    for (const auto &item : tempPoint) {
        if (item.trackId != curTrackId) {
            curTime = 0;
            curTrackId = item.trackId;
        }
        if (item.timestamp >= curTime + unitTime || curTime == 0) {
            flowIdResult.emplace_back(item);
            curTime = item.timestamp;
        }
    }
    for (const auto &item : flowPointSampleStruct.startPointMap[flowId]) {
        flowIdResult.emplace_back(flowEventsVec[item]);
    }
    flowPointSampleStruct.resultFlowIdSet.emplace(flowId);
}

void FlowAnalyzer::GroupSampleFlowPoint(const std::vector<FlowPoint> &flowEventsVec, uint64_t startTime,
    uint64_t endTime, FlowPointSampleStruct &flowPointSampleStruct) {
    uint64_t curTrackId = std::numeric_limits<uint64_t>::max();
    uint64_t index = 0;
    // 此处500是把屏幕平均分成500份，以一份屏幕的宽度为采集连线点的最小步长
    // endTime 和 startTime 作为参数传入时判断了 endTime > startTime
    uint64_t uintTime = (endTime - startTime) / 500;
    for (const auto &item : flowEventsVec) {
        if (curTrackId != item.trackId) {
            curTrackId = item.trackId;
            flowPointSampleStruct.curBeginLimitTime = uintTime;
            flowPointSampleStruct.curBeginStartTime = 0;
            flowPointSampleStruct.curEndLimitTime = uintTime;
            flowPointSampleStruct.curEndStartTime = 0;
        }
        // 收集全部开始点
        if (item.timestamp <= endTime && item.type == Protocol::LINE_START) {
            flowPointSampleStruct.startPointMap[item.flowId].push_back(index);
        }
        // 收集全部结束点
        if (item.timestamp >= startTime &&
            (item.type == Protocol::LINE_END || item.type == Protocol::LINE_END_OPTIONAL)) {
            flowPointSampleStruct.endPointMap[item.flowId].push_back(index);
        }
        index++;
        if (uintTime == 0 && item.timestamp < startTime) {
            continue;
        }
        if (uintTime == 0 && item.timestamp > endTime) {
            continue;
        }
        ComputePointOnScreen(flowPointSampleStruct, uintTime, item);
    }
}

void FlowAnalyzer::ComputePointOnScreen(
    FlowPointSampleStruct &flowPointSampleStruct, uint64_t uintTime, const FlowPoint &flowPoint) {
    if (uintTime == 0 && flowPoint.type == Protocol::LINE_START) {
        flowPointSampleStruct.startPointResultSet.emplace(flowPoint.flowId);
        return;
    }
    if (uintTime == 0 && (flowPoint.type == Protocol::LINE_END || flowPoint.type == Protocol::LINE_END_OPTIONAL)) {
        flowPointSampleStruct.endPointResultSet.emplace(flowPoint.flowId);
        return;
    }
    // 计算可能需要展示在屏幕上的开始点
    if (flowPoint.type == Protocol::LINE_START) {
        while (flowPoint.timestamp >= flowPointSampleStruct.curBeginLimitTime) {
            flowPointSampleStruct.curBeginLimitTime += uintTime;
            flowPointSampleStruct.curBeginStartTime += uintTime;
        }
        if (flowPoint.timestamp >= flowPointSampleStruct.curBeginStartTime) {
            flowPointSampleStruct.startPointResultSet.emplace(flowPoint.flowId);
            flowPointSampleStruct.curBeginLimitTime += uintTime;
            flowPointSampleStruct.curBeginStartTime += uintTime;
        }
    }
    // 计算可能需要展示在屏幕上的结束点
    if ((flowPoint.type == Protocol::LINE_END || flowPoint.type == Protocol::LINE_END_OPTIONAL)) {
        while (flowPoint.timestamp >= flowPointSampleStruct.curEndLimitTime) {
            flowPointSampleStruct.curEndStartTime += uintTime;
            flowPointSampleStruct.curEndLimitTime += uintTime;
        }
        if (flowPoint.timestamp >= flowPointSampleStruct.curEndStartTime) {
            flowPointSampleStruct.endPointResultSet.emplace(flowPoint.flowId);
            flowPointSampleStruct.curEndStartTime += uintTime;
            flowPointSampleStruct.curEndLimitTime += uintTime;
        }
    }
}

bool FlowAnalyzer::CompareTrackIdASC(const FlowPoint &first, const FlowPoint &second) {
    if (first.trackId < second.trackId) {
        return true;
    }
    if (first.trackId == second.trackId && first.id < second.id) {
        return true;
    }
    return false;
}

bool FlowAnalyzer::CompareFlowIdAndTimestampASC(const FlowPoint &first, const FlowPoint &second) {
    if (first.flowId < second.flowId) {
        return true;
    }
    if (first.flowId > second.flowId) {
        return false;
    }
    if (second.type != Protocol::LINE_START && first.type == Protocol::LINE_START) {
        return true;
    }
    if (first.type != Protocol::LINE_START && second.type == Protocol::LINE_START) {
        return false;
    }
    if (first.type != Protocol::LINE_START && second.type != Protocol::LINE_START &&
        first.timestamp < second.timestamp) {
        return true;
    }
    if (first.type != Protocol::LINE_START && second.type != Protocol::LINE_START &&
        first.timestamp > second.timestamp) {
        return false;
    }
    if (first.type != Protocol::LINE_START && second.type != Protocol::LINE_START && first.id < second.id) {
        return true;
    }
    if (first.type == Protocol::LINE_START && second.type == Protocol::LINE_START && first.id < second.id) {
        return true;
    }
    return false;
}
}
