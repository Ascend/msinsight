/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#ifndef PROFILER_SERVER_QUERYFREEATTRIBUTIONHANDLER_H
#define PROFILER_SERVER_QUERYFREEATTRIBUTIONHANDLER_H

#include "TimelineRequestHandler.h"

namespace Dic::Module::Timeline {
class QueryFreeAttributionSummaryHandler : public TimelineRequestHandler {
  public:
    QueryFreeAttributionSummaryHandler() { command = Protocol::REQ_RES_FREE_ATTRIBUTION_SUMMARY; }
    bool HandleRequest(std::unique_ptr<Protocol::Request> request) override;
};

class QueryFreeAttributionDetailsHandler : public TimelineRequestHandler {
  public:
    QueryFreeAttributionDetailsHandler() { command = Protocol::REQ_RES_FREE_ATTRIBUTION_DETAILS; }
    bool HandleRequest(std::unique_ptr<Protocol::Request> request) override;
};
}
#endif
