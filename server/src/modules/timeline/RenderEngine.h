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

#ifndef PROFILER_SERVER_RENDERENGINE_H
#define PROFILER_SERVER_RENDERENGINE_H
#include <unordered_set>
#include "RenderEngineInterface.h"
#include "DataBaseManager.h"
namespace Dic::Module::Timeline {
class RenderEngine : public RenderEngineInterface {
  public:
    static std::shared_ptr<RenderEngine> Instance() {
        static std::shared_ptr<RenderEngine> instance = std::make_shared<RenderEngine>();
        return instance;
    }
    RenderEngine() = default;
    RenderEngine(const RenderEngine &) = delete;
    RenderEngine &operator=(const RenderEngine &) = delete;
    RenderEngine(RenderEngine &&) = delete;
    RenderEngine &operator=(RenderEngine &&) = delete;
    ~RenderEngine() override = default;
    void SetDataEngineInterface(std::shared_ptr<DataEngineInterface>) override;
    void QueryThreadTraces(const Protocol::UnitThreadTracesParams &requestParams,
        Protocol::UnitThreadTracesBody &responseBody, uint64_t minTimestamp, uint64_t traceId) override;
    bool QueryFlowCategoryEvents(Protocol::FlowCategoryEventsParams &params, uint64_t minTimestamp,
        std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) override;
    void QueryThreadDetail(const Protocol::ThreadDetailParams &requestParams,
        Protocol::UnitThreadDetailBody &responseBody, uint64_t trackId) override;
    CompeteSliceDomain FindSliceByTimePoint(
        const std::string &fileId, const std::string &name, uint64_t timePoint, const std::string &metaType) override;
    std::vector<CompeteSliceDomain> QuerySliceDetailByNameList(const std::string &fileId, const DataType &type,
        const std::string &processName, const std::vector<std::string> &nameList);
    std::vector<CompeteSliceDomain> QueryMstxRLDetail(const std::string &rankId, const DataType &type,
        const std::vector<std::string> &nameList, uint64_t startTime = UINT64_MAX, uint64_t endTime = 0);
    std::unordered_map<uint64_t, std::pair<std::string, std::string>> GetAllThreadInfo(const ThreadQuery &query);

  private:
    std::shared_ptr<DataEngineInterface> dataEngine = nullptr;
    const std::unordered_set<std::string> hideAbleNameSet = {"SET_FLAG", "WAIT_FLAG", "set_event", "wait_event"};

    void ComputeSimulationFlows(const Protocol::FlowCategoryEventsParams &params,
        std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList,
        std::vector<FlowPoint> &flowPointResult);

    std::vector<FlowPoint> ComputeLockRangePoints(
        Protocol::FlowCategoryEventsParams &params, std::vector<FlowPoint> &flowEventsVec) const;
};
}

#endif // PROFILER_SERVER_RENDERENGINE_H
