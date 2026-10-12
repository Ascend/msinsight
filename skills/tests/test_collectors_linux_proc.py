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
import unittest
from pathlib import Path

SKILLS_ROOT = Path(__file__).resolve().parents[1]
ROOT = SKILLS_ROOT / "mindstudio-cpu-binding"
sys.path.insert(0, str(ROOT))

from scripts.collectors.linux_proc import (
    KEY_SCORE,
    ProcReader,
    classify_thread,
    collect_process,
)


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _build_proc_fixture(tmp: Path) -> Path:
    """Construct a minimal /proc fixture with one process and two threads.

    Process 12345: python train.py --local_rank=0  (NSpid host=12345 container=42)
      thread 12345  (main, comm=python)
      thread 12410  (DataLoader)
    """
    proc_root = tmp / "proc"
    pid_root = proc_root / "12345"

    _write(
        pid_root / "status",
        "Name:\tpython\n"
        "Pid:\t12345\n"
        "PPid:\t12000\n"
        "State:\tR (running)\n"
        "Threads:\t2\n"
        "Cpus_allowed_list:\t0-63\n"
        "Mems_allowed_list:\t0\n"
        "voluntary_ctxt_switches:\t1200\n"
        "nonvoluntary_ctxt_switches:\t340\n"
        "NSpid:\t12345\t42\n",
    )
    _write(
        pid_root / "stat",
        "12345 (python) R 12000 12000 0 0 -1 4194304 1234 0 0 0 500 30 0 0 20 0 2 0 12345 1024000 500 0 0 0 0 0 0 0 0 0 0 0 0 0 0 48 0 0 0 0 0 0 0 0 0 0\n",
    )
    _write(pid_root / "cmdline", "python\x00train.py\x00--local_rank=0\x00")

    main_task = pid_root / "task" / "12345"
    _write(
        main_task / "status",
        "Name:\tpython\nPid:\t12345\nState:\tR (running)\nCpus_allowed_list:\t0-63\n",
    )
    _write(
        main_task / "stat",
        "12345 (python) R 12000 12000 0 0 -1 0 1234 0 0 0 500 30 0 0 20 0 2 0 12345 1024000 500 0 0 0 0 0 0 0 0 0 0 0 0 0 0 48 0 0 0 0 0 0 0 0 0 0\n",
    )
    _write(main_task / "comm", "python\n")

    dl_task = pid_root / "task" / "12410"
    _write(
        dl_task / "status",
        "Name:\tDataLoader\nPid:\t12410\nState:\tS (sleeping)\nCpus_allowed_list:\t0-63\n",
    )
    _write(
        dl_task / "stat",
        "12410 (DataLoader) S 12345 12345 0 0 -1 0 200 0 0 0 100 20 0 0 20 0 2 0 12410 512000 300 0 0 0 0 0 0 0 0 0 0 0 0 0 0 5 0 0 0 0 0 0 0 0 0 0\n",
    )
    _write(dl_task / "comm", "DataLoader\n")

    return proc_root


class ClassifyThreadTest(unittest.TestCase):
    def test_sq_task_with_suffix(self):
        self.assertEqual(classify_thread(100, 12345, "dev0_sq_task"), "sq_task")

    def test_sq_task_short_form(self):
        self.assertEqual(classify_thread(200, 12345, "dev1_sq"), "sq_task")

    def test_npu_fixed_release_thread(self):
        self.assertEqual(classify_thread(300, 12345, "release_thread"), "npu_fixed")

    def test_npu_fixed_acl_thread(self):
        self.assertEqual(classify_thread(400, 12345, "acl_thread"), "npu_fixed")

    def test_npu_fixed_pt_autograd_numbered(self):
        self.assertEqual(classify_thread(401, 12345, "pt_autograd_0"), "npu_fixed")
        self.assertEqual(classify_thread(402, 12345, "pt_autograd_12"), "npu_fixed")

    def test_communication_hccl(self):
        self.assertEqual(classify_thread(500, 12345, "hccl_send"), "communication")

    def test_communication_nccl(self):
        self.assertEqual(classify_thread(501, 12345, "nccl_worker"), "communication")

    def test_blas_worker(self):
        self.assertEqual(classify_thread(600, 12345, "openblas-1"), "blas_worker")

    def test_openmp_worker(self):
        self.assertEqual(classify_thread(601, 12345, "omp_worker"), "openmp_worker")

    def test_dataloader(self):
        self.assertEqual(classify_thread(700, 12345, "DataLoader"), "dataloader")
        self.assertEqual(classify_thread(701, 12345, "pt_data_worker"), "dataloader")

    def test_tokenizer(self):
        self.assertEqual(classify_thread(800, 12345, "tokenizer_0"), "tokenizer")

    def test_engine_worker(self):
        self.assertEqual(classify_thread(900, 12345, "engine_worker"), "engine_worker")

    def test_main_scheduler_when_tid_equals_pid(self):
        self.assertEqual(classify_thread(12345, 12345, "python"), "main_scheduler")

    def test_unknown_fallback(self):
        self.assertEqual(classify_thread(999, 12345, "totally_random"), "unknown")

    def test_classification_is_case_insensitive(self):
        self.assertEqual(classify_thread(700, 12345, "DATALOADER"), "dataloader")

    def test_score_table_matches_design(self):
        self.assertEqual(KEY_SCORE["main_scheduler"], 100)
        self.assertEqual(KEY_SCORE["sq_task"], 95)
        self.assertEqual(KEY_SCORE["communication"], 85)
        self.assertEqual(KEY_SCORE["npu_fixed"], 80)
        self.assertEqual(KEY_SCORE["unknown"], 0)


class CollectProcessTest(unittest.TestCase):
    def setUp(self):
        import tempfile

        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.tmp = Path(tmp_ctx.name)
        self.reader = ProcReader(root=_build_proc_fixture(self.tmp))

    def test_returns_none_when_pid_missing(self):
        proc, avail = collect_process(self.reader, 99999)
        self.assertIsNone(proc)
        self.assertFalse(avail.to_dict()["complete"])
        self.assertEqual(avail.to_dict()["errors"][0]["component"], "linux_proc[99999]")

    def test_basic_process_fields(self):
        proc, avail = collect_process(self.reader, 12345)
        self.assertEqual(proc["pid"], 12345)
        self.assertEqual(proc["ppid"], 12000)
        self.assertEqual(proc["comm"], "python")
        self.assertEqual(proc["state"], "R (running)")
        self.assertEqual(proc["cpus_allowed_list"], "0-63")
        self.assertEqual(proc["mems_allowed_list"], "0")
        self.assertEqual(proc["num_threads"], 2)
        self.assertEqual(proc["voluntary_ctxt_switches"], 1200)
        self.assertEqual(proc["nonvoluntary_ctxt_switches"], 340)
        self.assertIsNone(proc["cpu_percent"])
        self.assertIsNone(proc["rank"])
        self.assertIsNone(proc["npu_device"])
        self.assertTrue(avail.to_dict()["complete"])

    def test_command_replaces_nul_with_space(self):
        proc, _ = collect_process(self.reader, 12345)
        self.assertEqual(proc["command"], "python train.py --local_rank=0")

    def test_command_redacts_sensitive_arguments(self):
        _write(
            self.reader.root / "12345" / "cmdline",
            "python\x00serve.py\x00--api-key\x00sk-secret\x00--connection-token=abc\x00--model\x00qwen\x00",
        )
        proc, _ = collect_process(self.reader, 12345)
        self.assertEqual(
            proc["command"],
            "python serve.py --api-key REDACTED --connection-token=REDACTED --model qwen",
        )

    def test_current_cpu_from_stat(self):
        proc, _ = collect_process(self.reader, 12345)
        self.assertEqual(proc["current_cpu"], 48)

    def test_nspid_chain(self):
        proc, _ = collect_process(self.reader, 12345)
        self.assertEqual(proc["nspid_chain"], [12345, 42])

    def test_threads_count_matches(self):
        proc, _ = collect_process(self.reader, 12345)
        self.assertEqual(len(proc["threads"]), 2)

    def test_main_thread_classified_main_scheduler(self):
        proc, _ = collect_process(self.reader, 12345)
        main = next(t for t in proc["threads"] if t["tid"] == 12345)
        self.assertEqual(main["key_class"], "main_scheduler")
        self.assertEqual(main["key_score"], 100)
        self.assertEqual(main["role_hint"], "main_scheduler")
        self.assertEqual(main["current_cpu"], 48)

    def test_dataloader_thread_classified(self):
        proc, _ = collect_process(self.reader, 12345)
        dl = next(t for t in proc["threads"] if t["tid"] == 12410)
        self.assertEqual(dl["key_class"], "dataloader")
        self.assertEqual(dl["key_score"], 70)
        self.assertEqual(dl["current_cpu"], 5)
        self.assertEqual(dl["name"], "DataLoader")

    def test_threads_sorted_by_tid(self):
        proc, _ = collect_process(self.reader, 12345)
        tids = [t["tid"] for t in proc["threads"]]
        self.assertEqual(tids, sorted(tids))

    def test_thread_default_numa_node_is_none(self):
        # numa_node is set by collect.py main flow (not linux_proc) — should be None here.
        proc, _ = collect_process(self.reader, 12345)
        for thread in proc["threads"]:
            self.assertIsNone(thread["numa_node"])


class ProcReaderTest(unittest.TestCase):
    def test_read_returns_none_for_missing_file(self):
        reader = ProcReader(root=Path("/nonexistent-root-xyz"))
        self.assertIsNone(reader.read(1234, "status"))

    def test_exists_returns_false_for_missing_pid(self):
        reader = ProcReader(root=Path("/nonexistent-root-xyz"))
        self.assertFalse(reader.exists(1234))


if __name__ == "__main__":
    unittest.main()
