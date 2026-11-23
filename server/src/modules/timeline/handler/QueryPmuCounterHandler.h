/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_QUERY_PMU_COUNTER_HANDLER_H
#define PROFILER_SERVER_QUERY_PMU_COUNTER_HANDLER_H
#include "TimelineRequestHandler.h"

namespace Dic::Module::Timeline {
class QueryPmuCounterHandler : public TimelineRequestHandler {
  public:
    QueryPmuCounterHandler() { command = Protocol::REQ_RES_PMU_COUNTER; }
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
}
#endif
