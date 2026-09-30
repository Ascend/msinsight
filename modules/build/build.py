#!/usr/bin/env python
# -*- coding: UTF-8 -*-
#
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
#
# build modules

import argparse
import json
import logging
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
MODULES_DIR = os.path.dirname(BUILD_DIR)
PLUGIN_DIR = os.path.join(os.path.dirname(MODULES_DIR), MODULES_DIR, 'framework', 'public', 'plugins')

MODULES_MAP = {
    'cluster': 'Cluster',
    'reinforcement-learning': 'RL',
    'memory': 'Memory',
    'operator': 'Operator',
    'compute': 'Compute',
    'statistic': 'Statistic',
    'leaks': 'Leaks',
    'memory-on-chip': 'MemoryOnChip',
    'timeline': 'Timeline',
    'insight_web_agent': 'InsightWebAgent',
}

BUILD_PROCESS_COUNT = 3


def missing_build_dependencies():
    missing = [] if os.path.isdir(os.path.join(MODULES_DIR, 'node_modules')) else ['node_modules']
    # Packages without their own dependencies legitimately reuse the workspace root.
    for module in ('lib', 'framework', *MODULES_MAP.keys()):
        with open(os.path.join(MODULES_DIR, module, 'package.json'), encoding='utf-8') as stream:
            manifest = json.load(stream)
        if any(manifest.get(key) for key in ('dependencies', 'devDependencies', 'optionalDependencies')):
            relative = os.path.join(module, 'node_modules')
            if not os.path.isdir(os.path.join(MODULES_DIR, relative)):
                missing.append(relative)
    suffix = '.cmd' if platform.system() == 'Windows' else ''
    for tool in ('cross-env', 'craco', 'react-scripts', 'esbuild'):
        relative = os.path.join('node_modules', '.bin', tool + suffix)
        if not os.path.isfile(os.path.join(MODULES_DIR, relative)):
            missing.append(relative)
    return missing


def prepare_dependencies(install_dependencies=True):
    missing = missing_build_dependencies()
    if not install_dependencies and not missing:
        logging.info('Using prepared frontend dependencies; dependency installation skipped.')
        return 0

    pnpm_cmd = 'pnpm.cmd' if platform.system() == 'Windows' else 'pnpm'
    command = [pnpm_cmd, 'install']
    if not install_dependencies:
        logging.warning('Offline frontend dependencies are missing (%s); falling back to dependency installation.',
                        ', '.join(missing))
        command.extend(['--force', '--prod=false', '--frozen-lockfile'])
    result = execute_cmd('modules', MODULES_DIR, command)
    if result != 0:
        logging.error('Failed to install frontend dependencies, %s', result)
        return 1
    missing = missing_build_dependencies()
    if missing:
        logging.error('Frontend dependencies remain incomplete after installation: %s', ', '.join(missing))
        return 1
    return 0


def clean():
    modules = list(MODULES_MAP.keys())
    for module in modules:
        plugin_dir = os.path.join(PLUGIN_DIR, MODULES_MAP.get(module))
        try:
            if os.path.exists(plugin_dir):
                shutil.rmtree(plugin_dir)
        except OSError as e:
            logging.info("Remove %s failed： %s", plugin_dir, e.strerror)


def execute_cmd(module, module_dir, cmd):
    with subprocess.Popen(cmd, cwd=module_dir, stdout=subprocess.PIPE) as proc:
        for line in iter(proc.stdout.readline, b''):
            logging.info('[%s]%s', module, line.decode('utf-8').strip())
        proc.communicate(timeout=600)
        return proc.returncode


def build_module(module):
    """
    构建单个模块，首先先npm install --force安装依赖，然后npm run build进行编译，最终拷贝结果到framework/public/plugins对应目录里

    :param module: 子模块
    :return: 0 表示构建成功， 1表示构建失败
    """
    logging.basicConfig(level=logging.INFO)
    logging.info('[%s]Start to build %s', MODULES_MAP.get(module), module)
    module_dir = os.path.join(MODULES_DIR, module)
    plugin_dir = os.path.join(PLUGIN_DIR, MODULES_MAP.get(module))
    if os.path.exists(plugin_dir):
        shutil.rmtree(plugin_dir)

    pnpm_cmd = 'pnpm.cmd' if platform.system() == 'Windows' else 'pnpm'

    result = execute_cmd(MODULES_MAP.get(module), module_dir, [pnpm_cmd, 'build'])
    if result != 0:
        logging.error('[%s]Failed to build %s, %s', MODULES_MAP.get(module), module, result)
        return 1

    shutil.copytree(os.path.join(module_dir, 'build'), plugin_dir)
    logging.info('[%s]Finish to build %s', MODULES_MAP.get(module), module)
    return result


def parallel_build(install_dependencies=True):
    """
    采用多进程实现并行构建，

    :return: 0 表示构建成功， 1表示构建失败；如果单个模块（进程）构建失败，则返回modules构建失败
    """
    logging.info('Start to build modules')

    if prepare_dependencies(install_dependencies) != 0:
        return 1

    modules = list(MODULES_MAP.keys())
    with multiprocessing.Pool(processes=BUILD_PROCESS_COUNT) as pool:
        results = pool.map(build_module, modules)

    for _, result in enumerate(results):
        if result != 0:
            return 1

    logging.info('Finish to build modules')
    return 0


def main():
    logging.basicConfig(level=logging.INFO)
    if platform.system() != 'Windows':
        multiprocessing.set_start_method('fork')

    parser = argparse.ArgumentParser()
    parser.add_argument('--no-install', action='store_true',
                        help='reuse prepared dependencies; force-install if required frontend dependencies are missing')
    args = parser.parse_args()

    clean()

    return parallel_build(install_dependencies=not args.no_install)


if __name__ == '__main__':
    sys.exit(main())
