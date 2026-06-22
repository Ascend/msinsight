#!/usr/bin/env python
# coding=utf-8
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
# build server
# python build.py build --release
#
# build test
# python build.py test --profile --project_type=test

import argparse
import logging
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys
from logging.handlers import TimedRotatingFileHandler

IS_WINDOWS = platform.system() == 'Windows'
IS_LINUX = platform.system() == 'Linux'
IS_DARWIN = platform.system() == 'Darwin'
FRAMEWORK = 'x86_64' if platform.platform().find('x86_64') > -1 else 'aarch64'
MAKE_JOBS = multiprocessing.cpu_count()

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
CMAKE_BUILD_DIR = os.path.join(BUILD_DIR, 'build')

HOME_DIR = os.path.dirname(BUILD_DIR)
PROJECT_DIR = os.path.dirname(HOME_DIR)
SCRIPTS_DIR = os.path.join(PROJECT_DIR, 'scripts')
THIRD_PARTY_DIR = os.path.join(HOME_DIR, 'third_party')
SRC_DIR = os.path.join(HOME_DIR, 'src')
CLUSTER_ANALYSE = 'cluster_analyse'
PROF_COMMON = 'prof_common'
CLUSTER_ANALYSE_DIR = os.path.join(THIRD_PARTY_DIR, 'msprof-analyze', 'msprof_analyze', CLUSTER_ANALYSE)
PROF_COMMON_DIR = os.path.join(THIRD_PARTY_DIR, 'msprof-analyze', 'msprof_analyze', PROF_COMMON)
PROTO_DIR = os.path.join(SRC_DIR, 'protos')

OUTPUT_DIR = os.path.join(HOME_DIR, 'output')
OUTPUT_OS_DIR = ''
OUTPUT_BIN_DIR = ''
OUTPUT_EXE_DIR = ''

LOG_DIR = os.path.join(BUILD_DIR, 'logs')
LOG_FILE = os.path.join(LOG_DIR, 'build.log')
LOG = logging.getLogger('root')

BUILD_TITLE = '[Server]'
CMAKE_POLICY_VERSION_MINIMUM = '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'


def build_log(build_info):
    LOG.info('%s%s', BUILD_TITLE, build_info)


def execute_cmd(cmd, work_path):
    with subprocess.Popen(cmd, cwd=work_path, stdout=subprocess.PIPE) as proc:
        for line in iter(proc.stdout.readline, b''):
            build_log(line.decode('utf-8').strip())
        try:
            proc.communicate(timeout=600)
        except subprocess.TimeoutExpired:
            LOG.error('Subprocess timed out')
            return 1
        return proc.returncode


def log_output(output):
    """log command output"""
    while True:
        line = output.stdout.readline()
        if not line:
            ret = output.poll()
            if ret and ret != 0:
                LOG.error('build error: %d!\n', ret)
                return False
            break
        build_log(line.decode('UTF-8').rstrip())
    return True


def get_gxx_type():
    if IS_WINDOWS:
        gxx_type = 'win_mingw64'
    elif IS_DARWIN:
        gxx_type = 'darwin'
    else:
        gxx_type = 'linux-' + FRAMEWORK
    return gxx_type


def build():
    build_log('begin build...\n')
    gxx_type = get_gxx_type()
    build_dir = os.path.join(CMAKE_BUILD_DIR, gxx_type)
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    build_cmds = ['cmake', HOME_DIR, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', CMAKE_POLICY_VERSION_MINIMUM]

    result = execute_cmd(build_cmds, build_dir)
    if result != 0:
        build_log('Failed to execute cmake command.')
        return result

    build_cmds = ['cmake', '--build', '.', '-j', str(MAKE_JOBS)]
    result = execute_cmd(build_cmds, build_dir)
    if result != 0:
        build_log('Failed to execute cmake build command.')
        return result
    copy_msprof2server_dir()
    move_snapdump2server_dir()
    if IS_WINDOWS or IS_DARWIN:
        if copy_python_interpreter() != 0:
            build_log('Failed to copy python interpreter.')
            return -1
        if pip_install_third_party_for_cluster_analysis() != 0:
            build_log('Failed to pip install third party for cluster analysis.')
            return -1
    build_log('end build.\n')
    return 0


def copy_msprof2server_dir():
    att_dir = os.path.join(OUTPUT_DIR, get_gxx_type())
    att_bin_dir = os.path.join(att_dir, 'bin')

    script_path = os.path.join(att_bin_dir, 'msprof_analyze')
    if os.path.exists(script_path):
        shutil.rmtree(script_path)
    shutil.copytree(CLUSTER_ANALYSE_DIR, os.path.join(script_path, CLUSTER_ANALYSE), copy_function=shutil.copy2)
    shutil.copytree(PROF_COMMON_DIR, os.path.join(script_path, PROF_COMMON), copy_function=shutil.copy2)


def move_snapdump2server_dir():
    bin_dir = os.path.join(OUTPUT_DIR, get_gxx_type(), 'bin')
    script_target_path = os.path.join(bin_dir, 'mem_snap_dump')
    if os.path.exists(script_target_path):
        shutil.rmtree(script_target_path)
    shutil.move(os.path.join(SCRIPTS_DIR, 'MemSnapDump'), script_target_path)


def copy_python_interpreter():
    python_interpreter_dir = os.getenv("MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER")
    if not python_interpreter_dir:
        build_log('Environment variable MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER does not exist.')
        return -1
    if not os.path.exists(python_interpreter_dir):
        build_log('Environment variable MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER is not a valid path.')
        return -1
    if not os.path.isdir(python_interpreter_dir):
        build_log('Environment variable MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER is not a valid folder.')
        return -1

    gxx_type = get_gxx_type()
    att_dir = os.path.join(OUTPUT_DIR, gxx_type)
    att_bin_dir = os.path.join(att_dir, 'bin')
    att_bin_python_dir = os.path.join(att_bin_dir, 'python')
    try:
        if os.path.exists(att_bin_python_dir):
            shutil.rmtree(att_bin_python_dir)
        # symlinks参数设置为True，macOS上Python解释器目录存在符号链接，如果设置为False会拷贝符号链接指向的对象，浪费磁盘空间
        shutil.copytree(python_interpreter_dir, att_bin_python_dir, symlinks=True)
    except Exception as e:
        build_log(f"Failed to copy Python interpreter to {att_bin_python_dir}. Error: {str(e)}")
        return -1
    build_log(f"Python interpreter was successfully copied to {att_bin_python_dir}.")
    return 0


def pip_install_third_party_for_cluster_analysis():
    gxx_type = get_gxx_type()
    server_output_dir = os.path.join(OUTPUT_DIR, gxx_type)
    server_output_bin_dir = os.path.join(server_output_dir, 'bin')
    server_output_bin_python_dir = os.path.join(server_output_bin_dir, 'python')

    if IS_WINDOWS:
        python_interpreter_path = os.path.join(server_output_bin_python_dir, 'python.exe')
        pip_site_packages_path = os.path.join(server_output_bin_python_dir, 'Lib', 'site-packages')
    elif IS_DARWIN:
        python_interpreter_path = os.path.join(server_output_bin_python_dir, 'bin', 'python3')
        version = '3.12'
        pip_site_packages_path = os.path.join(server_output_bin_python_dir, 'lib', 'python' + version, 'site-packages')
    else:
        return 0

    # 集群分析工具依赖的三方库在Windows系统和macOS系统上需要安装到构建产物里
    # requirements.txt包含了通过import *显式导入的三方库和pandas使用到的三方库
    # 这些三方库各自的依赖也需要安装到构建产物里，所有的直接和间接依赖都写在了requirements.txt中
    # 依赖关系：
    # tqdm->colorama
    # pandas->numpy, python-dateutil, pytz, tzdata
    # python-dateutil->six
    # sqlalchemy->greenlet, typing-extensions
    # Python解释器使用绝对路径，三方库安装到指定目录
    requirements_path = os.path.join(BUILD_DIR, 'requirements.txt')
    pip_install_cmds = [
        python_interpreter_path,
        '-m',
        'pip',
        'install',
        '-r',
        requirements_path,
        '--target',
        pip_site_packages_path,
        '-i',
        'https://mirrors.aliyun.com/pypi/simple/',
        '--only-binary',
        ':all:',
        '--no-cache-dir',
    ]
    result = execute_cmd(pip_install_cmds, None)
    if result != 0:
        build_log('Failed to pip install third party packages for cluster analysis.')
        return result
    build_log('Successfully pip install third party packages for cluster analysis.')
    return 0


def init_log(name):
    """init log config"""
    if not os.path.exists(LOG_DIR):
        os.makedirs(LOG_DIR)
    building_log = logging.getLogger(name)
    building_log.setLevel(logging.DEBUG)
    formatter = logging.Formatter('[%(asctime)s] %(message)s')
    streamhandler = logging.StreamHandler(sys.stdout)
    streamhandler.setLevel(logging.DEBUG)
    streamhandler.setFormatter(formatter)
    building_log.addHandler(streamhandler)
    filehandler = TimedRotatingFileHandler(LOG_FILE, when='W6', interval=1, backupCount=60)
    filehandler.setLevel(logging.DEBUG)
    filehandler.setFormatter(formatter)
    building_log.addHandler(filehandler)
    return building_log


def build_test():
    build_log('begin test build...\n')
    generator = 'MinGW Makefiles' if IS_WINDOWS else 'Ninja'
    build_dir = os.path.join(CMAKE_BUILD_DIR, "test")

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    build_cmds = [
        'cmake',
        HOME_DIR,
        '-G',
        generator,
        '-DCMAKE_BUILD_TYPE=Release',
        '-D_PROJECT_TYPE=test',
        CMAKE_POLICY_VERSION_MINIMUM,
    ]

    result = execute_cmd(build_cmds, build_dir)
    if result != 0:
        build_log('Failed to execute cmake command in test.')
        return result

    build_cmds = ['cmake', '--build', '.', '--target', 'insight_test', '-j', str(MAKE_JOBS)]
    result = execute_cmd(build_cmds, build_dir)
    if result != 0:
        build_log('Failed to execute cmake build command in test.')
        return result

    build_log('end test build.\n')
    return 0


def clean():
    build_dir = os.path.join(CMAKE_BUILD_DIR)
    shutil.rmtree(build_dir)


def main():
    """build entry"""
    parser = argparse.ArgumentParser(description='build dic server project')
    subparsers = parser.add_subparsers(help='sub command help')
    parser_build = subparsers.add_parser('build', help='build dic server')
    parser_build.add_argument(
        '--release', action='store_true', help='compatibility option; server builds are release builds by default'
    )
    parser_build.set_defaults(func=build)

    parser_test = subparsers.add_parser('test', help='test build')
    parser_test.set_defaults(func=build_test)

    parser_clean = subparsers.add_parser('clean', help='make clean')
    parser_clean.set_defaults(func=clean)

    args = parser.parse_args()
    if not hasattr(args, 'func'):
        args = parser.parse_args(['build'] + sys.argv[1:])

    return args.func()


if __name__ == '__main__':
    LOG = init_log('root')
    os.environ['LANG'] = 'C'
    sys.exit(main())
