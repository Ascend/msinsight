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

#ifndef PROFILER_SERVER_SLICERESPOTERFACE_H
#define PROFILER_SERVER_SLICERESPOTERFACE_H
#include <vector>
#include <unordered_map>
#include "DominQuery.h"
#include "DomainObject.h"
namespace Dic::Module::Timeline {
class IBaseSliceRepo {
  public:
    virtual ~IBaseSliceRepo() = default;
    /**
     * 根据trackId查询简单算子
     * @param sliceQuery
     * @param sliceVec
     */
    virtual void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) = 0;

    /**
     * 根据Id集合查询完整算子信息
     * @param sliceQuery
     * @param sliceIds
     * @param CompeteSliceVec
     */
    virtual void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) = 0;

    /**
     * 根据Id查询算子的shape和args信息
     * @param sliceQuery
     * @param competeSliceDomain
     */
    virtual bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) = 0;
};

class IFindSliceByNameList {
  public:
    virtual ~IFindSliceByNameList() = default;
    /**
     * 根据算子名查询算子明细列表
     * @param res
     * @return
     */
    virtual bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) = 0;
};

class IPythonFuncSlice {
  public:
    virtual ~IPythonFuncSlice() = default;
    /**
     * 根据TrackId查询所有调用栈算子的id,返回结果升序
     * @param sliceQuery
     * @param sliceIds
     */
    virtual void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) = 0;

    virtual bool QuerySliceByCatAndTimeRange(const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) {
        return false;
    }

    /**
     * 根据TrackId查询python function数据条数
     * @param sliceQuery
     * @return
     */
    virtual uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) = 0;
};

class IFindSliceByTimepointAndName {
  public:
    virtual ~IFindSliceByTimepointAndName() = default;
    /**
     * 根据给定的时刻和名字查询算子
     * @param sliceQuery
     * @param competeSliceDomain
     */
    virtual bool QuerySliceByTimepointAndName(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) = 0;
};

class IFindSliceByVagueNameAndTime {
  public:
    virtual ~IFindSliceByVagueNameAndTime() = default;
    /**
     * @brief 根据模糊名称和时间段查询算子
     */
    virtual bool QuerySliceByVagueNameAndTime(const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &res) = 0;
};

class ITextSlice {
  public:
    virtual ~ITextSlice() = default;
    /**
     * 查询时间范围内所有算子（包括与范围相交的）
     * @param sliceQuery
     * @param sliceVec
     */
    virtual void QueryCompeteSliceVecByTimeRangeAndTrackId(
        const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) = 0;

    /**
     * 查询所有泳道线程和进程信息
     * @param flowQuery
     * @param threadInfo
     */
    virtual void QueryAllThreadInfo(const ThreadQuery &flowQuery,
        std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) = 0;
};
}
#endif // PROFILER_SERVER_SLICERESPOTERFACE_H
