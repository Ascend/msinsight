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

loading_logger = get_logger("LOAD")
replay_logger = get_logger("REPLAY")


class SimulateDeviceSnapshot(HookerRegistry):
    device_snapshot: DeviceSnapshot
    hookers: Dict[int, SimulateHooker]

    device: int
    _loading_logger: Logger
    _replay_logger: Logger

    def __init__(self, snapshot_dict: dict, device: int):
        # 基于device初始化logger
        self._loading_logger = loading_logger.getChild(f"{device}")
        self._replay_logger = replay_logger.getChild(f"{device}")
        if not snapshot_dict:
            raise RuntimeError("Cannot init snapshot from empty data.")
        self._loading_logger.info("Loading snapshot data...")
        self.device_snapshot = DeviceSnapshot.from_dict(snapshot_dict, device, ignore_inactive_blocks=True)
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
        """
        背景：
            昇腾torch-npu在处理dump-snapshot时，粗暴的将每个dump时刻仍然存在的workspace（按流区分）池，分成三个连续的事件，且插入到snapshot事件列表的头
            T1. workspace_snapshot事件：workspace标志事件，具有size
            T2. segment_alloc事件：workspace segment扩容事件，size与workspace_snapshot中的size一致
            T3. alloc事件：全部分配掉segment_alloc，与segment_alloc中的size一致

            而该粗暴的适配造成了以下几个问题：
            1. workspace segment的状态错误
                - 其dump时刻仅存在一个完整size的inactive Block。这与其T3. alloc事件严重不符
                - active、allocated大小均为0，，导致数据不准确（社区Memory Viz可视化后同样存在该问题）
                - 预期在dump时刻的snapshot中，地址与workspace_snapshot相同且stream相同的segment，应该具有以下特征：
                    - active_size == allocated_size == total_size
                    - blocks的数组长度为1 且 blocks[0].size == total_size 且 blocks[0].state == active_allocated
            2. 无法真实反应workspace内存池的变化过程，仅记录dump时刻的状态。

        该方法旨在解决问题1的数据一致性问题，对于问题2需要pytorch-npu侧适配，以记录真实的内存池变化过程。
        """
        # 以连续事件组的形式，提取workspace相关事件
        # 遍历到不满足上述T1->T2->T3顺序的事件组或大小不正确时结束
        self._loading_logger.info("Recognized workspace events in snapshot, start adapting...")
        total_events = self.device_snapshot.trace_entries
        workspace_events_grp = []
        cur_grp_idx_start = 0
        while cur_grp_idx_start + 2 < len(total_events):
            workspace_events_grp = total_events[cur_grp_idx_start : cur_grp_idx_start + 3]
            if [event.action for event in workspace_events_grp] != ['workspace_snapshot', 'segment_alloc', 'alloc']:
                break
            workspace_snapshot = workspace_events_grp[0]
            segment_idx = self.device_snapshot.find_segment_idx_by_addr(
                workspace_snapshot.addr, workspace_snapshot.stream
            )
            if segment_idx < 0 or segment_idx >= len(self.device_snapshot.segments):
                self._loading_logger.warning(
                    "Workspace snapshot at addr %s (stream %s) not found in device snapshot",
                    workspace_snapshot.addr,
                    workspace_snapshot.stream,
                )
                break
            existed_seg = self.device_snapshot.segments[segment_idx]
            existed_seg.allocated_size = workspace_snapshot.size
            existed_seg.active_size = workspace_snapshot.size
            existed_seg.blocks = [
                Block(
                    size=existed_seg.total_size,
                    state=BlockState.ACTIVE_ALLOCATED,
                    address=existed_seg.address,
                    frames=existed_seg.frames,
                    segment_ptr=existed_seg,
                )
            ]
            cur_grp_idx_start += 3
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
