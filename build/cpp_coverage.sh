#!/bin/bash
# This script is used to generate llt-cpp coverage.
# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
# 整仓报告：${COV_DIR}/result/index.html
# 模块汇总：${COV_DIR}/coverage_summary.md（server/src/modules/<module>）
#           ${COV_DIR}/modules/<module>.info
# 归档与评论：汇总完成后默认调用 cpp_coverage_post.sh
#   报告提交到 msinsight-org/qa-test-assets；汇总回帖到 MR。缺令牌则跳过。

set -e
CUR_DIR=$(dirname $(readlink -f $0))
ARCH=$(uname -m)
TOP_DIR=${CUR_DIR}/..
cd ${TOP_DIR}/server/build
export LD_LIBRARY_PATH=${TOP_DIR}'/server/output/linux-'${ARCH}'/bin':${LD_LIBRARY_PATH}
python3 preprocess_third_party.py
python3 build.py test

COV_DIR=${TOP_DIR}/build_llt/output/cpp_coverage
BUILD_DIR=${TOP_DIR}/server/build

if [ ! -d "${COV_DIR}" ] ; then
    mkdir -p ${COV_DIR}
fi
rm -rf ${COV_DIR}/modules ${COV_DIR}/result

cd ../output/linux-${ARCH}/bin

./insight_test

cd ${BUILD_DIR}
test_dir=$(find ${BUILD_DIR} -name 'insight_test.dir')
echo ${test_dir}
echo "********************** Generate insight_test Coverage Start.************************"
LCOV_RC="--rc lcov_branch_coverage=1 --rc geninfo_no_exception_branch=1"
lcov -c -d ${test_dir} -o ${COV_DIR}/lcov_insight_test.info ${LCOV_RC}
lcov -r ${COV_DIR}/lcov_insight_test.info '*include*' -o ${COV_DIR}/lcov_insight_test.info ${LCOV_RC}
lcov -r ${COV_DIR}/lcov_insight_test.info '*test*' -o ${COV_DIR}/lcov_insight_test.info ${LCOV_RC}
lcov -r ${COV_DIR}/lcov_insight_test.info '*third_party*' -o ${COV_DIR}/lcov_insight_test.info ${LCOV_RC}
echo "********************** Generate insight_test Coverage Stop.*************************"

genhtml ${COV_DIR}/lcov_insight_test.info -o ${COV_DIR}/result --branch-coverage

# lcov --summary 的一行形如 "lines......: 60.0% (3 of 5 lines)"，没有数据时是 "no data found"。
coverage_cells() {
    lcov --summary "$1" ${LCOV_RC} 2>&1 | awk '
        BEGIN { lhit = 0; lfound = 0; lpct = "0.0%"; fhit = 0; ffound = 0; fpct = "0.0%"; bhit = 0; bfound = 0; bpct = "0.0%" }
        function take(hit, found, pct, value) {
            if ($2 == "no") { return "0 0 0.0%" }
            value = $3
            gsub(/[()]/, "", value)
            return value " " $5 " " $2
        }
        /lines\.\.*/ { split(take(), a, " "); lhit = a[1]; lfound = a[2]; lpct = a[3] }
        /functions\.\.*/ { split(take(), a, " "); fhit = a[1]; ffound = a[2]; fpct = a[3] }
        /branches\.\.*/ { split(take(), a, " "); bhit = a[1]; bfound = a[2]; bpct = a[3] }
        END {
            printf "%s/%s | %s | %s/%s | %s | %s/%s | %s", lhit, lfound, lpct, fhit, ffound, fpct, bhit, bfound, bpct
        }
    '
}

INFO=${COV_DIR}/lcov_insight_test.info
SUMMARY=${COV_DIR}/coverage_summary.md
mkdir -p ${COV_DIR}/modules
{
    echo "# insight_test 模块级覆盖率"
    echo ""
    echo "| 模块 | 行覆盖 | 行% | 函数覆盖 | 函数% | 分支覆盖 | 分支% | 报告 |"
    echo "| --- | --- | --- | --- | --- | --- | --- | --- |"
    echo "| （全部产品代码） | $(coverage_cells "${INFO}") | [HTML](result/index.html) |"
} > "${SUMMARY}"

modules_src="${TOP_DIR}/server/src/modules"
if [ -d "${modules_src}" ]; then
    while IFS= read -r module_path; do
        module=$(basename "${module_path}")
        if ! grep -F -q "/modules/${module}/" "${INFO}"; then
            continue
        fi
        out="${COV_DIR}/modules/${module}.info"
        lcov -e "${INFO}" "*/modules/${module}/*" -o "${out}" ${LCOV_RC}
        echo "| ${module} | $(coverage_cells "${out}") | [HTML](result/index.html) |" >> "${SUMMARY}"
    done < <(find "${modules_src}" -mindepth 1 -maxdepth 1 -type d | sort)
fi

echo "report: ${COV_DIR}"
cat "${SUMMARY}"

bash "${CUR_DIR}/cpp_coverage_post.sh"
