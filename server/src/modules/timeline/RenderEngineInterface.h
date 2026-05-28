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

#ifndef PROFILER_SERVER_RENDERENGINEINTERFACE_H
#define PROFILER_SERVER_RENDERENGINEINTERFACE_H
#include <memory>
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "DataEngineInterface.h"
namespace Dic {
namespace Module {
namespace Timeline {
class RenderEngineInterface {
  public:
    virtual ~RenderEngineInterface() = default;
    virtual void SetDataEngineInterface(std::shared_ptr<DataEngineInterface>) = 0;
    virtual void QueryThreadTraces(const Protocol::UnitThreadTracesParams &requestParams,
        Protocol::UnitThreadTracesBody &responseBody, uint64_t minTimestamp, uint64_t traceId) = 0;
    virtual bool QueryFlowCategoryEvents(Protocol::FlowCategoryEventsParams &params, uint64_t minTimestamp,
        std::vector<std::unique_ptr<Protocol::UnitSingleFlow>> &flowDetailList) = 0;
    virtual void QueryThreadDetail(const Protocol::ThreadDetailParams &requestParams,
        Protocol::UnitThreadDetailBody &responseBody, uint64_t trackId) = 0;
    virtual CompeteSliceDomain FindSliceByTimePoint(
        const std::string &fileId, const std::string &name, uint64_t timePoint, const std::string &metaType) = 0;
};
}
}
}
#endif // PROFILER_SERVER_RENDERENGINEINTERFACE_H
