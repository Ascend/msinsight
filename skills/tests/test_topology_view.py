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
from scripts.topology_view import (
    build_topology_view,
    render_topology_html,
    render_topology_text,
)
from tests.fixtures import multi_rank_snapshot


class TopologyViewTest(unittest.TestCase):
    def test_builds_topology_view_from_snapshot_and_plan(self):
        snapshot = multi_rank_snapshot()
        findings = diagnose(snapshot)
        plan = generate_plan(snapshot, findings)

        view = build_topology_view(snapshot, plan)

        self.assertEqual(view["summary"]["numa_count"], 2)
        self.assertEqual(view["summary"]["npu_count"], 2)
        self.assertEqual(view["summary"]["process_count"], 2)
        self.assertEqual(view["summary"]["cross_numa_process_count"], 2)

        node0 = view["numa_nodes"][0]
        self.assertEqual(node0["node"], 0)
        self.assertEqual(node0["cpus"], "0-31,64-95")
        self.assertEqual(node0["local_npus"][0]["device_id"], "0")
        self.assertEqual(node0["processes"][0]["pid"], 12345)
        self.assertEqual(node0["processes"][0]["status"], "cross-numa")
        self.assertEqual(node0["processes"][0]["target_cpu_list"], "0-31")
        self.assertIn("topology_graph", view)
        self.assertEqual(view["topology_graph"]["nodes"][0]["id"], "server")

    def test_render_topology_html_contains_relation_section(self):
        snapshot = multi_rank_snapshot()
        snapshot["npu_topology"]["devices"][0]["links"] = [{"target": "1", "type": "HCCS"}]
        findings = diagnose(snapshot)
        plan = generate_plan(snapshot, findings)
        view = build_topology_view(snapshot, plan)

        content = render_topology_html(view)

        self.assertIn("CPU / NPU / NUMA 拓扑关系", content)
        self.assertIn("topology-svg", content)
        self.assertIn("Server", content)
        self.assertIn("NUMA 0", content)
        self.assertIn("NPU 0", content)
        self.assertIn("PID 12345", content)
        self.assertIn("HCCS", content)
        self.assertIn("跨 NUMA", content)

    def test_render_topology_html_uses_clear_process_role_label(self):
        view = {
            "summary": {},
            "topology_graph": {},
            "numa_nodes": [
                {
                    "node": 0,
                    "cpus": "0-3",
                    "local_npus": [],
                    "processes": [
                        {
                            "pid": 12345,
                            "role": "tp0",
                            "npu_device": "npu0",
                            "current_cpu_list": "0-3",
                            "effective_cpu_list": "0-3",
                            "target_cpu_list": "0-3",
                            "status": "local",
                        }
                    ],
                }
            ],
            "orphan_processes": [],
            "warnings": [],
            "missing": [],
        }

        content = render_topology_html(view)

        self.assertIn("<h4>目标进程</h4>", content)
        self.assertIn("<strong>PID 12345</strong>", content)
        self.assertIn("<span>Role/Rank：tp0</span>", content)
        self.assertIn("<span>Comm：</span>", content)
        self.assertNotIn("<span>完整命令：</span>", content)
        self.assertNotIn("进程 / Rank / Worker", content)
        self.assertNotIn("PID 12345 / tp0", content)

    def test_build_topology_view_keeps_rank_map_label_without_prefixing_rank(self):
        snapshot = {
            "system": {},
            "numa_topology": {"nodes": [{"node": 0, "cpus": "0-3"}]},
            "npu_topology": {"devices": []},
            "processes": [
                {
                    "pid": 12345,
                    "rank": "tp0",
                    "comm": "VLLM::Worker_TP",
                    "cpus_allowed_list": "0-3",
                }
            ],
        }

        view = build_topology_view(snapshot, None)

        self.assertEqual(view["numa_nodes"][0]["processes"][0]["role"], "tp0")

    def test_dense_topology_expands_svg_viewbox_to_keep_nodes_separated(self):
        snapshot = multi_rank_snapshot()
        snapshot["numa_topology"]["nodes"] = [
            {"node": node, "cpus": f"{node * 16}-{node * 16 + 15}"} for node in range(4)
        ]
        snapshot["npu_topology"]["devices"] = [
            {
                "device_id": str(device),
                "pci_bus_id": f"0000:{device:02x}:00.0",
                "numa_node": device // 4,
            }
            for device in range(16)
        ]
        view = build_topology_view(snapshot, None)

        content = render_topology_html(view)

        graph_width = view["topology_graph"]["width"]
        self.assertIn(f'viewBox="0 0 {graph_width} 520"', content)
        self.assertGreater(graph_width, 800)
        npu_nodes = [node for node in view["topology_graph"]["nodes"] if node["kind"] == "npu"]
        self.assertGreaterEqual(min(node["x"] for node in npu_nodes), 80)
        self.assertLessEqual(max(node["x"] for node in npu_nodes), graph_width - 80)
        self.assertGreaterEqual(
            min(abs(left["x"] - right["x"]) for left, right in zip(npu_nodes, npu_nodes[1:])),
            120,
        )

    def test_missing_npu_topology_does_not_crash(self):
        snapshot = multi_rank_snapshot()
        snapshot.pop("npu_topology", None)

        view = build_topology_view(snapshot, None)
        content = render_topology_text(view)

        self.assertEqual(view["summary"]["npu_count"], 0)
        self.assertIn("npu_topology.devices", view["missing"])
        self.assertIn("缺少 npu_topology.devices", content)


if __name__ == "__main__":
    unittest.main()
