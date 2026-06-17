/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#ifndef PROFILER_SERVER_QUERYTOPWARPSTALLREASONHANDLER_H
#define PROFILER_SERVER_QUERYTOPWARPSTALLREASONHANDLER_H

#include "SourceRequestHandler.h"

namespace Dic {
namespace Module {
namespace Source {
class QueryTopWarpStallReasonHandler : public SourceRequestHandler {
  public:
    QueryTopWarpStallReasonHandler() { command = Protocol::REQ_RES_TOP_WARP_STALL_REASON; }
    ~QueryTopWarpStallReasonHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
}
}
}
#endif // PROFILER_SERVER_QUERYTOPWARPSTALLREASONHANDLER_H
