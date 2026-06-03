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

#ifndef PROFILER_SERVER_MEMSNAPSHOTDEFS_H
#define PROFILER_SERVER_MEMSNAPSHOTDEFS_H
#include "pch.h"

namespace Dic::Module::MemSnapshot {
const std::string TRACE_ENTRY_ACTION_SEG_MAP = "segment_map";
const std::string TRACE_ENTRY_ACTION_SEG_UNMAP = "segment_unmap";
const std::string TRACE_ENTRY_ACTION_SEG_ALLOC = "segment_alloc";
const std::string TRACE_ENTRY_ACTION_SEG_FREE = "segment_free";
const std::string TRACE_ENTRY_ACTION_ALLOC = "alloc";
const std::string TRACE_ENTRY_ACTION_FREE_REQUESTED = "free_requested";
const std::string TRACE_ENTRY_ACTION_FREE_COMPLETED = "free_completed";
const std::string TRACE_ENTRY_ACTION_WORKSPACE = "workspace_snapshot";

const std::string BLOCK_STATE_INACTIVE = "inactive";
const std::string BLOCK_STATE_ACTIVE_ALLOC = "active_allocated";
const std::string BLOCK_STATE_ACTIVE_PENDING_FREE = "active_pending_free";

const std::string DETAIL_TYPE_BLOCK = "block";
const std::string DETAIL_TYPE_EVENT = "event";
const std::string DETAIL_TYPE_SEGMENT = "segment";

const std::set<std::string> VALID_DETAIL_TYPES = {DETAIL_TYPE_BLOCK, DETAIL_TYPE_EVENT, DETAIL_TYPE_SEGMENT};

struct TraceEntry {
    int64_t id{-1};
    std::string action;
    uint64_t address{0};
    uint64_t size{0};
    uint64_t stream{0};
    uint64_t allocated{0};
    uint64_t active{0};
    uint64_t reserved{0};
    std::string callstack;
};
struct Block {
    int64_t id{0};
    uint64_t address{0};
    uint64_t size{0};
    uint64_t requestedSize{0};
    std::string state;
    int64_t allocEventId{-1};
    int64_t freeEventId{-1};
};
struct MemoryRecord {
    int64_t id{0};
    uint64_t allocated{0};
    uint64_t reserved{0};
    uint64_t active{0};

    MemoryRecord() = default;
    MemoryRecord(int64_t id, uint64_t allocated, uint64_t reserved, uint64_t active)
        : id(id), allocated(allocated), reserved(reserved), active(active) {}
};
struct Segment {
    uint64_t address{0};
    uint64_t totalSize{0};
    uint64_t stream{0};
    uint64_t allocated{0};
    uint64_t active{0};
    std::vector<Block> blocks;
    int64_t allocOrMapEventId{-1};
    int64_t freeOrUnmapEventId{-1};

    Segment() = default;

    Segment(const uint64_t address = 0, const uint64_t totalSize = 0, const uint64_t stream = 0,
        const int64_t allocOrMapEventId = -1, const int64_t freeOrUnmapEventId = -1)
        : address(address), totalSize(totalSize), stream(stream), allocOrMapEventId(allocOrMapEventId),
          freeOrUnmapEventId(freeOrUnmapEventId) {}
};
}
#endif // PROFILER_SERVER_MEMSNAPSHOTDEFS_H
