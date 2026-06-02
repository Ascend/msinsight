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

#ifndef PROFILER_SERVER_PERFORMANCETEST_H
#define PROFILER_SERVER_PERFORMANCETEST_H

#include <string>
#include <gtest/gtest.h>

namespace Performance {
struct CaseExecuteStatus {
    std::string id;
    std::string module;
    std::string caseName;
    std::string result;
    std::string refTime;
    std::string realTime;
};
const std::string SEPARATOR = ",";
const std::string TEST_RESULT_FILE_NAME = "performance_test.csv";

class PerformanceTest : public ::testing::Test {
  public:
    static std::string GetNumber();

    static void WriteTestCaseResult(const CaseExecuteStatus &result, bool clear);

  protected:
    static void SetUpTestCase();

    static void TearDownTestCase();

    void SetUp() override;

    void TearDown() override;

    static int Main(int argc, char **argv);

    static int number;
    static CaseExecuteStatus status;
    static std::string_view test1P2GBRootPath;
    static std::string_view test1P5GBRootPath;
};

}

#endif // PROFILER_SERVER_PERFORMANCETEST_H
