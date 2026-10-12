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

from scripts.report import render_report


class ReportTest(unittest.TestCase):
    def _render_minimal_report(self, findings):
        snapshot = {
            "processes": [],
            "numa_topology": {"nodes": []},
            "key_processes": {},
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, findings, plan, output)
            return output.read_text(encoding="utf-8")

    def test_report_contains_key_sections(self):
        snapshot = {
            "processes": [],
            "numa_topology": {"nodes": []},
            "key_processes": {
                "sq_task_threads": [{"pid": 100, "tid": 101, "name": "dev0_sq_task"}],
                "communication_threads": [{"pid": 100, "tid": 102, "name": "hcom"}],
            },
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, [], plan, output)
            content = output.read_text(encoding="utf-8")
        self.assertIn("mindstudio-cpu-binding CPU 绑核优化报告", content)
        self.assertIn("当前 CPU 绑定状态", content)
        self.assertIn("<th>Role/Rank</th>", content)
        self.assertIn("<th>Comm</th>", content)
        self.assertIn("<th>完整命令</th>", content)
        self.assertNotIn("<th>Rank</th>", content)
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
        content = self._render_minimal_report([])
        self.assertIn("<details class='card collapsible-section topology-view' open>", content)
        self.assertIn("<summary>CPU / NPU / NUMA 拓扑关系</summary>", content)
        self.assertIn("<details class='card collapsible-section' open>", content)
        self.assertIn("<summary>关键进程与线程</summary>", content)

    def test_report_shows_attention_section_before_summary_and_findings_before_status(
        self,
    ):
        findings = [
            {
                "id": "cross_numa",
                "severity": "high",
                "title": "跨 NUMA 绑核",
                "judgement": "Rank 0 当前 CPU 覆盖多个 NUMA 节点，需要收敛到本地 CPU。",
                "evidence": ["pid 123 uses NUMA 0 and 1"],
                "recommendations": ["绑定到 NPU 本地 NUMA CPU"],
            }
        ]
        content = self._render_minimal_report(findings)

        self.assertLess(content.index("需要关注的问题"), content.index("总体结论"))
        self.assertLess(content.index("问题发现"), content.index("当前 CPU 绑定状态"))
        self.assertIn("共识别 1 个问题", content)
        self.assertIn("High / Medium：1 / 0", content)
        self.assertIn("跨 NUMA 绑核", content)
        self.assertIn("Rank 0 当前 CPU 覆盖多个 NUMA 节点", content)

    def test_report_collapses_findings_when_more_than_three_with_high_open_only(self):
        findings = [
            {
                "id": f"finding_{index}",
                "severity": severity,
                "title": f"问题 {index}",
                "judgement": f"判断 {index}",
                "evidence": [f"证据 {index}"],
                "recommendations": [f"建议 {index}"],
            }
            for index, severity in enumerate(["high", "medium", "low", "medium"], start=1)
        ]
        content = self._render_minimal_report(findings)

        self.assertIn("<details class='card finding-card' open>", content)
        self.assertEqual(content.count("<details class='card finding-card'>"), 3)
        self.assertIn(
            "<summary><span class='badge high'>high</span> 问题 1 - 判断 1</summary>",
            content,
        )
        self.assertIn(
            "<summary><span class='badge medium'>medium</span> 问题 2 - 判断 2</summary>",
            content,
        )
        self.assertIn("finding_1", content)
        self.assertIn("证据 1", content)
        self.assertIn("建议 1", content)

    def test_report_shows_empty_findings_state_in_attention_and_full_sections(self):
        content = self._render_minimal_report([])

        self.assertIn("需要关注的问题", content)
        self.assertIn("未识别到明确 CPU 绑核问题。", content)
        self.assertIn("<h2>问题发现</h2>", content)
        self.assertGreaterEqual(content.count("未识别到明确 CPU 绑核问题。"), 2)

    def test_report_fills_main_scheduler_name_from_process_command(self):
        snapshot = {
            "processes": [
                {
                    "pid": 768219,
                    "comm": "vllm",
                    "command": "python -m vllm.entrypoints.openai.api_server",
                }
            ],
            "numa_topology": {"nodes": []},
            "key_processes": {"main_scheduler_pids": [768219]},
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, [], plan, output)
            content = output.read_text(encoding="utf-8")

        self.assertIn("<td>主调度进程</td><td>768219</td><td>768219</td>", content)
        self.assertIn("python -m vllm.entrypoints.openai.api_server", content)

    def test_report_uses_process_command_for_short_vllm_worker_thread_name(self):
        snapshot = {
            "processes": [
                {
                    "pid": 2961213,
                    "comm": "VLLM::Worker_TP",
                    "command": "VLLM::Worker_TP0",
                }
            ],
            "numa_topology": {"nodes": []},
            "key_processes": {
                "top_threads": [
                    {
                        "pid": 2961213,
                        "tid": 2962190,
                        "name": "VLLM::Worker",
                        "key_class": "engine_worker",
                    }
                ],
                "sq_task_threads": [
                    {
                        "pid": 2961213,
                        "tid": 2962191,
                        "name": "dev0_sq_task",
                        "key_class": "sq_task",
                    }
                ],
            },
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, [], plan, output)
            content = output.read_text(encoding="utf-8")

        self.assertIn("VLLM::Worker_TP0", content)
        self.assertNotIn("dev0_sq_task", content)

    def test_report_shows_process_tree_without_thread_details(self):
        snapshot = {
            "processes": [
                {
                    "pid": 100,
                    "ppid": 1,
                    "comm": "parent",
                    "command": "python parent.py",
                    "rank": "",
                    "npu_device": "",
                    "cpus_allowed_list": "0-3",
                },
                {
                    "pid": 101,
                    "ppid": 100,
                    "comm": "child",
                    "command": "python child.py",
                    "rank": 0,
                    "npu_device": 0,
                    "cpus_allowed_list": "4-7",
                },
            ],
            "process_tree": {
                "roots": [100],
                "nodes": [
                    {
                        "pid": 100,
                        "ppid": 1,
                        "children": [101],
                        "tree_root": 100,
                        "depth": 0,
                        "parent_missing": True,
                    },
                    {
                        "pid": 101,
                        "ppid": 100,
                        "children": [],
                        "tree_root": 100,
                        "depth": 1,
                        "parent_missing": False,
                    },
                ],
                "missing_parent_pids": [1],
            },
            "numa_topology": {"nodes": []},
            "key_processes": {
                "sq_task_threads": [{"pid": 101, "tid": 201, "name": "dev0_sq_task"}],
                "communication_threads": [{"pid": 101, "tid": 202, "name": "hcom"}],
                "npu_fixed_threads": [{"pid": 101, "tid": 204, "name": "npu_fixed"}],
                "dataloader_threads": [{"pid": 101, "tid": 205, "name": "dataloader"}],
                "top_threads": [{"pid": 101, "tid": 203, "name": "busy_worker"}],
            },
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [{"pid": 101, "target_cpu_list": "8-11", "effective_cpu_list": "4-7"}],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, [], plan, output)
            content = output.read_text(encoding="utf-8")

        self.assertIn("进程关系 / 父子进程树", content)
        self.assertLess(content.index("当前 CPU 绑定状态"), content.index("进程关系 / 父子进程树"))
        self.assertLess(
            content.index("进程关系 / 父子进程树"),
            content.index("CPU / NPU / NUMA 拓扑关系"),
        )
        self.assertIn("python parent.py", content)
        self.assertIn("python child.py", content)
        self.assertIn("4-7", content)
        self.assertIn("8-11", content)
        self.assertIn("识别到的关键线程", content)
        self.assertIn("本表按父子关系展示进程。", content)
        self.assertIn("具体线程明细请查看下方“关键进程与线程”章节。", content)
        self.assertIn(
            "SQ 线程 1 个，通信线程 1 个，NPU 固定线程 1 个，DataLoader 线程 1 个，高 CPU 线程 1 个",
            content,
        )
        self.assertIn("未识别到关键线程", content)
        self.assertIn("以下父进程未在本次采集中出现：1", content)
        tree_section = content[content.index("进程关系 / 父子进程树") : content.index("CPU / NPU / NUMA 拓扑关系")]
        self.assertNotIn("dev0_sq_task", tree_section)
        self.assertNotIn("npu_fixed", tree_section)
        self.assertNotIn("dataloader", tree_section)
        self.assertNotIn("dev0_sq_task", content)
        self.assertNotIn("npu_fixed", content)
        self.assertNotIn("dataloader", content)
        self.assertIn("python child.py", content)

    def test_report_builds_process_tree_when_snapshot_has_no_process_tree(self):
        snapshot = {
            "processes": [
                {
                    "pid": 101,
                    "ppid": 100,
                    "comm": "child",
                    "command": "python child.py",
                    "cpus_allowed_list": "4-7",
                },
                {
                    "pid": 100,
                    "ppid": 1,
                    "comm": "parent",
                    "command": "python parent.py",
                    "cpus_allowed_list": "0-3",
                },
            ],
            "numa_topology": {"nodes": []},
            "key_processes": {},
            "availability": {"missing": []},
        }
        plan = {
            "summary": "测试结论",
            "executor_backend": "dry-run",
            "apply_actions": [],
            "rollback_actions": [],
            "rollback_state_preview": {},
        }
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "report.html"
            render_report(snapshot, [], plan, output)
            content = output.read_text(encoding="utf-8")

        self.assertIn("进程关系 / 父子进程树", content)
        tree_section = content[content.index("进程关系 / 父子进程树") : content.index("CPU / NPU / NUMA 拓扑关系")]
        self.assertIn("python parent.py", tree_section)
        self.assertIn("python child.py", tree_section)
        self.assertLess(
            tree_section.index("python parent.py"),
            tree_section.index("python child.py"),
        )


if __name__ == "__main__":
    unittest.main()
