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

#ifndef PROFILER_SERVER_TEXTREPOSITORY_H
#define PROFILER_SERVER_TEXTREPOSITORY_H
#include "SliceRepoInterface.h"
#include "FlowRepoInterface.h"
#include "SimulationSliceRepoInterface.h"
namespace Dic::Module::Timeline {
class TextRepository : public IBaseSliceRepo,
                       public IPythonFuncSlice,
                       public ITextSlice,
                       public IFindSliceByTimepointAndName,
                       public IFindSliceByNameList,
                       public FlowRepoInterface,
                       public SimulationSliceRepoInterface {
  public:
    ~TextRepository() override = default;
    /** @SliceRepoInterface IBaseSliceRepo
     * 根据trackId查询简单算子
     * @param sliceQuery
     * @param sliceVec
     */
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    /** @SliceRepoInterface IPythonFuncSlice
     * 根据TrackId查询所有调用栈算子的id,返回结果升序
     * @param sliceQuery
     * @param sliceIds
     */
    void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override;
    bool QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;

    /** @SliceRepoInterface IPythonFuncSlice
     * 根据TrackId查询python function数据条数
     * @param sliceQuery
     * @return
     */
    uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) override;

    /** @SliceRepoInterface ITextSlice
     * 查询时间范围内所有算子（包括与范围相交的）
     * @param sliceQuery
     * @param sliceVec
     */
    void QueryCompeteSliceVecByTimeRangeAndTrackId(
        const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override;

    /** @SliceRepoInterface ITextSlice
     * 查询所有泳道线程和进程信息
     * @param flowQuery
     * @param threadInfo
     */
    void QueryAllThreadInfo(const ThreadQuery &flowQuery,
        std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) override;

    /** @SliceRepoInterface IBaseSliceRepo
     * 根据Id集合查询完整算子信息
     * @param sliceQuery
     * @param sliceIds
     * @param CompeteSliceVec
     */
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;

    /** @SliceRepoInterface IBaseSliceRepo
     * 根据Id查询算子的shape和args信息
     * @param sliceQuery
     * @param competeSliceDomain
     * @return
     */
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

    /** @SliceRepoInterface IFindSliceByTimepointAndName
     * 根据给定的时刻和名字查询算子
     * @param sliceQuery
     * @param competeSliceDomain
     * @return
     */
    bool QuerySliceByTimepointAndName(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

    /** @SliceRepoInterface IFindSliceByNameList
     * 根据算子名查询算子明细列表
     * @param params
     * @param res
     * @return
     */
    bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) override;

    /** @FlowRepoInterface
     * 查询时间范围内的所有连线点
     * @param flowQuery
     * @param flowPointVec
     */
    void QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;

    /** @FlowRepoInterface
     * 根据连线点id查询所有连线点
     * @param flowQuery
     * @param flowPointVec
     */
    void QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;

    /** @FlowRepoInterface
     * 根据连线类别查询所有连线
     * @param flowQuery
     * @param flowPointVec
     */
    void QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;

    /** @SimulationSliceRepoInterface
     * 查询所有 flag 算子
     * @param sliceQuery
     * @param competeSliceDomain
     */
    void QueryAllFlagSlice(const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &competeSliceDomain) override;

  private:
    const std::vector<std::string> COMMUNICATION_LIST = {"HCCL", "COMMUNICATION"};

    /* *
     * 根据算子信息查询算子的shape信息
     * @param sliceQuery
     * @param competeSliceDomain
     */
    void QueryShapeInfoBySlice(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) const;

    /* *
     * 根据算子id查询算子详情，包括开始时间，名字，duration，args
     * @param sliceQuery
     * @param competeSliceDomain
     * @return
     */
    bool QuerySliceDetailById(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) const;
};
}
#endif // PROFILER_SERVER_TEXTREPOSITORY_H
