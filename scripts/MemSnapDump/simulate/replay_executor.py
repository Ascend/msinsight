"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

    http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

from logging import Logger

from base import Block, BlockState, Segment, TraceEntry
from .simulated_caching_allocator import SimulatedCachingAllocator


class ReplayExecutor:
    def __init__(self, allocator: SimulatedCachingAllocator, replay_logger: Logger):
        self.allocator = allocator
        self.replay_logger = replay_logger

    def execute(self, event: TraceEntry) -> bool:
        if event.action in ["free", "free_completed"]:
            block = Block.build_from_event(event)
            block.state = BlockState.ACTIVE_ALLOCATED if event.action == "free" else BlockState.ACTIVE_PENDING_FREE
            return self.allocator.alloc_block(block)
        if event.action == "free_requested":
            return self.allocator.active_block(event)
        if event.action == "alloc":
            return self.allocator.free_block(event)
        if event.action in ["segment_free", "segment_unmap"]:
            segment = Segment.build_from_event(event)
            segment.free_or_unmap_event_idx = event.idx
            return self.allocator.alloc_or_map_segment(segment, merge=event.action == "segment_unmap")
        if event.action == "segment_alloc":
            return self.allocator.free_segment(event)
        if event.action == "segment_map":
            return self.allocator.unmap_segment(event)
        self.replay_logger.warning("Skip event%s during replay.", event.to_dict())
        return True
