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

    def test_diagnose_rank_worker_npu_numa_mismatch(self):
        snapshot = {
            "availability": {"missing": []},
            "system": {"online_cpus": "0-15"},
            "numa_topology": {
                "nodes": [
                    {"node": 0, "cpus": "0-7"},
                    {"node": 1, "cpus": "8-15"},
                ]
            },
            "npu_topology": {
                "devices": [
                    {"device_id": "0", "logical_id": "0", "numa_node": 0, "local_cpus": "0-7"},
                    {"device_id": "1", "logical_id": "1", "numa_node": 1, "local_cpus": "8-15"},
                ]
            },
            "workload": {"rank_mapping": [{"rank": "rank0", "pid": 100, "npu_device": "1"}]},
            "processes": [
                {
                    "pid": 100,
                    "rank": "rank0",
                    "npu_device": "1",
                    "cpus_allowed_list": "0-7",
                    "num_threads": 4,
                    "threads": [],
                }
            ],
            "cgroup": {"process_groups": []},
            "pytorch": {"threading": {}, "dataloader": {}},
            "runtime_sample": {"top_threads": []},
        }

        findings = diagnose(snapshot)

        self.assertTrue(any(finding["id"] == "R003" for finding in findings))

    def test_diagnose_binding_range_width_and_overlap_and_smt(self):
        snapshot = {
            "availability": {"missing": []},
            "system": {"online_cpus": "0-15"},
            "cpu_topology": {
                "physical_cores": [
                    {"core_key": "socket0-core0", "logical_cpus": [0, 8]},
                    {"core_key": "socket0-core1", "logical_cpus": [1, 9]},
                    {"core_key": "socket0-core2", "logical_cpus": [2, 10]},
                    {"core_key": "socket0-core3", "logical_cpus": [3, 11]},
                    {"core_key": "socket1-core0", "logical_cpus": [4, 12]},
                    {"core_key": "socket1-core1", "logical_cpus": [5, 13]},
                    {"core_key": "socket1-core2", "logical_cpus": [6, 14]},
                    {"core_key": "socket1-core3", "logical_cpus": [7, 15]},
                ]
            },
            "numa_topology": {
                "nodes": [
                    {"node": 0, "cpus": "0-3,8-11"},
                    {"node": 1, "cpus": "4-7,12-15"},
                ]
            },
            "npu_topology": {"devices": []},
            "workload": {"process_model": "multi-instance", "rank_mapping": []},
            "processes": [
                {
                    "pid": 100,
                    "rank": "rank0",
                    "npu_device": "0",
                    "cpus_allowed_list": "0-15",
                    "num_threads": 4,
                    "threads": [],
                },
                {
                    "pid": 200,
                    "rank": "rank1",
                    "npu_device": "1",
                    "cpus_allowed_list": "0",
                    "num_threads": 8,
                    "threads": [],
                },
                {
                    "pid": 300,
                    "rank": "rank2",
                    "npu_device": "2",
                    "cpus_allowed_list": "2-5",
                    "num_threads": 2,
                    "threads": [],
                },
            ],
            "cgroup": {"process_groups": []},
            "pytorch": {"threading": {}, "dataloader": {}},
            "runtime_sample": {"top_threads": []},
        }

        findings = diagnose(snapshot)
        ids = {finding["id"] for finding in findings}

        self.assertIn("R004", ids)
        self.assertIn("R005", ids)
        self.assertIn("R008", ids)
        self.assertIn("R009", ids)


if __name__ == "__main__":
    unittest.main()
