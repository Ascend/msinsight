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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERY_SYSTEM_VIEW_FTRACE_STAT_HANDLER_H
#define PROFILER_SERVER_QUERY_SYSTEM_VIEW_FTRACE_STAT_HANDLER_H

#include "TimelineRequestHandler.h"
#include "DataBaseManager.h"
#include "TextTraceDatabase.h"

namespace Dic {
namespace Module {
namespace Timeline {

class QuerySystemViewFtraceStatHandler : public TimelineRequestHandler {
  public:
    QuerySystemViewFtraceStatHandler() { command = Protocol::REQ_RES_SYSTEM_VIEW_FTRACE_STAT; };

    ~QuerySystemViewFtraceStatHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};

} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_QUERY_SYSTEM_VIEW_FTRACE_STAT_HANDLER_H
