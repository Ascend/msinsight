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
# sqlite编译和三方依赖整理

import os
import platform
import shutil
import stat
import sys
import tarfile
import urllib.request
import subprocess
import logging

from build import init_log

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
HOME_DIR = os.path.dirname(BUILD_DIR)
THIRD_PARTY_DIR = os.path.join(HOME_DIR, 'third_party')

SQLITE_DIR = 'sqlite'
SQLITE3_SRC_DIR = 'sqlite3_src'
SQLITE3_AUTOCONF_DIR = 'sqlite-autoconf-3460100'
SQLITE_SRC_TAR = 'sqlite_src.tar.gz'

BUILD_TITLE = '[Pre Process Third Party]'
LOG = logging.getLogger('root')

CHECK_FILE_LIST = [
    [
        os.path.join(SQLITE_DIR, 'include', 'sqlite3.h'),
        os.path.join(SQLITE3_AUTOCONF_DIR, 'sqlite3.h'),
        os.path.join(SQLITE3_SRC_DIR, 'build', 'sqlite3.h'),
    ],
    [
        os.path.join(SQLITE_DIR, 'include', 'sqlite3ext.h'),
        os.path.join(SQLITE3_AUTOCONF_DIR, 'sqlite3ext.h'),
        os.path.join(SQLITE3_SRC_DIR, 'build', 'sqlite3ext.h'),
    ],
    [
        os.path.join(SQLITE_DIR, 'src', 'shell.c'),
        os.path.join(SQLITE3_AUTOCONF_DIR, 'shell.c'),
        os.path.join(SQLITE3_SRC_DIR, 'build', 'shell.c'),
    ],
    [
        os.path.join(SQLITE_DIR, 'src', 'sqlite3.c'),
        os.path.join(SQLITE3_AUTOCONF_DIR, 'sqlite3.c'),
        os.path.join(SQLITE3_SRC_DIR, 'build', 'sqlite3.c'),
    ],
]

SQLITE3_SOURCE_URL = 'https://www.sqlite.org/2024/sqlite-autoconf-3460100.tar.gz'


def log(info):
    LOG.info('%s%s', BUILD_TITLE, info)


def exec_command(command, path):
    with subprocess.Popen(command, cwd=path, stdout=subprocess.PIPE) as process:
        for line in iter(process.stdout.readline, b''):
            log(line.decode('utf-8').strip())
        process.communicate(timeout=600)
        if process.returncode != 0:
            log('Failed to execute ' + ' '.join(command))
        return process.returncode


def is_mingw_available():
    """
    检查系统是否安装了 MinGW（优先通过 g++ -dumpmachine 判断目标平台）
    返回: (bool, str) -> (是否可用, 详细信息)
    """
    # Step 1: 检查 g++ 是否在 PATH 中
    gpp_path = shutil.which("g++")
    if not gpp_path:
        return False, "g++ not found in PATH"

    try:
        # Step 2: 优先通过 g++ -dumpmachine 获取目标平台三元组
        dumpmachine_result = subprocess.run(
            [gpp_path, "-dumpmachine"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5,
            check=False,
        )
        machine_output = (dumpmachine_result.stdout + dumpmachine_result.stderr).strip().lower()
        if dumpmachine_result.returncode == 0 and "mingw" in machine_output:
            return True, f"MinGW detected via g++ target {machine_output} at {gpp_path}"

        result = subprocess.run(
            [gpp_path, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5, check=False
        )
        if result.returncode != 0:
            return False, f"g++ failed: {result.stderr.strip()}"

        version_output = (result.stdout + result.stderr).strip().lower()

        # Step 3: 回退到 g++ --version 检查输出是否包含 mingw 关键字
        if "mingw" in version_output:
            return True, f"MinGW detected via g++ at {gpp_path}"
        else:
            return False, (
                f"g++ found but not MinGW (target: {machine_output[:100]}, version: {version_output[:100]}...)"
            )

    except Exception as e:
        return False, f"Error checking g++: {e}"


def prepare_sqlite_src() -> bool:
    log('start to prepare sqlite src')
    if platform.system() == "Windows":
        check_mingw_result, msg = is_mingw_available()
        log(msg)
        if not check_mingw_result:
            LOG.error(
                'MinGW environment check failed. Please ensure that MinGW is properly installed '
                'and the environment variables are correctly configured.'
            )
            return False
        if not os.path.exists(os.path.join(THIRD_PARTY_DIR, SQLITE3_AUTOCONF_DIR)):
            tar_path = os.path.join(THIRD_PARTY_DIR, SQLITE_SRC_TAR)
            urllib.request.urlretrieve(SQLITE3_SOURCE_URL, tar_path)
            with tarfile.open(tar_path) as tar:
                tar.extractall(THIRD_PARTY_DIR)
            os.remove(tar_path)
            return True
    else:
        build_path = os.path.join(THIRD_PARTY_DIR, SQLITE3_SRC_DIR, 'build')
        if os.path.exists(build_path):
            shutil.rmtree(build_path)
        os.mkdir(build_path)
        config_cmd = os.path.join(THIRD_PARTY_DIR, SQLITE3_SRC_DIR, 'configure')
        os.chmod(config_cmd, stat.S_IXUSR | stat.S_IRUSR)
        os.chdir(build_path)
        ret = exec_command(["../configure"], build_path)
        if ret != 0:
            LOG.error('Failed to configure parameter for sqlite')
            return False
        ret = exec_command(["make", "sqlite3.c"], build_path)
        if ret != 0:
            LOG.error('Failed to make sqlite3.c')
            return False

    log('finish to prepare sqlite3 src')
    return True


def reorganize_3rd_party():
    log('start to reorganize third party')
    for file in CHECK_FILE_LIST:
        dst_file = os.path.join(THIRD_PARTY_DIR, file[0])
        if not os.path.exists(dst_file):
            src_file = os.path.join(THIRD_PARTY_DIR, file[1] if platform.system() == "Windows" else file[2])
            if os.path.exists(src_file):
                shutil.copyfile(src_file, dst_file)
            else:
                LOG.error('%s The file is not exist: %s', BUILD_TITLE, src_file)

    log('finish to reorganize third party')


if __name__ == '__main__':
    LOG = init_log('root')
    if not prepare_sqlite_src():
        LOG.error('Failed to prepare sqlite src.')
        sys.exit(-1)
    reorganize_3rd_party()
