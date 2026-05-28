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

#ifndef PROFILER_SERVER_DATAENGINE_H
#define PROFILER_SERVER_DATAENGINE_H
#include "RepositoryFactoryInterface.h"
#include "DataEngineInterface.h"

namespace Dic::Module::Timeline {
class DataEngine : public DataEngineInterface {
  public:
    static std::shared_ptr<DataEngine> Instance() {
        static std::shared_ptr<DataEngine> instance = std::make_shared<DataEngine>();
        return instance;
    }
    DataEngine() = default;
    DataEngine(DataEngine &) = delete;
    DataEngine &operator=(DataEngine &) = delete;
    DataEngine(DataEngine &&) = delete;
    DataEngine &operator=(DataEngine &&) = delete;
    ~DataEngine() override = default;
    void SetRepositoryFactory(std::shared_ptr<RepositoryFactoryInterface> repositoryFactoryInterface) override;
    void QuerySimpleSliceWithOutNameByTrackId(
        const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override;
    void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override;
    uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) override;
    void QueryCompeteSliceVecByTimeRangeAndTrackId(
        const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override;
    void QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;
    void QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;
    void QueryAllThreadInfo(const ThreadQuery &flowQuery,
        std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) override;
    void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
        std::vector<CompeteSliceDomain> &CompeteSliceVec) override;
    void QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;
    void QueryAllFlagSlice(const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &competeSliceDomain) override;
    bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;

    bool QuerySliceByTimepointAndName(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override;
    bool QuerySliceDetailInfoByNameList(
        const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) override;

  private:
    std::shared_ptr<RepositoryFactoryInterface> respotoryFactory = nullptr;
};
}

#endif // PROFILER_SERVER_DATAENGINE_H
