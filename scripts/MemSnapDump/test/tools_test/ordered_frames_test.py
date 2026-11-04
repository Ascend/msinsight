"""有序帧扩展保留顺序、递归、空栈，且不改写原有调用栈文本。"""

import json
import sqlite3
import unittest

from base import TraceEntry
from tools.adaptors.ordered_frames import OrderedFrameWriter


class OrderedFrameWriterTest(unittest.TestCase):
    def setUp(self):
        self.conn = sqlite3.connect(":memory:")
        self.addCleanup(self.conn.close)
        self.writer = OrderedFrameWriter(self.conn)

    def event(self, index, frames):
        event = TraceEntry.from_dict({"action": "alloc", "frames": frames}, _raw_frames=True)
        event.idx = index
        return event

    def test_preserves_order_recursion_unicode_and_deduplicates_stacks(self):
        frame = {"filename": "中文/模型.py", "line": 128, "name": "forward"}
        frames = [frame, {"filename": "a.py", "line": 2, "name": "main"}, frame]
        event = self.event(1, frames)
        text = event.get_callstack()
        self.writer.insert(event)
        self.writer.insert(self.event(2, list(frames)))
        self.assertEqual(event.get_callstack(), text)
        self.assertEqual(self.conn.execute("SELECT COUNT(*) FROM pt_snap_frame_value").fetchone()[0], 2)
        self.assertEqual(self.conn.execute("SELECT COUNT(*) FROM pt_snap_stack").fetchone()[0], 1)
        rows = self.conn.execute(
            "SELECT f.frameJson FROM pt_snap_stack_frame sf JOIN pt_snap_frame_value f ON f.id=sf.frameId "
            "WHERE stackId=0 ORDER BY frameIndex"
        ).fetchall()
        self.assertEqual([json.loads(row[0]) for row in rows], frames)

    def test_distinguishes_empty_stack_from_missing_evidence(self):
        self.writer.insert(self.event(0, []))
        self.assertEqual(self.conn.execute("SELECT frameCount FROM pt_snap_stack").fetchone(), (0,))
        self.assertEqual(self.conn.execute("SELECT eventId FROM pt_snap_event_stack").fetchall(), [(0,)])

    def test_skips_synthetic_events_and_unencodable_optional_evidence(self):
        frame = {"filename": "x.py", "line": 1, "name": "x", "extra": float("nan")}
        self.writer.insert(self.event(-1, []))
        self.writer.insert(self.event(1, [frame]))
        self.assertEqual(self.conn.execute("SELECT COUNT(*) FROM pt_snap_event_stack").fetchone()[0], 0)

    def test_frame_objects_and_raw_frames_have_identical_wire_schema(self):
        frame = {"filename": "x.py", "line": 1, "name": "f"}
        event = TraceEntry.from_dict({"frames": [frame]})
        event.idx = 1
        self.writer.insert(event)
        self.writer.insert(self.event(2, [frame]))
        self.assertEqual(self.conn.execute("SELECT COUNT(*) FROM pt_snap_stack").fetchone()[0], 1)

    def test_deep_optional_stack_is_skipped_atomically_and_writer_recovers(self):
        valid = {"filename": "a.py", "line": 1, "name": "f"}
        for container in (dict, tuple):
            with self.subTest(container=container):
                extra = 0
                for _ in range(140):
                    extra = {"nested": extra} if container is dict else (extra,)
                event = self.event(0, [valid, {**valid, "extra": extra}])
                text = event.get_callstack()
                self.writer.insert(event)
                self.assertEqual(event.get_callstack(), text)
                for table in ("pt_snap_frame_value", "pt_snap_stack", "pt_snap_stack_frame", "pt_snap_event_stack"):
                    self.assertEqual(self.conn.execute(f'SELECT COUNT(*) FROM "{table}"').fetchone()[0], 0)
                self.assertEqual(self.writer.frames, {})
                self.assertEqual(self.writer.stacks, {})
                self.assertEqual(self.writer.containers, {})
        self.writer.insert(self.event(1, [valid]))
        self.assertEqual(self.conn.execute("SELECT eventId FROM pt_snap_event_stack").fetchall(), [(1,)])

    def test_json_depth_boundary_matches_reader_contract(self):
        for depth in (128, 129):
            with self.subTest(depth=depth):
                extra = 0
                for _ in range(depth - 2):
                    extra = (extra,)
                self.writer.insert(self.event(depth, [{"filename": "a", "line": 1, "name": "f", "extra": extra}]))
                rows = self.conn.execute("SELECT eventId FROM pt_snap_event_stack").fetchall()
                self.assertEqual(rows, [(128,)])


if __name__ == "__main__":
    unittest.main()
