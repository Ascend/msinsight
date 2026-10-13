# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
# Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
"""Run the bundled cluster/MFU source, not the independently installed CLI wheel.

The server copies this launcher beside its owned regular msprof_analyze package.
It preserves the third-party source and forwards the existing script arguments.
"""
from pathlib import Path
import runpy
import sys


def main():
    root = Path(__file__).resolve().parent
    package = root / 'msprof_analyze'
    package_root = package.resolve()
    package_init = package / '__init__.py'
    if (not package_root.is_relative_to(root) or not package_init.is_file()
            or not package_init.resolve().is_relative_to(package_root)):
        raise RuntimeError('Owned msprof package root is missing or escapes the bundled directory')

    entry = package / 'cluster_analyse' / 'cluster_analysis.py'
    if not entry.is_file() or not entry.resolve().is_relative_to(package_root):
        raise RuntimeError('Owned msprof entry is missing or escapes the bundled package')

    # -I intentionally retains site-packages: a regular package plus first position
    # is required to select the source tree instead of an installed CLI distribution.
    sys.path.insert(0, str(root))
    runpy.run_module('msprof_analyze.cluster_analyse.cluster_analysis', run_name='__main__', alter_sys=True)


if __name__ == '__main__':
    main()
