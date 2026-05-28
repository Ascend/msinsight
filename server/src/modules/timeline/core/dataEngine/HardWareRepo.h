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
#ifndef PROFILER_SERVER_HARDWAREREPO_H
#define PROFILER_SERVER_HARDWAREREPO_H
#include "TaskTable.h"
#include "TrackInfoManager.h"
#include "TaskPmuInfoTable.h"
#include "ComputeTaskInfoTable.h"
#include "StringIdsTable.h"
#include "SliceRepoInterface.h"
namespace Dic::Module::Timeline {
using Stmt = std::unique_ptr<SqlitePreparedStatement>;
class HardWareRepo : public IBaseSliceRepo, public IFindSliceByNameList {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;
    bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) override;

  protected:
    std::unique_ptr<TaskTable> taskTable = std::make_unique<TaskTable>();
    std::unique_ptr<TaskPmuInfoTable> taskPmuInfoTable = std::make_unique<TaskPmuInfoTable>();
    std::unique_ptr<ComputeTaskInfoTable> computeTaskInfoTable = std::make_unique<ComputeTaskInfoTable>();
    std::unique_ptr<StringIdsTable> stringIdsTable = std::make_unique<StringIdsTable>();

    virtual Stmt CreatPreparedStatement(const std::string &sql, const SliceQuery &sliceQuery);
    std::string GetDbPath(const SliceQuery &sliceQuery);
    void QuerySliceArgs(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask);

    void QuerySliceShape(
        const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask);
    void QuerySlicePmuInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, uint64_t globalTaskId);
    bool QueryMemoryInfo(
        const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetTask);
    static std::unique_ptr<SqlitePreparedStatement> PrepareStmtForQuerySimpleSliceWithOutNameByTrackId(
        const TrackInfo &trackInfo, const std::shared_ptr<VirtualTraceDatabase> &database,
        const SliceQuery &sliceQuery);
};
}
#endif // PROFILER_SERVER_HARDWAREREPO_H
