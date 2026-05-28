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

#ifndef PROFILER_SERVER_TIMELINE_REQUEST_HANDLER_H
#define PROFILER_SERVER_TIMELINE_REQUEST_HANDLER_H
#include <utility>

#include "RenderEngineInterface.h"
#include "ModuleRequestHandler.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "ProtocolDefs.h"
#include "TimelineErrorManager.h"

namespace Dic {
namespace Module {
namespace Timeline {
class TimelineRequestHandler : public ModuleRequestHandler {
  public:
    TimelineRequestHandler() { moduleName = MODULE_TIMELINE; }
    ~TimelineRequestHandler() override = default;
    bool HandleRequest(std::unique_ptr<Dic::Protocol::Request> requestPtr) override { return true; };
    void SetRenderEngine(std::shared_ptr<RenderEngineInterface> renderEngineInterface) {
        renderEngine = std::move(renderEngineInterface);
    };

  protected:
    std::shared_ptr<RenderEngineInterface> renderEngine = nullptr;
};
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_TIMELINE_REQUEST_HANDLER_H
