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

from scripts.collectors.npu_topology import collect_npu_topology, parse_npu_smi_info


NPU_SMI_INFO = """
+------------------------------------------------------------------------------------------------+
| npu-smi 25.3.rc1.2               Version: 25.3.rc1.2                                           |
+---------------------------+---------------+----------------------------------------------------+
| NPU   Name                | Health        | Power(W)    Temp(C)           Hugepages-Usage(page)|
| Chip                      | Bus-Id        | AICore(%)   Memory-Usage(MB)  HBM-Usage(MB)        |
+===========================+===============+====================================================+
| 0     910B2               | OK            | 90.1        33                0    / 0             |
| 0                         | 0000:C1:00.0  | 0           0    / 0          61715/ 65536         |
+===========================+===============+====================================================+
| 1     910B2               | OK            | 93.0        33                0    / 0             |
| 0                         | 0000:C2:00.0  | 0           0    / 0          61712/ 65536         |
+===========================+===============+====================================================+
| 2     910B2               | OK            | 89.0        33                0    / 0             |
| 0                         | 0000:81:00.0  | 0           0    / 0          61713/ 65536         |
+===========================+===============+====================================================+
+---------------------------+---------------+----------------------------------------------------+
| NPU     Chip              | Process id    | Process name             | Process memory(MB)      |
+===========================+===============+====================================================+
| 0       0                 | 2961213       | VLLMWorker_TP            | 58313                   |
+===========================+===============+====================================================+
| 2       0                 | 2961215       | VLLMWorker_TP            | 58313                   |
+===========================+===============+====================================================+
"""

NPU_SMI_TOPO = """
           NPU0       NPU1       NPU2       CPU Affinity
NPU0       X          HCCS       HCCS       144-167
NPU1       HCCS       X          HCCS       144-167
NPU2       HCCS       HCCS       X          96-119
"""

NUMA_NODES = [
    {"node": 4, "cpus": "96-119"},
    {"node": 6, "cpus": "144-167"},
]


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class NpuTopologyTest(unittest.TestCase):
    def setUp(self):
        tmp_ctx = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(tmp_ctx.cleanup)
        self.tmp = Path(tmp_ctx.name)
        self.sys_root = self.tmp / "sys"

    def test_parse_npu_smi_info_extracts_devices_and_processes(self):
        parsed = parse_npu_smi_info(NPU_SMI_INFO)

        self.assertEqual(parsed["devices"]["0"]["pci_bus_id"], "0000:C1:00.0")
        self.assertEqual(parsed["devices"]["0"]["model"], "910B2")
        self.assertEqual(parsed["devices"]["0"]["health"], "OK")
        self.assertEqual(parsed["devices"]["2"]["pci_bus_id"], "0000:81:00.0")
        self.assertEqual(parsed["processes"], {"0": [2961213], "2": [2961215]})

    def test_collect_uses_pci_numa_as_primary_source(self):
        _write(
            self.sys_root / "bus" / "pci" / "devices" / "0000:C1:00.0" / "numa_node",
            "7\n",
        )

        topology, availability = collect_npu_topology(
            npu_smi_info_text=NPU_SMI_INFO,
            npu_smi_topo_text=NPU_SMI_TOPO,
            numa_nodes=NUMA_NODES,
            sys_root=self.sys_root,
        )

        device0 = topology["devices"][0]
        self.assertEqual(device0["device_id"], "0")
        self.assertEqual(device0["logical_id"], "0")
        self.assertEqual(device0["pci_bus_id"], "0000:C1:00.0")
        self.assertEqual(device0["numa_node"], 7)
        self.assertEqual(device0["numa_source"], "pci")
        self.assertEqual(device0["local_cpus"], "144-167")
        self.assertEqual(topology["processes_by_device"], {"0": [2961213], "2": [2961215]})
        self.assertTrue(availability.to_dict()["complete"])

    def test_collect_falls_back_to_topo_affinity_when_pci_numa_missing(self):
        topology, availability = collect_npu_topology(
            npu_smi_info_text=NPU_SMI_INFO,
            npu_smi_topo_text=NPU_SMI_TOPO,
            numa_nodes=NUMA_NODES,
            sys_root=self.sys_root,
        )

        device2 = topology["devices"][2]
        self.assertEqual(device2["numa_node"], 4)
        self.assertEqual(device2["numa_source"], "topo_affinity")
        self.assertIn("npu_topology.devices[0].numa_node", availability.to_dict()["partial"])

    def test_user_map_preserves_numeric_zero_logical_id(self):
        topology, _ = collect_npu_topology(
            npu_smi_info_text=NPU_SMI_INFO,
            npu_smi_topo_text=NPU_SMI_TOPO,
            numa_nodes=NUMA_NODES,
            npu_map={"1": {"logical_id": 0, "numa_node": 3}},
            sys_root=self.sys_root,
        )

        device1 = topology["devices"][1]
        self.assertEqual(device1["device_id"], "1")
        self.assertEqual(device1["logical_id"], "0")

    def test_user_map_overrides_detected_values(self):
        topology, availability = collect_npu_topology(
            npu_smi_info_text=NPU_SMI_INFO,
            npu_smi_topo_text=NPU_SMI_TOPO,
            numa_nodes=NUMA_NODES,
            npu_map={
                "1": {
                    "numa_node": 3,
                    "local_cpus": "24-47",
                    "pci_bus_id": "0000:FF:00.0",
                }
            },
            sys_root=self.sys_root,
        )

        device1 = topology["devices"][1]
        self.assertEqual(device1["pci_bus_id"], "0000:FF:00.0")
        self.assertEqual(device1["numa_node"], 3)
        self.assertEqual(device1["numa_source"], "user_map")
        self.assertEqual(device1["local_cpus"], "24-47")
        self.assertTrue(availability.to_dict()["complete"])

    def test_missing_all_sources_records_missing_devices(self):
        topology, availability = collect_npu_topology(
            npu_smi_info_text="",
            npu_smi_topo_text="",
            numa_nodes=NUMA_NODES,
            sys_root=self.sys_root,
        )

        self.assertEqual(topology["devices"], [])
        self.assertIn("npu_topology.devices", availability.to_dict()["missing"])
        self.assertFalse(availability.to_dict()["complete"])


if __name__ == "__main__":
    unittest.main()
