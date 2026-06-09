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

from scripts.collectors.linux_cpu import (
    collect_cpu_topology,
    collect_distance_matrix,
    collect_smt_siblings,
)


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class LinuxCpuTopologyTest(unittest.TestCase):
    def test_parses_lscpu_e_rows(self):
        text = (
            "CPU CORE SOCKET NODE ONLINE MAXMHZ MINMHZ MHZ\n"
            "0   0    0      0    yes    2600.0000 200.0000 2400.0000\n"
            "1   1    0      0    yes    2600.0000 200.0000 2390.0000\n"
        )
        result = collect_cpu_topology(text, smt_siblings_by_cpu={0: [0], 1: [1]})

        self.assertEqual(len(result["cpus"]), 2)
        cpu0 = result["cpus"][0]
        self.assertEqual(cpu0["cpu"], 0)
        self.assertEqual(cpu0["core_id"], 0)
        self.assertEqual(cpu0["socket_id"], 0)
        self.assertEqual(cpu0["numa_node"], 0)
        self.assertTrue(cpu0["online"])
        self.assertEqual(cpu0["max_mhz"], 2600.0)
        self.assertEqual(cpu0["current_mhz"], 2400.0)
        self.assertEqual(cpu0["smt_siblings"], [0])

    def test_aggregates_physical_cores_from_smt_siblings(self):
        text = (
            "CPU CORE SOCKET NODE ONLINE MAXMHZ MINMHZ MHZ\n"
            "0   0    0      0    yes    2600 200 2400\n"
            "1   1    0      0    yes    2600 200 2400\n"
            "64  0    0      0    yes    2600 200 2400\n"
        )
        result = collect_cpu_topology(text, smt_siblings_by_cpu={0: [0, 64], 1: [1], 64: [0, 64]})

        self.assertEqual(len(result["physical_cores"]), 2)
        core0 = next(core for core in result["physical_cores"] if core["core_id"] == 0)
        self.assertEqual(core0["core_key"], "socket0-core0")
        self.assertEqual(core0["socket_id"], 0)
        self.assertEqual(core0["numa_node"], 0)
        self.assertEqual(core0["logical_cpus"], [0, 64])

    def test_missing_frequency_values_become_none(self):
        text = "CPU CORE SOCKET NODE ONLINE MAXMHZ MINMHZ MHZ\n0 0 0 0 yes - - -\n"
        result = collect_cpu_topology(text, smt_siblings_by_cpu={0: [0]})

        self.assertIsNone(result["cpus"][0]["max_mhz"])
        self.assertIsNone(result["cpus"][0]["current_mhz"])

    def test_offline_cpu_flag(self):
        text = "CPU CORE SOCKET NODE ONLINE MAXMHZ MINMHZ MHZ\n0 0 0 0 no 2600 200 0\n"
        result = collect_cpu_topology(text, smt_siblings_by_cpu={0: [0]})
        self.assertFalse(result["cpus"][0]["online"])

    def test_empty_lscpu_e_returns_empty_lists(self):
        result = collect_cpu_topology("", smt_siblings_by_cpu={})
        self.assertEqual(result["cpus"], [])
        self.assertEqual(result["physical_cores"], [])


class LinuxCpuSysfsTest(unittest.TestCase):
    def setUp(self):
        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.tmp = Path(tmp_ctx.name)
        self.sys_root = self.tmp / "sys"

    def test_collect_smt_siblings_from_sysfs(self):
        _write(
            self.sys_root / "devices" / "system" / "cpu" / "cpu0" / "topology" / "thread_siblings_list",
            "0,64\n",
        )
        _write(
            self.sys_root / "devices" / "system" / "cpu" / "cpu1" / "topology" / "thread_siblings_list",
            "1\n",
        )
        _write(self.sys_root / "devices" / "system" / "cpu" / "online", "0-1,64\n")

        result = collect_smt_siblings(self.sys_root)

        self.assertEqual(result[0], [0, 64])
        self.assertEqual(result[1], [1])
        self.assertEqual(result[64], [64])

    def test_collect_smt_siblings_defaults_missing_file_to_self(self):
        _write(self.sys_root / "devices" / "system" / "cpu" / "online", "0-1\n")
        _write(
            self.sys_root / "devices" / "system" / "cpu" / "cpu0" / "topology" / "thread_siblings_list",
            "0\n",
        )

        result = collect_smt_siblings(self.sys_root)

        self.assertEqual(result[0], [0])
        self.assertEqual(result[1], [1])

    def test_collect_distance_matrix(self):
        _write(
            self.sys_root / "devices" / "system" / "node" / "node0" / "distance",
            "10 11\n",
        )
        _write(
            self.sys_root / "devices" / "system" / "node" / "node1" / "distance",
            "11 10\n",
        )

        matrix = collect_distance_matrix(self.sys_root)

        self.assertEqual(matrix, [[10, 11], [11, 10]])

    def test_collect_distance_matrix_skips_invalid_rows(self):
        _write(
            self.sys_root / "devices" / "system" / "node" / "node0" / "distance",
            "10 11\n",
        )
        _write(
            self.sys_root / "devices" / "system" / "node" / "node1" / "distance",
            "bad row\n",
        )

        matrix = collect_distance_matrix(self.sys_root)

        self.assertEqual(matrix, [[10, 11]])


if __name__ == "__main__":
    unittest.main()
