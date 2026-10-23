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

"""Shared helpers for CI build script contract tests."""

import os
import subprocess
from pathlib import Path


SCRIPTS_DIR = Path(__file__).parents[2] / 'ci-build-scripts'
SCRIPT = SCRIPTS_DIR / 'ci-build-common.sh'
# Host or CI values must not leak into default-value assertions.
PIPELINE_ENV_VARS = (
    'WORKSPACE',
    'SOURCE_DIR',
    'BRANCH',
    'BUILD_TYPE',
    'CI_BUILD_ACTION',
    'CI_TOOLS_DIR',
    'BUILD_VERSION',
    'WHL_VERSION',
    'UNIFIED_BUILD_TIMESTAMP',
    'unified_build_timestamp',
    'PIP_INDEX_URL',
    'CI_UPLOAD_CREDENTIALS',
    'REPO_URL',
    'REMOTE_URL',
    'CODE_DIR',
    'TIMESTAMP',
    'ARTIFACT_DIR',
    'AK',
    'SK',
    'APPLY_UWEBSOCKETS_PATCH',
    'CONDA_HOME',
    'CONDA_ENV_NAME',
    'MINDSTUDIO_INSIGHT_PYTHON_INTERPRETER',
    'INSIGHT_APP_SIGN',
    'MACOSX_DEPLOYMENT_TARGET',
    'LOGIN_KEYCHAIN_PASSWORD',
)


def isolated_env(env=None):
    test_env = os.environ.copy()
    for name in PIPELINE_ENV_VARS:
        test_env.pop(name, None)
    if env:
        test_env.update(env)
    return test_env


def run_bash(command: str, *args: str, env=None, script=None) -> subprocess.CompletedProcess:
    return subprocess.run(
        ['bash', '-c', command, 'test', str(script or SCRIPT), *args],
        check=False,
        capture_output=True,
        text=True,
        env=isolated_env(env),
    )
