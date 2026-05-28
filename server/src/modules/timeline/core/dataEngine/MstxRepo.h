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

#ifndef PROFILER_SERVER_MSTXREPO_H
#define PROFILER_SERVER_MSTXREPO_H
#include "MstxEventsTable.h"
#include "EnumMstxEventTypeTable.h"
#include "StringIdsTable.h"
#include "SliceRepoInterface.h"
namespace Dic::Module::Timeline {
class MstxRepo : public IBaseSliceRepo, public IFindSliceByNameList {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;
    bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) override;

  protected:
    std::unique_ptr<MstxEventsTable> mstxEventsTable = std::make_unique<MstxEventsTable>();
    std::unique_ptr<EnumMstxEventTypeTable> enumMstxEventTypeTable = std::make_unique<EnumMstxEventTypeTable>();
    std::unique_ptr<StringIdsTable> stringIdsTable = std::make_unique<StringIdsTable>();
};
}
#endif // PROFILER_SERVER_MSTXREPO_H
