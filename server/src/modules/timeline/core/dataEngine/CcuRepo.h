/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_CCUREPO_H
#define PROFILER_SERVER_CCUREPO_H

#include "SliceRepoInterface.h"
namespace Dic::Module::Timeline {
class CcuRepo : public IBaseSliceRepo {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &competeSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

  private:
    static void MergeArgs(document_t &json, const std::string &args);
};
}
#endif // PROFILER_SERVER_CCUREPO_H
