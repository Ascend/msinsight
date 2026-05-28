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

import argparse
import json
import logging
import multiprocessing
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from typing import List

PROJECT_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))


class Const:
    DEFAULT_BUILD_VERSION = '26.0.0'
    WINDOWS_OS = 'Windows'
    MAC_OS = 'Darwin'
    LINUX_OS = 'Linux'
    OUT_DIR = 'out'
    PRODUCT_DIR = 'product'
    FRAMEWORK_DIR = 'framework'
    MODULES_DIR = 'modules'
    SERVER_DIR = 'server'
    SCRIPT_DIR = 'scripts'
    BUILD_DIR = 'build'
    SRC_DIR = 'src'
    MANIFEST_DIR = 'manifest'
    DEPENDENCY_DIR = 'dependency'
    CONFIG_INI = 'config.ini'
    JUPYTERLAB_PLUGINS_DIR = os.path.join('plugins', 'jupyterlab')
    PLATFORM_DIR = os.path.join(PROJECT_PATH, 'platform')
    PLATFORM_PREVIEW_DIR = os.path.join(PLATFORM_DIR, 'preview')
    PLATFORM_TARGET_DIR = os.path.join(PLATFORM_DIR, 'target')
    ASCEND_INSIGHT_PREFIX = 'MindStudio-Insight'
    ASCEND_INSIGHT = 'MindStudio_Insight'
    BIN_SUFFIX = '.exe' if platform.system() == WINDOWS_OS else ''
    PRODUCT_FORMAT = {
        # Windows NSI安装程序
        WINDOWS_OS: '.exe',
        # MacOS下的app dmg安装包
        MAC_OS: '.dmg',
        # Linux下压缩文件
        LINUX_OS: '.zip',
    }
    PACKAGE_SUFFIX = PRODUCT_FORMAT.get(platform.system(), '.zip')
    MAC_OS_APPNAME = 'MindStudioInsight.app'
    PYTHON = 'python' if platform.system() == WINDOWS_OS else 'python3'
    PNPM = 'pnpm.cmd' if platform.system() == WINDOWS_OS else 'pnpm'
    CARGO = 'cargo.exe' if platform.system() == WINDOWS_OS else 'cargo'
    GRADLE = 'gradle.bat' if platform.system() == WINDOWS_OS else 'gradle'
    GRADLEW = 'gradlew.bat' if platform.system() == WINDOWS_OS else 'gradlew'
    JUPYTER = 'jupyter'
    PIP = 'pip' if platform.system() == WINDOWS_OS else 'pip3'
    PLUGINS_VERSION_PLACEHOLDER = '{plugins_version}'
    # 证书设置为“-”代表缺省临时签名，需要使用签名证书时通过环境变量INSIGHT_APP_SIGN指定签名证书名或证书id
    MAC_SIGNATURE_CERTIFICATE_ID = "-"


class BuildContext:
    def __init__(self, build_version: str, whl_version: str, os_tag: str):
        self.build_version = build_version
        self.whl_version = whl_version
        self.os_tag = os_tag


def init():
    clean()
    os.makedirs(os.path.join(PROJECT_PATH, Const.OUT_DIR))
    os.makedirs(os.path.join(PROJECT_PATH, Const.PRODUCT_DIR))


def clean():
    out = os.path.join(PROJECT_PATH, Const.OUT_DIR)
    if os.path.exists(out):
        shutil.rmtree(out)
    script_memsnap_dump_test_dir = os.path.join(PROJECT_PATH, Const.SCRIPT_DIR, 'MemSnapDump', 'test')
    if os.path.exists(script_memsnap_dump_test_dir):
        shutil.rmtree(script_memsnap_dump_test_dir)
    ascend_insight = os.path.join(PROJECT_PATH, Const.PRODUCT_DIR)
    if os.path.exists(ascend_insight):
        shutil.rmtree(ascend_insight)
    framework_dist = os.path.join(PROJECT_PATH, Const.MODULES_DIR, Const.FRAMEWORK_DIR, 'build')
    if os.path.exists(framework_dist):
        shutil.rmtree(framework_dist)
    modules = [
        'cluster',
        'memory',
        'timeline',
        'compute',
        'jupyter',
        'operator',
        'lib',
        'statistic',
        'leaks',
        'reinforcement-learning',
        'memory-on-chip',
    ]
    for module in modules:
        build_dir = os.path.join(PROJECT_PATH, Const.MODULES_DIR, module, Const.BUILD_DIR)
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)


# 后台下载npm和cargo依赖，与server并行执行，以缩短构建时间
def download_dependency_background() -> bool:
    modules_dir = os.path.join(PROJECT_PATH, Const.MODULES_DIR)
    build_modules = [Const.PNPM, 'install', '--force']
    platform_dir = os.path.join(PROJECT_PATH, Const.PLATFORM_DIR)
    build_platform = [Const.CARGO, 'fetch']
    try:
        with subprocess.Popen(build_modules, cwd=modules_dir, stdout=subprocess.PIPE):
            logging.info('[download dependency]Start to download dependency for modules in background.')
        with subprocess.Popen(build_platform, cwd=platform_dir, stdout=subprocess.PIPE):
            logging.info('[download dependency]Start to download dependency for platform in background.')
        return True
    except Exception as e:
        logging.error('[download dependency] Failed to start to download dependency in background. %s.', e)
        return False


def traverse_folder_and_chmod(path, dir_mode, file_mode):
    os.chmod(path, dir_mode)
    for root, dirs, files in os.walk(path):
        for one in files:
            os.chmod(os.path.join(root, one), file_mode)
        os.chmod(root, dir_mode)


def build_server():
    output_path = os.path.join(PROJECT_PATH, Const.SERVER_DIR, 'output')
    if os.path.exists(output_path):
        shutil.rmtree(output_path)
    os.mkdir(output_path)

    build_path = os.path.join(PROJECT_PATH, Const.SERVER_DIR, Const.BUILD_DIR)
    # 开源软件预处理，只要是生成sqlite的源码
    result = exec_command([Const.PYTHON, 'preprocess_third_party.py'], build_path, Const.SERVER_DIR)
    if result != 0:
        return 1
    # 编译代码
    result = exec_command([Const.PYTHON, 'build.py', 'build'], build_path, Const.SERVER_DIR)
    if result != 0:
        return 1
    # 归一化构建产物目录，方便后续其他组件拷贝
    for tmp in os.listdir(output_path):
        tmp_path = os.path.join(output_path, Const.BUILD_DIR, Const.SERVER_DIR)
        os.makedirs(tmp_path)
        bin_path = os.path.join(output_path, tmp, 'bin')
        for file in os.listdir(bin_path):
            if os.path.isdir(os.path.join(bin_path, file)):
                shutil.copytree(os.path.join(bin_path, file), os.path.join(tmp_path, file))
                continue
            if file.endswith('.a'):  # 跳过.a文件
                continue
            shutil.copyfile(os.path.join(bin_path, file), os.path.join(tmp_path, file))
    return 0


def build_frontend():
    os.putenv('npm_config_strict_ssl', 'false')
    os.putenv('npm_config_registry', 'https://registry.npmmirror.com/')
    os.putenv('DISABLE_ESLINT_PLUGIN', 'true')

    module_name = 'frontend'
    module_build_path = os.path.join(PROJECT_PATH, Const.MODULES_DIR, Const.BUILD_DIR)
    result = exec_command([Const.PYTHON, 'build.py'], module_build_path, module_name)
    if result != 0:
        return 1

    framework_path = os.path.join(PROJECT_PATH, Const.MODULES_DIR, Const.FRAMEWORK_DIR)
    result = exec_command([Const.PNPM, 'install'], framework_path, module_name)
    if result != 0:
        return 1

    result = exec_command([Const.PNPM, 'build'], framework_path, module_name)
    if result != 0:
        return 1

    return 0


def set_npm_config():
    os.putenv('npm_config_strict_ssl', 'false')
    os.putenv('npm_config_registry', 'https://registry.npmmirror.com/')


def update_jupyterlab_plugin_version(jupyterlab_version, plugin_path):
    package_json_path = os.path.join(plugin_path, 'package.json')
    if not os.path.exists(package_json_path):
        return 1

    with open(package_json_path, 'r', encoding='utf-8') as file:
        try:
            package_data = json.load(file)
        except json.JSONDecodeError:
            return 1

    package_data['version'] = jupyterlab_version

    with open(package_json_path, 'w', encoding='utf-8') as file:
        try:
            json.dump(package_data, file, indent=2)
        except json.JSONDecodeError:
            return 1
    return 0


def get_os_platform():
    os_system = platform.system().lower()
    os_machine = platform.machine().lower()
    if 'windows' in os_system:
        return 'win_' + os_machine
    return os_system + '_' + os_machine


def get_os_tag():
    os_info = platform.platform()
    if os_info.find(Const.WINDOWS_OS) > -1:
        return 'win'
    framework = 'x86_64' if os_info.find('x86_64') > -1 else 'aarch64'
    if os_info.find('mac') > -1:
        return 'macos_' + framework
    return 'linux_' + framework


def get_package_name(version: str, os_tag: str):
    return f"{Const.ASCEND_INSIGHT_PREFIX}_{version}_{os_tag}{Const.PACKAGE_SUFFIX}"


def build_jupyterlab(jupyterlab_version, os_name):
    # 设置环境变量暂时不构建jupyterlab
    if os.getenv('BUILD_JUPYTERLAB', '').lower() != 'true':
        logging.info('The JupyterLab extension is not compiled because BUILD_JUPYTERLAB is not set.')
        return 0

    plugin_path = os.path.join(PROJECT_PATH, Const.JUPYTERLAB_PLUGINS_DIR)
    requirements_path = os.path.join(plugin_path, 'requirements.txt')

    # 下载构建依赖
    result = exec_command(
        [
            Const.PIP,
            'install',
            '--retries',
            '5',
            '--timeout',
            '120',
            '-i',
            'https://pypi.org/simple',
            '-r',
            requirements_path,
        ],
        plugin_path,
        'jupyterlab_plugin',
    )
    if result != 0:
        return 1

    # 设置npm代理
    set_npm_config()

    # 拷贝前后端资源
    copy_resource_in_jupyterlab(plugin_path)

    # 修改jupyterlab中package.json版本
    result = update_jupyterlab_plugin_version(jupyterlab_version, plugin_path)
    if result != 0:
        return 1

    # 1. 清理jupyterlab环境
    result = exec_command([Const.JUPYTER, 'lab', 'clean'], plugin_path, 'jupyterlab_plugin')
    if result != 0:
        return 1
    # 2. 构建whl包
    setup_path, output_path = 'setup.py', 'output'
    result = exec_command(
        [Const.PYTHON, setup_path, 'bdist_wheel', '--plat-name=' + get_os_platform(), '--dist-dir', output_path],
        plugin_path,
        'jupyterlab_plugin',
    )
    if result != 0:
        return 1

    # 3. 此处暂时需要构建两次
    setup_path, output_path = 'setup.py', 'output'
    result = exec_command(
        [Const.PYTHON, setup_path, 'bdist_wheel', '--plat-name=' + get_os_platform(), '--dist-dir', output_path],
        plugin_path,
        'jupyterlab_plugin',
    )
    if result != 0:
        return 1

    # copy jupyterlab plugin to out directory
    plugin_name = 'mindstudio_insight_jupyterlab-' + jupyterlab_version + '-py3-none-' + get_os_platform() + '.whl'
    dst_file = os.path.join(PROJECT_PATH, Const.OUT_DIR, plugin_name)
    whl_source_path = os.path.join(plugin_path, 'output')
    for file in os.listdir(whl_source_path):
        if file.endswith('.whl'):
            shutil.copy(os.path.join(whl_source_path, file), dst_file)

    return 0


def assemble_profiler_runtime(profiler_path):
    shutil.copytree(
        os.path.join(PROJECT_PATH, Const.MODULES_DIR, Const.FRAMEWORK_DIR, 'build'),
        os.path.join(profiler_path, 'frontend'),
    )
    shutil.copytree(
        os.path.join(PROJECT_PATH, Const.SERVER_DIR, 'output', 'build', 'server'), os.path.join(profiler_path, 'server')
    )
    shutil.copyfile(
        os.path.join(PROJECT_PATH, "build", "plugin_install.py"), os.path.join(profiler_path, "plugin_install.py")
    )
    shutil.copytree(os.path.join(PROJECT_PATH, Const.SCRIPT_DIR), os.path.join(profiler_path, Const.SCRIPT_DIR))


def copy_resource_in_jupyterlab(plugin_path):
    # 拷贝前后端资源
    jupyterlab_path = 'mindstudio_insight_jupyterlab'
    resources_dir = 'resources'
    resources_path = os.path.join(plugin_path, jupyterlab_path, resources_dir)
    if not os.path.exists(resources_path):
        os.makedirs(resources_path, 0o750)
    profiler_path = os.path.join(resources_path, "profiler")
    if not os.path.exists(profiler_path):
        os.makedirs(profiler_path, 0o750)
    assemble_profiler_runtime(profiler_path)


def build_package(version, os_name):
    os.putenv('CARGO_REGISTRY', 'https://mirrors.tuna.tsinghua.edu.cn/git/crates.io-index')
    if os.getenv('BEPHOME') is not None:  # 规避目前cargo不能跑bep问题
        os.putenv('LD_PRELOAD', '')

    preview_dir = Const.PLATFORM_PREVIEW_DIR
    target_dir = Const.PLATFORM_TARGET_DIR
    os.putenv('CARGO_TARGET_DIR', target_dir)

    # 清理构建缓存
    resource_dir = 'resources'
    build_cache_paths = [preview_dir, target_dir]
    for tmp_path in build_cache_paths:
        if os.path.exists(tmp_path):
            traverse_folder_and_chmod(tmp_path, 0o750, 0o750)
            shutil.rmtree(tmp_path)
        os.makedirs(tmp_path, exist_ok=True)
    # 拷贝前后端文件
    shutil.copytree(os.path.join(Const.PLATFORM_DIR, resource_dir), os.path.join(preview_dir, resource_dir))
    profiler_path = os.path.join(preview_dir, resource_dir, 'profiler')
    os.mkdir(profiler_path, 0o750)
    assemble_profiler_runtime(profiler_path)
    # 在macos下使用cargo bundle --release直接构建为app
    if platform.system() == Const.MAC_OS:
        cmd_list = [Const.CARGO, 'bundle', '--release']
        set_mac_app_signature_certificate_id()
    else:
        cmd_list = [Const.CARGO, 'build', '--release']
    package_name = Const.ASCEND_INSIGHT_PREFIX + '_' + version + '_' + os_name + Const.PACKAGE_SUFFIX

    result = exec_command(cmd_list, Const.PLATFORM_DIR, 'bin_package')
    if result != 0:
        return 1
    return zip_package(profiler_path, package_name, preview_dir, target_dir)


file_names = {
    (Const.WINDOWS_OS, 'bin'): 'MindStudioInsight.exe',
    (Const.WINDOWS_OS, 'target'): 'MindStudio-Insight.exe',
    (Const.MAC_OS, 'target'): 'MindStudio Insight',
}


def package_win(dst_file: str, preview_dir: str) -> bool:
    """
        Windows版本打包，输出件为单一的MindStudio-Insight_{版本}_win.exe
    :param dst_file: 目标文件
    :return: 是否成功
    """
    shutil.copytree(os.path.join(Const.PLATFORM_DIR, 'config'), os.path.join(preview_dir, 'config'))  # 仅Windows需要
    bundle_path = os.path.join(Const.PLATFORM_DIR, 'bundle')
    shutil.copyfile(os.path.join(bundle_path, 'installer.nsi'), os.path.join(preview_dir, 'installer.nsi'))
    NSIS_PATH = os.getenv('NSIS_PATH', 'C:\\Program Files (x86)\\NSIS')
    nsis_cmd = os.path.join(NSIS_PATH, 'bin', 'makensis.exe')
    result = exec_command(
        [nsis_cmd, os.path.join('preview', 'installer.nsi')], os.path.dirname(preview_dir), 'bin_package'
    )
    if result != 0:
        return False

    # 将产物拷贝到目标文件
    for tmp in os.listdir(preview_dir):
        if not tmp.startswith(Const.ASCEND_INSIGHT_PREFIX + '_'):
            continue
        shutil.copyfile(os.path.join(preview_dir, tmp), dst_file)
        return True
    # 如果没找到insight.exe产物则返回false
    return False


def package_linux(dst_file: str, preview_dir: str) -> bool:
    """
        Linux版本打包，输出件为MindStudio-Insight_{版本}_linux_{arch}.zip压缩包
    :param dst_file: 目标文件
    :return: 是否成功
    """
    system = platform.system()
    target_file = file_names.get((system, 'target'), 'MindStudio-Insight')
    insight_bin_file = os.path.join(preview_dir, target_file)
    if not os.path.exists(insight_bin_file):
        logging.error(
            '[%s] %s', 'bin_package_linux', f'Linux packaging failed: executable file {insight_bin_file} not found.'
        )
        return False
    os.chmod(insight_bin_file, 0o550)  # 将ascend_insight二进制赋权为550
    shutil.make_archive(dst_file[:-4], 'zip', preview_dir)
    return True


def package_mac(dst_file: str, package_name: str, preview_dir: str, target_dir: str) -> bool:
    system = platform.system()
    bin_file = file_names.get((system, 'bin'), 'MindStudioInsight')
    app_dir = os.path.join(target_dir, 'release', 'bundle', 'osx', Const.MAC_OS_APPNAME)
    app_bin_file_dir = os.path.join(app_dir, 'Contents', 'MacOS')
    preview_app = os.path.join(preview_dir, Const.MAC_OS_APPNAME)
    os.chmod(os.path.join(app_bin_file_dir, bin_file), 0o550)  # 4、app内二进制文件 ascend_insight 550
    shutil.copytree(os.path.join(preview_dir, 'resources'), os.path.join(app_bin_file_dir, 'resources'))
    python_src_dir = os.path.join(app_bin_file_dir, 'resources', 'profiler', 'server', 'python')
    python_dst_dir = os.path.join(app_dir, 'Contents', 'Resources', 'python')
    if os.path.exists(python_dst_dir):
        shutil.rmtree(python_dst_dir)
    shutil.move(python_src_dir, python_dst_dir)
    shutil.move(app_dir, preview_app)
    # 签名app
    if "aarch64" in package_name and not resign_mac_app(preview_app):
        return False
    if not chmod_mac_app(preview_app, 'aarch64' if 'aarch64' in package_name else 'x86_64'):
        return False
    # 通过dmgbuild打包
    if not build_dmg_for_mac_app(dst_file):
        logging.info('[%s] %s', 'bin_package', 'Build dmg for application failed.')
        return False
    # 将dmg文件设置为640
    os.chmod(dst_file, 0o640)
    return True


def build_platform_package(dst_file: str, package_name: str, preview_dir: str, target_dir: str) -> bool:
    system = platform.system()
    if system == Const.WINDOWS_OS:
        return package_win(dst_file, preview_dir)
    if system == Const.MAC_OS:
        return package_mac(dst_file, package_name, preview_dir, target_dir)
    return package_linux(dst_file, preview_dir)


def zip_package(profiler_path, package_name, preview_dir: str, target_dir: str):
    system = platform.system()
    bin_file = file_names.get((system, 'bin'), 'MindStudioInsight')
    target_file = file_names.get((system, 'target'), 'MindStudio-Insight')
    # MacOs通过cargo bundle打包后的产物为app, 不拷贝二进制可执行文件
    if not system == Const.MAC_OS:
        shutil.copyfile(os.path.join(target_dir, 'release', bin_file), os.path.join(preview_dir, target_file))
    # 打包
    dst_file = os.path.join(PROJECT_PATH, Const.OUT_DIR, package_name)
    # 此时目录preview/下的目录结构
    # MindStudio-Insight        rust底座打包二进制
    # resources                 资源目录
    # -- images                 icns等图片资源
    # -- license                许可证信息
    # -- profiler               前后端文件目录
    # ---- frontend             前端文件夹
    # ---- plugin_install.py    插件安装脚本
    # ---- server               后端文件夹
    # ------ libmsinsight       profiler_server库文件
    # ------ libsqlite          profiler_server库文件
    # ------ profiler_server    后端二进制
    # ------ {cluster_analyze}  可能为源码引入或二进制
    server_dir = os.path.join(profiler_path, Const.SERVER_DIR)
    msprof_analyze_dir = os.path.join(server_dir, "msprof_analyze")
    traverse_folder_and_chmod(preview_dir, 0o750, 0o440)
    traverse_folder_and_chmod(server_dir, 0o750, 0o550)
    if not os.path.isdir(msprof_analyze_dir):
        logging.error('msprof_analyze runtime directory not found: %s', msprof_analyze_dir)
        return 1
    traverse_folder_and_chmod(msprof_analyze_dir, 0o750, 0o440)
    return 0 if build_platform_package(dst_file, package_name, preview_dir, target_dir) else 1


def resign_mac_app(preview_app: str):
    resources_dir = os.path.join(preview_app, "Contents/MacOS/resources")
    server_dir = os.path.join(resources_dir, "profiler/server")
    msprof_analyze_dir = os.path.join(server_dir, "msprof_analyze")
    # 签名前设置resources权限, 否则无法签名通过
    traverse_folder_and_chmod(resources_dir, 0o750, 0o640)
    # 重签
    if not sign_mac_app(preview_app, Const.MAC_SIGNATURE_CERTIFICATE_ID):
        return False
    logging.info('[%s] %s', 'bin_package', 'MacOS application resigned successfully, start to build dmg')
    # 签名后重新设置resources最小权限
    traverse_folder_and_chmod(resources_dir, 0o750, 0o440)
    traverse_folder_and_chmod(server_dir, 0o750, 0o550)
    if not os.path.isdir(msprof_analyze_dir):
        logging.error('msprof_analyze runtime directory not found: %s', msprof_analyze_dir)
        return False
    traverse_folder_and_chmod(msprof_analyze_dir, 0o750, 0o440)
    return True


def chmod_mac_app(app_path: str, arch: str) -> bool:
    path_list = get_mac_app_structure(app_path, arch)
    if not path_list:
        logging.warning(
            'Failed to get structure of %s, no further permission modification actions will be performed.', app_path
        )
        return False
    # 非递归地将目录设置为750, 文件设置为440
    for path in path_list:
        try:
            os.chmod(path, 0o750 if os.path.isdir(path) else 0o440)
        except Exception as e:
            logging.error('An exception occurred while performing chmod. Path:%s, Error:%s', path, e)
            return False
    return True


def get_mac_app_structure(app_path: str, arch: str) -> list:
    """
    获取mac app捆绑包特定的目录结构
    :param app_path: app路径 如 /tmp/example.app
    :return: 捆绑包内的文件、目录
    """
    app_structure_paths = [app_path]
    contents_dir = os.path.join(app_path, 'Contents')
    macos_dir = os.path.join(contents_dir, 'MacOS')
    resources_dir = os.path.join(contents_dir, 'Resources')
    info_file = os.path.join(contents_dir, 'Info.plist')
    icon_file = os.path.join(resources_dir, 'MindStudioInsight.icns')
    app_structure_paths.extend([contents_dir, macos_dir, resources_dir, info_file, icon_file])
    if 'aarch64' in arch:
        sign_dir = os.path.join(contents_dir, '_CodeSignature')
        sign_file = os.path.join(sign_dir, 'CodeResources')
        app_structure_paths.extend([sign_dir, sign_file])
    for path in app_structure_paths:
        if not os.path.exists(path):
            logging.warning('%s not found.', path)
            return []
    return app_structure_paths


def sign_mac_app(app_path: str, certificate_id: str = Const.MAC_SIGNATURE_CERTIFICATE_ID) -> bool:
    if not os.path.exists(app_path):
        return False
    logging.info('[%s] Start to sign/resign MacOS application, using certificate %s', 'bin_package', certificate_id)
    sign_cmd_list = ["codesign", "--force", "-s", certificate_id, "--deep", "--timestamp=none", app_path]
    result = exec_command(sign_cmd_list, os.path.dirname(app_path), 'bin_package')
    if result != 0:
        logging.error('[%s] %s', 'bin_package', 'MacOS application signed failed.')
        return False
    return True


def build_dmg_for_mac_app(dst_file) -> bool:
    cmd_list = ["dmgbuild", "-s", "macos_dmg_settings.json", '"MindStudio Installer"', dst_file]
    result = exec_command(cmd_list, os.path.join(PROJECT_PATH, Const.PLATFORM_DIR, 'bundle'), 'bin_package')
    if result != 0:
        return False
    return True


def exec_command(command, path, module_name):
    logging.basicConfig(level=logging.INFO)
    with subprocess.Popen(command, cwd=path, stdout=subprocess.PIPE) as process:
        for line in iter(process.stdout.readline, b''):
            logging.info('[%s]%s', module_name, line.decode('utf-8').strip())
        process.communicate(timeout=600)
    if process.returncode != 0:
        logging.error('[%s]Failed to execute %s.', module_name, ' '.join(command))
    return process.returncode


# 加载版本相关信息，参数为默认的版本数据
def init_version_info(build_version: str):
    build_time = datetime.now(tz=timezone.utc).strftime("%Y/%m/%d")
    # 1. 【前端】 创建（更新）版本信息文件
    update_frontend_version(build_version, build_time)
    # 2. 【后端】 创建（更新）版本信息文件
    # 3. 【产物】设置产物版本信息文件
    update_app_version(build_version)


# 创建、修改版本信息文件，文件目录在framework/src/下，文件名为version_info.json
def update_frontend_version(version, build_time):
    output_path = os.path.join(PROJECT_PATH, Const.MODULES_DIR, Const.FRAMEWORK_DIR, Const.SRC_DIR, 'version_info.json')
    with open(output_path, 'w', encoding='utf-8') as file:
        data = {'version': version, 'modifyTime': build_time}
        file.write(json.dumps(data))


def extract_numeric_part(version: str) -> List:
    parts = version.split('.')
    numeric_part_list = []
    for p in parts:
        if p.isdigit():
            numeric_part_list.append(p)
        else:
            break
    # eg. VERSIONINFO -> FILEVERSION: 8, 2, 0, 0
    while len(numeric_part_list) < 4:
        numeric_part_list.append('0')
    return numeric_part_list


def replace_version_block(content: str, version_info: str, version_info_value: str, version: str) -> str:
    lines = content.splitlines()
    new_lines = []
    current_year = datetime.now().year

    for line in lines:
        stripped = line.strip()

        if stripped.startswith("FILEVERSION"):
            new_lines.append(re.sub(r'^(\s*)FILEVERSION\s+[\d,\s]+$', f'\\1FILEVERSION {version_info}', line))
        elif stripped.startswith("PRODUCTVERSION"):
            new_lines.append(re.sub(r'^(\s*)PRODUCTVERSION\s+[\d,\s]+$', f'\\1PRODUCTVERSION {version_info}', line))
        elif re.match(r'^\s*VALUE\s+"FileVersion"', line):
            new_lines.append(
                re.sub(
                    r'^(\s*)VALUE\s+"FileVersion",\s*".*?"$', f'\\1VALUE "FileVersion", "{version_info_value}"', line
                )
            )
        elif re.match(r'^\s*VALUE\s+"ProductVersion"', line):
            new_lines.append(
                re.sub(r'^(\s*)VALUE\s+"ProductVersion",\s*".*?"$', f'\\1VALUE "ProductVersion", "{version}"', line)
            )
        elif re.match(r'^\s*VALUE\s+"LegalCopyright"', line):
            new_lines.append(re.sub(r'(\d{4})-\d{4}', rf'\1-{current_year}', line))
        else:
            new_lines.append(line)

    return "\n".join(new_lines)


# 更新Windows产物exe版本信息，文件platform\bundle\main.rc
def update_winexe_version_info(version: str) -> None:
    numeric_part_list = extract_numeric_part(version)
    version_info = ', '.join(numeric_part_list[:4])
    version_info_value = '.'.join(numeric_part_list[:4])

    platform_bundle_path = os.path.join(PROJECT_PATH, Const.PLATFORM_DIR, 'bundle', 'main.rc')
    try:
        with open(platform_bundle_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        logging.error('Failed to find main.rc file.')
    except Exception as e:
        logging.error('Failed to read main.rc because %s', e)

    content = replace_version_block(content, version_info, version_info_value, version)

    try:
        with open(platform_bundle_path, 'w', encoding='utf-8') as f:
            f.write(content)
    except Exception as e:
        logging.error('Failed to write main.rc because %s', e)


def build_product_parallel(build_version, whl_version, os_name):
    logging.info('Start to build products')
    funcs = [build_package, build_jupyterlab]
    args_list = [(build_version, os_name), (whl_version, os_name)]

    with multiprocessing.Pool(processes=min(multiprocessing.cpu_count(), len(funcs))) as pool:
        results = []
        for func, args in zip(funcs, args_list):
            results.append(pool.apply_async(func, args))
        pool.close()
        pool.join()

    for func, result in zip(funcs, results):
        if result.get() != 0:
            logging.error('Failed to execute %s, and see the log for the error cause.', func)
            return 1

    logging.info('Finish to build products.')
    return 0


# 替换文件内容，对文件内容中的占位符使用指定内容替换
def replace_placeholders_in_file(file_path, placeholder, replacement):
    with open(file_path, 'r+', encoding='utf-8') as file:
        # 读取文件内容
        content = file.read()
        # 进行占位符替换
        content = content.replace(placeholder, replacement)
        # 将文件指针移到开头以便重写文件内容
        file.seek(0)
        file.write(content)
        file.truncate()  # 清除文件指针当前位置后面的内容


def update_app_version(build_version):
    # 替换main.rc信息
    update_winexe_version_info(build_version)
    # 替换installer.nsi中的版本信息
    installer_nsi_path = os.path.join(PROJECT_PATH, Const.PLATFORM_DIR, 'bundle', 'installer.nsi')
    replace_placeholders_in_file(installer_nsi_path, Const.PLUGINS_VERSION_PLACEHOLDER, build_version)
    # 替换Cargo.toml中的版本信息
    cargo_toml_path = os.path.join(PROJECT_PATH, Const.PLATFORM_DIR, "Cargo.toml")
    replace_placeholders_in_file(cargo_toml_path, Const.PLUGINS_VERSION_PLACEHOLDER, build_version)


def set_mac_app_signature_certificate_id():
    if os.getenv('INSIGHT_APP_SIGN'):
        Const.MAC_SIGNATURE_CERTIFICATE_ID = os.getenv('INSIGHT_APP_SIGN')


def clean_backend_build_cache():
    build_path = os.path.join(PROJECT_PATH, Const.SERVER_DIR, Const.BUILD_DIR)
    result = exec_command([Const.PYTHON, 'build.py', 'clean'], build_path, Const.SERVER_DIR)
    if result < 0:
        logging.error("Clean backend build cache failed")
        return
    output_path = os.path.join(PROJECT_PATH, Const.SERVER_DIR, 'output')
    shutil.rmtree(output_path)


def clean_build_cache():
    clean_backend_build_cache()
    return 0


def parse_args():
    parser = argparse.ArgumentParser(description='Build for Insight')
    parser.add_argument('-r', '--revision', type=str, default=None, help='Specify the revision version')
    parser.add_argument('-b', '--build_version', type=str, default=None, help='Specify the build version')
    parser.add_argument('-w', '--whl_version', type=str, default=None, help='Specify the whl version')
    parser.add_argument('type', nargs='?', default=None, help='Optional type, e.g. clean')
    return parser.parse_known_args()


def is_clean_command(args) -> bool:
    return bool(args.type and args.type.lower() == 'clean')


def build_context_from_args(args) -> BuildContext:
    build_version = args.build_version or Const.DEFAULT_BUILD_VERSION
    whl_version = args.whl_version or Const.DEFAULT_BUILD_VERSION
    return BuildContext(
        build_version=build_version,
        whl_version=whl_version,
        os_tag=get_os_tag(),
    )


def prepare(context: BuildContext):
    init_version_info(context.build_version)
    init()


def run_parallel_functions(named_functions):
    if not named_functions:
        return 0

    with multiprocessing.Pool(processes=min(multiprocessing.cpu_count(), len(named_functions))) as pool:
        results = []
        for name, func in named_functions:
            results.append((name, pool.apply_async(func)))
        pool.close()
        pool.join()

    for name, result in results:
        if result.get() != 0:
            logging.error('Failed to build %s.', name)
            return 1
    return 0


def build_core_artifacts(context: BuildContext):
    return run_parallel_functions(
        [
            ('server', build_server),
            ('frontend', build_frontend),
        ]
    )


def build_optional_artifacts(context: BuildContext):
    return 0


def package_products(context: BuildContext):
    return build_product_parallel(context.build_version, context.whl_version, context.os_tag)


def main():
    logging.basicConfig(level=logging.INFO)
    args, unknown = parse_args()
    if unknown:
        logging.warning('[%s] Ignoring unrecognized argument(s): %s', 'main', ' '.join(unknown))
    if is_clean_command(args):
        return clean_build_cache()

    context = build_context_from_args(args)
    prepare(context)

    result = build_core_artifacts(context)
    if result != 0:
        return result

    result = build_optional_artifacts(context)
    if result != 0:
        return result

    return package_products(context)


if __name__ == "__main__":
    sys.exit(main())
