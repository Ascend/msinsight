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

#ifndef PROFILER_SERVER_OPERATORPROTOCOLUTIL_H
#define PROFILER_SERVER_OPERATORPROTOCOLUTIL_H

#include "GlobalDefs.h"
#include "OperatorProtocolEvent.h"
#include "OperatorProtocolResponse.h"

namespace Dic::Protocol {
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);

template <>
std::optional<document_t> ToResponseJson<OperatorCategoryInfoResponse>(const OperatorCategoryInfoResponse &res);

template <>
std::optional<document_t> ToResponseJson<OperatorComputeUnitInfoResponse>(const OperatorComputeUnitInfoResponse &res);

template <>
std::optional<document_t> ToResponseJson<OperatorStatisticInfoResponse>(const OperatorStatisticInfoResponse &res);

template <> std::optional<document_t> ToResponseJson<OperatorDetailInfoResponse>(const OperatorDetailInfoResponse &res);

template <> std::optional<document_t> ToResponseJson<OperatorMoreInfoResponse>(const OperatorMoreInfoResponse &res);

template <>
std::optional<document_t> ToResponseJson<OperatorExportDetailsResponse>(const OperatorExportDetailsResponse &res);

template <typename EVENT> std::optional<document_t> ToEventJson(const EVENT &event);

template <> std::optional<document_t> ToEventJson<OperatorParseStatusEvent>(const OperatorParseStatusEvent &event);

template <> std::optional<document_t> ToEventJson<OperatorParseClearEvent>(const OperatorParseClearEvent &event);
}

#endif // PROFILER_SERVER_OPERATORPROTOCOLUTIL_H
