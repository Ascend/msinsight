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

#ifndef PROFILER_SERVER_HCCLREPO_H
#define PROFILER_SERVER_HCCLREPO_H
#include "TaskTable.h"
#include "CommucationTaskInfoTable.h"
#include "CommucationOpTable.h"
#include "TrackInfoManager.h"
#include "StringIdsTable.h"
#include "EnumHcclDataTypeTable.h"
#include "EnumHcclLinkTypeTable.h"
#include "EnumHcclTransportTypeTable.h"
#include "EnumHcclRdmaTypeTable.h"
#include "NpuInfoRepo.h"
#include "SliceRepoInterface.h"
namespace Dic::Module::Timeline {
class HcclRepo : public IBaseSliceRepo {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;
    void SetTaskTable(std::unique_ptr<TaskTable>);
    void SetCommucationOpTable(std::unique_ptr<CommucationOpTable>);
    void SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr);
    void SetCommucationTaskInfoTable(std::unique_ptr<CommucationTaskInfoTable>);

  protected:
    std::unique_ptr<TaskTable> taskTable = std::make_unique<TaskTable>();
    std::unique_ptr<CommucationOpTable> commucationOpTable = std::make_unique<CommucationOpTable>();
    std::unique_ptr<NpuInfoRepo> npuInfoRepo = std::make_unique<NpuInfoRepo>();
    std::unique_ptr<CommucationTaskInfoTable> commucationTaskInfoTable = std::make_unique<CommucationTaskInfoTable>();
    std::unique_ptr<StringIdsTable> stringIdsTable = std::make_unique<StringIdsTable>();
    std::unique_ptr<EnumHcclDataTypeTable> enumHcclDataTypeTable = std::make_unique<EnumHcclDataTypeTable>();
    std::unique_ptr<EnumHcclTransportTypeTable> enumHcclTransportTypeTable =
        std::make_unique<EnumHcclTransportTypeTable>();
    std::unique_ptr<EnumHcclLinkTypeTable> enumHcclLinkTypeTable = std::make_unique<EnumHcclLinkTypeTable>();
    std::unique_ptr<EnumHcclRdmaTypeTable> enumHcclRdmaTypeTable = std::make_unique<EnumHcclRdmaTypeTable>();
    virtual void QuerySimpleSliceFromGroupTrackByDevice(std::vector<CommucationTaskOpPO> &commucationTaskOpPOVec,
        const TrackInfo &trackInfo, const std::string &tid, const SliceQuery &sliceQuery);

  private:
    const std::string groupSuffix = "group";
    const std::string globalSrcRank = "globalSrcRank";
    const std::string globalDstRank = "globalDstRank";

    std::vector<uint64_t> QueryGlobalTaskIdsByRank(const TrackInfo &trackInfo);

    std::vector<uint64_t> QueryOpIdsByGlabalTaskIds(const TrackInfo &trackInfo, const std::vector<uint64_t> &globalIds);

    void QueryGroupSliceByIds(const std::vector<uint64_t> &sliceIds, std::vector<CompeteSliceDomain> &competeSliceVec,
        const TrackInfo &trackInfo);

    void QueryPlaneSliceByIds(const std::vector<uint64_t> &sliceIds, std::vector<CompeteSliceDomain> &competeSliceVec,
        const TrackInfo &trackInfo);

    void QuerySimpleSliceFromGroupTrack(std::vector<SliceDomain> &sliceVec, const TrackInfo &trackInfo,
        const std::string &suffix, const SliceQuery &sliceQuery);

    void QuerySimpleSliceFromPlaneTrack(
        std::vector<SliceDomain> &sliceVec, TrackInfo &trackInfo, const SliceQuery &sliceQuery);

    bool QueryGroupSliceDetailInfo(
        const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TrackInfo &trackInfo);

    std::string QueryTransportName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo);

    std::string QueryDataTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo);

    std::string QueryLinkTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo);

    std::string QueryRdmaTypeName(const SliceQuery &sliceQuery, CommucationTaskInfoPO &targetTaskInfo);

    std::string QueryBandwidth(const SliceQuery &sliceQuery, const TaskPO &targetPO);

    void SetPlaneSliceArgs(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const TaskPO &targetPO,
        CommucationTaskInfoPO &targetTaskInfo);

    static std::string GetRealRankByLocalRank(uint64_t localRank, std::vector<std::string> &realRankList);

    std::optional<ParallelGroupInfo> GetGroupInfoByGroupNameId(uint64_t groupNameId, const std::string &fileId);

    bool QueryPlaneSliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain);
};
}
#endif // PROFILER_SERVER_HCCLREPO_H
