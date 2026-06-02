/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "PerformanceTest.h"
#include <fstream>

namespace Performance {
CaseExecuteStatus PerformanceTest::status = {};
int PerformanceTest::number = 0;
std::string_view PerformanceTest::test1P2GBRootPath;
std::string_view PerformanceTest::test1P5GBRootPath;

void PerformanceTest::SetUpTestCase() {
    status = {"No.", "Module", "CaseName", "Result", "Baseline(ms)", "Real Time(ms)"};
    WriteTestCaseResult(status, true);
    number = 0;
#ifdef _WIN32
    test1P2GBRootPath = R"(D:\data\2G\ubuntu125_7611_20240206073731998_ascend_pt)";
    test1P5GBRootPath = R"(D:\data\5G\ubuntu125_72115_20240206115521705_ascend_pt)";
#endif
}

void PerformanceTest::TearDownTestCase() {}

int PerformanceTest::Main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

void PerformanceTest::WriteTestCaseResult(const CaseExecuteStatus &result, bool clear) {
    std::ofstream outfile;
    outfile.open(TEST_RESULT_FILE_NAME, std::ofstream::out | (clear ? std::ofstream::trunc : std::ofstream::app));

    outfile << result.id << SEPARATOR;
    outfile << result.module << SEPARATOR;
    outfile << result.caseName << SEPARATOR;
    outfile << result.result << SEPARATOR;
    outfile << result.refTime << SEPARATOR;
    outfile << result.realTime << std::endl;
    outfile.close();
}

std::string PerformanceTest::GetNumber() { return std::to_string(++number); }

void PerformanceTest::SetUp() {
    Test::SetUp();
    status.id = GetNumber();
    status.module = "";
    status.caseName = "";
    status.result = "failed";
    status.refTime = "";
    status.realTime = "";
}

void PerformanceTest::TearDown() {
    WriteTestCaseResult(status, false);
    Test::TearDown();
}

}
