# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

"""Cache validation must not depend on consumer-added SQLite objects."""

import hashlib
import json
import sqlite3
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

from util.snapshot_cache import seal_manifest, source_hashes, validate_cache


class SnapshotCacheTest(unittest.TestCase):
    def test_member_log_is_not_duplicated_by_parent_handlers(self):
        import io
        import logging
        from unittest.mock import patch

        from util.snapshot_cache import LOGGER

        output = io.StringIO()
        handler = logging.StreamHandler(output)
        self.addCleanup(handler.close)
        with patch.object(LOGGER, "handlers", [handler]), patch.object(logging.getLogger(), "handlers", [handler]):
            validate_cache(self.directory, self.hash)
        self.assertEqual(output.getvalue().count("Snapshot cache member checked:"), 1)

    def setUp(self):
        # unittest owns the resource until addCleanup runs after each test.
        self.temp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.source = self.directory / "input.pkl"
        self.source.write_bytes(b"trusted-test-source")
        self.hash = source_hashes(self.source)[1]
        self.db = self.directory / "slice.db"
        with closing(sqlite3.connect(self.db)) as conn:
            conn.executescript(
                'CREATE TABLE trace_entry_0(id INTEGER PRIMARY KEY, action INTEGER, address INTEGER, '
                'size INTEGER, stream INTEGER, allocated INTEGER, active INTEGER, reserved INTEGER, callstack TEXT);'
                'CREATE TABLE block_0(id INTEGER PRIMARY KEY, address INTEGER, size INTEGER, requestedSize INTEGER, '
                'state INTEGER, allocEventId INTEGER, freeEventId INTEGER);'
                'CREATE TABLE dictionary("table" TEXT, "column" TEXT, "key" TEXT, "value" TEXT);'
                "INSERT INTO trace_entry_0 VALUES(0,4,100,10,0,10,10,20,'stack');"
                "INSERT INTO block_0 VALUES(0,100,10,10,1,0,-1);"
                "INSERT INTO dictionary VALUES('trace_entry_0','action','4','alloc');"
                "INSERT INTO dictionary VALUES('block_0','state','1','active_allocated');"
            )
        self.manifest = {
            "schemaVersion": 1,
            "status": "complete",
            "sourceFile": str(self.source),
            "cacheHash": self.hash,
            "eventsPerSlice": 500000,
            "devices": {
                "0": {
                    "eventCount": 1,
                    "sliceCount": 1,
                    "readySlices": [0],
                    "slices": [{"index": 0, "startEventId": 0, "endEventId": 0, "file": "slice.db", "ready": True}],
                }
            },
        }
        seal_manifest(self.directory, self.manifest)
        self.save()

    def save(self):
        (self.directory / "manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")

    def test_consumer_write_after_sidecar_check_is_allowed(self):
        from unittest.mock import patch

        from util import snapshot_cache

        connect = sqlite3.connect
        writers = []
        self.addCleanup(lambda: [writer.close() for writer in writers])

        def open_during_write(*args, **kwargs):
            # Start the consumer after preflight, when no sidecars existed.
            writer = connect(self.db)
            writers.append(writer)
            writer.execute("PRAGMA journal_mode=WAL")
            writer.execute("CREATE TABLE consumer_cache(value INTEGER)")
            writer.commit()
            return connect(*args, **kwargs)

        with patch.object(snapshot_cache.sqlite3, "connect", side_effect=open_during_write):
            self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_legacy_physical_seal_accepts_closed_wal_database(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("PRAGMA journal_mode=WAL")
        self.manifest["ptSnap"] = {"members": {"slice.db": source_hashes(self.db)[0]}}
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_source_hash_matches_shared_salt(self):
        raw, salted = source_hashes(self.source)
        self.assertEqual(raw, hashlib.sha256(self.source.read_bytes()).hexdigest())
        self.assertEqual(salted, hashlib.sha256(b"mem_snapshot_parser_v3" + self.source.read_bytes()).hexdigest())

    def test_previous_parser_version_requires_reparse(self):
        self.manifest["cacheHash"] = hashlib.sha256(b"mem_snapshot_parser_v2" + self.source.read_bytes()).hexdigest()
        self.save()
        with self.assertRaisesRegex(ValueError, "parser salt changed"):
            validate_cache(self.directory, source_hashes(self.source)[1])

    def test_extra_tables_indexes_and_dictionary_do_not_invalidate(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.executescript(
                'CREATE TABLE memory_allocation_cache_v1_0(value INTEGER);'
                'CREATE TABLE arbitrary_consumer_data(value TEXT);'
                'CREATE INDEX consumer_index ON block_0(address);'
                'ALTER TABLE trace_entry_0 ADD COLUMN consumer_note TEXT;'
                'CREATE VIEW consumer_view AS SELECT id FROM block_0;'
                'CREATE TRIGGER consumer_trigger AFTER UPDATE ON block_0 BEGIN SELECT 1; END;'
                "INSERT INTO arbitrary_consumer_data VALUES('extra');"
                "INSERT INTO dictionary VALUES('arbitrary_consumer_data','value','1','extra');"
            )
        (self.directory / "parse.log").write_text("consumer log")
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("VACUUM")
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_changed_source_is_rejected(self):
        self.source.write_bytes(b"changed")
        with self.assertRaises(ValueError):
            validate_cache(self.directory, source_hashes(self.source)[1])

    def test_closed_wal_cache_preserves_database_bytes(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("PRAGMA journal_mode=WAL")
        before = self.db.read_bytes()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        self.assertEqual(self.db.read_bytes(), before)
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_rollback_journal_is_rejected(self):
        Path(str(self.db) + '-journal').write_bytes(b'')
        with self.assertRaisesRegex(ValueError, 'rollback journal'):
            validate_cache(self.directory, self.hash)

    def test_deeply_nested_manifest_is_a_validation_error(self):
        (self.directory / 'manifest.json').write_text('[' * 200000 + '0' + ']' * 200000)
        with self.assertRaisesRegex(ValueError, 'nesting'):
            validate_cache(self.directory, self.hash)

    def test_member_validation_reports_completed_members(self):
        from unittest.mock import patch

        from util import snapshot_cache

        with patch.object(snapshot_cache.LOGGER, 'info') as info:
            validate_cache(self.directory, self.hash)
        info.assert_called_once_with('Snapshot cache member checked: %s', 'slice.db')

    def test_missing_or_corrupt_db_is_rejected_without_creating_it(self):
        self.db.unlink()
        with self.assertRaises(ValueError):
            validate_cache(self.directory, self.hash)
        self.assertFalse(self.db.exists())
        self.db.write_bytes(b"not a database")
        with self.assertRaises(sqlite3.DatabaseError):
            validate_cache(self.directory, self.hash)

    def test_incomplete_manifest_or_event_range_is_rejected(self):
        for mutation in (
            lambda: self.manifest.update(status="building"),
            lambda: self.manifest["devices"]["0"].update(readySlices=[]),
        ):
            mutation()
            self.save()
            with self.assertRaises(ValueError):
                validate_cache(self.directory, self.hash)
            self.manifest["status"] = "complete"
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("DELETE FROM trace_entry_0")
            conn.commit()
        self.manifest["devices"]["0"]["readySlices"] = [0]
        self.save()
        with self.assertRaises(ValueError):
            validate_cache(self.directory, self.hash)

    def test_missing_json_and_unsafe_member_are_rejected(self):
        (self.directory / "manifest.json").unlink()
        with self.assertRaises(OSError):
            validate_cache(self.directory, self.hash)
        self.manifest["devices"]["0"]["slices"][0]["file"] = "../outside.db"
        self.save()
        with self.assertRaises(ValueError):
            validate_cache(self.directory, self.hash)

    def test_legacy_complete_artifact_is_structurally_validated(self):
        del self.manifest["parserCache"]
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_parser_device_selection_does_not_hide_producer_selection(self):
        self.manifest["ptSnap"] = {"identity": {"requestedDevice": 0}}
        self.save()
        with self.assertRaisesRegex(ValueError, "selected device"):
            validate_cache(self.directory, self.hash)
        self.assertEqual(validate_cache(self.directory, self.hash, device=0), self.manifest)

    def test_declared_device_must_match_inventory_and_request(self):
        for selected in (1, True, "0", -1):
            with self.subTest(selected=selected):
                self.manifest["parserCache"]["requestedDevice"] = selected
                self.save()
                with self.assertRaises(ValueError):
                    validate_cache(self.directory, self.hash, device=0)

    def test_device_filtered_cache_cannot_satisfy_all_devices(self):
        seal_manifest(self.directory, self.manifest, device=0)
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash, device=0), self.manifest)
        for selection in (None, 1):
            with self.assertRaises(ValueError):
                validate_cache(self.directory, self.hash, device=selection)

    def test_invalid_manifest_shape_is_rejected(self):
        for data in ([], {**self.manifest, "parserCache": []}, {**self.manifest, "schemaVersion": True}):
            (self.directory / "manifest.json").write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                validate_cache(self.directory, self.hash)

    def test_duplicate_json_keys_are_rejected(self):
        (self.directory / "manifest.json").write_text('{"status":"building","status":"complete"}')
        with self.assertRaises(ValueError):
            validate_cache(self.directory, self.hash)

    def test_manifest_changed_during_validation_is_rejected(self):
        from unittest.mock import patch

        from util import snapshot_cache

        original = snapshot_cache.parser_members

        def change_manifest(directory, manifest, **kwargs):
            members = original(directory, manifest, **kwargs)
            self.manifest['consumerRevision'] = 1
            self.save()
            return members

        with patch.object(snapshot_cache, 'parser_members', side_effect=change_manifest):
            with self.assertRaisesRegex(ValueError, 'manifest changed'):
                validate_cache(self.directory, self.hash)

    def test_parser_salt_changes_cache_identity(self):
        from unittest.mock import patch

        from util import snapshot_cache

        with patch.object(snapshot_cache, "PARSER_HASH_SALT", b"another-parser-version"):
            with self.assertRaises(ValueError):
                validate_cache(self.directory, source_hashes(self.source)[1])

    def test_default_validation_rejects_missing_required_table(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("DROP TABLE block_0")
            conn.commit()
        with self.assertRaisesRegex(ValueError, "Missing parser table"):
            validate_cache(self.directory, self.hash)

    def test_default_validation_rejects_missing_member(self):
        self.db.unlink()
        with self.assertRaises(ValueError):
            validate_cache(self.directory, self.hash)
        self.assertFalse(self.db.exists())

    def test_default_validation_rejects_unreadable_database(self):
        self.db.write_bytes(b"not a database")
        with self.assertRaises(sqlite3.DatabaseError):
            validate_cache(self.directory, self.hash)

    def test_oom_without_dictionary_reference_is_accepted(self):
        with closing(sqlite3.connect(self.db)) as conn:
            with conn:
                conn.execute("UPDATE trace_entry_0 SET action='oom'")
        seal_manifest(self.directory, self.manifest)
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_cache_validation_never_scans_rows_or_pages(self):
        from unittest.mock import patch

        statements = []
        connect = sqlite3.connect

        def traced(*args, **kwargs):
            conn = connect(*args, **kwargs)
            conn.set_trace_callback(statements.append)
            return conn

        with patch.object(sqlite3, "connect", side_effect=traced):
            seal_manifest(self.directory, self.manifest)
            self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        sql = "\n".join(statements).upper()
        self.assertNotIn("QUICK_CHECK", sql)
        self.assertNotIn("INTEGRITY_CHECK", sql)
        self.assertNotIn("DISTINCT", sql)
        without_endpoints = sql.replace("ORDER BY ID DESC LIMIT 1", "").replace("ORDER BY ID LIMIT 1", "")
        self.assertNotIn("ORDER BY", without_endpoints)

    def test_legacy_content_digests_are_not_compared(self):
        self.manifest["parserCache"]["members"] = {"slice.db": "old-content-digest"}
        self.manifest["ptSnap"] = {
            "memberHashKind": "parser-v1",
            "members": {"slice.db": "different-old-digest"},
        }
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
