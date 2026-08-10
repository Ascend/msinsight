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

from scripts.cpulist import (
    format_cpu_list,
    intersect_cpu_lists,
    numa_nodes_for_cpu_list,
    parse_cpu_list,
)


class CpuListTest(unittest.TestCase):
    def test_parse_and_format(self):
        self.assertEqual(parse_cpu_list("0-3,8,10-12"), {0, 1, 2, 3, 8, 10, 11, 12})
        self.assertEqual(format_cpu_list({0, 1, 2, 4, 7, 8}), "0-2,4,7-8")

    def test_intersect(self):
        self.assertEqual(intersect_cpu_lists("0-7", "4-9"), {4, 5, 6, 7})

    def test_numa_nodes_for_cpu_list(self):
        nodes = [{"node": 0, "cpus": "0-3"}, {"node": 1, "cpus": "4-7"}]
        self.assertEqual(numa_nodes_for_cpu_list("1,5", nodes), {0, 1})


if __name__ == "__main__":
    unittest.main()
