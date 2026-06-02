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

#include <gtest/gtest.h>
#include "PerformanceTest.h"
#include "DataBaseManager.h"
#include "KernelParse.h"
#include "ParserStatusManager.h"

using namespace Performance;
using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Summary;
const std::string OPERATOR_MODULE = "Operator";
const int TIME_REFERENCE_200MS = 200;
const int TIME_REFERENCE_500MS = 500;

class KernelParseTest : PerformanceTest {};

TEST_F(PerformanceTest, testKernelParser1P2GBTime) {
    auto start = std::chrono::high_resolution_clock::now();
    KernelParse::Instance().Parse({std::string(test1P2GBRootPath), "", ""});
    while (true) {
        Timeline::ParserStatus status = Timeline::ParserStatusManager::Instance().GetParserStatus(KERNEL_PREFIX + "0");
        if (status == Timeline::ParserStatus::FINISH) {
            break;
        }
        std::chrono::milliseconds duration(10); // 间隔 10 ms
        std::this_thread::sleep_for(duration);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    status.module = OPERATOR_MODULE;
    status.caseName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    status.result = cost <= TIME_REFERENCE_200MS ? "pass" : "failed";
    status.refTime = std::to_string(TIME_REFERENCE_200MS);
    status.realTime = std::to_string(cost);
    KernelParse::Instance().Reset();
}

TEST_F(PerformanceTest, testKernelParser1P5GBTime) {
    auto start = std::chrono::high_resolution_clock::now();
    KernelParse::Instance().Parse({std::string(test1P5GBRootPath), "", ""});
    while (true) {
        Timeline::ParserStatus status = Timeline::ParserStatusManager::Instance().GetParserStatus(KERNEL_PREFIX + "0");
        if (status == Timeline::ParserStatus::FINISH) {
            break;
        }
        std::chrono::milliseconds duration(10); // 间隔 10 ms
        std::this_thread::sleep_for(duration);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    status.module = OPERATOR_MODULE;
    status.caseName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    status.result = cost <= TIME_REFERENCE_500MS ? "pass" : "failed";
    status.refTime = std::to_string(TIME_REFERENCE_500MS);
    status.realTime = std::to_string(cost);
    KernelParse::Instance().Reset();
}
