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

from scripts.collectors.key_processes import collect_key_processes, scan_sq_task_threads


def _thread(tid, name, key_class, key_score, current_cpu=0, numa_node=0, cpu_percent=None):
    return {
        "tid": tid,
        "name": name,
        "key_class": key_class,
        "key_score": key_score,
        "current_cpu": current_cpu,
        "numa_node": numa_node,
        "cpu_percent": cpu_percent,
        "cpus_allowed_list": "0-7",
        "npu_device": None,
    }


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class KeyProcessesTest(unittest.TestCase):
    def test_collects_key_threads_by_class_from_target_processes(self):
        processes = [
            {
                "pid": 100,
                "comm": "python",
                "npu_device": "0",
                "threads": [
                    _thread(100, "python", "main_scheduler", 100),
                    _thread(101, "HcclWorker", "communication", 85),
                    _thread(102, "acl_thread", "npu_fixed", 80),
                    _thread(103, "DataLoader", "dataloader", 70),
                    _thread(104, "idle", "unknown", 0),
                ],
            }
        ]

        result = collect_key_processes(processes, npu_topology={}, target_pids=[100])

        self.assertEqual(result["discovery_sources"], ["target_pids"])
        self.assertEqual(result["main_scheduler_pids"], [100])
        self.assertEqual(result["communication_threads"][0]["tid"], 101)
        self.assertIn("cpu_percent", result["communication_threads"][0])
        self.assertEqual(result["npu_fixed_threads"][0]["tid"], 102)
        self.assertEqual(result["dataloader_threads"][0]["tid"], 103)
        self.assertEqual(result["top_threads"][0]["key_score"], 100)
        self.assertNotIn("unknown_threads", result)

    def test_adds_npu_smi_host_pids_from_topology(self):
        processes = [
            {
                "pid": 100,
                "comm": "python",
                "threads": [_thread(100, "python", "main_scheduler", 100)],
            }
        ]
        npu_topology = {"processes_by_device": {"0": [100], "1": [200]}}

        result = collect_key_processes(processes, npu_topology=npu_topology, target_pids=[100])

        self.assertIn("npu_smi_info", result["discovery_sources"])
        self.assertEqual(
            result["npu_smi_host_pids"],
            [{"npu_id": "0", "pid": 100}, {"npu_id": "1", "pid": 200}],
        )

    def test_scan_sq_task_threads_from_proc_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc_root = Path(tmp) / "proc"
            _write(proc_root / "2" / "task" / "88" / "comm", "dev0_sq_task\n")
            _write(proc_root / "2" / "task" / "89" / "comm", "kworker/0:1\n")
            _write(proc_root / "300" / "task" / "188" / "comm", "dev7_sq\n")

            result = scan_sq_task_threads(proc_root)

        self.assertEqual(
            result,
            [
                {"npu_id": 0, "pid": 2, "tid": 88, "name": "dev0_sq_task"},
                {"npu_id": 7, "pid": 300, "tid": 188, "name": "dev7_sq"},
            ],
        )

    def test_extra_keywords_match_process_command_and_thread_name(self):
        processes = [
            {
                "pid": 100,
                "comm": "python",
                "command": "python serve.py --model qwen",
                "threads": [
                    _thread(100, "python", "main_scheduler", 100),
                    _thread(101, "tokenizer", "tokenizer", 60),
                ],
            }
        ]

        result = collect_key_processes(
            processes,
            npu_topology={},
            target_pids=[100],
            extra_keywords=["qwen", "token"],
        )

        self.assertIn("user_extra", result["discovery_sources"])
        self.assertEqual(result["user_extra_matches"][0]["pid"], 100)
        self.assertEqual(result["user_extra_matches"][0]["keyword"], "qwen")
        self.assertEqual(result["user_extra_matches"][1]["tid"], 101)
        self.assertEqual(result["user_extra_matches"][1]["keyword"], "token")

    def test_collect_includes_sq_pattern_when_proc_root_is_given(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc_root = Path(tmp) / "proc"
            _write(proc_root / "2" / "task" / "88" / "comm", "dev0_sq_task\n")

            result = collect_key_processes([], npu_topology={}, target_pids=[], proc_root=proc_root)

        self.assertIn("sq_pattern", result["discovery_sources"])
        self.assertEqual(
            result["sq_task_threads"],
            [{"npu_id": 0, "pid": 2, "tid": 88, "name": "dev0_sq_task"}],
        )

    def test_collect_limits_sq_pattern_scan_to_target_pids(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc_root = Path(tmp) / "proc"
            _write(proc_root / "100" / "task" / "188" / "comm", "dev0_sq_task\n")
            _write(proc_root / "200" / "task" / "288" / "comm", "dev1_sq_task\n")

            result = collect_key_processes([], npu_topology={}, target_pids=[100], proc_root=proc_root)

        self.assertEqual(
            result["sq_task_threads"],
            [{"npu_id": 0, "pid": 100, "tid": 188, "name": "dev0_sq_task"}],
        )

    def test_top_threads_prefers_runtime_cpu_percent_when_available(self):
        processes = [
            {
                "pid": 100,
                "comm": "python",
                "threads": [
                    _thread(100, "python", "main_scheduler", 100, cpu_percent=1.0),
                    _thread(101, "HcclWorker", "communication", 85, cpu_percent=80.0),
                ],
            }
        ]

        result = collect_key_processes(processes, npu_topology={}, target_pids=[100])

        self.assertEqual(result["top_threads"][0]["tid"], 101)
        self.assertEqual(result["top_threads"][0]["cpu_percent"], 80.0)


if __name__ == "__main__":
    unittest.main()
