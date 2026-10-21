"""Cache validation must not depend on consumer-added SQLite objects."""

import hashlib
import json
import sqlite3
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

from util.snapshot_cache import _RowEncoder, seal_manifest, source_hashes, validate_cache


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

    def test_row_encoder_preserves_digest_bytes_and_bounds_cache(self):
        encoder = _RowEncoder()
        for stack in ('', 'ascii', '\u4e2d\u6587', '"\\\n\t', None, 'x' * (8 * 1024 * 1024)):
            row = (1, 2, 3, 4, 0, 5, 6, 7, stack)
            expected = json.dumps(row, ensure_ascii=True, separators=(',', ':'), allow_nan=False).encode()
            self.assertEqual(encoder.encode(row, True), expected)
            self.assertEqual(encoder.encode(row, True), expected)
        for index in range(3000):
            encoder.encode((index, str(index)), True)
        self.assertLessEqual(len(encoder.stacks), 2048)
        self.assertLessEqual(encoder.size, 8 * 1024 * 1024)

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

    def test_base_write_after_sidecar_check_is_detected(self):
        from unittest.mock import patch

        from util import snapshot_cache

        connect = sqlite3.connect
        writers = []
        self.addCleanup(lambda: [writer.close() for writer in writers])

        def open_after_base_write(*args, **kwargs):
            writer = connect(self.db)
            writers.append(writer)
            writer.execute("PRAGMA journal_mode=WAL")
            writer.execute("UPDATE block_0 SET size=size+1")
            writer.commit()
            return connect(*args, **kwargs)

        with patch.object(snapshot_cache.sqlite3, "connect", side_effect=open_after_base_write):
            with self.assertRaisesRegex(ValueError, "parser data"):
                validate_cache(self.directory, self.hash)

    def test_consumer_commit_during_row_scan_is_allowed(self):
        from unittest.mock import patch

        from util import snapshot_cache

        with closing(sqlite3.connect(self.db)) as writer:
            writer.execute("PRAGMA journal_mode=WAL")
            encode = snapshot_cache._RowEncoder.encode
            committed = False

            def encode_while_writing(encoder, row, inline_stack):
                nonlocal committed
                if not committed:
                    writer.execute("CREATE TABLE consumer_cache(value INTEGER)")
                    writer.commit()
                    writer.execute("PRAGMA wal_checkpoint(PASSIVE)")
                    committed = True
                return encode(encoder, row, inline_stack)

            with patch.object(snapshot_cache._RowEncoder, "encode", encode_while_writing):
                self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
            self.assertTrue(committed)

    def test_parser_seal_does_not_override_producer_seal(self):
        self.manifest["ptSnap"] = {
            "memberHashKind": "parser-v1",
            "members": {"slice.db": "0" * 64},
        }
        self.save()
        with self.assertRaisesRegex(ValueError, "member hash"):
            validate_cache(self.directory, self.hash)

    def test_logical_producer_seal_without_parser_seal_is_checked(self):
        self.manifest["ptSnap"] = {
            "memberHashKind": "parser-v1",
            "members": self.manifest.pop("parserCache")["members"],
        }
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("UPDATE block_0 SET size=size+1")
            conn.commit()
        with self.assertRaisesRegex(ValueError, "member hash"):
            validate_cache(self.directory, self.hash)

    def test_legacy_producer_seal_is_not_overridden_by_parser_seal(self):
        self.manifest["ptSnap"] = {"members": {"slice.db": source_hashes(self.db)[0]}}
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("CREATE TABLE consumer_extra(value INTEGER)")
        with self.assertRaisesRegex(ValueError, "member hash"):
            validate_cache(self.directory, self.hash)

    def test_legacy_physical_seal_accepts_closed_wal_database(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("PRAGMA journal_mode=WAL")
        self.manifest["ptSnap"] = {"members": {"slice.db": source_hashes(self.db)[0]}}
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)

    def test_legacy_physical_seal_rejects_committed_wal(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("PRAGMA journal_mode=WAL")
        self.manifest["ptSnap"] = {"members": {"slice.db": source_hashes(self.db)[0]}}
        self.save()
        with closing(sqlite3.connect(self.db)) as writer:
            writer.execute("CREATE TABLE consumer_cache(value INTEGER)")
            writer.commit()
            with self.assertRaisesRegex(ValueError, "sidecars"):
                validate_cache(self.directory, self.hash)

    def test_source_hash_matches_shared_salt(self):
        raw, salted = source_hashes(self.source)
        self.assertEqual(raw, hashlib.sha256(self.source.read_bytes()).hexdigest())
        self.assertEqual(salted, hashlib.sha256(b"mem_snapshot_parser_v2" + self.source.read_bytes()).hexdigest())

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

    def test_modified_base_data_is_rejected(self):
        for table, assignment in (
            ("block_0", "size=size+1"),
            ("trace_entry_0", "allocated=allocated+1"),
            ("dictionary", '"value"=\'changed\''),
        ):
            with self.subTest(table=table):
                with closing(sqlite3.connect(self.db)) as conn:
                    conn.execute(f"UPDATE {table} SET {assignment}")
                    conn.commit()
                with self.assertRaises(ValueError):
                    validate_cache(self.directory, self.hash)
                seal_manifest(self.directory, self.manifest)
                self.save()

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

    def test_corrupt_secondary_index_is_rejected(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute('CREATE INDEX block_address ON block_0(address)')
            page_size = conn.execute('PRAGMA page_size').fetchone()[0]
            root = conn.execute("SELECT rootpage FROM sqlite_master WHERE name='block_address'").fetchone()[0]
        with self.db.open('r+b') as file:
            file.seek((root - 1) * page_size)
            file.write(b'\xff' * page_size)
        with self.assertRaises((ValueError, sqlite3.DatabaseError)):
            validate_cache(self.directory, self.hash)

    def test_deeply_nested_manifest_is_a_validation_error(self):
        (self.directory / 'manifest.json').write_text('[' * 200000 + '0' + ']' * 200000)
        with self.assertRaisesRegex(ValueError, 'nesting'):
            validate_cache(self.directory, self.hash)

    def test_legacy_missing_dictionary_reference_is_rejected(self):
        del self.manifest['parserCache']
        self.save()
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("DELETE FROM dictionary WHERE \"table\"='block_0'")
            conn.commit()
        with self.assertRaisesRegex(ValueError, 'dictionary reference'):
            validate_cache(self.directory, self.hash)

    def test_legacy_producer_physical_hash_is_checked(self):
        del self.manifest['parserCache']
        self.manifest['ptSnap'] = {'members': {'slice.db': source_hashes(self.db)[0]}}
        self.save()
        self.assertEqual(validate_cache(self.directory, self.hash), self.manifest)
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute('UPDATE block_0 SET size=size+1')
            conn.commit()
        with self.assertRaisesRegex(ValueError, 'member hash'):
            validate_cache(self.directory, self.hash)

    def test_member_validation_reports_completed_members(self):
        from unittest.mock import patch

        from util import snapshot_cache

        with patch.object(snapshot_cache.LOGGER, 'info') as info:
            validate_cache(self.directory, self.hash)
        info.assert_called_once_with('Snapshot cache member checked: %s', 'slice.db')

    def test_active_wal_is_read_instead_of_ignoring_committed_rows(self):
        with closing(sqlite3.connect(self.db)) as conn:
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("UPDATE block_0 SET size=size+1")
            conn.commit()
            self.assertTrue(Path(str(self.db) + "-wal").exists())
            with self.assertRaises(ValueError):
                validate_cache(self.directory, self.hash)

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

        def change_manifest(directory, manifest):
            members = original(directory, manifest)
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
