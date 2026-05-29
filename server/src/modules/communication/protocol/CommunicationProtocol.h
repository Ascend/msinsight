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

#ifndef PROFILER_SERVER_COMMUNICATION_PROTOCOL_H
#define PROFILER_SERVER_COMMUNICATION_PROTOCOL_H

#include "ProtocolMessage.h"

namespace Dic {
namespace Protocol {
class CommunicationProtocol : public ProtocolUtil {
  public:
    CommunicationProtocol() = default;
    ~CommunicationProtocol() override = default;

  private:
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;

    // json to request
    static std::unique_ptr<Request> ToOperatorDetailsRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDurationSlowRankRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToBandwidthDataRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDistributionRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToIterationsRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToDurationRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorNamesRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMatrixOpNamesRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMatrixGroupRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToMatrixListRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToCommunicationAdvisorRequest(const json_t &json, std::string &error);

    static std::optional<document_t> ToOperatorDetailsResponse(const Response &response);
    static std::optional<document_t> ToBandwidthDataResponse(const Response &response);
    static std::optional<document_t> ToDistributionResponse(const Response &response);
    static std::optional<document_t> ToIterationsResponse(const Response &response);
    static std::optional<document_t> ToOperatorNamesResponse(const Response &response);
    static std::optional<document_t> ToMatrixOpNamesResponse(const Response &response);
    static std::optional<document_t> ToDurationResponse(const Response &response);
    static std::optional<document_t> ToOperatorListResponse(const Response &response);
    static std::optional<document_t> ToMatrixGroupResponse(const Response &response);
    static std::optional<document_t> ToMatrixListResponse(const Response &response);
    static std::optional<document_t> ToCommunicationAdvisorResponse(const Response &response);
    static std::optional<document_t> ToDurationSlowRankResponse(const Dic::Protocol::Response &response);
    // response to json
};
} // namespace Protocol
} // namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_PROTOCOL_H
