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

#ifndef PROFILER_SERVER_PYTHONAPIREPO_H
#define PROFILER_SERVER_PYTHONAPIREPO_H
#include "SliceRepoInterface.h"
#include "PytorchApiTable.h"
#include "PytorchCallchainsTable.h"
#include "StringIdsTable.h"
namespace Dic::Module::Timeline {
class PythonApiRepo : public IBaseSliceRepo,
                      public IPythonFuncSlice,
                      public IFindSliceByTimepointAndName,
                      public IFindSliceByVagueNameAndTime {
  public:
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override;
    uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

    ~PythonApiRepo() override = default;

    bool QuerySliceByTimepointAndName(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

    bool QuerySliceByVagueNameAndTime(
        const Dic::Module::Timeline::SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &res) override;

  protected:
    std::unique_ptr<PytorchApiTable> pytorchApiTable = std::make_unique<PytorchApiTable>();
    std::unique_ptr<StringIdsTable> stringIdsTable = std::make_unique<StringIdsTable>();
    std::unique_ptr<PytorchCallchainsTable> pytorchCallchainsTable = std::make_unique<PytorchCallchainsTable>();

    std::string QuerySliceCallStack(const SliceQuery &sliceQuery, const PytorchApiPO &target);

    void QuerySliceArgs(
        const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain, const PytorchApiPO &target);

  private:
    const std::string pythonApiTid = "pytorch";
};
}
#endif // PROFILER_SERVER_PYTHONAPIREPO_H
