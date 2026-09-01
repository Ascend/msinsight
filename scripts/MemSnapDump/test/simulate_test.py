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

import unittest
from pathlib import Path
from base import TraceEntry, DeviceSnapshot, Segment, Block, BlockState
from util.file_util import load_pickle_to_dict
from util.logger import suppress_logs, restore_logs
from simulate import SimulateDeviceSnapshot, SimulateHooker
from simulate.allocator_context import AllocatorContext
from simulate.hooker_defs import AllocatorHooker
from simulate.simulated_caching_allocator import SimulatedCachingAllocator
from test.common import valid_segments

test_data_dir = Path(__file__).parent.parent.resolve() / 'test' / 'test-data'


class ReplayEventHooker(SimulateHooker):
    def __init__(self, test_util, valid_interval: int = 100):
        self.test_util = test_util
        self.replay_count = 0
        self.valid_interval = valid_interval

    def pre_undo_event(self, wait4undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        return True

    def post_undo_event(self, already_undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        if self.replay_count % self.valid_interval == 0:
            valid_segments(current_snapshot.segments, self.test_util)
        self.replay_count += 1
        return True


class ReplayBlockHooker(AllocatorHooker):
    def __init__(self, test_util: unittest.TestCase):
        self.test_util = test_util
        self._segment = None
        self.pre_seg_allocated_size = 0
        self.pre_seg_active_size = 0
        self.pre_snapshot_total_allocated_size = 0
        self.pre_snapshot_total_active_size = 0

    def pre_replay_alloc_block(self, wait4alloc_block: Block, current_snapshot: DeviceSnapshot):
        super().pre_replay_alloc_block(wait4alloc_block, current_snapshot)
        self.test_util.assertNotEqual(wait4alloc_block.state, BlockState.INACTIVE)
        _segment_idx = current_snapshot.find_segment_idx_by_addr(wait4alloc_block.address)
        self.test_util.assertTrue(0 <= _segment_idx < len(current_snapshot.segments))
        self._segment = current_snapshot.segments[_segment_idx]
        self.pre_seg_allocated_size = self._segment.allocated_size
        self.pre_seg_active_size = self._segment.active_size
        self.pre_snapshot_total_allocated_size = current_snapshot.total_allocated
        self.pre_snapshot_total_active_size = current_snapshot.total_activated

    def post_replay_alloc_block(self, allocated_block: Block, current_snapshot: DeviceSnapshot):
        super().post_replay_alloc_block(allocated_block, current_snapshot)
        self.test_util.assertEqual(self.pre_seg_active_size + allocated_block.size, self._segment.active_size)
        self.test_util.assertEqual(
            self.pre_snapshot_total_active_size + allocated_block.size, current_snapshot.total_activated
        )
        if allocated_block.state == BlockState.ACTIVE_ALLOCATED:
            self.test_util.assertEqual(self.pre_seg_allocated_size + allocated_block.size, self._segment.allocated_size)
            self.test_util.assertEqual(
                self.pre_snapshot_total_allocated_size + allocated_block.size, current_snapshot.total_allocated
            )

    def post_replay_free_block(self, released_block: Block, current_snapshot: DeviceSnapshot):
        super().post_replay_free_block(released_block, current_snapshot)
        self.test_util.assertIsNotNone(released_block.segment_ptr)
        self.test_util.assertIn(released_block.segment_ptr, current_snapshot.segments)


class TestSimulate(unittest.TestCase):
    snapshot_path = test_data_dir / 'snapshot_1768383987920985470.pkl'
    vmem_snapshot_path = test_data_dir / 'snapshot_expandable.pkl'
    snapshot_with_empty_cache_path = test_data_dir / 'snapshot_with_empty_cache.pkl'
    vmem_snapshot_with_empty_cache_path = test_data_dir / 'snapshot_with_empty_cache_expandable.pkl'

    @classmethod
    def setUpClass(cls):
        suppress_logs()

    @classmethod
    def tearDownClass(cls):
        restore_logs()

    @staticmethod
    def get_simulate_snapshot(snapshot_path: Path):
        return SimulateDeviceSnapshot(load_pickle_to_dict(snapshot_path), 0)

    def testBlockHookerInSnapshot(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.snapshot_path)
        valid_segments(snapshot.device_snapshot.segments, self)
        snapshot.register_allocator_hooker(ReplayBlockHooker(self))
        self.assertTrue(snapshot.replay())

    def testBlockHookerInVmemSnapshot(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.vmem_snapshot_path)
        valid_segments(snapshot.device_snapshot.segments, self)
        snapshot.register_allocator_hooker(ReplayBlockHooker(self))
        self.assertTrue(snapshot.replay())

    def testBlockHookerInSnapshotWithEmptyCache(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.snapshot_with_empty_cache_path)
        valid_segments(snapshot.device_snapshot.segments, self)
        snapshot.register_allocator_hooker(ReplayBlockHooker(self))
        self.assertTrue(snapshot.replay())

    def testBlockHookerInVmemSnapshotWithEmptyCache(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.vmem_snapshot_with_empty_cache_path)
        valid_segments(snapshot.device_snapshot.segments, self)
        snapshot.register_allocator_hooker(ReplayBlockHooker(self))
        self.assertTrue(snapshot.replay())

    def testReplaySnapshot(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.snapshot_path)
        snapshot.register_hooker(ReplayEventHooker(self))
        self.assertTrue(snapshot.replay())

    def testReplayVmemSnapshot(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.vmem_snapshot_path)
        snapshot.register_hooker(ReplayEventHooker(self))
        self.assertTrue(snapshot.replay())

    def testReplaySnapshotWithEmptyCache(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.snapshot_with_empty_cache_path)
        snapshot.register_hooker(ReplayEventHooker(self))
        self.assertTrue(snapshot.replay())

    def testReplayVmemSnapshotWithEmptyCache(self):
        snapshot = TestSimulate.get_simulate_snapshot(self.vmem_snapshot_with_empty_cache_path)
        snapshot.register_hooker(ReplayEventHooker(self))
        self.assertTrue(snapshot.replay())


class CapturePostFreeHooker(AllocatorHooker):
    def __init__(self):
        self.released_block = None

    def post_replay_free_block(self, released_block: Block, current_snapshot: DeviceSnapshot):
        self.released_block = released_block


class TestPostFreeHookSegmentPtr(unittest.TestCase):
    def test_post_free_hook_keeps_original_segment_ptr(self):
        segment = Segment(
            address=0x10000,
            total_size=4096,
            stream=0,
            allocated_size=1024,
            active_size=1024,
            blocks=[],
        )
        block = Block(
            size=1024,
            requested_size=1024,
            address=0x10000,
            state=BlockState.ACTIVE_ALLOCATED,
            segment_ptr=segment,
        )
        segment.blocks.append(block)

        snapshot = DeviceSnapshot()
        snapshot.segments = [segment]
        snapshot.trace_entries = []
        snapshot.total_allocated = 1024
        snapshot.total_reserved = 4096
        snapshot.total_activated = 1024
        snapshot.device = 0

        allocator = SimulatedCachingAllocator(AllocatorContext(snapshot))
        hooker = CapturePostFreeHooker()
        allocator.register_hooker(hooker)

        alloc_event = TraceEntry(action="alloc", addr=0x10000, size=1024, stream=0, idx=1)
        self.assertTrue(allocator.free_block(alloc_event))
        self.assertIsNotNone(hooker.released_block)
        self.assertIs(hooker.released_block.segment_ptr, segment)
        self.assertIsNone(block.segment_ptr)
        self.assertEqual(segment.blocks, [])


def _workspace_snapshot_dict():
    return {
        "segments": [
            {
                "address": 1000,
                "total_size": 4096,
                "stream": 1,
                "segment_type": "small",
                "allocated_size": 0,
                "active_size": 0,
                "device": 0,
                "is_expandable": False,
                "frames": [],
                "blocks": [
                    {
                        "size": 4096,
                        "requested_size": 4096,
                        "state": "inactive",
                        "address": 1000,
                        "frames": [],
                    }
                ],
            }
        ],
        "device_traces": [
            [
                {"action": "workspace_snapshot", "addr": 1000, "size": 4096, "stream": 1, "frames": []},
                {"action": "segment_alloc", "addr": 1000, "size": 4096, "stream": 1, "frames": []},
                {"action": "alloc", "addr": 1000, "size": 4096, "stream": 1, "frames": []},
            ]
        ],
    }


class TestWorkspaceSnapshotAdapt(unittest.TestCase):
    def test_workspace_triplet_corrects_dump_time_occupancy(self):
        snapshot = SimulateDeviceSnapshot(_workspace_snapshot_dict(), 0)
        device_snapshot = snapshot.device_snapshot
        self.assertTrue(snapshot.simulated_allocator_context.workspace_flag)
        self.assertEqual(len(device_snapshot.segments), 1)
        seg = device_snapshot.segments[0]
        self.assertEqual(seg.allocated_size, 4096)
        self.assertEqual(seg.active_size, 4096)
        self.assertEqual(len(seg.blocks), 1)
        self.assertEqual(seg.blocks[0].state, BlockState.ACTIVE_ALLOCATED)
        self.assertEqual(device_snapshot.total_allocated, 4096)
        self.assertEqual(device_snapshot.total_activated, 4096)
        self.assertEqual(device_snapshot.total_reserved, 4096)
