#!/bin/bash
# This script is used to generate fuzz test case.
# Copyright (c) 2025 Huawei Technologies Co.,Ltd.
# msinsight is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
set -o errexit

dir_build_fuzz=$(pwd)
## need sudo
# Secodefuzz build only needed on first running
cd ${dir_build_fuzz}/../third_party/secodefuzz
bash build.sh gcc

cd ${dir_build_fuzz}
if [ -d "${dir_build_fuzz}/output" ]; then
  rm -rf ${dir_build_fuzz}/output
fi
mkdir ${dir_build_fuzz}/output
cd ${dir_build_fuzz}/output
echo "*************** FUZZCODE  START *****************"
cmake -D_PROJECT_TYPE=fuzz ${dir_build_fuzz}/..
echo "*************** CMAKE  FINISH "
cpu_number=$(nproc)
make -j${cpu_number} insight_fuzz
echo "*************** MAKE  FINISH "

if [ -d "${dir_build_fuzz}/report" ]; then
  rm -rf ${dir_build_fuzz}/report
fi
mkdir ${dir_build_fuzz}/report
cd ${dir_build_fuzz}/report
cp -r ${dir_build_fuzz}/../src/test/fuzz/test_data ${dir_build_fuzz}/report
echo "*************** RUN insight_fuzz... "
system_arch=$(arch)
${dir_build_fuzz}/../output/linux-${system_arch}/bin/insight_fuzz > insight_fuzz_test_details.log
echo "**************** FUZZCODE  END ******************"
echo "Check more details in report/insight_fuzz_test_details.log"
