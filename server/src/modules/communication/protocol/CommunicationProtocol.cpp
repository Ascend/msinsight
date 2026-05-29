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
#include "CommunicationProtocolUtil.h"
#include "CommunicationProtocolRequest.h"
#include "CommunicationProtocolResponse.h"
#include "CommunicationProtocol.h"

namespace Dic {
namespace Protocol {
void CommunicationProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_OPERATOR_DETAILS, ToOperatorDetailsRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_DISTRIBUTION, ToDistributionRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_BANDWIDTH, ToBandwidthDataRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_ITERATIONS, ToIterationsRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_OPERATORNAMES, ToOperatorNamesRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_SORT_OP, ToMatrixOpNamesRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_LIST, ToDurationRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_MATRIX_GROUP, ToMatrixGroupRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH, ToMatrixListRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_OPERATOR_LISTS, ToDurationRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_ADVISOR, ToCommunicationAdvisorRequest);
    jsonToReqFactory.emplace(REQ_RES_COMMUNICATION_DURATION_SLOW_RANK_LIST, ToDurationRequest);
}

void CommunicationProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_OPERATOR_DETAILS, ToOperatorDetailsResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_DISTRIBUTION, ToDistributionResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_BANDWIDTH, ToBandwidthDataResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_ITERATIONS, ToIterationsResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_OPERATORNAMES, ToOperatorNamesResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_SORT_OP, ToMatrixOpNamesResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_LIST, ToDurationResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_OPERATOR_LISTS, ToOperatorListResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_MATRIX_GROUP, ToMatrixGroupResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH, ToMatrixListResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_ADVISOR, ToCommunicationAdvisorResponse);
    resToJsonFactory.emplace(REQ_RES_COMMUNICATION_DURATION_SLOW_RANK_LIST, ToDurationSlowRankResponse);
}

void CommunicationProtocol::RegisterEventToJsonFuncs() {}

#pragma region <<Json To Request>>

std::unique_ptr<Request> CommunicationProtocol::ToOperatorDetailsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorDetailsRequest> reqPtr = std::make_unique<OperatorDetailsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of operator details.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.orderBy, json["params"], "orderBy");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.order, json["params"], "order");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pageSize, json["params"], "pageSize");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentPage, json["params"], "currentPage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.queryType, json["params"], "queryType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToDistributionRequest(const json_t &json, std::string &error) {
    std::unique_ptr<DistributionDataRequest> reqPtr = std::make_unique<DistributionDataRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of distribution request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.operatorName, json["params"], "operatorName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.transportType, json["params"], "transportType");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToBandwidthDataRequest(const json_t &json, std::string &error) {
    std::unique_ptr<BandwidthDataRequest> reqPtr = std::make_unique<BandwidthDataRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of bandwidth data request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.rankId, json["params"], "rankId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.operatorName, json["params"], "operatorName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToMatrixGroupRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MatrixGroupRequest> reqPtr = std::make_unique<MatrixGroupRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of matrix group request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineIterationId, json["params"], "baselineIterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToMatrixListRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MatrixBandwidthRequest> reqPtr = std::make_unique<MatrixBandwidthRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info to matrix list request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.operatorName, json["params"], "operatorName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineIterationId, json["params"], "baselineIterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineGroupIdHash, json["params"], "baselineGroupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToIterationsRequest(const json_t &json, std::string &error) {
    std::unique_ptr<IterationsRequest> reqPtr = std::make_unique<IterationsRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of iterations request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToDurationRequest(const json_t &json, std::string &error) {
    std::unique_ptr<DurationListRequest> reqPtr = std::make_unique<DurationListRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of duration request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.operatorName, json["params"], "operatorName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    if (json["params"].HasMember("targetOperatorName")) {
        JsonUtil::SetByJsonKeyValue(reqPtr->params.targetOperatorName, json["params"], "targetOperatorName");
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.isCompare, json["params"], "isCompare");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineIterationId, json["params"], "baselineIterationId");
    if (json["params"].HasMember("rankList") && json["params"]["rankList"].IsArray()) {
        for (const auto &rankId : json["params"]["rankList"].GetArray()) {
            if (rankId.IsString()) {
                reqPtr->params.rankList.emplace_back(rankId.GetString());
            }
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineGroupIdHash, json["params"], "baselineGroupIdHash");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToOperatorNamesRequest(const json_t &json, std::string &error) {
    std::unique_ptr<OperatorNamesRequest> reqPtr = std::make_unique<OperatorNamesRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of operator names request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    if (json["params"].HasMember("rankList") && json["params"]["rankList"].IsArray()) {
        for (const auto &rankId : json["params"]["rankList"].GetArray()) {
            if (rankId.IsString()) {
                reqPtr->params.rankList.emplace_back(rankId.GetString());
            }
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToMatrixOpNamesRequest(const json_t &json, std::string &error) {
    std::unique_ptr<MatrixSortOpNamesRequest> reqPtr = std::make_unique<MatrixSortOpNamesRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of matrix op names request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.iterationId, json["params"], "iterationId");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.stage, json["params"], "stage");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.pgName, json["params"], "pgName");
    if (json["params"].HasMember("rankList") && json["params"]["rankList"].IsArray()) {
        for (const auto &rankId : json["params"]["rankList"].GetArray()) {
            if (rankId.IsString()) {
                reqPtr->params.rankList.emplace_back(rankId.GetString());
            }
        }
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.groupIdHash, json["params"], "groupIdHash");
    return reqPtr;
}

std::unique_ptr<Request> CommunicationProtocol::ToCommunicationAdvisorRequest(
    const Dic::json_t &json, std::string &error) {
    std::unique_ptr<CommunicationAdvisorRequest> reqPtr = std::make_unique<CommunicationAdvisorRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info of communication advisor request.";
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.clusterPath, json["params"], "clusterPath");
    return reqPtr;
}
#pragma endregion

#pragma region <<Response To Json>>

std::optional<document_t> CommunicationProtocol::ToOperatorDetailsResponse(const Response &response) {
    return ToResponseJson<OperatorDetailsResponse>(dynamic_cast<const OperatorDetailsResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToBandwidthDataResponse(const Response &response) {
    return ToResponseJson<BandwidthDataResponse>(dynamic_cast<const BandwidthDataResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToDistributionResponse(const Response &response) {
    return ToResponseJson<DistributionResponse>(dynamic_cast<const DistributionResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToIterationsResponse(const Response &response) {
    return ToResponseJson<IterationsOrRanksResponse>(dynamic_cast<const IterationsOrRanksResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToOperatorNamesResponse(const Response &response) {
    return ToResponseJson<OperatorNamesResponse>(dynamic_cast<const OperatorNamesResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToMatrixOpNamesResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<MatrixSortOpNamesResponse>(dynamic_cast<const MatrixSortOpNamesResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToDurationResponse(const Response &response) {
    return ToResponseJson<DurationResponse>(dynamic_cast<const DurationResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToOperatorListResponse(const Response &response) {
    return ToResponseJson<OperatorListsResponse>(dynamic_cast<const OperatorListsResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToMatrixGroupResponse(const Response &response) {
    return ToResponseJson<MatrixGroupResponse>(dynamic_cast<const MatrixGroupResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToMatrixListResponse(const Response &response) {
    return ToResponseJson<MatrixListResponse>(dynamic_cast<const MatrixListResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToCommunicationAdvisorResponse(
    const Dic::Protocol::Response &response) {
    return ToResponseJson<CommunicationAdvisorResponse>(dynamic_cast<const CommunicationAdvisorResponse &>(response));
}

std::optional<document_t> CommunicationProtocol::ToDurationSlowRankResponse(const Dic::Protocol::Response &response) {
    return ToResponseJson<CommunicationSlowRankAnalysisResponse>(
        dynamic_cast<const CommunicationSlowRankAnalysisResponse &>(response));
}

#pragma endregion
} // namespace Protocol
} // namespace Dic
