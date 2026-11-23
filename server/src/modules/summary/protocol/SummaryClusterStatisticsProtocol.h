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

#ifndef PROFILER_SERVER_SUMMARY_CLUSTER_STATISTICS_PROTOCOL_H
#define PROFILER_SERVER_SUMMARY_CLUSTER_STATISTICS_PROTOCOL_H

#include <vector>
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {
struct SummaryClusterStatisticsRequest : public Request {
    SummaryClusterStatisticsRequest() : Request(std::string_view{}) {}

    std::string clusterPath;
    std::string step;

    bool CheckParams(std::string &errorMsg) const {
        return CheckStrParamValid(clusterPath, errorMsg) && CheckStrParamValidEmptyAllowed(step, errorMsg);
    }
};

struct SummaryClusterTime {
    double computingNotOverlapped = 0;
    double communicationNotOverlapped = 0;
    double computingCommunicationOverlapped = 0;
    double free = 0;
    double preparing = 0;
};

struct SummaryRankTime {
    std::string rankId;
    SummaryClusterTime time;
    double total = 0;
    double computing = 0;
    double communication = 0;
    double computeRatio = 0;
    double communicationRatio = 0;
};

struct SummaryCommunicationTime {
    std::string rankId;
    double transit = 0;
    double wait = 0;
    double synchronization = 0;
    double free = 0;
};

struct SummaryTransportBandwidth {
    std::string transportType;
    double bandwidth = 0;
};

struct SummaryClusterTimeResponse : public Response {
    SummaryClusterTimeResponse() : Response(REQ_RES_SUMMARY_CLUSTER_TIME) {}
    SummaryClusterTime body;
};

struct SummaryRankTimeResponse : public Response {
    SummaryRankTimeResponse() : Response(REQ_RES_SUMMARY_RANK_TIME) {}
    std::vector<SummaryRankTime> body;
};

struct SummaryCommunicationTimeResponse : public Response {
    SummaryCommunicationTimeResponse() : Response(REQ_RES_SUMMARY_COMMUNICATION_TIME) {}
    std::vector<SummaryCommunicationTime> body;
};

struct SummaryClusterBandwidthResponse : public Response {
    SummaryClusterBandwidthResponse() : Response(REQ_RES_SUMMARY_CLUSTER_BANDWIDTH) {}
    std::vector<SummaryTransportBandwidth> body;
};
} // namespace Dic::Protocol

#endif // PROFILER_SERVER_SUMMARY_CLUSTER_STATISTICS_PROTOCOL_H
