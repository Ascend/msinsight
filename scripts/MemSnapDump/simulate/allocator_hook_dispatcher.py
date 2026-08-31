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

import copy

from base import Block, DeviceSnapshot, Segment
from .hooker_defs import AllocatorHooker


class HookerRegistry:
    hookers: dict

    def register_hooker(self, hooker):
        idx = hash(hooker)
        self.hookers[idx] = hooker
        return idx

    def unregister_hooker(self, hooker_id: int):
        if hooker_id in self.hookers:
            del self.hookers[hooker_id]


class AllocatorHookDispatcher(HookerRegistry):
    def __init__(self):
        self.hookers: dict[int, AllocatorHooker] = {}

    def pre_replay_alloc_block(self, block: Block, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.pre_replay_alloc_block(block, snapshot)

    def post_replay_alloc_block(self, block: Block, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.post_replay_alloc_block(block, snapshot)

    def pre_replay_free_block(self, block: Block, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.pre_replay_free_block(block, snapshot)

    def post_replay_free_block(self, block: Block, snapshot: DeviceSnapshot, use_copy: bool = False):
        payload = copy.copy(block) if use_copy else block
        for hooker in self.hookers.values():
            hooker.post_replay_free_block(payload, snapshot)

    def pre_replay_map_or_alloc_segment(self, segment: Segment, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.pre_replay_map_or_alloc_segment(segment, snapshot)

    def post_replay_map_or_alloc_segment(self, segment: Segment, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.post_replay_map_or_alloc_segment(segment, snapshot)

    def pre_replay_unmap_or_free_segment(self, segment: Segment, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.pre_replay_unmap_or_free_segment(segment, snapshot)

    def post_replay_unmap_or_free_segment(self, segment: Segment, snapshot: DeviceSnapshot):
        for hooker in self.hookers.values():
            hooker.post_replay_unmap_or_free_segment(segment, snapshot)
