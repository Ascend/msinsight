# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
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
from tests.fixtures import multi_rank_snapshot


class DiagnoseTest(unittest.TestCase):
    def test_sample_triggers_core_rules(self):
        snapshot = multi_rank_snapshot()
        findings = diagnose(snapshot)
        ids = {finding["id"] for finding in findings}
        self.assertIn("R002", ids)
        self.assertIn("R008", ids)

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
                    {
                        "device_id": "0",
                        "logical_id": "0",
                        "numa_node": 0,
                        "local_cpus": "0-7",
                    },
                    {
                        "device_id": "1",
                        "logical_id": "1",
                        "numa_node": 1,
                        "local_cpus": "8-15",
                    },
                ]
            },
            "workload": {"rank_mapping": [{"rank": "rank0", "pid": 100, "npu_device": "1"}]},
            "processes": [
                {
                    "pid": 100,
                    "rank": "rank0",
                    "npu_device": "1",
                    "cpus_allowed_list": "0-7",
                    "threads": [],
                }
            ],
        }
        findings = diagnose(snapshot)
        ids = {finding["id"] for finding in findings}
        self.assertIn("R003", ids)

    def test_diagnose_binding_range_too_narrow_and_thread_oversubscription(self):
        snapshot = {
            "availability": {"missing": []},
            "system": {"online_cpus": "0-7"},
            "numa_topology": {"nodes": [{"node": 0, "cpus": "0-7"}]},
            "processes": [
                {
                    "pid": 100,
                    "cpus_allowed_list": "0-1",
                    "num_threads": 8,
                    "threads": [],
                }
            ],
            "pytorch": {
                "threading": {"torch_num_threads": 8, "omp_num_threads": 8},
                "dataloader": {"num_workers": 4},
            },
            "cgroup": {
                "process_groups": [
                    {
                        "pid": 100,
                        "cpuset_cpus_effective": "0-1",
                    }
                ]
            },
        }
        findings = diagnose(snapshot)
        ids = {finding["id"] for finding in findings}
        self.assertIn("R005", ids)
        self.assertIn("R006", ids)


if __name__ == "__main__":
    unittest.main()
