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
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

ROOT = Path(__file__).resolve().parents[1] / "mindstudio-cpu-binding"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from scripts import cli


class CliTest(unittest.TestCase):
    def test_analyze_reports_invalid_snapshot_json(self):
        with TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            snapshot = tmp_path / "bad.json"
            snapshot.write_text("{bad json", encoding="utf-8")

            code = cli.main(["analyze", "--snapshot", str(snapshot), "--out", str(tmp_path / "out")])

            self.assertEqual(code, 1)

    def test_collect_topology_requires_lscpu_file_with_actionable_message(self):
        with TemporaryDirectory() as tmp:
            code = cli.main(["collect-topology", "--out", str(Path(tmp) / "topology.json")])

            self.assertEqual(code, 1)

    def test_discover_processes_requires_ps_file_with_actionable_message(self):
        with TemporaryDirectory() as tmp:
            code = cli.main(["discover-processes", "--out", str(Path(tmp) / "processes.json")])

            self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main()
