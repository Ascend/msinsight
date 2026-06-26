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
# MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

# ruff: noqa: E402
import sys
import unittest
from pathlib import Path

SKILLS_ROOT = Path(__file__).resolve().parents[1]
ROOT = SKILLS_ROOT / "mindstudio-cpu-binding"
sys.path.insert(0, str(ROOT))

from scripts.process_tree import build_process_tree


class ProcessTreeTest(unittest.TestCase):
    def test_single_root_tree_tracks_depths_and_missing_parent(self):
        result = build_process_tree(
            [
                {"pid": 100, "ppid": 1},
                {"pid": 101, "ppid": 100},
                {"pid": 102, "ppid": 101},
            ]
        )

        self.assertEqual(result["roots"], [100])
        self.assertEqual(result["missing_parent_pids"], [1])
        by_pid = {node["pid"]: node for node in result["nodes"]}
        self.assertEqual(by_pid[100]["children"], [101])
        self.assertEqual(by_pid[101]["children"], [102])
        self.assertEqual(by_pid[102]["children"], [])
        self.assertEqual(by_pid[100]["depth"], 0)
        self.assertEqual(by_pid[101]["depth"], 1)
        self.assertEqual(by_pid[102]["depth"], 2)
        self.assertTrue(by_pid[100]["parent_missing"])
        self.assertFalse(by_pid[101]["parent_missing"])
        self.assertFalse(by_pid[102]["parent_missing"])
        self.assertEqual(by_pid[100]["tree_root"], 100)
        self.assertEqual(by_pid[101]["tree_root"], 100)
        self.assertEqual(by_pid[102]["tree_root"], 100)

    def test_multiple_roots_are_returned_in_pid_order(self):
        result = build_process_tree(
            [
                {"pid": 201, "ppid": 200},
                {"pid": 101, "ppid": 100},
                {"pid": 200, "ppid": 0},
                {"pid": 100, "ppid": 0},
            ]
        )

        self.assertEqual(result["roots"], [100, 200])
        by_pid = {node["pid"]: node for node in result["nodes"]}
        self.assertEqual(by_pid[100]["children"], [101])
        self.assertEqual(by_pid[200]["children"], [201])
        self.assertEqual(by_pid[101]["tree_root"], 100)
        self.assertEqual(by_pid[201]["tree_root"], 200)

    def test_missing_parent_process_becomes_root(self):
        result = build_process_tree([{"pid": 101, "ppid": 999}])

        self.assertEqual(result["roots"], [101])
        self.assertEqual(result["missing_parent_pids"], [999])
        self.assertEqual(result["nodes"][0]["parent_missing"], True)
        self.assertEqual(result["nodes"][0]["tree_root"], 101)
        self.assertEqual(result["nodes"][0]["depth"], 0)

    def test_cycle_chooses_smallest_pid_as_root_without_infinite_recursion(self):
        result = build_process_tree(
            [
                {"pid": 100, "ppid": 101},
                {"pid": 101, "ppid": 100},
            ]
        )

        self.assertEqual(result["roots"], [100])
        self.assertEqual([node["pid"] for node in result["nodes"]], [100, 101])
        by_pid = {node["pid"]: node for node in result["nodes"]}
        self.assertEqual(by_pid[100]["tree_root"], 100)
        self.assertEqual(by_pid[101]["tree_root"], 100)
        self.assertEqual(by_pid[100]["depth"], 0)
        self.assertEqual(by_pid[101]["depth"], 1)
        self.assertEqual(by_pid[100]["children"], [101])
        self.assertEqual(by_pid[101]["children"], [])

    def test_invalid_missing_pid_ignored_and_numeric_strings_accepted(self):
        result = build_process_tree(
            [
                {"pid": None, "ppid": 0},
                {"ppid": 0},
                {"pid": "abc", "ppid": 0},
                {"pid": "100", "ppid": "0"},
                {"pid": "101", "ppid": "100"},
            ]
        )

        self.assertEqual(result["roots"], [100])
        self.assertEqual([node["pid"] for node in result["nodes"]], [100, 101])
        by_pid = {node["pid"]: node for node in result["nodes"]}
        self.assertEqual(by_pid[100]["ppid"], 0)
        self.assertEqual(by_pid[101]["ppid"], 100)
        self.assertEqual(by_pid[100]["children"], [101])


if __name__ == "__main__":
    unittest.main()
