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

from pathlib import Path
import json
from jupyter_packaging import wrap_installers, npm_builder, get_data_files
import setuptools

HERE = Path(__file__).parent.resolve()

# The name of the project
PROJECT_NAME = "mindstudio_insight_jupyterlab"

lab_path = HERE / PROJECT_NAME / "labextension"
config_path = HERE / "jupyter-config" / "server-config"

# Representative files that should exist after a successful build
jstargets = [
    str(lab_path / "package.json"),
]

# 指定要复制的文件进行打包
data_files_spec = [
    (f"share/jupyter/labextensions/@MindStudio/{PROJECT_NAME}", str(lab_path), "**/*"),
    (f"share/jupyter/labextensions/@MindStudio/{PROJECT_NAME}", str(HERE), "install.json"),
    ("etc/jupyter/jupyter_server_config.d", str(config_path), "mindstudio_insight_jupyterlab.json"),
]
data_files = get_data_files(data_files_spec)

# frontend build command
builder = npm_builder(HERE, build_cmd="build:prod", npm=["npm"])

# 使用 wrap_installers 代替 create_cmdclass
cmdclass = wrap_installers(pre_dist=builder, ensured_targets=jstargets)

long_description_path = HERE / "README.md"
long_description = long_description_path.read_text() if long_description_path.exists() else ""

# Get the package info from package.json
pkg_json = json.loads((HERE / "package.json").read_bytes())

package_data_spec = {
    PROJECT_NAME: ["**/*"],
}

setup_args = dict(
    name=PROJECT_NAME,
    version=pkg_json["version"],
    description=pkg_json["description"],
    author=pkg_json["author"]["name"],
    author_email=pkg_json["author"]["email"],
    license=pkg_json["license"],
    long_description=long_description,
    long_description_content_type="text/markdown",
    package_data=package_data_spec,
    cmdclass=cmdclass,
    data_files=data_files,
    packages=setuptools.find_packages(),
    install_requires=[
        "jupyterlab>=4.0,<5",
        "tornado<=6.4.2",
    ],
    zip_safe=False,
    include_package_data=True,
    python_requires=">=3.8",
    platforms="Linux, Mac OS X, Windows",
    keywords=["Jupyter", "JupyterLab", "JupyterLab3", "MindStudio", "MindStudio_Insight"],
    entry_points={
        'console_scripts': [
            'mindstudio-insight-jupyterlab = mindstudio_insight_jupyterlab.application:main',
        ],
        'jupyter_serverextension': [
            'mindstudio-insight-jupyterlab = mindstudio_insight_jupyterlab.mindstudio_insight_jupyterlab:'
            'load_jupyter_server_extension',
        ],
    },
    classifiers=[
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        "License :: OSI Approved :: BSD License",
        "Framework :: Jupyter",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python",
    ],
)

if __name__ == "__main__":
    setuptools.setup(**setup_args)
