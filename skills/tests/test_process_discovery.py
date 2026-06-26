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
# pylint: disable=no-name-in-module
# pylint: disable=duplicate-code
import json
import sys
import tempfile
import unittest
from pathlib import Path

SKILLS_ROOT = Path(__file__).resolve().parents[1]
ROOT = SKILLS_ROOT / "mindstudio-cpu-binding"
sys.path.insert(0, str(SKILLS_ROOT))
sys.path.insert(0, str(ROOT))

from scripts.cli import main as cli_main
from scripts.process_discovery import discover_processes_from_text, main
from tests.fixtures import (
    NPU_SMI_INFO_TEXT,
    PS_TEXT,
    VLLM_NPU_SMI_INFO_TEXT,
    VLLM_PS_TEXT,
)


class ProcessDiscoveryTest(unittest.TestCase):
    def test_discovers_serving_roles_and_npu_mapping_from_sample_text(self):
        result = discover_processes_from_text(PS_TEXT, NPU_SMI_INFO_TEXT)

        self.assertEqual(result["schema_version"], "0.1.0")
        self.assertEqual(result["summary"]["candidate_process_count"], 6)
        self.assertEqual(result["summary"]["npu_process_count"], 2)

        by_pid = {process["pid"]: process for process in result["processes"]}
        self.assertEqual(by_pid[1000]["role_hint"], "vllm_api_server")
        self.assertEqual(by_pid[1010]["role_hint"], "vllm_tp_worker")
        self.assertEqual(by_pid[1010]["npu_device"], "0")
        self.assertEqual(by_pid[1011]["npu_device"], "1")
        self.assertEqual(by_pid[1020]["role_hint"], "tokenizer")
        self.assertEqual(by_pid[1031]["role_hint"], "sglang_scheduler")
        self.assertIn(1010, by_pid[1000]["children"])

    def test_discovers_vllm_runtime_roles_from_process_text(self):
        result = discover_processes_from_text(VLLM_PS_TEXT, VLLM_NPU_SMI_INFO_TEXT)

        self.assertEqual(result["summary"]["candidate_process_count"], 9)
        self.assertEqual(result["summary"]["npu_process_count"], 2)

        by_pid = {process["pid"]: process for process in result["processes"]}
        self.assertEqual(by_pid[1000]["role_hint"], "vllm_api_server")
        self.assertEqual(by_pid[1010]["role_hint"], "vllm_engine_core")
        self.assertEqual(by_pid[1020]["role_hint"], "vllm_tp_worker")
        self.assertEqual(by_pid[1021]["role_hint"], "vllm_tp_worker")
        self.assertEqual(by_pid[1030]["role_hint"], "vllm_tokenizer")
        self.assertEqual(by_pid[1031]["role_hint"], "vllm_input_processor")
        self.assertEqual(by_pid[1032]["role_hint"], "vllm_detokenizer")
        self.assertEqual(by_pid[1033]["role_hint"], "vllm_ipc")
        self.assertEqual(by_pid[1040]["role_hint"], "ray_worker")
        self.assertEqual(by_pid[1020]["npu_device"], "0")
        self.assertEqual(by_pid[1021]["npu_device"], "1")
        self.assertIn(1010, by_pid[1000]["children"])
        self.assertIn(1020, by_pid[1010]["children"])
        self.assertIn(1021, by_pid[1010]["children"])

    def test_vllm_specific_roles_require_vllm_or_known_vllm_process_names(self):
        ps_text = (
            "PID PPID COMM COMMAND\n"
            "100 1 python python -m custom.engine_core\n"
            "101 1 python python -m custom.input_processor\n"
            "102 1 python python -m custom.detokenizer\n"
            "103 1 python python -m custom.tensor_ipc\n"
        )
        result = discover_processes_from_text(ps_text, "", keyword="custom")

        by_pid = {process["pid"]: process for process in result["processes"]}
        self.assertEqual(by_pid[100]["role_hint"], "engine_worker")
        self.assertEqual(by_pid[101]["role_hint"], "npu_process")
        self.assertEqual(by_pid[102]["role_hint"], "tokenizer")
        self.assertEqual(by_pid[103]["role_hint"], "npu_process")

    def test_process_discovery_cli_writes_json(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            ps_file = Path(tmpdir) / "ps.txt"
            npu_smi_info_file = Path(tmpdir) / "npu-smi-info.txt"
            output = Path(tmpdir) / "processes.json"
            ps_file.write_text(PS_TEXT, encoding="utf-8")
            npu_smi_info_file.write_text(NPU_SMI_INFO_TEXT, encoding="utf-8")
            exit_code = main(
                [
                    "--ps-file",
                    str(ps_file),
                    "--npu-smi-info-file",
                    str(npu_smi_info_file),
                    "--out",
                    str(output),
                ]
            )
            data = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 0)
        self.assertEqual(data["processes"][0]["pid"], 1000)

    def test_kernel_threads_are_excluded(self):
        ps_text = (
            "PID PPID COMM COMMAND\n"
            "2 0 kthreadd [kthreadd]\n"
            "8 2 kworker/0:0H-kb [kworker/0:0H-kb]\n"
            "25 2 kworker/1:0H-ev [kworker/1:0H-ev]\n"
            "100 1 python python -m vllm.worker.worker --local-rank 0\n"
        )
        result = discover_processes_from_text(ps_text, "")

        pids = {process["pid"] for process in result["processes"]}
        self.assertNotIn(2, pids)
        self.assertNotIn(8, pids)
        self.assertNotIn(25, pids)
        self.assertIn(100, pids)

    def test_npu_smi_parser_ignores_device_rows_without_process_pid(self):
        ps_text = "PID PPID COMM COMMAND\n100 1 python python -m vllm.entrypoints.openai.api_server\n"
        npu_smi_text = "| 0                         | 0000:C1:00.0  | 0           |\n"

        result = discover_processes_from_text(ps_text, npu_smi_text)

        self.assertEqual(result["summary"]["npu_process_count"], 0)

    def test_redacts_sensitive_command_arguments(self):
        ps_text = (
            "PID PPID COMM COMMAND\n"
            "100 1 python python -m vllm.entrypoints.openai.api_server --api-key sk-secret --connection-token=abc --model qwen\n"
        )
        result = discover_processes_from_text(ps_text, "")

        self.assertEqual(
            result["processes"][0]["command"],
            "python -m vllm.entrypoints.openai.api_server --api-key REDACTED --connection-token=REDACTED --model qwen",
        )

    def test_shared_cli_discover_processes_command_writes_json(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            ps_file = Path(tmpdir) / "ps.txt"
            npu_smi_info_file = Path(tmpdir) / "npu-smi-info.txt"
            output = Path(tmpdir) / "processes.json"
            ps_file.write_text(PS_TEXT, encoding="utf-8")
            npu_smi_info_file.write_text(NPU_SMI_INFO_TEXT, encoding="utf-8")
            exit_code = cli_main(
                [
                    "discover-processes",
                    "--ps-file",
                    str(ps_file),
                    "--npu-smi-info-file",
                    str(npu_smi_info_file),
                    "--out",
                    str(output),
                ]
            )
            data = json.loads(output.read_text(encoding="utf-8"))

        self.assertEqual(exit_code, 0)
        self.assertEqual(data["summary"]["candidate_process_count"], 6)


if __name__ == "__main__":
    unittest.main()
