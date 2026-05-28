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

#ifndef PROFILER_SERVER_QUERYOVERALLMOREDETAILS_H
#define PROFILER_SERVER_QUERYOVERALLMOREDETAILS_H
#include "TimelineRequestHandler.h"
#include "DataBaseManager.h"
#include "SystemViewOverallHelper.h"

namespace Dic::Module::Timeline {
class QueryOverallMoreDetailsHandler : public TimelineRequestHandler {
  public:
    QueryOverallMoreDetailsHandler() { command = Protocol::REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS; };

    ~QueryOverallMoreDetailsHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static void GetComputingOverallMetricDetails(const std::shared_ptr<VirtualTraceDatabase> &database,
        const SystemViewOverallReqParam &params, UnitThreadsOperatorsResponse &response, uint64_t minTimestamp,
        SystemViewOverallHelper &overallHelper);
    static void GetPagedData(std::vector<SameOperatorsDetails> &filteredEvents, const SystemViewOverallReqParam &params,
        UnitThreadsOperatorsResponse &response);
    static void GetCommunicationOverallMetricDetails(const std::shared_ptr<VirtualTraceDatabase> &database,
        const SystemViewOverallReqParam &params, UnitThreadsOperatorsResponse &response, uint64_t minTimestamp,
        SystemViewOverallHelper &overallHelper);
};
}
#endif // PROFILER_SERVER_QUERYOVERALLMOREDETAILS_H
