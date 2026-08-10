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
sys.path.insert(0, str(SKILLS_ROOT))
sys.path.insert(0, str(ROOT))

from scripts.diagnose import diagnose
from scripts.planner import generate_plan
from tests.fixtures import multi_rank_snapshot


class PlannerTest(unittest.TestCase):
    def test_plan_contains_apply_and_rollback_actions(self):
        snapshot = multi_rank_snapshot()
        findings = diagnose(snapshot)
        plan = generate_plan(snapshot, findings)
        self.assertEqual(plan["executor_backend"], "dry-run")
        self.assertTrue(plan["requires_confirmation"])
        self.assertGreaterEqual(len(plan["apply_actions"]), 1)
        self.assertEqual(len(plan["apply_actions"]), len(plan["rollback_actions"]))
        self.assertIn("rollback_state_preview", plan)


if __name__ == "__main__":
    unittest.main()
