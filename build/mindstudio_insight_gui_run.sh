#!/bin/bash

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

export CI=true
cd server/build
python3 download_third_party.py
python3 preprocess_third_party.py
python3 build.py build
cd ..
ARCH=$(uname -m)
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PWD}/output/linux-${ARCH}/bin
cd ../modules/build
python3 build.py
cd ../../e2e
mkdir log
npm ci
npm run test:smoke
