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

from scripts.collectors.linux_cgroup import CgroupReader, collect_cgroup_for_pid


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class CgroupV2Test(unittest.TestCase):
    def setUp(self):
        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.tmp = Path(tmp_ctx.name)
        self.proc_root = self.tmp / "proc"
        self.cgroup_root = self.tmp / "cgroup"
        self.reader = CgroupReader(cgroup_root=self.cgroup_root, proc_root=self.proc_root)

    def test_v2_collects_cpuset_cpu_max_and_stat(self):
        _write(self.proc_root / "12345" / "cgroup", "0::/user.slice/session.scope\n")
        base = self.cgroup_root / "user.slice" / "session.scope"
        _write(base / "cpuset.cpus.effective", "0-31\n")
        _write(base / "cpuset.mems.effective", "0\n")
        _write(base / "cpu.max", "400000 100000\n")
        _write(base / "cpu.weight", "100\n")
        _write(
            base / "cpu.stat",
            "usage_usec 5000000\nuser_usec 3000000\nsystem_usec 2000000\nnr_periods 10000\nnr_throttled 12\nthrottled_usec 3456\n",
        )

        result, avail = collect_cgroup_for_pid(self.reader, 12345)

        self.assertEqual(result["version"], "v2")
        group = result["process_groups"][0]
        self.assertEqual(group["pid"], 12345)
        self.assertEqual(group["path"], "/user.slice/session.scope")
        self.assertEqual(group["cpuset_cpus_effective"], "0-31")
        self.assertEqual(group["cpuset_mems_effective"], "0")
        self.assertEqual(group["cpu_max"], "400000 100000")
        self.assertEqual(group["cpu_quota_us"], 400000)
        self.assertEqual(group["cpu_period_us"], 100000)
        self.assertEqual(group["cpu_weight"], 100)
        self.assertEqual(group["nr_periods"], 10000)
        self.assertEqual(group["nr_throttled"], 12)
        self.assertEqual(group["throttled_usec"], 3456)
        self.assertTrue(avail.to_dict()["complete"])

    def test_v2_cpu_max_max_quota_has_none_quota(self):
        _write(self.proc_root / "12345" / "cgroup", "0::/docker/abc.scope\n")
        base = self.cgroup_root / "docker" / "abc.scope"
        _write(base / "cpuset.cpus.effective", "0-191\n")
        _write(base / "cpu.max", "max 100000\n")

        result, _ = collect_cgroup_for_pid(self.reader, 12345)

        group = result["process_groups"][0]
        self.assertEqual(group["cpu_max"], "max 100000")
        self.assertIsNone(group["cpu_quota_us"])
        self.assertEqual(group["cpu_period_us"], 100000)

    def test_v2_falls_back_to_parent_for_cpuset_when_leaf_missing(self):
        _write(
            self.proc_root / "12345" / "cgroup",
            "0::/user.slice/user-1017.slice/session.scope\n",
        )
        _write(self.cgroup_root / "user.slice" / "cpuset.cpus.effective", "0-191\n")
        _write(self.cgroup_root / "user.slice" / "cpuset.mems.effective", "0-7\n")
        _write(
            self.cgroup_root / "user.slice" / "user-1017.slice" / "session.scope" / "cpu.stat",
            "nr_periods 0\n",
        )

        result, avail = collect_cgroup_for_pid(self.reader, 12345)

        group = result["process_groups"][0]
        self.assertEqual(group["cpuset_cpus_effective"], "0-191")
        self.assertEqual(group["cpuset_mems_effective"], "0-7")
        self.assertIn(
            "cgroup.process_groups[12345].cpuset_cpus_effective",
            avail.to_dict()["partial"],
        )

    def test_missing_proc_cgroup_returns_error(self):
        result, avail = collect_cgroup_for_pid(self.reader, 99999)

        self.assertIsNone(result)
        self.assertFalse(avail.to_dict()["complete"])
        self.assertEqual(avail.to_dict()["errors"][0]["component"], "linux_cgroup[99999]")


class CgroupV1Test(unittest.TestCase):
    def setUp(self):
        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.tmp = Path(tmp_ctx.name)
        self.proc_root = self.tmp / "proc"
        self.cgroup_root = self.tmp / "cgroup"
        self.reader = CgroupReader(cgroup_root=self.cgroup_root, proc_root=self.proc_root)

    def test_v1_collects_cpuset_quota_and_stat(self):
        _write(
            self.proc_root / "12345" / "cgroup",
            "12:cpuset:/docker/abc123\n11:cpu,cpuacct:/docker/abc123\n",
        )
        _write(
            self.cgroup_root / "cpuset" / "docker" / "abc123" / "cpuset.effective_cpus",
            "0-15\n",
        )
        _write(
            self.cgroup_root / "cpuset" / "docker" / "abc123" / "cpuset.effective_mems",
            "0\n",
        )
        _write(
            self.cgroup_root / "cpu" / "docker" / "abc123" / "cpu.cfs_quota_us",
            "400000\n",
        )
        _write(
            self.cgroup_root / "cpu" / "docker" / "abc123" / "cpu.cfs_period_us",
            "100000\n",
        )
        _write(
            self.cgroup_root / "cpu" / "docker" / "abc123" / "cpu.stat",
            "nr_periods 5000\nnr_throttled 10\nthrottled_time 500000\n",
        )

        result, avail = collect_cgroup_for_pid(self.reader, 12345)

        self.assertEqual(result["version"], "v1")
        group = result["process_groups"][0]
        self.assertEqual(group["pid"], 12345)
        self.assertEqual(group["path"], "/docker/abc123")
        self.assertEqual(group["cpuset_cpus_effective"], "0-15")
        self.assertEqual(group["cpuset_mems_effective"], "0")
        self.assertEqual(group["cpu_quota_us"], 400000)
        self.assertEqual(group["cpu_period_us"], 100000)
        self.assertEqual(group["nr_periods"], 5000)
        self.assertEqual(group["nr_throttled"], 10)
        self.assertEqual(group["throttled_usec"], 500000)
        self.assertTrue(avail.to_dict()["complete"])

    def test_v1_falls_back_to_cpuset_cpus_when_effective_missing(self):
        _write(self.proc_root / "12345" / "cgroup", "12:cpuset:/docker/abc123\n")
        _write(self.cgroup_root / "cpuset" / "docker" / "abc123" / "cpuset.cpus", "0-7\n")
        _write(self.cgroup_root / "cpuset" / "docker" / "abc123" / "cpuset.mems", "0\n")

        result, avail = collect_cgroup_for_pid(self.reader, 12345)

        self.assertEqual(result["version"], "v1")
        self.assertEqual(result["process_groups"][0]["cpuset_cpus_effective"], "0-7")
        self.assertEqual(result["process_groups"][0]["cpuset_mems_effective"], "0")
        self.assertIn(
            "cgroup.process_groups[12345].cpuset_cpus_effective",
            avail.to_dict()["partial"],
        )

    def test_v1_collects_cpu_controller_from_combined_mount(self):
        _write(
            self.proc_root / "12345" / "cgroup",
            "12:cpuset:/docker/abc123\n11:cpu,cpuacct:/docker/abc123\n",
        )
        _write(
            self.cgroup_root / "cpuset" / "docker" / "abc123" / "cpuset.effective_cpus",
            "0-15\n",
        )
        _write(
            self.cgroup_root / "cpu,cpuacct" / "docker" / "abc123" / "cpu.cfs_quota_us",
            "200000\n",
        )
        _write(
            self.cgroup_root / "cpu,cpuacct" / "docker" / "abc123" / "cpu.cfs_period_us",
            "100000\n",
        )
        _write(
            self.cgroup_root / "cpu,cpuacct" / "docker" / "abc123" / "cpu.stat",
            "nr_periods 3\nnr_throttled 1\nthrottled_time 1000\n",
        )

        result, _ = collect_cgroup_for_pid(self.reader, 12345)

        group = result["process_groups"][0]
        self.assertEqual(group["cpu_quota_us"], 200000)
        self.assertEqual(group["cpu_period_us"], 100000)
        self.assertEqual(group["nr_periods"], 3)
        self.assertEqual(group["nr_throttled"], 1)
        self.assertEqual(group["throttled_usec"], 1000)


class CgroupReaderTest(unittest.TestCase):
    def test_read_proc_missing_returns_none(self):
        reader = CgroupReader(cgroup_root=Path("/nonexistent-cgroup"), proc_root=Path("/nonexistent-proc"))
        self.assertIsNone(reader.read_proc(1, "cgroup"))

    def test_read_sys_missing_returns_none(self):
        reader = CgroupReader(cgroup_root=Path("/nonexistent-cgroup"), proc_root=Path("/nonexistent-proc"))
        self.assertIsNone(reader.read_sys("cpu.max"))


if __name__ == "__main__":
    unittest.main()
