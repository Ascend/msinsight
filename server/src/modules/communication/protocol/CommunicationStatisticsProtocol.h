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
#ifndef PROFILER_SERVER_COMMUNICATION_STATISTICS_PROTOCOL_H
#define PROFILER_SERVER_COMMUNICATION_STATISTICS_PROTOCOL_H

#include "ProtocolMessage.h"
#include "ProtocolDefs.h"
#include "CommunicationStatistics.h"

namespace Dic::Protocol {
struct CommunicationStatisticsParams {
    std::string clusterPath;
    std::string iterationId;
    bool CheckParams(std::string &error) const;
    std::string GetStep() const;
};

struct GroupBandwidthParams : CommunicationStatisticsParams {
    std::string transportType;
    bool CheckParams(std::string &error) const;
};

struct GroupStatisticsParams : CommunicationStatisticsParams {
    std::string groupName;
    bool CheckParams(std::string &error) const;
};

struct RankStatisticsParams : GroupStatisticsParams {
    Module::Communication::RankStatisticsOptions options;
    bool CheckParams(std::string &error) const;
};

struct CommunicationGroupRangeRatioRequest : Request {
    CommunicationGroupRangeRatioRequest() : Request(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO) {}
    CommunicationStatisticsParams params;
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error);
};

struct CommunicationGroupRangeRatioResponse : JsonResponse {
    CommunicationGroupRangeRatioResponse() : JsonResponse(REQ_RES_COMMUNICATION_GROUP_RANGE_RATIO) {}
    std::vector<Module::Communication::GroupStatistic> items;
    std::optional<document_t> ToJson() const override;
};

struct CommunicationGroupBandwidthRequest : Request {
    CommunicationGroupBandwidthRequest() : Request(REQ_RES_COMMUNICATION_GROUP_BANDWIDTH) {}
    GroupBandwidthParams params;
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error);
};

struct CommunicationGroupBandwidthResponse : JsonResponse {
    CommunicationGroupBandwidthResponse() : JsonResponse(REQ_RES_COMMUNICATION_GROUP_BANDWIDTH) {}
    std::vector<Module::Communication::GroupStatistic> items;
    std::optional<document_t> ToJson() const override;
};

struct CommunicationOperatorFluctuationRequest : Request {
    CommunicationOperatorFluctuationRequest() : Request(REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION) {}
    GroupStatisticsParams params;
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error);
};

struct CommunicationOperatorFluctuationResponse : JsonResponse {
    CommunicationOperatorFluctuationResponse() : JsonResponse(REQ_RES_COMMUNICATION_OPERATOR_FLUCTUATION) {}
    std::vector<Module::Communication::OperatorFluctuation> items;
    std::optional<document_t> ToJson() const override;
};

struct CommunicationRankStatisticsRequest : Request {
    CommunicationRankStatisticsRequest() : Request(REQ_RES_COMMUNICATION_RANK_STATISTICS) {}
    RankStatisticsParams params;
    static std::unique_ptr<Request> FromJson(const json_t &json, std::string &error);
};

struct CommunicationRankStatisticsResponse : JsonResponse {
    CommunicationRankStatisticsResponse() : JsonResponse(REQ_RES_COMMUNICATION_RANK_STATISTICS) {}
    std::vector<Module::Communication::RankStatistic> items;
    uint64_t total = 0;
    int64_t currentPage = 1;
    int64_t pageSize = 20;
    std::optional<document_t> ToJson() const override;
};
}

#endif
