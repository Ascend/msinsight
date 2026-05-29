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

#ifndef PROFILER_SERVER_COMMUNICATION_PROTOCOL_UTIL_H
#define PROFILER_SERVER_COMMUNICATION_PROTOCOL_UTIL_H

#include <optional>
#include "CommunicationProtocolResponse.h"

namespace Dic {
namespace Protocol {
// response
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);
template <> std::optional<document_t> ToResponseJson<OperatorDetailsResponse>(const OperatorDetailsResponse &response);
template <> std::optional<document_t> ToResponseJson<DistributionResponse>(const DistributionResponse &response);
template <> std::optional<document_t> ToResponseJson<BandwidthDataResponse>(const BandwidthDataResponse &response);
template <>
std::optional<document_t> ToResponseJson<IterationsOrRanksResponse>(const IterationsOrRanksResponse &response);
template <> std::optional<document_t> ToResponseJson<OperatorNamesResponse>(const OperatorNamesResponse &response);
template <>
std::optional<document_t> ToResponseJson<MatrixSortOpNamesResponse>(const MatrixSortOpNamesResponse &response);
template <> std::optional<document_t> ToResponseJson<DurationResponse>(const DurationResponse &response);
template <> std::optional<document_t> ToResponseJson<OperatorListsResponse>(const OperatorListsResponse &response);

template <> std::optional<document_t> ToResponseJson<MatrixGroupResponse>(const MatrixGroupResponse &response);
template <> std::optional<document_t> ToResponseJson<MatrixListResponse>(const MatrixListResponse &response);

template <>
std::optional<document_t> ToResponseJson<CommunicationAdvisorResponse>(const CommunicationAdvisorResponse &response);
template <>
std::optional<document_t> ToResponseJson<CommunicationSlowRankAnalysisResponse>(
    const CommunicationSlowRankAnalysisResponse &response);
std::optional<document_t> MatrixDataToJson(const MatrixData &matrixData, Document::AllocatorType &allocator);

} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_COMMUNICATION_PROTOCOL_UTIL_H
