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
import json
import sqlite3
import shutil
import unittest
from contextlib import closing
from pathlib import Path
from unittest import mock
from util.file_util import load_pickle_to_dict
from util.logger import suppress_logs, restore_logs
from base import Block, TraceEntry, DeviceSnapshot, BlockState
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

    def get_slice_db(self, output_dir: Path, device: int, slice_index: int = 0) -> Path:
        return output_dir / f"device_{device}" / f"slice_{slice_index:05d}.db"

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
        output_dir = self.cache_dir / self.snapshot_dump_db
        self.assertTrue(snapshot2db.dump(self.snapshot_path, output_dir, 0))
        slice_db = self.get_slice_db(output_dir, 0)
        self.assertTrue(slice_db.is_file())
        snapshot = SimulateDeviceSnapshot(load_pickle_to_dict(self.snapshot_path), 0)
        snapshot.register_hooker(SnapshotDbHookerForTest(slice_db, 0, self))
        self.assertTrue(snapshot.replay())

    def test_prepare_output_directory_removes_previous_slice_artifacts(self):
        output_dir = self.cache_dir / 'stale_artifacts'
        stale_device_dir = output_dir / 'device_0'
        stale_device_dir.mkdir(parents=True)
        (stale_device_dir / 'slice_00000.db').write_bytes(b'stale')
        (output_dir / snapshot2db.MANIFEST_FILE_NAME).write_text('{}', encoding='utf-8')
        (output_dir / f'{snapshot2db.MANIFEST_FILE_NAME}.tmp').write_text('{}', encoding='utf-8')

        snapshot2db._prepare_output_directory(output_dir)

        self.assertFalse(stale_device_dir.exists())
        self.assertFalse((output_dir / snapshot2db.MANIFEST_FILE_NAME).exists())
        self.assertFalse((output_dir / f'{snapshot2db.MANIFEST_FILE_NAME}.tmp').exists())

    def testVemSnapshot2Db(self):
        output_dir = self.cache_dir / self.vmem_snapshot_dump_db
        self.assertTrue(snapshot2db.dump(self.vmem_snapshot_path, output_dir, 0))
        slice_db = self.get_slice_db(output_dir, 0)
        self.assertTrue(slice_db.is_file())
        vmem_snapshot = SimulateDeviceSnapshot(load_pickle_to_dict(self.vmem_snapshot_path), 0)
        vmem_snapshot.register_hooker(SnapshotDbHookerForTest(slice_db, 0, self, is_expandable=True))
        self.assertTrue(vmem_snapshot.replay())

    def testEmptyDeviceSnapshot(self):
        self.assertFalse(snapshot2db.dump(self.snapshot_path, self.cache_dir / self.snapshot_dump_db, 1))
        self.assertFalse(os.path.exists(self.cache_dir / self.snapshot_dump_db))
        leftovers = list(self.cache_dir.glob(".*.tmp"))
        self.assertEqual(leftovers, [])

    def testFailedRedumpLeavesBuildingManifest(self):
        dest = self.cache_dir / "failed_redump"
        self.assertTrue(snapshot2db.dump(self.snapshot_path, dest, 0))
        original_replay = SimulateDeviceSnapshot.replay_until

        def fail_replay(self, _min_event_idx, _on_event_processed=None):
            return False

        SimulateDeviceSnapshot.replay_until = fail_replay
        try:
            self.assertFalse(snapshot2db.dump(self.snapshot_path, dest, 0))
        finally:
            SimulateDeviceSnapshot.replay_until = original_replay
        manifest = json.loads((dest / snapshot2db.MANIFEST_FILE_NAME).read_text(encoding="utf-8"))
        self.assertEqual(manifest["status"], "building")

    def testDatabaseInitializationFailureCleansTemporaryFile(self):
        dest = self.cache_dir / "database_init_failure"

        def fail_snapshot_db(path):
            Path(path).touch()
            raise RuntimeError("database initialization failed")

        with mock.patch.object(snapshot2db, "SnapshotDb", side_effect=fail_snapshot_db):
            self.assertFalse(snapshot2db.dump(self.snapshot_path, dest, 0))
        manifest = json.loads((dest / snapshot2db.MANIFEST_FILE_NAME).read_text(encoding="utf-8"))
        self.assertEqual(manifest["status"], "building")

    def testDumpAllMultipleDeviceSnapshot(self):
        output_dir = self.cache_dir / self.multi_devices_snapshot_dump_db
        self.assertTrue(snapshot2db.dump(self.multi_devices_snapshot_path, output_dir))
        self.assertTrue((output_dir / snapshot2db.MANIFEST_FILE_NAME).is_file())
        snapshot_0 = SimulateDeviceSnapshot(load_pickle_to_dict(self.multi_devices_snapshot_path), 0)
        snapshot_0.register_hooker(SnapshotDbHookerForTest(self.get_slice_db(output_dir, 0), 0, self))
        self.assertTrue(snapshot_0.replay())
        snapshot_1 = SimulateDeviceSnapshot(load_pickle_to_dict(self.multi_devices_snapshot_path), 1)
        snapshot_1.register_hooker(SnapshotDbHookerForTest(self.get_slice_db(output_dir, 1), 1, self))
        self.assertTrue(snapshot_1.replay())

    def testSliceManifestAndTruncatedBlockBackfill(self):
        snapshot_path = test_data_dir / 'snapshot_expandable.pkl'
        output_dir = self.cache_dir / 'sliced_snapshot'
        self.assertTrue(snapshot2db.dump(snapshot_path, output_dir, 0, events_per_slice=2000, cache_hash='hash-v1'))
        manifest = json.loads((output_dir / snapshot2db.MANIFEST_FILE_NAME).read_text(encoding='utf-8'))
        device = manifest['devices']['0']
        self.assertEqual(manifest['status'], 'complete')
        self.assertEqual(manifest['cacheHash'], 'hash-v1')
        self.assertEqual(device['sliceCount'], 5)
        self.assertEqual(device['readySlices'], [0, 1, 2, 3, 4])
        self.assertEqual(
            [(item['startEventId'], item['endEventId']) for item in device['slices']],
            [(0, 1999), (2000, 3999), (4000, 5999), (6000, 7999), (8000, 8091)],
        )

        block_slices = {}
        for item in device['slices']:
            with closing(sqlite3.connect(output_dir / item['file'])) as conn:
                for block_id, alloc_event_id in conn.execute(
                    'SELECT id, allocEventId FROM block_0 WHERE allocEventId >= 0'
                ):
                    self.assertEqual(block_id, alloc_event_id)
                    block_slices.setdefault(block_id, set()).add(item['index'])
        self.assertTrue(any(len(slice_indexes) > 1 for slice_indexes in block_slices.values()))

    def testBackfillsRunOnlyAfterAllSlicesAreReady(self):
        snapshot_path = test_data_dir / 'snapshot_expandable.pkl'
        output_dir = self.cache_dir / 'deferred_backfills'
        operation_order = []
        original_mark_ready = snapshot2db.SnapshotSliceManifest.mark_ready
        original_flush_backfills = snapshot2db.TruncatedBlockRegistry.flush_backfills

        def record_mark_ready(manifest, device, slice_index):
            operation_order.append(('ready', slice_index))
            return original_mark_ready(manifest, device, slice_index)

        def record_flush_backfills(registry):
            operation_order.append(('backfill', None))
            return original_flush_backfills(registry)

        with (
            mock.patch.object(snapshot2db.SnapshotSliceManifest, 'mark_ready', record_mark_ready),
            mock.patch.object(snapshot2db.TruncatedBlockRegistry, 'flush_backfills', record_flush_backfills),
        ):
            self.assertTrue(snapshot2db.dump(snapshot_path, output_dir, 0, events_per_slice=2000))

        ready_positions = [index for index, operation in enumerate(operation_order) if operation[0] == 'ready']
        backfill_positions = [index for index, operation in enumerate(operation_order) if operation[0] == 'backfill']
        self.assertEqual(len(ready_positions), 5)
        self.assertEqual(len(backfill_positions), 1)
        self.assertGreater(backfill_positions[0], max(ready_positions))

    def testTruncatedBlockReusesStableIdAcrossSlices(self):
        registry = snapshot2db.TruncatedBlockRegistry()
        block = Block(address=0x1000, size=1024, requested_size=1024, state=BlockState.ACTIVE_ALLOCATED)

        first_id = registry.register(block, 'slice_1.db', 0)
        second_id = registry.register(block, 'slice_0.db', 0)

        self.assertLess(first_id, 0)
        self.assertEqual(first_id, second_id)

    def testResolvedTruncatedBlockIdsBackfillToAllocEventId(self):
        registry = snapshot2db.TruncatedBlockRegistry()
        blocks = [
            Block(address=0x1000 + index * 0x1000, size=1024, requested_size=1024, state=BlockState.ACTIVE_ALLOCATED)
            for index in range(3)
        ]
        database_paths = [self.cache_dir / 'backfill_slice_1.db', self.cache_dir / 'backfill_slice_0.db']
        registered_ids = []
        for block in blocks:
            block_ids = [registry.register(block, str(path), 0) for path in database_paths]
            self.assertEqual(block_ids[0], block_ids[1])
            registered_ids.append(block_ids[0])

        for path in database_paths:
            with closing(sqlite3.connect(path)) as conn:
                conn.execute('CREATE TABLE block_0 (id INTEGER PRIMARY KEY, allocEventId INTEGER)')
                conn.executemany(
                    'INSERT INTO block_0 (id, allocEventId) VALUES (?, -1)',
                    [(block_id,) for block_id in registered_ids],
                )
                conn.commit()

        blocks[1].alloc_event_idx = 42
        registry.resolve(blocks[1])
        registry.flush_backfills()
        registry.compact_unresolved_block_ids()

        expected = [(-2, -1), (-1, -1), (42, 42)]
        for path in database_paths:
            with closing(sqlite3.connect(path)) as conn:
                records = conn.execute('SELECT id, allocEventId FROM block_0 ORDER BY id').fetchall()
                self.assertEqual(records, expected)
        self.assertEqual(registry.published_block_id(blocks[1]), 42)

    def testPublishedSliceBlockIdentitiesSurviveFinalization(self):
        snapshot_path = test_data_dir / 'snapshot_expandable.pkl'
        output_dir = self.cache_dir / 'stable_published_ids'
        ready_identities = {}
        original_mark_ready = snapshot2db.SnapshotSliceManifest.mark_ready

        def capture_ready(manifest, device, slice_index):
            original_mark_ready(manifest, device, slice_index)
            slice_db = self.get_slice_db(output_dir, device, slice_index)
            with closing(sqlite3.connect(slice_db)) as conn:
                ready_identities[slice_index] = conn.execute('SELECT id, address, size FROM block_0').fetchall()

        with mock.patch.object(snapshot2db.SnapshotSliceManifest, 'mark_ready', capture_ready):
            self.assertTrue(snapshot2db.dump(snapshot_path, output_dir, 0, events_per_slice=2000))
        self.assertEqual(sorted(ready_identities), [0, 1, 2, 3, 4])

        for slice_index in ready_identities:
            with closing(sqlite3.connect(self.get_slice_db(output_dir, 0, slice_index))) as conn:
                final_rows = conn.execute('SELECT id, address, size, allocEventId FROM block_0').fetchall()
            unresolved = sorted(block_id for block_id, _, _, alloc_event_id in final_rows if alloc_event_id < 0)
            if unresolved:
                self.assertEqual(unresolved, list(range(-len(unresolved), 0)))
            for block_id, _, _, alloc_event_id in final_rows:
                if alloc_event_id >= 0:
                    self.assertEqual(block_id, alloc_event_id)

    def testApplyBlockTableUpdatesRetriesSqliteLock(self):
        db_path = self.cache_dir / 'retry_lock.db'
        with closing(sqlite3.connect(db_path)) as conn:
            conn.execute('CREATE TABLE block_0 (id INTEGER PRIMARY KEY, allocEventId INTEGER)')
            conn.execute('INSERT INTO block_0 VALUES (1, -1)')
            conn.commit()

        attempts = {'count': 0}
        real_connect = sqlite3.connect

        def connect_with_lock(path, *args, **kwargs):
            attempts['count'] += 1
            if attempts['count'] == 1:
                raise sqlite3.OperationalError('database is locked')
            return real_connect(path, *args, **kwargs)

        with mock.patch.object(snapshot2db.sqlite3, 'connect', side_effect=connect_with_lock):
            snapshot2db._apply_block_table_updates(
                str(db_path),
                'UPDATE "block_0" SET "id" = ?, "allocEventId" = ? WHERE "id" = ?',
                [(42, 42, 1)],
            )
        self.assertGreaterEqual(attempts['count'], 2)
        with closing(sqlite3.connect(db_path)) as conn:
            self.assertEqual(conn.execute('SELECT id, allocEventId FROM block_0').fetchone(), (42, 42))

    def testBlockTableSqlRejectsInvalidDeviceAndQuotesKnownTable(self):
        self.assertEqual(
            snapshot2db._backfill_block_sql(0),
            'UPDATE "block_0" SET "id" = ?, "allocEventId" = ? WHERE "id" = ?',
        )
        self.assertEqual(snapshot2db._compact_block_id_sql(1), 'UPDATE "block_1" SET "id" = ? WHERE "id" = ?')
        with self.assertRaises(ValueError):
            snapshot2db._quoted_block_table_name(-1)

    def testDumpFailsWhenFinalizationBackfillFails(self):
        output_dir = self.cache_dir / 'finalization_lock_failure'
        with mock.patch.object(
            snapshot2db.TruncatedBlockRegistry,
            'flush_backfills',
            side_effect=sqlite3.OperationalError('database is locked'),
        ):
            self.assertFalse(snapshot2db.dump(self.snapshot_path, output_dir, 0))
        self.assertTrue(self.get_slice_db(output_dir, 0).is_file())
        manifest = json.loads((output_dir / snapshot2db.MANIFEST_FILE_NAME).read_text(encoding='utf-8'))
        self.assertEqual(manifest['status'], 'building')
