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

from typing import Dict

from util import get_logger
from logging import Logger
from base import DeviceSnapshot, Block, BlockState

from .allocator_context import AllocatorContext
from .allocator_hook_dispatcher import HookerRegistry
from .hooker_defs import SimulateHooker, AllocatorHooker
from .replay_executor import ReplayExecutor
from .simulated_caching_allocator import SimulatedCachingAllocator
from . import snapshot_lookup, snapshot_mutator

loading_logger = get_logger("LOAD")
replay_logger = get_logger("REPLAY")


class SimulateDeviceSnapshot(HookerRegistry):
    device_snapshot: DeviceSnapshot
    hookers: Dict[int, SimulateHooker]

    device: int
    _loading_logger: Logger
    _replay_logger: Logger

    def __init__(self, snapshot_dict: dict, device: int, *, _raw_frames: bool = False):
        # 基于device初始化logger
        self._loading_logger = loading_logger.getChild(f"{device}")
        self._replay_logger = replay_logger.getChild(f"{device}")
        if not snapshot_dict:
            raise RuntimeError("Cannot init snapshot from empty data.")
        self._loading_logger.info("Loading snapshot data...")
        self.device_snapshot = DeviceSnapshot.from_dict(
            snapshot_dict, device, ignore_inactive_blocks=True, _raw_frames=_raw_frames
        )
        self._loading_logger.info(
            "Finished to load snapshot data: total of %s entries and %s segments.",
            len(self.device_snapshot.trace_entries),
            len(self.device_snapshot.segments),
        )
        self.hookers = dict[int, SimulateHooker]()
        self.simulated_allocator_context = AllocatorContext(snapshot=self.device_snapshot)
        self.simulated_allocator = SimulatedCachingAllocator(self.simulated_allocator_context)
        self.replay_executor = ReplayExecutor(self.simulated_allocator, self._replay_logger)
        # 识别昇腾torch-npu采集的snapshot中的workspace事件
        if self.device_snapshot.trace_entries and self.device_snapshot.trace_entries[0].action == 'workspace_snapshot':
            self.simulated_allocator_context.workspace_flag = True
            # 适配npu workspace问题
            self._adapt_workspace_snapshot()

    def _adapt_workspace_snapshot(self):
        """Fix dump-time occupancy for torch-npu workspace pools.

        torch-npu prepends consecutive ``workspace_snapshot`` + ``segment_alloc``
        + ``alloc`` groups for each live workspace pool, but dumps the matching
        segment as one inactive block with allocated/active size 0. After
        ``ignore_inactive_blocks=True`` that block is gone, so replay cannot
        find it and dump-time curves stay at zero.

        Matched groups become a single ``active_allocated`` block covering the
        segment, with allocated/active/device totals equal to the workspace
        size. Missing segments, size mismatches, internally inconsistent
        triplets, or leftover live blocks warn and stop later groups.
        ``workspace_flag`` remains the fallback for those skipped cases.
        """
        self._loading_logger.info("Recognized workspace events in snapshot, start adapting...")
        snapshot = self.device_snapshot
        events = snapshot.trace_entries
        group_start = 0
        while group_start + 2 < len(events):
            workspace_snapshot = events[group_start]
            segment_alloc = events[group_start + 1]
            alloc = events[group_start + 2]
            if (
                workspace_snapshot.action != "workspace_snapshot"
                or segment_alloc.action != "segment_alloc"
                or alloc.action != "alloc"
            ):
                break
            if (
                segment_alloc.addr != workspace_snapshot.addr
                or segment_alloc.size != workspace_snapshot.size
                or alloc.addr != workspace_snapshot.addr
                or alloc.size != workspace_snapshot.size
            ):
                self._loading_logger.warning(
                    "Workspace snapshot triplet at addr %s (stream %s) is internally inconsistent",
                    workspace_snapshot.addr,
                    workspace_snapshot.stream,
                )
                break
            _, existed_seg = snapshot_lookup.find_segment(snapshot, workspace_snapshot.addr, workspace_snapshot.stream)
            if existed_seg is None:
                self._loading_logger.warning(
                    "Workspace snapshot at addr %s (stream %s) not found in device snapshot",
                    workspace_snapshot.addr,
                    workspace_snapshot.stream,
                )
                break
            if existed_seg.total_size != workspace_snapshot.size:
                self._loading_logger.warning(
                    "Workspace snapshot at addr %s (stream %s) size %s does not match segment total_size %s",
                    workspace_snapshot.addr,
                    workspace_snapshot.stream,
                    workspace_snapshot.size,
                    existed_seg.total_size,
                )
                break
            if existed_seg.blocks:
                n_live = len(existed_seg.blocks)
                self._loading_logger.warning(
                    "Workspace snapshot at addr %s (stream %s) skipped because the matching segment still has %s live block%s",
                    workspace_snapshot.addr,
                    workspace_snapshot.stream,
                    n_live,
                    "s" if n_live != 1 else "",
                )
                break
            snapshot.total_allocated -= existed_seg.allocated_size
            snapshot.total_activated -= existed_seg.active_size
            existed_seg.allocated_size = 0
            existed_seg.active_size = 0
            existed_seg.blocks = []
            snapshot_mutator.attach_block(
                snapshot,
                existed_seg,
                Block(
                    size=existed_seg.total_size,
                    requested_size=existed_seg.total_size,
                    address=existed_seg.address,
                    state=BlockState.ACTIVE_ALLOCATED,
                    frames=existed_seg.frames,
                    segment_ptr=existed_seg,
                ),
                0,
            )
            group_start += 3
        self._loading_logger.info("Finished to adapt workspace snapshot.")

    def register_allocator_hooker(self, hooker: AllocatorHooker) -> int:
        return self.simulated_allocator.register_hooker(hooker)

    def unregister_allocator_hooker(self, hooker_id: int):
        self.simulated_allocator.unregister_hooker(hooker_id)

    def replay(self) -> bool:
        """
        开始仿真回放内存事件
        """
        # 倒序遍历
        total_size = len(self.device_snapshot.trace_entries)
        self._replay_logger.info("Replaying %s entries in snapshot...", total_size)
        progress_update_point = [0.25, 0.5, 0.75]
        while self.device_snapshot.trace_entries:
            for hooker in self.hookers.values():
                if hooker and not hooker.pre_undo_event(self.device_snapshot.trace_entries[-1], self.device_snapshot):
                    self._replay_logger.error(
                        "An interruption occurred during the replay of the single event pre hook."
                    )
                    return False
            event = self.device_snapshot.trace_entries[-1]
            self.simulated_allocator_context.set_current_undo_event(event)
            if not self.replay_executor.execute(event):
                self._replay_logger.error("An interruption occurred during the replay of the single event.")
                return False
            self.device_snapshot.trace_entries.pop()
            current_size = len(self.device_snapshot.trace_entries)
            if progress_update_point and progress_update_point[-1] * total_size >= current_size:
                self._replay_logger.info(
                    "%s%% of entries have been processed, %s entries remain.",
                    (1 - progress_update_point[-1]) * 100,
                    current_size,
                )
                progress_update_point.pop()
            for hooker in self.hookers.values():
                if hooker and not hooker.post_undo_event(event, self.device_snapshot):
                    self._replay_logger.error(
                        "An interruption occurred during the replay of the single event post hook."
                    )
                    return False
        self._replay_logger.info("All events have been successfully replayed.")
        return True
