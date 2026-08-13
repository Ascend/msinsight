/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_SINGLE_RANK_COMMUNICATION_JSON_PARSER_H
#define PROFILER_SERVER_SINGLE_RANK_COMMUNICATION_JSON_PARSER_H

#include <optional>
#include <string>

namespace Dic {
namespace Module {
namespace Timeline {

/**
 * Convert a single-rank profiler communication.json into the RANK_LOCAL
 * communication-detail schema used by Timeline.
 *
 * A cache next to traceDbPath is reused only while the canonical source path,
 * file size, and modification time match the metadata stored in that cache. A
 * newly parsed cache is published atomically, so a malformed or interrupted
 * source never replaces the last complete cache.
 *
 * @return the cache database path, or std::nullopt when validation/parsing fails.
 */
std::optional<std::string> PrepareSingleRankCommunicationJsonCache(
    const std::string &traceDbPath, const std::string &communicationJsonPath);

} // namespace Timeline
} // namespace Module
} // namespace Dic

#endif // PROFILER_SERVER_SINGLE_RANK_COMMUNICATION_JSON_PARSER_H
