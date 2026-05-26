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

#ifndef PROFILER_SERVER_MEMORYPROTOCOLUTIL_H
#define PROFILER_SERVER_MEMORYPROTOCOLUTIL_H

#include <optional>
#include "GlobalDefs.h"
#include "MemoryProtocolRespose.h"

namespace Dic {
namespace Protocol {
// response
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);
template <>
std::optional<document_t> ToResponseJson<MemoryOperatorComparisonResponse>(
    const MemoryOperatorComparisonResponse &response);
template <>
std::optional<document_t> ToResponseJson<MemoryComponentComparisonResponse>(
    const MemoryComponentComparisonResponse &response);
template <> std::optional<document_t> ToResponseJson<MemoryViewResponse>(const MemoryViewResponse &response);
template <> std::optional<document_t> ToResponseJson<MemoryTypeResponse>(const MemoryTypeResponse &response);
template <>
std::optional<document_t> ToResponseJson<MemoryResourceTypeResponse>(const MemoryResourceTypeResponse &response);
template <>
std::optional<document_t> ToResponseJson<MemoryStaticOperatorGraphResponse>(
    const MemoryStaticOperatorGraphResponse &response);
template <>
std::optional<document_t> ToResponseJson<MemoryStaticOperatorSizeResponse>(
    const MemoryStaticOperatorSizeResponse &response);
template <>
std::optional<document_t> ToResponseJson<MemoryOperatorSizeResponse>(const MemoryOperatorSizeResponse &response);
std::optional<document_t> ToMemoryOperatorJson(const MemoryOperator &op, Document::AllocatorType &allocator);
std::optional<document_t> ToMemoryComponentJson(const MemoryComponent &component, Document::AllocatorType &allocator);
std::optional<document_t> ToMemoryStaticOperatorJson(const StaticOperatorItem &op, Document::AllocatorType &allocator);

} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_MEMORYPROTOCOLUTIL_H
