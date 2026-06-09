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
from scripts.planner import generate_plan
from scripts.snapshot import load_snapshot
from scripts.topology_view import (
    build_topology_view,
    render_topology_html,
    render_topology_text,
)


class TopologyViewTest(unittest.TestCase):
    def test_builds_topology_view_from_snapshot_and_plan(self):
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
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
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
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

    def test_dense_topology_expands_svg_viewbox_to_keep_nodes_separated(self):
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
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
        snapshot = load_snapshot(ROOT / "samples" / "snapshot.multi-rank.json")
        snapshot.pop("npu_topology", None)

        view = build_topology_view(snapshot, None)
        content = render_topology_text(view)

        self.assertEqual(view["summary"]["npu_count"], 0)
        self.assertIn("npu_topology.devices", view["missing"])
        self.assertIn("缺少 npu_topology.devices", content)


if __name__ == "__main__":
    unittest.main()
