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

from scripts.diagnose import diagnose
from scripts.planner import generate_plan
from scripts.report import render_report
from scripts.snapshot import load_snapshot


class ReportTest(unittest.TestCase):
    def test_report_contains_key_sections(self):
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
        findings = diagnose(snapshot)
        plan = generate_plan(snapshot, findings)
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, findings, plan, output)
            content = output.read_text(encoding="utf-8")
        self.assertIn("mindstudio-cpu-binding CPU 绑核优化报告", content)
        self.assertIn("当前 CPU 绑定状态", content)
        self.assertIn("CPU / NPU / NUMA 拓扑关系", content)
        self.assertIn("关键进程与线程", content)
        self.assertIn("SQ 线程", content)
        self.assertIn("dev0_sq_task", content)
        self.assertIn("通信线程", content)
        self.assertIn("问题发现", content)
        self.assertIn("推荐方案与回滚预览", content)
        self.assertIn("rollback-state", content)

    def test_report_makes_topology_and_key_process_sections_collapsible_by_default(
        self,
    ):
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
        findings = diagnose(snapshot)
        plan = generate_plan(snapshot, findings)
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, findings, plan, output)
            content = output.read_text(encoding="utf-8")
        self.assertIn("<details class='card collapsible-section topology-view' open>", content)
        self.assertIn("<summary>CPU / NPU / NUMA 拓扑关系</summary>", content)
        self.assertIn("<details class='card collapsible-section' open>", content)
        self.assertIn("<summary>关键进程与线程</summary>", content)


if __name__ == "__main__":
    unittest.main()
