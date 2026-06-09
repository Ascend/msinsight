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

from scripts.cli import main as cli_main, parse_args
from scripts.collect import CollectConfig, _parse_rank_map, collect_snapshot


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _stat(pid: int, name: str, processor: int = 4, utime: int = 0, stime: int = 0) -> str:
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
        "0",
        "0",
        str(utime),
        str(stime),
    ]
    fields.extend(["0"] * 21)
    fields.append(str(processor))
    fields.extend(["0"] * 8)
    return f"{pid} ({name}) " + " ".join(fields) + "\n"


def _status(pid: int, name: str, threads: int = 1) -> str:
    return (
        f"Name:\t{name}\n"
        "State:\tR (running)\n"
        "PPid:\t1\n"
        f"Threads:\t{threads}\n"
        "Cpus_allowed_list:\t0-7\n"
        "Mems_allowed_list:\t0\n"
        f"NSpid:\t{pid}\n"
        "voluntary_ctxt_switches:\t1\n"
        "nonvoluntary_ctxt_switches:\t2\n"
    )


class CollectE2ETest(unittest.TestCase):
    def test_collect_snapshot_from_injected_roots_and_text(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            proc_root = root / "proc"
            cgroup_root = root / "cgroup"
            sys_root = root / "sys"
            out = root / "snapshot.json"

            _write(proc_root / "100" / "status", _status(100, "python", threads=2))
            _write(
                proc_root / "100" / "stat",
                _stat(100, "python", processor=4, utime=100, stime=20),
            )
            _write(proc_root / "100" / "cmdline", "python\x00train.py\x00")
            _write(
                proc_root / "100" / "environ",
                "RANK=0\x00LOCAL_RANK=0\x00ASCEND_VISIBLE_DEVICES=0\x00SECRET_TOKEN=x\x00",
            )
            _write(proc_root / "100" / "cgroup", "0::/job.slice\n")
            _write(proc_root / "100" / "task" / "100" / "status", _status(100, "python"))
            _write(
                proc_root / "100" / "task" / "100" / "stat",
                _stat(100, "python", processor=4, utime=100, stime=20),
            )
            _write(proc_root / "100" / "task" / "100" / "comm", "python\n")
            _write(
                proc_root / "100" / "task" / "101" / "status",
                _status(101, "HcclWorker"),
            )
            _write(
                proc_root / "100" / "task" / "101" / "stat",
                _stat(101, "HcclWorker", processor=5, utime=50, stime=10),
            )
            _write(proc_root / "100" / "task" / "101" / "comm", "HcclWorker\n")
            _write(proc_root / "loadavg", "1.00 2.00 3.00 1/100 9\n")

            _write(cgroup_root / "job.slice" / "cpuset.cpus.effective", "0-7\n")
            _write(cgroup_root / "job.slice" / "cpuset.mems.effective", "0\n")
            _write(cgroup_root / "job.slice" / "cpu.max", "max 100000\n")
            _write(cgroup_root / "job.slice" / "cpu.weight", "100\n")
            _write(
                cgroup_root / "job.slice" / "cpu.stat",
                "nr_periods 10\nnr_throttled 0\nthrottled_usec 0\n",
            )
            _write(
                sys_root / "bus" / "pci" / "devices" / "0000:01:00.0" / "numa_node",
                "0\n",
            )

            lscpu_text = """Architecture: aarch64
CPU(s): 8
Thread(s) per core: 1
Core(s) per socket: 8
Socket(s): 1
On-line CPU(s) list: 0-7
NUMA node0 CPU(s): 0-7
"""
            lscpu_e_text = "CPU CORE SOCKET NODE ONLINE MAXMHZ MINMHZ MHZ\n0 0 0 0 yes 2600 200 2400\n4 4 0 0 yes 2600 200 2400\n5 5 0 0 yes 2600 200 2400\n"
            npu_info_text = """| NPU   Name                | Health        | Power(W)    Temp(C)           Hugepages-Usage(page)|
| Chip                      | Bus-Id        | AICore(%)   Memory-Usage(MB)  HBM-Usage(MB)        |
| 0     910B2               | OK            | 90.1        33                0    / 0             |
| 0                         | 0000:01:00.0  | 0           0    / 0          1/ 65536             |
| NPU     Chip              | Process id    | Process name             | Process memory(MB)      |
| 0       0                 | 100           | python                   | 1                       |
"""
            npu_topo_text = "NPU0 CPU Affinity\nNPU0 X 0-7\n"

            snapshot = collect_snapshot(
                CollectConfig(
                    pids=[100],
                    out=out,
                    proc_root=proc_root,
                    cgroup_root=cgroup_root,
                    sys_root=sys_root,
                    lscpu_text=lscpu_text,
                    lscpu_e_text=lscpu_e_text,
                    npu_smi_info_text=npu_info_text,
                    npu_smi_topo_text=npu_topo_text,
                    no_runtime_sample=True,
                    no_raw=True,
                )
            )

            self.assertEqual(snapshot["schema_version"], "0.1.1")
            self.assertEqual(snapshot["workload"]["target_pids"], [100])
            self.assertEqual(snapshot["processes"][0]["threads"][1]["key_class"], "communication")
            self.assertEqual(snapshot["cgroup"]["process_groups"][0]["cpuset_cpus_effective"], "0-7")
            self.assertTrue(snapshot["pytorch"]["detected"])
            self.assertNotIn("SECRET_TOKEN", snapshot["pytorch"]["env"])
            self.assertEqual(snapshot["npu_topology"]["devices"][0]["numa_node"], 0)
            self.assertEqual(snapshot["key_processes"]["communication_threads"][0]["tid"], 101)
            self.assertEqual(snapshot["runtime_sample"]["top_threads"], [])
            self.assertTrue(out.exists())

    def test_collect_cli_returns_2_for_missing_pid(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "snapshot.json"
            exit_code = cli_main(
                [
                    "collect",
                    "--pid",
                    "999999",
                    "--out",
                    str(output),
                    "--no-runtime-sample",
                ]
            )

        self.assertEqual(exit_code, 2)

    def test_collect_cli_rejects_non_positive_sample_seconds(self):
        for value in ["-1", "0"]:
            with self.subTest(value=value), self.assertRaises(SystemExit):
                parse_args(
                    [
                        "collect",
                        "--pid",
                        "1",
                        "--sample-seconds",
                        value,
                        "--out",
                        "snapshot.json",
                    ]
                )

    def test_rank_map_rejects_malformed_entries(self):
        with self.assertRaises(ValueError):
            _parse_rank_map("rank0=100:npu0,rank1:101:npu1")
        with self.assertRaises(ValueError):
            _parse_rank_map("rank0=bad:npu0")


if __name__ == "__main__":
    unittest.main()
