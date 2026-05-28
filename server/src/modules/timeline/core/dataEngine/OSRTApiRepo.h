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

#ifndef PROFILER_SERVER_OSRTAPI_H
#define PROFILER_SERVER_OSRTAPI_H
#include "SliceRepoInterface.h"
#include "VirtualTraceDatabase.h"
namespace Dic::Module::Timeline {
class OSRTApiRepo : public IBaseSliceRepo {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &competeSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

  protected:
    static void QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(const std::shared_ptr<VirtualTraceDatabase> &database,
        std::string &processId, std::vector<SliceDomain> &sliceVec, const SliceQuery &sliceQuery);
    void QueryCompeteSliceByIdsExecuteSQL(std::shared_ptr<VirtualTraceDatabase> database,
        const std::vector<uint64_t> &sliceIds, std::vector<CompeteSliceDomain> &competeSliceVec);
    bool QuerySliceDetailInfoExecuteSQL(std::shared_ptr<VirtualTraceDatabase> database, const std::string &sliceId,
        CompeteSliceDomain &competeSliceDomain);
};
}
#endif // PROFILER_SERVER_OSRTAPI_H
