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

#include "pch.h"
#include "SummaryProtocolUtil.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"
#include "SummaryProtocol.h"

namespace Dic {
namespace Protocol {
void SummaryProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_SUMMARY_QUERY_TOP_DATA, ToTopNRequest);
    jsonToReqFactory.emplace(REQ_RES_SUMMARY_STATISTIC, ToStatisticsRequest);
    jsonToReqFactory.emplace(REQ_RES_COMPUTE_DETAIL, ToComputeDetailRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_DETAIL, ToCommunicationRequest);
    jsonToReqFactory.emplace(REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY, ToQueryParallelStrategyRequest);
    jsonToReqFactory.emplace(REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY, ToSetParallelStrategyRequest);
    jsonToReqFactory.emplace(REQ_RES_PIPELINE_FWD_BWD_TIMELINE, ToQueryFwdBwdTimelineRequest);
    jsonToReqFactory.emplace(REQ_RES_PARALLELISM_ARRANGEMENT_ALL, ToQueryParallelismArrangementRequest);
    jsonToReqFactory.emplace(REQ_RES_PARALLELISM_PERFORMANCE_DATA, ToQueryParallelismPerformanceRequest);
    jsonToReqFactory.emplace(REQ_RES_IMPORT_EXPERT_DATA, ToImportExpertDataRequest);
    jsonToReqFactory.emplace(REQ_RES_QUERY_EXPERT_HOTSPOT, ToQueryExpertHotspotRequest);
    jsonToReqFactory.emplace(REQ_RES_QUERY_MODEL_INFO, ToQueryModelInfoRequest);
    jsonToReqFactory.emplace(REQ_RES_SUMMARY_SLOW_RANK_ADVISOR, ToSummarySlowRankAdvisorRequest);
}

void SummaryProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_SUMMARY_QUERY_TOP_DATA, ToTopNResponse);
    resToJsonFactory.emplace(REQ_RES_SUMMARY_STATISTIC, ToStatisticsResponse);
    resToJsonFactory.emplace(REQ_RES_COMPUTE_DETAIL, ToComputeDetailResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_DETAIL, ToCommunicationResponse);
    resToJsonFactory.emplace(REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY, ToQueryParallelStrategyResponse);
    resToJsonFactory.emplace(REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY, ToSetParallelStrategyResponse);
    resToJsonFactory.emplace(REQ_RES_PIPELINE_FWD_BWD_TIMELINE, ToQueryFwdBwdTimelineResponse);
    resToJsonFactory.emplace(REQ_RES_PARALLELISM_ARRANGEMENT_ALL, ToQueryParallelismArrangementResponse);
    resToJsonFactory.emplace(REQ_RES_PARALLELISM_PERFORMANCE_DATA, ToQueryParallelismPerformanceResponse);
    resToJsonFactory.emplace(REQ_RES_IMPORT_EXPERT_DATA, ToImportExpertDataResponse);
    resToJsonFactory.emplace(REQ_RES_QUERY_EXPERT_HOTSPOT, ToQueryExpertHotspotResponse);
    resToJsonFactory.emplace(REQ_RES_QUERY_MODEL_INFO, ToQueryModelInfoResponse);
    resToJsonFactory.emplace(REQ_RES_SUMMARY_SLOW_RANK_ADVISOR, ToSummarySlowRankAdvisorResponse);
}

void SummaryProtocol::RegisterEventToJsonFuncs() {}

#pragma region <<Json To Request>>

std::unique_ptr<Request> SummaryProtocol::ToTopNRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SummaryTopRankRequest> reqPtr = std::make_unique<SummaryTopRankRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of topN request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToStatisticsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SummaryStatisticRequest> reqPtr = std::make_unique<SummaryStatisticRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of statistics request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stepId, json["params"], "stepId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timeFlag, json["params"], "timeFlag");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToComputeDetailRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ComputeDetailRequest> reqPtr = std::make_unique<ComputeDetailRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of compute detail request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.dbPath, json["params"], "dbPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timeFlag, json["params"], "timeFlag");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToCommunicationRequest(const json_t &json, std::string &error) {
    std::unique_ptr<CommunicationDetailRequest> reqPtr = std::make_unique<CommunicationDetailRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of communication request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.timeFlag, json["params"], "timeFlag");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryParallelStrategyRequest(const json_t &json, std::string &error) {
    std::unique_ptr<QueryParallelStrategyRequest> reqPtr = std::make_unique<QueryParallelStrategyRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query parallel strategy request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToSetParallelStrategyRequest(const json_t &json, std::string &error) {
    std::unique_ptr<SetParallelStrategyRequest> reqPtr = std::make_unique<SetParallelStrategyRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of set parallel strategy request.";
        return nullptr;
    }
    std::vector<std::string> keys = {KEY_ALGORITHM, KEY_TP_SIZE, KEY_PP_SIZE, KEY_DP_SIZE};
    for (auto &item : keys) {
        if (!json["params"].HasMember(item.c_str())) {
            error = "Set parallel strategy request didn't have key: " + item;
            return nullptr;
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.algorithm, json["params"], KEY_ALGORITHM);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.tpSize, json["params"], KEY_TP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.ppSize, json["params"], KEY_PP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.dpSize, json["params"], KEY_DP_SIZE);
    if (json["params"].HasMember(KEY_CP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.cpSize, json["params"], KEY_CP_SIZE);
    }
    if (json["params"].HasMember(KEY_EP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.epSize, json["params"], KEY_EP_SIZE);
    }
    if (json["params"].HasMember(KEY_MOE_TP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.moeTpSize, json["params"], KEY_MOE_TP_SIZE);
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryFwdBwdTimelineRequest(const json_t &json, std::string &error) {
    std::unique_ptr<PipelineFwdBwdTimelineRequest> reqPtr = std::make_unique<PipelineFwdBwdTimelineRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query fwd/bwd timeline request.";
        return nullptr;
    }
    if (!json.HasMember("params") || !json["params"].HasMember("stepId") || !json["params"].HasMember("stageId")) {
        error = "Failed to set request parameter of query fwd/bwd timeline request due to missing parameter.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stepId, json["params"], "stepId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stageId, json["params"], "stageId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryParallelismArrangementRequest(const json_t &json, std::string &error) {
    std::unique_ptr<QueryParallelismArrangementRequest> reqPtr = std::make_unique<QueryParallelismArrangementRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query parallelism arrangement request.";
        return nullptr;
    }
    std::vector<std::string> keys = {KEY_ALGORITHM, KEY_TP_SIZE, KEY_PP_SIZE, KEY_DP_SIZE, KEY_EP_SIZE, KEY_DIMENSION};
    for (auto &item : keys) {
        if (!json["params"].HasMember(item.c_str())) {
            error = "Query parallelism arrangement request didn't have key: " + item;
            return nullptr;
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.algorithm, json["params"], KEY_ALGORITHM);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.tpSize, json["params"], KEY_TP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.ppSize, json["params"], KEY_PP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.dpSize, json["params"], KEY_DP_SIZE);
    if (json["params"].HasMember(KEY_CP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.cpSize, json["params"], KEY_CP_SIZE);
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.epSize, json["params"], KEY_EP_SIZE);
    if (json["params"].HasMember(KEY_MOE_TP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.moeTpSize, json["params"], KEY_MOE_TP_SIZE);
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.dimension, json["params"], KEY_DIMENSION);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryParallelismPerformanceRequest(const json_t &json, std::string &error) {
    std::unique_ptr<QueryParallelismPerformanceRequest> reqPtr = std::make_unique<QueryParallelismPerformanceRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query parallelism performance request.";
        return nullptr;
    }
    std::vector<std::string> keys = {
        KEY_ALGORITHM, KEY_TP_SIZE, KEY_PP_SIZE, KEY_DP_SIZE, KEY_EP_SIZE, KEY_DIMENSION, KEY_STEP};
    for (auto &item : keys) {
        if (!json["params"].HasMember(item.c_str())) {
            error = "Query parallelism performance request didn't have key: " + item;
            return nullptr;
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.algorithm, json["params"], KEY_ALGORITHM);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.tpSize, json["params"], KEY_TP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.ppSize, json["params"], KEY_PP_SIZE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.dpSize, json["params"], KEY_DP_SIZE);
    if (json["params"].HasMember(KEY_CP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.cpSize, json["params"], KEY_CP_SIZE);
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.config.epSize, json["params"], KEY_EP_SIZE);
    if (json["params"].HasMember(KEY_MOE_TP_SIZE.c_str())) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.config.moeTpSize, json["params"], KEY_MOE_TP_SIZE);
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.dimension, json["params"], KEY_DIMENSION);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], KEY_IS_COMPARE);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineStep, json["params"], KEY_BASELINE_STEP);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.step, json["params"], KEY_STEP);
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToImportExpertDataRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ImportExpertDataRequest> reqPtr = std::make_unique<ImportExpertDataRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of import expert data request.";
        return nullptr;
    }
    if (!json.HasMember("params")) {
        error = "Failed to set request parameter of import expert data request due to missing parameter.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.filePath, json["params"], "filePath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.version, json["params"], "version");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryExpertHotspotRequest(const json_t &json, std::string &error) {
    std::unique_ptr<QueryExpertHotspotRequest> reqPtr = std::make_unique<QueryExpertHotspotRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query expert hotspot data request.";
        return nullptr;
    }
    if (!json.HasMember("params")) {
        error = "Failed to set request parameter of query expert hotspot data request due to missing parameter.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.modelStage, json["params"], "modelStage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.version, json["params"], "version");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.layerNum, json["params"], "layerNum");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.expertNum, json["params"], "expertNum");
    if (json["params"].HasMember("denseLayerList") && json["params"]["denseLayerList"].IsArray()) {
        for (const auto &denseLayer : json["params"]["denseLayerList"].GetArray()) {
            if (denseLayer.IsInt()) {
                reqPtr->params.denseLayerList.emplace_back(denseLayer.GetInt());
            }
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToQueryModelInfoRequest(const json_t &json, std::string &error) {
    std::unique_ptr<QueryModelInfoRequest> reqPtr = std::make_unique<QueryModelInfoRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of query model info request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> SummaryProtocol::ToSummarySlowRankAdvisorRequest(const json_t &json, std::string &error) {
    return ToQueryParallelismArrangementRequest(json, error);
}

#pragma endregion

#pragma region <<Response To Json>>

std::optional<document_t> SummaryProtocol::ToTopNResponse(const Response &response) {
    return ToResponseJson<SummaryTopRankResponse>(dynamic_cast<const SummaryTopRankResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToStatisticsResponse(const Response &response) {
    return ToResponseJson<SummaryStatisticsResponse>(dynamic_cast<const SummaryStatisticsResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToComputeDetailResponse(const Response &response) {
    return ToResponseJson<ComputeDetailResponse>(dynamic_cast<const ComputeDetailResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToCommunicationResponse(const Response &response) {
    return ToResponseJson<CommunicationDetailResponse>(dynamic_cast<const CommunicationDetailResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryParallelStrategyResponse(const Response &response) {
    return ToResponseJson<QueryParallelStrategyResponse>(dynamic_cast<const QueryParallelStrategyResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToSetParallelStrategyResponse(const Response &response) {
    return ToResponseJson<SetParallelStrategyResponse>(dynamic_cast<const SetParallelStrategyResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryFwdBwdTimelineResponse(const Response &response) {
    return ToResponseJson<PipelineFwdBwdTimelineResponse>(
        dynamic_cast<const PipelineFwdBwdTimelineResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryParallelismArrangementResponse(const Response &response) {
    return ToResponseJson<ParallelismArrangementResponse>(
        dynamic_cast<const ParallelismArrangementResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryParallelismPerformanceResponse(const Response &response) {
    return ToResponseJson<ParallelismPerformanceResponse>(
        dynamic_cast<const ParallelismPerformanceResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToImportExpertDataResponse(const Response &response) {
    return ToResponseJson<ImportExpertDataResponse>(dynamic_cast<const ImportExpertDataResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryExpertHotspotResponse(const Response &response) {
    return ToResponseJson<QueryExpertHotspotResponse>(dynamic_cast<const QueryExpertHotspotResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToQueryModelInfoResponse(const Response &response) {
    return ToResponseJson<QueryModelInfoResponse>(dynamic_cast<const QueryModelInfoResponse &>(response));
}

std::optional<document_t> SummaryProtocol::ToSummarySlowRankAdvisorResponse(const Response &response) {
    return ToResponseJson<SummarySlowRankAdvisorResponse>(
        dynamic_cast<const SummarySlowRankAdvisorResponse &>(response));
}
#pragma endregion
} // namespace Protocol
} // namespace Dic
