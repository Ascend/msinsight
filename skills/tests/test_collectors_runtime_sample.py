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

# ruff: noqa: E402
# pylint: disable=duplicate-code
# pylint: disable=no-name-in-module
import sys
import tempfile
import unittest
from pathlib import Path

SKILLS_ROOT = Path(__file__).resolve().parents[1]
ROOT = SKILLS_ROOT / "mindstudio-cpu-binding"
sys.path.insert(0, str(ROOT))

from scripts.collectors.runtime_sample import (
    ProcSampleReader,
    collect_proc_runtime_sample,
    merge_runtime_sample,
)


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _stat(pid: int, name: str, utime: int, stime: int, processor: int) -> str:
    fields = [
        "S",
        "1",
        "1",
        "1",
        "0",
        "-1",
        "0",
        "0",
        "0",
        "0",
        "0",
        str(utime),
        str(stime),
    ]
    fields.extend(["0"] * 23)
    fields.append(str(processor))
    fields.extend(["0"] * 8)
    return f"{pid} ({name}) " + " ".join(fields) + "\n"


class RuntimeSampleTest(unittest.TestCase):
    def setUp(self):
        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.proc_root = Path(tmp_ctx.name) / "proc"

    def test_collect_proc_sample_computes_thread_cpu_percent_and_top_threads(self):
        _write(self.proc_root / "loadavg", "1.00 2.00 3.00 4/500 12345\n")
        _write(
            self.proc_root / "100" / "task" / "100" / "stat",
            _stat(100, "main", 100, 20, 4),
        )
        _write(
            self.proc_root / "100" / "task" / "101" / "stat",
            _stat(101, "worker", 50, 10, 5),
        )
        reader = ProcSampleReader(self.proc_root)
        before = reader.snapshot([100])
        _write(
            self.proc_root / "100" / "task" / "100" / "stat",
            _stat(100, "main", 150, 20, 4),
        )
        _write(
            self.proc_root / "100" / "task" / "101" / "stat",
            _stat(101, "worker", 70, 20, 6),
        )
        after = reader.snapshot([100])

        sample, availability = collect_proc_runtime_sample(
            before,
            after,
            sample_seconds=2.0,
            clock_ticks=100,
            top_threads=2,
            cpu_to_numa={4: 0, 5: 0, 6: 1},
            loadavg_text=reader.read_loadavg(),
        )

        self.assertEqual(sample["sample_seconds"], 2.0)
        self.assertEqual(sample["system_loadavg"], [1.0, 2.0, 3.0])
        self.assertEqual(sample["process_cpu_percent_total"], 40.0)
        self.assertTrue(sample["cpu_migration_observed"])
        self.assertEqual(sample["top_threads"][0]["tid"], 100)
        self.assertEqual(sample["top_threads"][0]["name"], "main")
        self.assertEqual(sample["top_threads"][0]["cpu_percent"], 25.0)
        self.assertEqual(sample["top_threads"][1]["tid"], 101)
        self.assertEqual(sample["top_threads"][1]["cpu_percent"], 15.0)
        self.assertEqual(
            sample["cpu_usage_by_numa"],
            [
                {"numa_node": 0, "cpu_percent": 25.0},
                {"numa_node": 1, "cpu_percent": 15.0},
            ],
        )
        self.assertNotIn("runtime_sample.cpu_usage_by_numa", availability.to_dict()["partial"])

    def test_collect_proc_sample_skips_threads_missing_from_after_snapshot(self):
        before = {
            100: {
                100: {"utime": 10, "stime": 0, "processor": 0},
                101: {"utime": 20, "stime": 0, "processor": 0},
            }
        }
        after = {100: {100: {"utime": 20, "stime": 0, "processor": 0}}}

        sample, availability = collect_proc_runtime_sample(
            before,
            after,
            sample_seconds=1.0,
            clock_ticks=100,
            top_threads=10,
            cpu_to_numa={0: 0},
            loadavg_text="",
        )

        self.assertEqual(len(sample["top_threads"]), 1)
        self.assertEqual(sample["top_threads"][0]["tid"], 100)
        self.assertIn("runtime_sample.threads[101]", availability.to_dict()["partial"])

    def test_merge_runtime_sample_backfills_process_and_thread_cpu_percent(self):
        processes = [
            {
                "pid": 100,
                "cpu_percent": None,
                "threads": [
                    {"tid": 100, "cpu_percent": None},
                    {"tid": 101, "cpu_percent": None},
                ],
            }
        ]
        sample = {
            "threads": [
                {"pid": 100, "tid": 101, "cpu_percent": 15.0},
                {"pid": 100, "tid": 100, "cpu_percent": 25.0},
            ],
            "process_cpu_percent_by_pid": {100: 40.0},
            "top_threads": [{"pid": 100, "tid": 100, "cpu_percent": 25.0}],
        }

        merge_runtime_sample(processes, sample)

        self.assertEqual(processes[0]["cpu_percent"], 40.0)
        self.assertEqual(processes[0]["threads"][0]["cpu_percent"], 25.0)
        self.assertEqual(processes[0]["threads"][1]["cpu_percent"], 15.0)

    def test_process_cpu_percent_uses_all_threads_not_only_top_threads(self):
        before = {
            100: {
                100: {"utime": 0, "stime": 0, "processor": 0},
                101: {"utime": 0, "stime": 0, "processor": 0},
            }
        }
        after = {
            100: {
                100: {"utime": 10, "stime": 0, "processor": 0},
                101: {"utime": 20, "stime": 0, "processor": 0},
            }
        }

        sample, _ = collect_proc_runtime_sample(
            before,
            after,
            sample_seconds=1.0,
            clock_ticks=10,
            top_threads=1,
            cpu_to_numa={0: 0},
            loadavg_text="",
        )

        self.assertEqual(len(sample["top_threads"]), 1)
        self.assertEqual(sample["process_cpu_percent_by_pid"], {100: 300.0})
        self.assertEqual(sample["process_cpu_percent_total"], 300.0)
        self.assertEqual(len(sample["threads"]), 2)

    def test_negative_cpu_delta_is_clamped_to_zero(self):
        before = {100: {100: {"utime": 20, "stime": 0, "processor": 0}}}
        after = {100: {100: {"utime": 10, "stime": 0, "processor": 0}}}

        sample, _ = collect_proc_runtime_sample(
            before,
            after,
            sample_seconds=1.0,
            clock_ticks=10,
            top_threads=10,
            cpu_to_numa={0: 0},
            loadavg_text="",
        )

        self.assertEqual(sample["top_threads"][0]["cpu_percent"], 0.0)
        self.assertEqual(sample["process_cpu_percent_total"], 0.0)

    def test_empty_sample_marks_runtime_sample_missing(self):
        sample, availability = collect_proc_runtime_sample(
            before={},
            after={},
            sample_seconds=1.0,
            clock_ticks=100,
            top_threads=10,
            cpu_to_numa={},
            loadavg_text="",
        )

        self.assertEqual(sample["top_threads"], [])
        self.assertIn("runtime_sample.top_threads", availability.to_dict()["missing"])
        self.assertFalse(availability.to_dict()["complete"])


if __name__ == "__main__":
    unittest.main()
