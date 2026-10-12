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

from scripts.collectors.runtime_env import ENV_WHITELIST, collect_pytorch_env


class RuntimeEnvTest(unittest.TestCase):
    def test_detects_pytorch_training_command(self):
        result = collect_pytorch_env(
            cmdline="python train.py --local_rank=0",
            environ="",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertTrue(result["pytorch"]["detected"])

    def test_detects_vllm_command(self):
        result = collect_pytorch_env(
            cmdline="python -m vllm.entrypoints.openai.api_server --model qwen",
            environ="",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertTrue(result["pytorch"]["detected"])

    def test_non_python_command_is_not_detected(self):
        result = collect_pytorch_env(
            cmdline="/usr/sbin/sshd -D",
            environ="",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertFalse(result["pytorch"]["detected"])

    def test_parses_distributed_and_threading_environment(self):
        environ = (
            "RANK=1\x00LOCAL_RANK=0\x00WORLD_SIZE=8\x00BACKEND=hccl\x00"
            "OMP_NUM_THREADS=16\x00MKL_NUM_THREADS=8\x00OPENBLAS_NUM_THREADS=4\x00"
            "KMP_AFFINITY=granularity=fine\x00KMP_BLOCKTIME=1\x00"
        )
        result = collect_pytorch_env(
            cmdline="python train.py",
            environ=environ,
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        pytorch = result["pytorch"]
        self.assertTrue(pytorch["distributed"]["enabled"])
        self.assertEqual(pytorch["distributed"]["rank"], 1)
        self.assertEqual(pytorch["distributed"]["local_rank"], 0)
        self.assertEqual(pytorch["distributed"]["world_size"], 8)
        self.assertEqual(pytorch["distributed"]["backend"], "hccl")
        self.assertEqual(pytorch["threading"]["omp_num_threads"], "16")
        self.assertEqual(pytorch["threading"]["mkl_num_threads"], "8")
        self.assertEqual(pytorch["threading"]["openblas_num_threads"], "4")
        self.assertEqual(pytorch["threading"]["kmp_affinity"], "granularity=fine")
        self.assertEqual(pytorch["threading"]["kmp_blocktime"], "1")

    def test_cli_overrides_torch_and_dataloader_fields(self):
        result = collect_pytorch_env(
            cmdline="python train.py",
            environ="",
            torch_num_threads=8,
            torch_num_interop_threads=2,
            dataloader_workers=4,
            dataloader_pin_memory=True,
            dataloader_prefetch_factor=3,
        )
        pytorch = result["pytorch"]
        self.assertEqual(pytorch["threading"]["torch_num_threads"], 8)
        self.assertEqual(pytorch["threading"]["torch_num_interop_threads"], 2)
        self.assertEqual(pytorch["dataloader"]["num_workers"], 4)
        self.assertTrue(pytorch["dataloader"]["pin_memory"])
        self.assertEqual(pytorch["dataloader"]["prefetch_factor"], 3)
        self.assertEqual(pytorch["dataloader"]["source"], "cli")

    def test_detects_npu_backend_from_ascend_env(self):
        result = collect_pytorch_env(
            cmdline="python train.py",
            environ="ASCEND_VISIBLE_DEVICES=0,1\x00",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertTrue(result["pytorch"]["npu_backend"]["detected"])
        self.assertEqual(result["pytorch"]["npu_backend"]["name"], "torch_npu")

    def test_detects_npu_backend_from_cmdline(self):
        result = collect_pytorch_env(
            cmdline="python -c 'import torch_npu'",
            environ="",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertTrue(result["pytorch"]["npu_backend"]["detected"])

    def test_env_only_contains_whitelisted_keys(self):
        self.assertIn("OMP_NUM_THREADS", ENV_WHITELIST)
        result = collect_pytorch_env(
            cmdline="python train.py",
            environ="OMP_NUM_THREADS=16\x00SECRET_TOKEN=abc\x00ASCEND_VISIBLE_DEVICES=0\x00",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertEqual(
            result["pytorch"]["env"],
            {"OMP_NUM_THREADS": "16", "ASCEND_VISIBLE_DEVICES": "0"},
        )

    def test_invalid_integer_env_fields_become_none(self):
        result = collect_pytorch_env(
            cmdline="python train.py",
            environ="RANK=not-int\x00WORLD_SIZE=\x00",
            torch_num_threads=None,
            torch_num_interop_threads=None,
            dataloader_workers=None,
            dataloader_pin_memory=None,
            dataloader_prefetch_factor=None,
        )
        self.assertIsNone(result["pytorch"]["distributed"]["rank"])
        self.assertIsNone(result["pytorch"]["distributed"]["world_size"])


if __name__ == "__main__":
    unittest.main()
