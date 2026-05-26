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

#ifndef PROFILER_SERVER_FINDSLICEBYALLOCATIONTIMEHANDLER_H
#define PROFILER_SERVER_FINDSLICEBYALLOCATIONTIMEHANDLER_H
#include "RenderEngineInterface.h"
#include "OperatorMemoryService.h"
#include "MemoryRequestHandler.h"

namespace Dic::Module::Memory {
class FindSliceByAllocationTimeHandler : public MemoryRequestHandler {
  public:
    explicit FindSliceByAllocationTimeHandler(std::shared_ptr<Timeline::RenderEngineInterface> renderPtr,
        std::shared_ptr<OperatorMemoryService> operatorMemoryServicePtr = std::make_shared<OperatorMemoryService>())
        : renderEngine(std::move(renderPtr)), operatorMemoryService(std::move(operatorMemoryServicePtr)) {
        command = Protocol::REQ_RES_MEMORY_FIND_SLICE;
    };

    ~FindSliceByAllocationTimeHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    std::shared_ptr<Timeline::RenderEngineInterface> renderEngine = nullptr;
    std::shared_ptr<OperatorMemoryService> operatorMemoryService = nullptr;

    void populateResponseData(
        MemoryFindSliceResponse &response, OperatorDomain &target, Timeline::CompeteSliceDomain &slice);
};
} // namespace Dic::Module::Memory

#endif // PROFILER_SERVER_FINDSLICEBYALLOCATIONTIMEHANDLER_H
