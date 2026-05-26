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

#ifndef PROFILER_SERVER_OPERATORPROTOCOL_H
#define PROFILER_SERVER_OPERATORPROTOCOL_H

#include "pch.h"
#include "OperatorProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {

const std::string OP_TYPE_GROUP = "Operator Type";
const std::string INPUT_SHAPE_GROUP = "Input Shape";
const std::string OPERATOR_GROUP = "Operator";

class OperatorProtocol : public ProtocolUtil {
  public:
    OperatorProtocol() = default;
    ~OperatorProtocol() override = default;

  private:
    void RegisterJsonToRequestFuncs() override;
    void RegisterResponseToJsonFuncs() override;
    void RegisterEventToJsonFuncs() override;

    static std::unique_ptr<Request> ToOperatorCategoryInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorComputeUnitInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorStatisticInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorDetailInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorMoreInfoRequest(const json_t &json, std::string &error);
    static std::unique_ptr<Request> ToOperatorExportDetailsRequest(const json_t &json, std::string &error);

    static std::optional<document_t> ToOperatorCategoryInfoResponse(const Response &response);
    static std::optional<document_t> ToOperatorComputeUnitInfoResponse(const Response &response);
    static std::optional<document_t> ToOperatorStatisticInfoResponse(const Response &response);
    static std::optional<document_t> ToOperatorDetailInfoResponse(const Response &response);
    static std::optional<document_t> ToOperatorMoreInfoResponse(const Response &response);
    static std::optional<document_t> ToOperatorExportDetailsResponse(const Response &response);

    template <typename T>
    static void ToOperatorInfoRequestFilters(std::unique_ptr<T> &reqPtr, const json_t &json, std::string &error);

    static std::optional<document_t> ToOperatorParseStatusEvent(const Event &event);
    static std::optional<document_t> ToOperatorParseClearEvent(const Event &event);
};
}

#endif // PROFILER_SERVER_OPERATORPROTOCOL_H
