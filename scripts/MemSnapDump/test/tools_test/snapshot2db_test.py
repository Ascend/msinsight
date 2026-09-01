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

import os
import shutil
import unittest
from pathlib import Path
from unittest import mock
from util.file_util import load_pickle_to_dict
from util.logger import suppress_logs, restore_logs
from base import TraceEntry, DeviceSnapshot, BlockState
from tools.adaptors import snapshot2db
from simulate import SimulateDeviceSnapshot, SimulateHooker

from test.tools_test.snapshot_db_analyze import SnapshotDbHandler


class SnapshotDbHookerForTest(SimulateHooker):
    def __init__(self, dump_db_path: str, device: int, test_util: unittest.TestCase, is_expandable=False):
        self.db_handler = SnapshotDbHandler(dump_db_path, device)
        self.event_count = 0
        self.test_util = test_util
        self.is_expandable = is_expandable

    def pre_undo_event(self, wait4undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        self.event_count += 1
        if self.event_count % 100 == 0:
            db_segments = self.db_handler.get_segments_by_event_id(wait4undo_event.idx)
            self.test_util.assertEqual(len(db_segments), len(current_snapshot.segments))
            for i in range(len(db_segments)):
                db_segment = db_segments[i]
                snapshot_segment = current_snapshot.segments[i]
                self.test_util.assertEqual(db_segment.active_size, snapshot_segment.active_size)
                self.test_util.assertEqual(db_segment.total_size, snapshot_segment.total_size)
                idx = 0
                for seg_block in snapshot_segment.blocks:
                    if seg_block.state == BlockState.INACTIVE:
                        continue
                    db_block = db_segment.blocks[idx]
                    idx += 1
                    self.test_util.assertEqual(seg_block.size, db_block.size)
                    self.test_util.assertEqual(seg_block.requested_size, db_block.requested_size)
                    self.test_util.assertEqual(seg_block.address, db_block.address)
        return True

    def post_undo_event(self, already_undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        return True


test_data_dir = Path(__file__).parent.parent.resolve() / 'test-data'


class Snapshot2DbTest(unittest.TestCase):
    snapshot_path = test_data_dir / 'snapshot_with_empty_cache.pkl'
    vmem_snapshot_path = test_data_dir / 'snapshot_with_empty_cache_expandable.pkl'
    multi_devices_snapshot_path = test_data_dir / 'snapshot_with_multi_devices.pkl'
    snapshot_dump_db = 'leaks_dump_1.db'
    vmem_snapshot_dump_db = 'leaks_dump_2.db'
    multi_devices_snapshot_dump_db = 'multi_devices_dump.db'

    @classmethod
    def setUpClass(cls):
        suppress_logs()
        cls.cache_dir = test_data_dir / 'tmp'
        if os.path.exists(cls.cache_dir):
            shutil.rmtree(cls.cache_dir)
        os.mkdir(cls.cache_dir)

    @classmethod
    def tearDownClass(cls):
        restore_logs()
        shutil.rmtree(cls.cache_dir)

    def testSnapshot2Db(self):
        self.assertTrue(snapshot2db.dump(self.snapshot_path, self.cache_dir / self.snapshot_dump_db, 0))
        self.assertTrue(os.path.exists(self.cache_dir / self.snapshot_dump_db))
        snapshot = SimulateDeviceSnapshot(load_pickle_to_dict(self.snapshot_path), 0)
        snapshot.register_hooker(SnapshotDbHookerForTest(self.cache_dir / self.snapshot_dump_db, 0, self))
        self.assertTrue(snapshot.replay())

    def testVemSnapshot2Db(self):
        self.assertTrue(snapshot2db.dump(self.vmem_snapshot_path, self.cache_dir / self.vmem_snapshot_dump_db, 0))
        self.assertTrue(os.path.exists(self.cache_dir / self.vmem_snapshot_dump_db))
        vmem_snapshot = SimulateDeviceSnapshot(load_pickle_to_dict(self.vmem_snapshot_path), 0)
        vmem_snapshot.register_hooker(
            SnapshotDbHookerForTest(self.cache_dir / self.vmem_snapshot_dump_db, 0, self, is_expandable=True)
        )
        self.assertTrue(vmem_snapshot.replay())

    def testEmptyDeviceSnapshot(self):
        self.assertFalse(snapshot2db.dump(self.snapshot_path, self.cache_dir / self.snapshot_dump_db, 1))
        self.assertFalse(os.path.exists(self.cache_dir / self.snapshot_dump_db))
        leftovers = list(self.cache_dir.glob(".*.tmp"))
        self.assertEqual(leftovers, [])

    def testFailedRedumpPreservesExistingDatabase(self):
        dest = self.cache_dir / "preserve_on_fail.db"
        self.assertTrue(snapshot2db.dump(self.snapshot_path, dest, 0))
        original = dest.read_bytes()
        original_replay = SimulateDeviceSnapshot.replay

        def fail_replay(self):
            return False

        SimulateDeviceSnapshot.replay = fail_replay
        try:
            self.assertFalse(snapshot2db.dump(self.snapshot_path, dest, 0))
        finally:
            SimulateDeviceSnapshot.replay = original_replay
        self.assertTrue(dest.is_file())
        self.assertEqual(dest.read_bytes(), original)
        leftovers = list(self.cache_dir.glob(".*.tmp"))
        self.assertEqual(leftovers, [])

    def testDatabaseInitializationFailureCleansTemporaryFile(self):
        dest = self.cache_dir / "database_init_failure.db"
        dest.write_bytes(b"existing database")

        def fail_snapshot_db(path):
            Path(path).touch()
            raise RuntimeError("database initialization failed")

        with mock.patch.object(snapshot2db, "SnapshotDb", side_effect=fail_snapshot_db):
            with self.assertRaisesRegex(RuntimeError, "database initialization failed"):
                snapshot2db.dump(self.snapshot_path, dest, 0)

        self.assertEqual(dest.read_bytes(), b"existing database")
        leftovers = list(self.cache_dir.glob(f".{dest.name}.*.tmp"))
        self.assertEqual(leftovers, [])

    def testDumpAllMultipleDeviceSnapshot(self):
        self.assertTrue(
            snapshot2db.dump(self.multi_devices_snapshot_path, self.cache_dir / self.multi_devices_snapshot_dump_db)
        )
        self.assertTrue(os.path.exists(self.cache_dir / self.multi_devices_snapshot_dump_db))
        snapshot_0 = SimulateDeviceSnapshot(load_pickle_to_dict(self.multi_devices_snapshot_path), 0)
        snapshot_0.register_hooker(
            SnapshotDbHookerForTest(self.cache_dir / self.multi_devices_snapshot_dump_db, 0, self)
        )
        self.assertTrue(snapshot_0.replay())
        snapshot_1 = SimulateDeviceSnapshot(load_pickle_to_dict(self.multi_devices_snapshot_path), 1)
        snapshot_1.register_hooker(
            SnapshotDbHookerForTest(self.cache_dir / self.multi_devices_snapshot_dump_db, 1, self)
        )
        self.assertTrue(snapshot_1.replay())
