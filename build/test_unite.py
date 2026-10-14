#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2025 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# Export unified frontend coverage to the existing CI artifact formats.
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def export_reports(summary, root=ROOT):
    """Reject partial/failed runs before publishing the legacy aggregate."""
    if not summary.get("fullWorkspace") or not summary.get("testsPassed"):
        raise ValueError("Coverage requires a successful full workspace run: npm run test:ci")
    if not summary.get("modules"):
        raise ValueError("No module coverage found")

    report_dir = root / "llt_cov"
    report_dir.mkdir(exist_ok=True)
    fields = ["module", "total_line", "line_cov", "total_method", "method_cov", "total_branch", "branch_cov"]
    with (report_dir / "total_cov.csv").open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        for module, metrics in summary["modules"].items():
            row = {"module": f"modules/{module}"}
            for metric, name in [("lines", "line"), ("functions", "method"), ("branches", "branch")]:
                counts = metrics[metric]
                row[f"total_{name}"] = counts["total"]
                row[f"{name}_cov"] = counts["covered"] / counts["total"] if counts["total"] else 1
            writer.writerow(row)

    junit_dir = root / "junit"
    junit_dir.mkdir(exist_ok=True)
    payload = {
        name: {"percentage": str(summary["total"][metric]["pct"])}
        for metric, name in [("lines", "lineCoverage"), ("functions", "methodCoverage"), ("branches", "branchCoverage")]
    }
    (junit_dir / "junit.json").write_text(json.dumps(payload, indent=4), encoding="utf-8")


def main():
    summary_path = ROOT / "modules" / "coverage" / "module-summary.json"
    with summary_path.open(encoding="utf-8") as source:
        export_reports(json.load(source))


if __name__ == "__main__":
    main()
