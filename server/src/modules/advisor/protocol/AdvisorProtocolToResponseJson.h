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

#ifndef PROFILER_SERVER_ADVISORPROTOCOLTORESPONSEJSON_H
#define PROFILER_SERVER_ADVISORPROTOCOLTORESPONSEJSON_H

#include <optional>
#include "GlobalDefs.h"
#include "AdvisorProtocolResponse.h"

namespace Dic::Protocol {
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);
template <>
std::optional<document_t> ToResponseJson<AffinityOptimizerResponse>(const AffinityOptimizerResponse &response);
template <> std::optional<document_t> ToResponseJson<AffinityAPIResponse>(const AffinityAPIResponse &response);
template <> std::optional<document_t> ToResponseJson<OperatorFusionResponse>(const OperatorFusionResponse &response);
template <> std::optional<document_t> ToResponseJson<AICpuOperatorResponse>(const AICpuOperatorResponse &response);
template <> std::optional<document_t> ToResponseJson<AclnnOperatorResponse>(const AclnnOperatorResponse &response);
template <>
std::optional<document_t> ToResponseJson<OperatorDispatchResponse>(const OperatorDispatchResponse &response);

class AdvisorProtocolToResponseJson {
  public:
    static std::optional<document_t> ToAffinityOptimizerResponse(const Response &response);
    static std::optional<document_t> ToAffinityAPIResponse(const Response &response);
    static std::optional<document_t> ToOperatorFusionResponse(const Response &response);
    static std::optional<document_t> ToAICpuOperatorResponse(const Response &response);
    static std::optional<document_t> ToAclnnOperatorResponse(const Response &response);
    static std::optional<document_t> ToOperatorDispatchResponse(const Response &response);
};
}

#endif // PROFILER_SERVER_ADVISORPROTOCOLTORESPONSEJSON_H
