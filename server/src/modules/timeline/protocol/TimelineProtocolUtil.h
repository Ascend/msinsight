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

#ifndef PROFILER_SERVER_TIMELINE_PROTOCOL_UTIL_H
#define PROFILER_SERVER_TIMELINE_PROTOCOL_UTIL_H

#include <optional>
#include "TimelineProtocolResponse.h"
#include "TimelineProtocolEvent.h"

namespace Dic {
namespace Protocol {
void SetBodyAtt(
    const ImportActionResponse &response, MemoryPoolAllocator<::rapidjson::CrtAllocator> &allocator, json_t &body);
// response
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);
template <> std::optional<document_t> ToResponseJson<ImportActionResponse>(const ImportActionResponse &response);
template <>
std::optional<document_t> ToResponseJson<UnitThreadTracesResponse>(const UnitThreadTracesResponse &response);
template <>
std::optional<document_t> ToResponseJson<UnitThreadTracesSummaryResponse>(
    const UnitThreadTracesSummaryResponse &response);
template <> std::optional<document_t> ToResponseJson<CreateCurveResponse>(const CreateCurveResponse &response);
template <> std::optional<document_t> ToResponseJson<UnitThreadsResponse>(const UnitThreadsResponse &response);
template <>
std::optional<document_t> ToResponseJson<UnitThreadDetailResponse>(const UnitThreadDetailResponse &response);
template <> std::optional<document_t> ToResponseJson<UnitFlowsResponse>(const UnitFlowsResponse &response);
template <> std::optional<document_t> ToResponseJson<SetCardAliasResponse>(const SetCardAliasResponse &response);
template <> std::optional<document_t> ToResponseJson<ResetWindowResponse>(const ResetWindowResponse &response);
template <> std::optional<document_t> ToResponseJson<SearchCountResponse>(const SearchCountResponse &response);
template <> std::optional<document_t> ToResponseJson<SearchSliceResponse>(const SearchSliceResponse &response);
template <> std::optional<document_t> ToResponseJson<RemoteDeleteResponse>(const RemoteDeleteResponse &response);
template <>
std::optional<document_t> ToResponseJson<FlowCategoryListResponse>(const FlowCategoryListResponse &response);
template <>
std::optional<document_t> ToResponseJson<FlowCategoryEventsResponse>(const FlowCategoryEventsResponse &response);
template <> std::optional<document_t> ToResponseJson<UnitCounterResponse>(const UnitCounterResponse &response);
template <> std::optional<document_t> ToResponseJson<SystemViewResponse>(const SystemViewResponse &response);
template <>
std::optional<document_t> ToResponseJson<ExpAnaAICoreFreqResponse>(const ExpAnaAICoreFreqResponse &response);
template <> std::optional<document_t> ToResponseJson<EventsViewResponse>(const EventsViewResponse &response);
template <> std::optional<document_t> ToResponseJson<KernelDetailsResponse>(const KernelDetailsResponse &response);
template <> std::optional<document_t> ToResponseJson<KernelE2ETimeResponse>(const KernelE2ETimeResponse &response);
template <> std::optional<document_t> ToResponseJson<OneKernelResponse>(const OneKernelResponse &response);
template <>
std::optional<document_t> ToResponseJson<UnitThreadsOperatorsResponse>(const UnitThreadsOperatorsResponse &response);
template <> std::optional<document_t> ToResponseJson<SearchAllSlicesResponse>(const SearchAllSlicesResponse &response);
template <>
std::optional<document_t> ToResponseJson<TableDataNameListResponse>(const TableDataNameListResponse &response);
template <> std::optional<document_t> ToResponseJson<TableDataDetailResponse>(const TableDataDetailResponse &response);
template <> std::optional<document_t> ToResponseJson<ParseCardsResponse>(const ParseCardsResponse &response);
template <>
std::optional<document_t> ToResponseJson<CommunicationKernelResponse>(const CommunicationKernelResponse &response);
template <>
std::optional<document_t> ToResponseJson<SystemViewOverallResponse>(const SystemViewOverallResponse &response);
template <>
std::optional<document_t> ToResponseJson<SystemViewFtraceStatResponse>(const SystemViewFtraceStatResponse &response);
template <> std::optional<document_t> ToResponseJson<RankOffsetResponse>(const RankOffsetResponse &response);
// event
template <typename EVENT> std::optional<document_t> ToEventJson(const EVENT &event);
template <> std::optional<document_t> ToEventJson<ParseSuccessEvent>(const ParseSuccessEvent &event);
template <> std::optional<document_t> ToEventJson<ParseFailEvent>(const ParseFailEvent &event);
template <> std::optional<document_t> ToEventJson<ParseClusterCompletedEvent>(const ParseClusterCompletedEvent &event);
template <> std::optional<document_t> ToEventJson<AllSuccessEvent>(const AllSuccessEvent &event);
template <> std::optional<document_t> ToEventJson<ParseMemoryCompletedEvent>(const ParseMemoryCompletedEvent &event);
template <> std::optional<document_t> ToEventJson<ModuleResetEvent>(const ModuleResetEvent &event);
template <> std::optional<document_t> ToEventJson<ParseProgressEvent>(const ParseProgressEvent &event);
template <> std::optional<document_t> ToEventJson<ParseHeatmapCompletedEvent>(const ParseHeatmapCompletedEvent &event);
template <> std::optional<document_t> ToEventJson<ParseUnitCompletedEvent>(const ParseUnitCompletedEvent &event);
} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_TIMELINE_PROTOCOL_UTIL_H
