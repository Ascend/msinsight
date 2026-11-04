# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
# Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
"""Guard the transport-independent query and the file exchange entry point."""

import json
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from contextlib import closing
from pathlib import Path
from unittest.mock import patch

from tools.frame_tree import (
    MAX_UINT64,
    Budget,
    QueryCancelledError,
    Sources,
    cancellation_file,
    decode_frame_json,
    main,
    query_frame_tree,
    read_stack,
)


class FrameTreeTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="frame-tree-中文 "))
        self.addCleanup(shutil.rmtree, self.root)
        (self.root / "device_0").mkdir()
        self.manifest = self.root / "manifest.json"
        self.paths = [self.root / f"device_0/slice_{i:05d}.db" for i in range(2)]
        for path in self.paths:
            self.sql(
                path,
                """
                CREATE TABLE trace_entry_0(id INTEGER PRIMARY KEY,action INTEGER);
                CREATE TABLE block_0(id INTEGER PRIMARY KEY,allocEventId INTEGER,freeEventId INTEGER,
                                     size INTEGER,requestedSize INTEGER);
                CREATE TABLE pt_snap_frame_value(id INTEGER PRIMARY KEY,frameJson TEXT);
                CREATE TABLE pt_snap_stack(stackId INTEGER PRIMARY KEY,frameCount INTEGER);
                CREATE TABLE pt_snap_stack_frame(stackId INTEGER,frameIndex INTEGER,frameId INTEGER,
                                                PRIMARY KEY(stackId,frameIndex));
                CREATE TABLE pt_snap_event_stack(eventId INTEGER PRIMARY KEY,stackId INTEGER);
            """,
            )
        self.sql(
            self.paths[0],
            """
            INSERT INTO trace_entry_0 VALUES(0,2),(1,4),(2,4);
            INSERT INTO pt_snap_frame_value VALUES(0,'{"filename":"模型.py","line":1,"name":"f"}');
            INSERT INTO pt_snap_stack VALUES(0,2),(1,0);
            INSERT INTO pt_snap_stack_frame VALUES(0,0,0),(0,1,0);
            INSERT INTO pt_snap_event_stack VALUES(1,0),(2,1);
        """,
        )
        self.sql(
            self.paths[1],
            """
            INSERT INTO trace_entry_0 VALUES(3,5),(4,6),(5,4);
            INSERT INTO block_0 VALUES(1,1,4,100,80),(2,2,-1,40,35),(-1,-1,-1,20,10),
                                      (-2,-1,5,10,8),(5,5,-1,0,0);
        """,
        )
        self.data = {
            "schemaVersion": 1,
            "status": "complete",
            "extensions": {"ptSnapOrderedFrames": {"version": 2}},
            "devices": {
                "0": {
                    "eventCount": 6,
                    "sliceCount": 2,
                    "readySlices": [0, 1],
                    "slices": [
                        {
                            "index": i,
                            "startEventId": i * 3,
                            "endEventId": i * 3 + 2,
                            "file": f"device_0/slice_{i:05d}.db",
                            "ready": True,
                        }
                        for i in range(2)
                    ],
                }
            },
        }
        self.save_manifest()

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.data), encoding="utf-8")

    @staticmethod
    def sql(path, script):
        with closing(sqlite3.connect(path)) as conn:
            conn.executescript(script)

    def query(self, event=3, include_static=True, **kwargs):
        return query_frame_tree(self.manifest, "0", event, include_static, **kwargs)

    def test_cross_window_recursion_and_self_metrics(self):
        tree = self.query()
        self.assertEqual(tree["coverage"], [2, 2, 2])
        nodes = tree["nodes"]
        self.assertEqual(len(nodes), 6)
        self.assertEqual(nodes[0]["total"], [170, 133, 4])
        self.assertEqual(nodes[0]["childCount"], 4)
        self.assertEqual(nodes[2]["parentId"], nodes[1]["nodeId"])
        self.assertEqual(nodes[2]["depth"], 2)
        self.assertEqual(nodes[2]["self"], [100, 80, 1])
        self.assertEqual(nodes[2]["frame"]["filename"], "模型.py")

    def test_after_event_lifetime_and_include_static(self):
        self.assertEqual(self.query(4)["nodes"][0]["total"][0], 70)
        self.assertEqual(self.query(5)["nodes"][0]["total"][0], 60)
        self.assertEqual(self.query(include_static=False)["nodes"][0]["total"][0], 140)

    def test_missing_frames_do_not_drop_occupancy(self):
        self.sql(self.paths[0], "DELETE FROM pt_snap_stack_frame WHERE frameIndex=1;")
        tree = self.query()
        self.assertEqual(tree["coverage"], [2, 2, 1])
        self.assertEqual(tree["nodes"][0]["total"][0], 170)
        self.assertTrue(any(n["kind"] == "missingFrame" and n["total"][0] == 100 for n in tree["nodes"]))

    def test_legacy_and_unknown_extension(self):
        for version in (None, 3):
            self.data["extensions"] = {} if version is None else {"ptSnapOrderedFrames": {"version": version}}
            self.save_manifest()
            self.assertEqual(self.query()["coverage"], [2, 2, 0])

    def test_malformed_schema_and_duplicate_json_degrade(self):
        self.sql(
            self.paths[0],
            """UPDATE pt_snap_frame_value
            SET frameJson='{"filename":"a","line":1,"name":"f","name":"g"}';""",
        )
        self.assertEqual(self.query()["coverage"], [2, 2, 1])
        self.sql(
            self.paths[0],
            """ALTER TABLE pt_snap_event_stack RENAME TO old_event;
            CREATE TABLE pt_snap_event_stack(eventId INTEGER,stackId INTEGER);
            INSERT INTO pt_snap_event_stack VALUES(1,0),(1,1),(2,1);""",
        )
        self.assertEqual(self.query()["coverage"], [2, 2, 0])

    def test_deep_and_nonfinite_optional_frames_degrade_without_losing_occupancy(self):
        extra = 0
        for _ in range(140):
            extra = {"nested": extra}
        for value in (json.dumps(extra), "NaN", "Infinity", "1e999"):
            with self.subTest(value=value[:40]):
                raw = '{"filename":"a","line":1,"name":"f","extra":' + value + '}'
                with closing(sqlite3.connect(self.paths[0])) as conn:
                    conn.execute("UPDATE pt_snap_frame_value SET frameJson=?", (raw,))
                    conn.commit()
                tree = self.query()
                self.assertEqual(tree["coverage"], [2, 2, 1])
                self.assertEqual(tree["nodes"][0]["total"], [170, 133, 4])
                self.assertTrue(any(node["kind"] == "missingFrame" for node in tree["nodes"]))

    def test_many_live_allocations_read_their_shared_stack_once(self):
        count = 1000
        with closing(sqlite3.connect(self.paths[0])) as conn:
            conn.executemany("INSERT INTO trace_entry_0 VALUES(?,4)", ((i,) for i in range(3, count + 1)))
            conn.execute("DELETE FROM pt_snap_event_stack")
            conn.executemany("INSERT INTO pt_snap_event_stack VALUES(?,0)", ((i,) for i in range(1, count + 1)))
            conn.commit()
        with closing(sqlite3.connect(self.paths[1])) as conn:
            conn.execute("UPDATE trace_entry_0 SET id=id+?", (count - 2,))
            conn.execute("DELETE FROM block_0")
            conn.executemany("INSERT INTO block_0 VALUES(?,?,-1,1,1)", ((i, i) for i in range(1, count + 1)))
            conn.commit()
        device = self.data["devices"]["0"]
        device["eventCount"] = count + 4
        device["slices"][0]["endEventId"] = count
        device["slices"][1].update(startEventId=count + 1, endEventId=count + 3)
        self.save_manifest()
        with patch("tools.frame_tree.read_stack", wraps=read_stack) as reader:
            tree = self.query(count + 1)
        self.assertEqual(reader.call_count, 1)
        self.assertEqual(tree["coverage"], [count, count, count])
        self.assertEqual(tree["nodes"][0]["total"], [count, count, count])

    def test_optional_json_depth_boundary_and_finite_numbers(self):
        for depth in (128, 129):
            extra = 0
            for _ in range(depth - 2):
                extra = (extra,)
            raw = json.dumps({"filename": "a", "line": 1, "name": "f", "extra": extra})
            if depth == 128:
                self.assertEqual(decode_frame_json(raw)["name"], "f")
            else:
                with self.assertRaisesRegex(ValueError, "nesting"):
                    decode_frame_json(raw)
        self.assertEqual(decode_frame_json('{"extra":1.25}'), {"extra": 1.25})
        for value in ("NaN", "Infinity", "-Infinity", "1e999", "-1e999"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                decode_frame_json('{"extra":' + value + '}')

    def test_empty_live_set_and_invalid_request(self):
        self.sql(self.paths[1], "DELETE FROM block_0;")
        self.assertEqual(len(self.query()["nodes"]), 1)
        self.assertEqual(self.query()["coverage"], [0, 0, 0])
        for event in (-1, 6, True, 1.5, 2**53):
            with self.assertRaises(ValueError):
                self.query(event)
        for device in ("00", "0;DROP", "1", 0):
            with self.assertRaises(ValueError):
                query_frame_tree(self.manifest, device, 3)
        with self.assertRaises(TypeError):
            self.query(include_static=1)

    def test_incomplete_or_missing_member(self):
        self.data["status"] = "building"
        self.save_manifest()
        with self.assertRaises(ValueError):
            self.query()
        self.data["status"] = "complete"
        self.save_manifest()
        self.paths[0].unlink()
        with self.assertRaises(ValueError):
            self.query()

    def test_unsafe_member_and_noncontiguous_ranges(self):
        slices = self.data["devices"]["0"]["slices"]
        slices[0]["file"] = "../outside.db"
        self.save_manifest()
        with self.assertRaises(ValueError):
            self.query()
        slices[0]["file"] = "device_0/slice_00000.db"
        slices[1]["startEventId"] = 4
        self.save_manifest()
        with self.assertRaises(ValueError):
            self.query()

    def test_uint64_and_overflow(self):
        self.sql(self.paths[1], "DELETE FROM block_0; INSERT INTO block_0 VALUES(1,2,-1,9007199254740993,0);")
        self.assertEqual(self.query()["nodes"][0]["total"][0], 9007199254740993)
        self.sql(
            self.paths[1],
            f"""DELETE FROM block_0;
            INSERT INTO block_0 VALUES(1,2,-1,{2**63 - 1},0),(2,2,-1,{2**63 - 1},0);""",
        )
        self.assertEqual(self.query()["nodes"][0]["total"][0], MAX_UINT64 - 1)
        self.sql(self.paths[1], "INSERT INTO block_0 VALUES(3,2,-1,2,0);")
        with self.assertRaisesRegex(ValueError, "overflow"):
            self.query()

    def test_timeout_and_node_budget(self):
        with self.assertRaisesRegex(ValueError, "limit"):
            self.query(timeout=0)
        with patch("tools.frame_tree.MAX_NODES", 2), self.assertRaisesRegex(ValueError, "too many nodes"):
            self.query()

    def test_changed_generation_is_rejected(self):
        original = Sources.signature
        calls = []

        def signature(sources):
            value = original(sources)
            calls.append(value)
            return value if len(calls) == 1 else ()

        with patch.object(Sources, "signature", signature), self.assertRaisesRegex(ValueError, "changed"):
            self.query()

    def test_wal_commits_are_visible_without_checkpoint(self):
        with closing(sqlite3.connect(self.paths[1])) as writer:
            writer.execute("PRAGMA journal_mode=WAL")
            self.assertEqual(self.query()["nodes"][0]["total"][0], 170)
            writer.execute("INSERT INTO block_0 VALUES(9,2,-1,9,9)")
            writer.commit()
            self.assertEqual(self.query()["nodes"][0]["total"][0], 179)

    def test_deep_unicode_tree_uses_iterative_preorder(self):
        self.sql(
            self.paths[0],
            """DELETE FROM pt_snap_stack_frame WHERE stackId=0;
            UPDATE pt_snap_stack SET frameCount=512 WHERE stackId=0;"""
            + "".join(f"INSERT INTO pt_snap_stack_frame VALUES(0,{i},0);" for i in range(512)),
        )
        nodes = self.query()["nodes"]
        for i in range(1, 513):
            self.assertEqual(nodes[i]["depth"], i)
            self.assertEqual(nodes[i]["parentId"], nodes[i - 1]["nodeId"])
        self.assertEqual(nodes[512]["self"], [100, 80, 1])

    def test_bad_manifest_and_missing_target_event(self):
        self.manifest.write_text("[]", encoding="utf-8")
        with self.assertRaises(ValueError):
            self.query()
        self.save_manifest()
        self.sql(self.paths[1], "DELETE FROM trace_entry_0 WHERE id=3;")
        with self.assertRaisesRegex(ValueError, "event does not exist"):
            self.query()

    def test_partial_oversized_transfer_is_not_successful(self):
        output = self.root / "too-large.json"
        args = [
            "--manifest",
            str(self.manifest),
            "--device",
            "0",
            "--event",
            "3",
            "--include-static",
            "1",
            "--output",
            str(output),
        ]
        tree = self.query()
        with (
            patch("tools.frame_tree.MAX_WORK_BYTES", 1),
            patch("tools.frame_tree.query_frame_tree", return_value=tree),
            self.assertRaisesRegex(ValueError, "transfer budget"),
        ):
            main(args)
        with self.assertRaises(json.JSONDecodeError):
            json.loads(output.read_text(encoding="utf-8"))

    def test_cancel_before_query_and_subsequent_query_succeeds(self):
        with self.assertRaises(QueryCancelledError):
            self.query(cancelled=lambda: True)
        self.assertGreater(len(self.query()["nodes"]), 1)

    def test_cancel_during_python_tree_build(self):
        cancelled = threading.Event()
        from tools.frame_tree import build_tree

        def cancel_build(groups, budget):
            cancelled.set()
            return build_tree(groups, budget)

        with patch("tools.frame_tree.build_tree", side_effect=cancel_build), self.assertRaises(QueryCancelledError):
            self.query(cancelled=cancelled.is_set)

    def test_sqlite_interrupt_restores_cancellation_and_closes_connection(self):
        cancelled = threading.Event()
        budget = Budget(cancelled=cancelled.is_set)
        sources = Sources(self.manifest, "0", 3, budget)
        with self.assertRaises(QueryCancelledError), sources.open(self.paths[1]) as conn:

            def cancel_now():
                cancelled.set()
                return 0

            conn.create_function("cancel_now", 0, cancel_now)
            conn.execute(
                "WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM n WHERE x<1000000) "
                "SELECT sum(x+cancel_now()) FROM n"
            ).fetchone()
        with self.assertRaises(sqlite3.ProgrammingError):
            conn.execute("SELECT 1")
        self.assertGreater(len(self.query()["nodes"]), 1)

    def cancel_args(self, output):
        return [
            "--manifest",
            str(self.manifest),
            "--device",
            "0",
            "--event",
            "3",
            "--include-static",
            "1",
            "--output",
            str(output),
        ]

    def test_existing_cancel_marker_leaves_no_result(self):
        marker, output = self.root / "cancel", self.root / "cancelled.json"
        marker.touch()
        self.assertEqual(main(self.cancel_args(output) + ["--cancel-file", str(marker)]), 2)
        self.assertFalse(output.exists())

    def test_cancel_during_output_removes_partial_result(self):
        output = self.root / "cancelled.json"
        cancelled = threading.Event()
        tree = self.query()

        def chunks(*args, **kwargs):
            yield "{"
            cancelled.set()
            yield "}"

        with (
            patch("tools.frame_tree.cancellation_file", return_value=cancelled.is_set),
            patch("tools.frame_tree.query_frame_tree", return_value=tree),
            patch("tools.frame_tree.json.JSONEncoder.iterencode", side_effect=chunks),
        ):
            self.assertEqual(main(self.cancel_args(output)), 2)
        self.assertFalse(output.exists())

    def test_marker_polling_is_throttled_and_sticky(self):
        marker = self.root / "cancel"
        cancelled = cancellation_file(marker)
        with patch("tools.frame_tree.time.monotonic", side_effect=[1, 1.01, 1.03, 2]):
            self.assertFalse(cancelled())
            marker.touch()
            self.assertFalse(cancelled())
            self.assertTrue(cancelled())
            marker.unlink()
            self.assertTrue(cancelled())

    def test_marker_interrupts_sqlite_in_real_python_process(self):
        marker, ready = self.root / "cancel", self.root / "ready"
        code = """
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from tools.frame_tree import Budget, Sources, QueryCancelledError, cancellation_file
budget = Budget(cancelled=cancellation_file(sys.argv[4]))
sources = Sources(sys.argv[2], '0', 3, budget)
try:
    with sources.open(Path(sys.argv[3])) as conn:
        Path(sys.argv[5]).touch()
        conn.execute('WITH RECURSIVE n(x) AS (VALUES(1) UNION ALL '
                     'SELECT x+1 FROM n WHERE x<100000000) SELECT sum(x) FROM n').fetchone()
except QueryCancelledError:
    raise SystemExit(2)
raise SystemExit(1)
"""
        with subprocess.Popen(
            [
                sys.executable,
                "-c",
                code,
                str(Path(__file__).resolve().parents[2]),
                str(self.manifest),
                str(self.paths[1]),
                str(marker),
                str(ready),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        ) as process:
            try:
                deadline = time.monotonic() + 5
                while not ready.exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(ready.exists(), "Child did not reach the SQLite query")
                marker.touch()
                _, error = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 2, error.decode(errors="replace"))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()

    def test_file_entry_point_and_failure_envelope(self):
        output = self.root / "query-result.json"
        args = [
            "--manifest",
            str(self.manifest),
            "--device",
            "0",
            "--event",
            "3",
            "--include-static",
            "1",
            "--output",
            str(output),
        ]
        self.assertEqual(main(args), 0)
        self.assertEqual(json.loads(output.read_text(encoding="utf-8")), self.query())
        with self.assertRaises(FileExistsError):
            main(args)
        output.unlink()
        self.paths[0].unlink()
        self.assertEqual(main(args), 1)
        result = json.loads(output.read_text(encoding="utf-8"))
        self.assertIn("error", result)
        self.assertNotIn("nodes", result)


if __name__ == "__main__":
    unittest.main()
