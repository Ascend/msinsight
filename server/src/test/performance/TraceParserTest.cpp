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
#include "FileUtil.h"
#include "DataBaseManager.h"
#include "JsonFileParserManager.h"
#include "ParserStatusManager.h"

using namespace Performance;
using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Timeline;
const std::string TIMELINE_MODULE = "Timeline";
const int TIME_REFERENCE_1S = 1000;
const int TIME_REFERENCE_2S = 2000;

class TraceParserTest : PerformanceTest {};

TEST_F(PerformanceTest, testTraceParser1P2GBTime) {
    auto start = std::chrono::high_resolution_clock::now();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, "");
    DataBaseManager::Instance().CreateTraceConnectionPool(
        "0", FileUtil::SplicePath(test1P2GBRootPath, "ASCEND_PROFILER_OUTPUT", "mindstudio_insight_data.db"));
    JsonFileParserManager::GetTraceFileParser().Parse(
        {FileUtil::SplicePath(test1P2GBRootPath, "ASCEND_PROFILER_OUTPUT", "trace_view.json")}, "0", "", "");
    while (true) {
        ParserStatus status = ParserStatusManager::Instance().GetParserStatus("0");
        if (status == ParserStatus::FINISH) {
            break;
        }
        std::chrono::milliseconds duration(50); // 间隔 50 ms
        std::this_thread::sleep_for(duration);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    status.module = TIMELINE_MODULE;
    status.caseName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    status.result = cost <= TIME_REFERENCE_1S ? "pass" : "failed";
    status.refTime = std::to_string(TIME_REFERENCE_1S);
    status.realTime = std::to_string(cost);
    JsonFileParserManager::GetTraceFileParser().Reset();
}

TEST_F(PerformanceTest, testTraceParser1P5GBTime) {
    auto start = std::chrono::high_resolution_clock::now();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, "");
    DataBaseManager::Instance().CreateTraceConnectionPool(
        "0", FileUtil::SplicePath(test1P5GBRootPath, "ASCEND_PROFILER_OUTPUT", "mindstudio_insight_data.db"));
    JsonFileParserManager::GetTraceFileParser().Parse(
        {FileUtil::SplicePath(test1P5GBRootPath, "ASCEND_PROFILER_OUTPUT", "trace_view.json")}, "0", "", "");
    while (true) {
        ParserStatus status = ParserStatusManager::Instance().GetParserStatus("0");
        if (status == ParserStatus::FINISH) {
            break;
        }
        std::chrono::milliseconds duration(50); // 间隔 50 ms
        std::this_thread::sleep_for(duration);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto cost = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    status.module = TIMELINE_MODULE;
    status.caseName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    status.result = cost <= TIME_REFERENCE_2S ? "pass" : "failed";
    status.refTime = std::to_string(TIME_REFERENCE_2S);
    status.realTime = std::to_string(cost);
    JsonFileParserManager::GetTraceFileParser().Reset();
}
