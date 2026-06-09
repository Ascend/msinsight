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

from scripts.diagnose import diagnose
from scripts.snapshot import load_snapshot


class DiagnoseTest(unittest.TestCase):
    def test_sample_triggers_core_rules(self):
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
        findings = diagnose(snapshot)
        ids = {finding["id"] for finding in findings}
        self.assertIn("R001", ids)
        self.assertIn("R002", ids)
        self.assertIn("R006", ids)
        self.assertIn("R010", ids)


if __name__ == "__main__":
    unittest.main()
