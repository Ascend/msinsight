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

"""Inline test fixtures for mindstudio-cpu-binding tests."""

from __future__ import annotations

from typing import Any

LSCPU_TEXT = """Architecture:                    aarch64
CPU(s):                          128
On-line CPU(s) list:             0-127
Thread(s) per core:              2
Core(s) per socket:              32
Socket(s):                       2
NUMA node(s):                    2
NUMA node0 CPU(s):               0-31,64-95
NUMA node1 CPU(s):               32-63,96-127
"""

NPU_SMI_TOPO_TEXT = """        NPU0    NPU1    CPU Affinity
NPU0    X       HCCS    0-31,64-95
NPU1    HCCS    X       32-63,96-127
"""

PS_TEXT = """PID PPID COMM COMMAND
1000 1 python python -m vllm.entrypoints.openai.api_server --model qwen --tensor-parallel-size 2
1010 1000 python python -m vllm.worker.worker --local-rank 0
1011 1000 python python -m vllm.worker.worker --local-rank 1
1020 1000 python python tokenizer_worker --pool-size 4
1030 1 python python -m sglang.launch_server --model-path qwen
1031 1030 python python -m sglang.srt.managers.scheduler
2000 1 bash bash
"""

NPU_SMI_INFO_TEXT = """| NPU     Chip                | Process id    | Process name             | Process memory |
| 0       0                   | 1010          | python                   | 1024           |
| 1       0                   | 1011          | python                   | 1024           |
"""

VLLM_PS_TEXT = """PID PPID COMM COMMAND
1000 1 python python -m vllm.entrypoints.openai.api_server --model qwen --tensor-parallel-size 2
1010 1000 python python -m vllm.v1.engine.core --data-parallel-rank 0
1020 1010 python VllmWorker-0 --rank 0 --local-rank 0
1021 1010 python VllmWorker-1 --rank 1 --local-rank 1
1030 1000 python python -m vllm.transformers_utils.tokenizer_group.tokenizer_group
1031 1000 python python -m vllm.v1.engine.input_processor
1032 1000 python python -m vllm.v1.engine.detokenizer
1033 1010 python python -m vllm.v1.engine.tensor_ipc
1040 1 ray::RayWorker ray::RayWorker
"""

VLLM_NPU_SMI_INFO_TEXT = """| NPU     Chip                | Process id    | Process name             | Process memory |
| 0       0                   | 1020          | python                   | 1024           |
| 1       0                   | 1021          | python                   | 1024           |
"""


def multi_rank_snapshot() -> dict[str, Any]:
    return {
        "schema_version": "0.1.1",
        "workload": {
            "process_model": "multi-rank",
            "rank_mapping": [
                {"rank": "0", "pid": 12345, "npu_device": "0"},
                {"rank": "1", "pid": 23456, "npu_device": "1"},
            ],
        },
        "system": {
            "total_logical_cpus": 128,
            "online_cpus": "0-127",
        },
        "numa_topology": {
            "nodes": [
                {
                    "node": 0,
                    "cpus": "0-31,64-95",
                    "logical_cpu_count": 64,
                    "physical_core_count": 32,
                },
                {
                    "node": 1,
                    "cpus": "32-63,96-127",
                    "logical_cpu_count": 64,
                    "physical_core_count": 32,
                },
            ],
            "distance_matrix": [],
        },
        "npu_topology": {
            "vendor": "ascend",
            "devices": [
                {
                    "device_id": "0",
                    "logical_id": "0",
                    "pci_bus_id": "0000:01:00.0",
                    "numa_node": 0,
                    "local_cpus": "0-31",
                    "health": "OK",
                    "links": [],
                },
                {
                    "device_id": "1",
                    "logical_id": "1",
                    "pci_bus_id": "0000:41:00.0",
                    "numa_node": 1,
                    "local_cpus": "32-63",
                    "health": "OK",
                    "links": [],
                },
            ],
        },
        "cpu_topology": {"physical_cores": []},
        "processes": [
            {
                "pid": 12345,
                "ppid": 1,
                "rank": "0",
                "npu_device": "0",
                "comm": "python",
                "command": "python train.py --rank 0",
                "cpus_allowed_list": "0-63",
                "num_threads": 4,
                "threads": [
                    {"tid": 12346, "numa_node": 0},
                    {"tid": 12347, "numa_node": 1},
                ],
            },
            {
                "pid": 23456,
                "ppid": 1,
                "rank": "1",
                "npu_device": "1",
                "comm": "python",
                "command": "python train.py --rank 1",
                "cpus_allowed_list": "0-63",
                "num_threads": 4,
                "threads": [
                    {"tid": 23457, "numa_node": 0},
                    {"tid": 23458, "numa_node": 1},
                ],
            },
        ],
        "cgroup": {"process_groups": []},
        "pytorch": {"threading": {}, "dataloader": {}},
        "runtime_sample": {
            "top_threads": [
                {"pid": 12345, "tid": 12346, "numa_node": 0, "cpu_percent": 50.0},
                {"pid": 12345, "tid": 12347, "numa_node": 1, "cpu_percent": 40.0},
                {"pid": 23456, "tid": 23457, "numa_node": 0, "cpu_percent": 55.0},
                {"pid": 23456, "tid": 23458, "numa_node": 1, "cpu_percent": 45.0},
            ]
        },
        "key_processes": {},
        "availability": {"complete": True, "missing": [], "partial": [], "errors": []},
    }
