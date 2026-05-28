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

#ifndef PROFILER_SERVER_FLOWANALYZER_H
#define PROFILER_SERVER_FLOWANALYZER_H
#include <vector>
#include <set>
#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "FlowRepoInterface.h"
#include "TimelineProtocolResponse.h"
#include "TraceDatabaseDef.h"
namespace Dic::Module::Timeline {
struct FlowPointSampleStruct {
    // 连线去重集合
    std::unordered_set<std::string> resultFlowIdSet;
    // 开始点集合
    std::unordered_map<std::string, std::vector<uint64_t>> startPointMap;
    // 结束点集合
    std::unordered_map<std::string, std::vector<uint64_t>> endPointMap;
    // 开始点的采样集合
    std::unordered_set<std::string> startPointResultSet;
    // 结束点的采样集合
    std::unordered_set<std::string> endPointResultSet;
    // 开始点所在区间最大时间
    uint64_t curBeginLimitTime = 0;
    // 开始点所在区间最小时间
    uint64_t curBeginStartTime = 0;
    // 结束点所在区间最大时间
    uint64_t curEndLimitTime = 0;
    // 结束点所在区间最小时间
    uint64_t curEndStartTime = 0;
};
class FlowAnalyzer {
  public:
    explicit FlowAnalyzer();
    ~FlowAnalyzer() = default;
    void SetRepository(std::unique_ptr<FlowRepoInterface> repositoryDependency);
    /* *
     * 计算选中的算子相关联的所有连线点
     * @param flowQuery
     * @param sliceId
     * @return
     */
    std::vector<FlowPoint> ComputeAllFlowPointBySliceId(FlowQuery &flowQuery, const std::string &sliceId);
    /* *
     * 根据算子id查询算子上的连线点
     * @param flowQuery
     * @param sliceId
     * @return
     */
    std::unordered_set<std::string> ComputeOnSliceFlowPointBySliceId(
        const FlowQuery &flowQuery, const std::string &sliceId);
    void ComputeCategoryAndFlowMap(const std::vector<FlowDetailDto> &flowDetailVec,
        std::map<std::string, std::vector<Protocol::UnitSingleFlow>> &flowMap, uint64_t minTimestamp);
    void SortByTrackIdASC(std::vector<FlowPoint> &FlowCategoryEventsDtoVec);
    static void SortByFlowIdAndTimestampASC(std::vector<FlowPoint> &flowCategoryEventsDtoVec);
    void ComputeScreenFlowPoint(const std::vector<FlowPoint> &flowEventsVec, uint64_t startTime, uint64_t endTime,
        std::vector<FlowPoint> &flowIdResult);
    static void ComputeUintFlows(const std::vector<FlowPoint> &flowEventsVec, const std::string &category,
        std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList);
    /* *
     * 根据连线点计算点所在的算子
     * @param flowPoint
     * @param sliceVec
     * @return
     */
    std::vector<SliceDomain>::const_iterator ComputeSliceByFlowPoint(
        const FlowPoint &flowPoint, const std::vector<SliceDomain> &sliceVec) const;

  private:
    std::unique_ptr<FlowRepoInterface> repository;
    static bool CompareTrackIdASC(const FlowPoint &first, const FlowPoint &second);
    static bool CompareFlowIdAndTimestampASC(const FlowPoint &first, const FlowPoint &second);
    static void GroupSampleFlowPoint(const std::vector<FlowPoint> &flowEventsVec, uint64_t startTime, uint64_t endTime,
        FlowPointSampleStruct &flowPointSampleStruct);
    static void ComputePointOnScreen(
        FlowPointSampleStruct &flowPointSampleStruct, uint64_t uintTime, const FlowPoint &flowPoint);
    void OfferFlowPointPair(const std::vector<FlowPoint> &flowEventsVec, std::vector<FlowPoint> &flowIdResult,
        FlowPointSampleStruct &flowPointSampleStruct, const std::string &flowId, uint64_t unitTime) const;

    static Protocol::FlowLocation &ComputeLocation(
        Protocol::FlowLocation &location, const FlowPoint &flow, const std::string &type);
};
}

#endif // PROFILER_SERVER_FLOWANALYZER_H
