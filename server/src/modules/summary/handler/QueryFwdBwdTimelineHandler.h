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

#ifndef PROFILER_SERVER_QUERYFWDBWDTIMELINEHANDLER_H
#define PROFILER_SERVER_QUERYFWDBWDTIMELINEHANDLER_H

#include <map>
#include "SummaryProtocolResponse.h"
#include "SummaryRequestHandler.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Summary {

class QueryFwdBwdTimelineHandler : public SummaryRequestHandler {
  public:
    QueryFwdBwdTimelineHandler() { command = Protocol::REQ_RES_PIPELINE_FWD_BWD_TIMELINE; }
    ~QueryFwdBwdTimelineHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static bool QueryFwdBwdTimelineByRank(
        const std::string &rankId, const std::string &stepId, const std::string &clusterPath);
    static std::map<std::string, PipelineFwdBwdTimelineByRank> dataMap;
    static bool QueryFwdBwdTimelineFromFlow(const std::string &rankId, const std::string &stepId,
        const std::shared_ptr<Dic::Module::Timeline::VirtualTraceDatabase> &database);
    static bool QueryFwdBwdTimelineFromMstx(const std::string &rankId, const std::string &stepId,
        const std::shared_ptr<Dic::Module::Timeline::VirtualTraceDatabase> &database);
    static void CalFlowInfo(std::vector<FlowInfo> &flowList, const std::vector<std::string> &rankIds);
};

} // Dic::Module::Summary

#endif // PROFILER_SERVER_QUERYFWDBWDTIMELINEHANDLER_H
