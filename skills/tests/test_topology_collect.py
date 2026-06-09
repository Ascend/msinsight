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
import json
import sys
import tempfile
import unittest
from pathlib import Path

SKILLS_ROOT = Path(__file__).resolve().parents[1]
ROOT = SKILLS_ROOT / "mindstudio-cpu-binding"
sys.path.insert(0, str(ROOT))

from scripts.cli import main as cli_main
from scripts.topology_collect import collect_topology_from_text, main


class TopologyCollectTest(unittest.TestCase):
    def test_collect_topology_from_sample_text(self):
        lscpu_text = (ROOT / "samples" / "lscpu.sample.txt").read_text(encoding="utf-8")
        npu_topo_text = (ROOT / "samples" / "npu-smi-topo.sample.txt").read_text(encoding="utf-8")

        result = collect_topology_from_text(lscpu_text, npu_topo_text)

        self.assertEqual(result["system"]["total_logical_cpus"], 128)
        self.assertTrue(result["system"]["smt_enabled"])
        self.assertEqual(result["numa_topology"]["nodes"][0]["cpus"], "0-31,64-95")
        self.assertEqual(result["numa_topology"]["nodes"][1]["physical_core_count"], 32)
        self.assertEqual(result["npu_topology"]["vendor"], "ascend")
        self.assertEqual(result["npu_topology"]["devices"][0]["device_id"], "0")
        self.assertEqual(result["npu_topology"]["devices"][0]["numa_node"], 0)
        self.assertEqual(result["npu_topology"]["devices"][0]["local_cpus"], "0-31,64-95")
        self.assertEqual(
            result["npu_topology"]["devices"][0]["links"][0],
            {"target": "1", "type": "HCCS"},
        )
        self.assertEqual(result["availability"]["missing"], [])

    def test_cli_writes_topology_json_from_sample_files(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "topology.json"
            exit_code = main(
                [
                    "--lscpu-file",
                    str(ROOT / "samples" / "lscpu.sample.txt"),
                    "--npu-smi-topo-file",
                    str(ROOT / "samples" / "npu-smi-topo.sample.txt"),
                    "--out",
                    str(output),
                ]
            )
            data = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 0)
        self.assertEqual(data["npu_topology"]["devices"][1]["device_id"], "1")
        self.assertEqual(data["npu_topology"]["devices"][1]["numa_node"], 1)

    def test_cli_collect_topology_command_writes_json(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            output = Path(tmpdir) / "topology.json"
            exit_code = cli_main(
                [
                    "collect-topology",
                    "--lscpu-file",
                    str(ROOT / "samples" / "lscpu.sample.txt"),
                    "--npu-smi-topo-file",
                    str(ROOT / "samples" / "npu-smi-topo.sample.txt"),
                    "--out",
                    str(output),
                ]
            )
            data = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 0)
        self.assertEqual(data["numa_topology"]["nodes"][0]["node"], 0)
        self.assertEqual(data["npu_topology"]["devices"][0]["links"][0]["type"], "HCCS")


if __name__ == "__main__":
    unittest.main()
