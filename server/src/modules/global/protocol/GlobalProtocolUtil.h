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

#ifndef PROFILER_SERVER_GLOBAL_PROTOCOL_UTIL_H
#define PROFILER_SERVER_GLOBAL_PROTOCOL_UTIL_H

#include <optional>
#include "GlobalProtocolResponse.h"
#include "GlobalProtocolEvent.h"

namespace Dic {
namespace Protocol {
// response
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response);
template <> std::optional<document_t> ToResponseJson<FilesGetResponse>(const FilesGetResponse &response);
template <> std::optional<document_t> ToResponseJson<TokenHeartCheckResponse>(const TokenHeartCheckResponse &response);
template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoUpdateResponse>(
    const ProjectExplorerInfoUpdateResponse &response);
template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoGetResponse>(
    const ProjectExplorerInfoGetResponse &response);
template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoDeleteResponse>(
    const ProjectExplorerInfoDeleteResponse &response);
template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoClearResponse>(
    const ProjectExplorerInfoClearResponse &response);
template <>
std::optional<document_t> ToResponseJson<ProjectCheckValidResponse>(const ProjectCheckValidResponse &response);
template <> std::optional<document_t> ToResponseJson<BaselineSettingResponse>(const BaselineSettingResponse &response);
template <> std::optional<document_t> ToResponseJson<BaselineCancelResponse>(const BaselineCancelResponse &response);
// event
template <typename EVENT> std::optional<document_t> ToEventJson(const EVENT &event);
template <> std::optional<document_t> ToEventJson<ReadFileFailEvent>(const ReadFileFailEvent &event);
} // end of namespace Protocol
} // end of namespace Dic

#endif // PROFILER_SERVER_GLOBAL_PROTOCOL_UTIL_H
