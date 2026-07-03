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
# 下载三方依赖

import os
import subprocess

from build import init_log, log_output

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
HOME_DIR = os.path.dirname(BUILD_DIR)
THIRD_PARTY_DIR = os.path.join(HOME_DIR, 'third_party')


BUILD_TITLE = '[Download Third Party]'
LOG = init_log('root')

OPEN_SOURCE = [
    ['googletest', 'v1.13.0', 'https://gitcode.com/GitHub_Trending/go/googletest.git'],
    ['mockcpp', 'master', 'https://gitcode.com/Ascend/mockcpp.git'],
    ['libuv', 'v1.44.2', 'https://gitcode.com/gh_mirrors/li/libuv.git'],
    ['uSockets', 'v0.8.6', 'https://gitcode.com/gh_mirrors/us/uSockets.git'],
    ['uWebSockets', 'v20.48.0', 'https://gitcode.com/gh_mirrors/uw/uWebSockets.git'],
    ['rapidjson', 'master', 'https://gitcode.com/GitHub_Trending/ra/rapidjson.git'],
    ['sqlite3_src', 'version-3.46.1', 'https://gitcode.com/gh_mirrors/sq/sqlite.git'],
    ['msprof-analyze', '26.1.0', 'https://gitcode.com/Ascend/msprof-analyze.git'],
]


def log(info):
    LOG.info('%s%s', BUILD_TITLE, info)


def download_3rd_party():
    log('start to download third party')

    for source in OPEN_SOURCE:
        if os.path.exists(os.path.join(THIRD_PARTY_DIR, source[0])):
            continue
        download_cmd = ['git', 'clone', '-b', source[1], source[2], source[0], '--depth=1']
        with subprocess.Popen(download_cmd, cwd=THIRD_PARTY_DIR, stdout=subprocess.PIPE) as output:
            log_output(output)

    log('finish to download third party')


if __name__ == '__main__':
    download_3rd_party()
