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

from scripts.collectors.availability import Availability


class AvailabilityTest(unittest.TestCase):
    def test_empty_is_complete(self):
        avail = Availability()
        self.assertTrue(avail.to_dict()["complete"])
        self.assertEqual(avail.to_dict()["missing"], [])
        self.assertEqual(avail.to_dict()["partial"], [])
        self.assertEqual(avail.to_dict()["errors"], [])

    def test_missing_marks_incomplete(self):
        avail = Availability()
        avail.add_missing("npu_topology.devices[0].numa_node")
        data = avail.to_dict()
        self.assertFalse(data["complete"])
        self.assertIn("npu_topology.devices[0].numa_node", data["missing"])

    def test_partial_does_not_make_incomplete(self):
        avail = Availability()
        avail.add_partial("cpu_topology.cpus")
        data = avail.to_dict()
        self.assertTrue(data["complete"])
        self.assertEqual(data["partial"], ["cpu_topology.cpus"])

    def test_errors_mark_incomplete(self):
        avail = Availability()
        avail.add_error("npu_topology", "npu-smi not found")
        data = avail.to_dict()
        self.assertFalse(data["complete"])
        self.assertEqual(len(data["errors"]), 1)
        self.assertEqual(data["errors"][0]["component"], "npu_topology")
        self.assertEqual(data["errors"][0]["message"], "npu-smi not found")

    def test_merge_combines_all_fields(self):
        a1 = Availability()
        a1.add_missing("field_a")
        a1.add_partial("field_p1")
        a1.add_error("comp_a", "boom")

        a2 = Availability()
        a2.add_missing("field_b")
        a2.add_partial("field_p2")
        a2.add_error("comp_b", "kaboom")

        a1.merge(a2)
        data = a1.to_dict()
        self.assertEqual(set(data["missing"]), {"field_a", "field_b"})
        self.assertEqual(set(data["partial"]), {"field_p1", "field_p2"})
        self.assertEqual(len(data["errors"]), 2)

    def test_merge_dedupes_missing_and_partial(self):
        a1 = Availability()
        a1.add_missing("dup")
        a1.add_partial("dupp")

        a2 = Availability()
        a2.add_missing("dup")
        a2.add_partial("dupp")

        a1.merge(a2)
        data = a1.to_dict()
        self.assertEqual(data["missing"], ["dup"])
        self.assertEqual(data["partial"], ["dupp"])

    def test_merge_dedupes_identical_errors(self):
        a1 = Availability()
        a1.add_error("comp", "same")
        a2 = Availability()
        a2.add_error("comp", "same")
        a1.merge(a2)
        self.assertEqual(len(a1.to_dict()["errors"]), 1)

    def test_to_dict_returns_fresh_lists(self):
        avail = Availability()
        avail.add_missing("x")
        d1 = avail.to_dict()
        d1["missing"].append("mutated")
        d2 = avail.to_dict()
        self.assertEqual(d2["missing"], ["x"])

    def test_add_missing_dedupes_within_single_instance(self):
        avail = Availability()
        avail.add_missing("same")
        avail.add_missing("same")
        self.assertEqual(avail.to_dict()["missing"], ["same"])


if __name__ == "__main__":
    unittest.main()
