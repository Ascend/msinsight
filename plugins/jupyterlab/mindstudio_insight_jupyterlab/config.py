#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

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

import os
from dataclasses import dataclass
from typing import Optional

from traitlets import TraitError, Unicode, default
from traitlets.config import LoggingConfigurable


# Linux PATH_MAX includes the terminating NUL; keep one byte available for it.
LINUX_PATH_MAX = 4096


def _validate_directory_path(path):
    if len(os.fsencode(path)) >= LINUX_PATH_MAX:
        raise ValueError("Directory path is too long")
    if any(ord(char) < 32 or ord(char) == 127 for char in path):
        raise ValueError("Control characters are not allowed")


class MSInsight(LoggingConfigurable):
    initial_dir = Unicode(
        "", help="Initial directory for the Insight file selector. This does not preload or import a project."
    ).tag(config=True)

    @default("initial_dir")
    def _default_initial_dir(self):
        return os.environ.get("MINDSTUDIO_INSIGHT_INITIAL_DIR", "")


@dataclass(frozen=True)
class InsightFileSelectorConfig:
    default_browse_dir: str = ""
    error: Optional[str] = None


def load_insight_file_selector_config(server_app):
    """Save the Insight file selector configuration without inspecting or enumerating the directory."""
    try:
        value = MSInsight(parent=server_app).initial_dir
        if not value:
            return InsightFileSelectorConfig()
        _validate_directory_path(value)
        path = os.path.expanduser(value)
        if path.startswith("~"):
            raise ValueError("Cannot expand the home directory")
        if not os.path.isabs(path):
            path = os.path.join(server_app.root_dir, path)
        path = os.path.abspath(path)
        _validate_directory_path(path)
        return InsightFileSelectorConfig(path)
    except (TraitError, ValueError, OSError):
        return InsightFileSelectorConfig(
            error=(
                "Invalid Insight file selector configuration: "
                f"MSInsight.initial_dir must resolve to a valid path shorter than {LINUX_PATH_MAX} "
                "filesystem-encoded bytes without control characters."
            )
        )
