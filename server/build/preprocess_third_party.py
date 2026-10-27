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

import hashlib
import os
import platform
import shutil
import stat
import sys
import tarfile
import urllib.request
import subprocess
import logging
import tempfile
import time

# Import the sibling build.py; Pylint can resolve a different module named build.
from build import init_log  # pylint: disable=no-name-in-module

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
SQLITE3_SOURCE_SHA256 = '67d3fe6d268e6eaddcae3727fce58fcc8e9c53869bdd07a0c61e38ddf2965071'
SQLITE3_CACHE_FILE = SQLITE3_AUTOCONF_DIR + '.tar.gz'


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


def sqlite_stage(phase, status, started, level=logging.INFO, detail=''):
    LOG.log(
        level,
        '%s sqlite %s status=%s elapsed=%.3fs %s',
        BUILD_TITLE,
        phase,
        status,
        time.perf_counter() - started,
        detail,
    )


def verify_sqlite_archive(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest() == SQLITE3_SOURCE_SHA256


def remove_sqlite_temporary(path):
    if path is None:
        return
    try:
        os.remove(path)
    except FileNotFoundError:
        pass
    except OSError as error:
        # Exception text can contain paths or a credential-bearing override URL.
        LOG.warning('%s sqlite temporary cleanup failed (%s)', BUILD_TITLE, type(error).__name__)


def read_sqlite_cache(cache_dir, workspace_archive):
    started = time.perf_counter()
    if not cache_dir:
        sqlite_stage('cache_read_verify', 'unconfigured', started)
        return False
    try:
        # Verify a private snapshot, so a concurrent replacement cannot change the
        # bytes used for extraction after verification. Close handles before replace.
        shutil.copyfile(os.path.join(cache_dir, SQLITE3_CACHE_FILE), workspace_archive)
        valid = verify_sqlite_archive(workspace_archive)
    except FileNotFoundError:
        sqlite_stage('cache_read_verify', 'missing', started)
        return False
    except OSError as error:
        sqlite_stage('cache_read_verify', 'unreadable', started, logging.WARNING, type(error).__name__)
        return False
    sqlite_stage(
        'cache_read_verify', 'hit' if valid else 'invalid', started, logging.INFO if valid else logging.WARNING
    )
    return valid


def publish_sqlite_cache(verified_archive, cache_dir):
    started = time.perf_counter()
    if not cache_dir:
        sqlite_stage('cache_write', 'skipped', started)
        return
    temporary = None
    try:
        os.makedirs(cache_dir, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix='.sqlite-cache-', suffix='.tmp', dir=cache_dir, delete=False) as handle:
            temporary = handle.name
        shutil.copyfile(verified_archive, temporary)
        os.replace(temporary, os.path.join(cache_dir, SQLITE3_CACHE_FILE))
        sqlite_stage('cache_write', 'success', started)
    except OSError as error:
        sqlite_stage('cache_write', 'failed', started, logging.WARNING, type(error).__name__)
    finally:
        # Never remove another Job's temporary file, including leftovers after a kill.
        remove_sqlite_temporary(temporary)


def prepare_windows_sqlite_archive():
    cache_dir = os.environ.get('MSINSIGHT_SQLITE3_CACHE_DIR', '')
    source_url = os.environ.get('MSINSIGHT_SQLITE3_URL') or SQLITE3_SOURCE_URL
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            prefix='sqlite-download-', suffix='.tar.gz', dir=THIRD_PARTY_DIR, delete=False
        ) as handle:
            temporary = handle.name
        hit = read_sqlite_cache(cache_dir, temporary)
        started = time.perf_counter()
        if hit:
            sqlite_stage('download', 'skipped', started)
            sqlite_stage('download_verify', 'skipped', started, detail='verified_cache_snapshot')
            sqlite_stage('cache_write', 'skipped', started)
        else:
            try:
                urllib.request.urlretrieve(source_url, temporary)
            except Exception as error:
                # Do not log URL or exception message: either may contain credentials.
                sqlite_stage('download', 'failed', started, logging.ERROR, type(error).__name__)
                return False
            sqlite_stage('download', 'success', started)
            started = time.perf_counter()
            try:
                valid = verify_sqlite_archive(temporary)
            except OSError as error:
                sqlite_stage('download_verify', 'failed', started, logging.ERROR, type(error).__name__)
                return False
            if not valid:
                sqlite_stage('download_verify', 'invalid', started, logging.ERROR)
                return False
            sqlite_stage('download_verify', 'valid', started)
            publish_sqlite_cache(temporary, cache_dir)
        started = time.perf_counter()
        try:
            with tarfile.open(temporary) as tar:
                tar.extractall(THIRD_PARTY_DIR, filter="data")
        except (OSError, tarfile.TarError) as error:
            sqlite_stage('extract', 'failed', started, logging.ERROR, type(error).__name__)
            return False
        sqlite_stage('extract', 'success', started)
        return True
    except OSError as error:
        LOG.error('%s sqlite source preparation failed (%s)', BUILD_TITLE, type(error).__name__)
        return False
    finally:
        remove_sqlite_temporary(temporary)


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
            return prepare_windows_sqlite_archive()
        log('sqlite source directory already exists; archive verification skipped')
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
