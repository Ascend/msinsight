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
#include <filesystem>
#include "pch.h"
#include "SummaryDef.h"
#include "ThreadPool.h"
#include "TextSummaryDataBase.h"
#include "FileParser.h"
#include "ConstantDefs.h"
#define private public
#include "KernelParse.h"
#undef private
#include "DataBaseManager.h"
#include "TestSuit.h"
#include "FileUtil.h"
#include "GlobalProtocolResponse.h"
using namespace Dic::Module::Summary;
using namespace Dic::Module::Timeline;

static std::vector<std::string> tmpFiles = {
    FileUtil::SplicePath(TestSuit::GetSrcTestPath(), "msprof", "normal", "op_summary_20230919172304.csv"),
    FileUtil::SplicePath(
        TestSuit::GetSrcTestPath(), "msprof", "normal", "mindstudio_profiler_output", "op_summary_20230919172304.csv"),
    FileUtil::SplicePath(TestSuit::GetSrcTestPath(), "msprof", "slice", "op_summary_slice_3_20230919172304.csv"),
    FileUtil::SplicePath(TestSuit::GetSrcTestPath(), "msprof", "slice", "mindstudio_profiler_output",
        "memory_record_slic_2_20230919172305.csv")};
class KernelParseTest : public KernelParse, public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::ofstream outfile;
        for (const auto &item : tmpFiles) {
            outfile.open(Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), item), std::ios::out);
            outfile.close();
        }
    }
    static void TearDownTestSuite() {
        for (const auto &item : tmpFiles) {
            std::remove(Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), item).c_str());
        }
    }

  protected:
    void SetUp() override { testDataPath = TestSuit::GetTestDataFile(); }

    void TearDown() override { DataBaseManager::Instance().Clear(DatabaseType::SUMMARY); }

    static void CheckKernelData(Kernel &a, const Kernel &b) {
        EXPECT_EQ(a.rankId, b.rankId);
        EXPECT_EQ(a.stepId, b.stepId);
        EXPECT_EQ(a.name, b.name);
        EXPECT_EQ(a.type, b.type);
        EXPECT_EQ(a.state, b.state);
        EXPECT_EQ(a.acceleratorCore, b.acceleratorCore);
        EXPECT_EQ(a.startTime, b.startTime);
        EXPECT_EQ(a.duration, b.duration);
        EXPECT_EQ(a.waitTime, b.waitTime);
        EXPECT_EQ(a.blockNum, b.blockNum);
        EXPECT_EQ(a.inputShapes, b.inputShapes);
        EXPECT_EQ(a.inputDataTypes, b.inputDataTypes);
        EXPECT_EQ(a.inputFormats, b.inputFormats);
        EXPECT_EQ(a.outputShapes, b.outputShapes);
        EXPECT_EQ(a.outputDataTypes, b.outputDataTypes);
        EXPECT_EQ(a.outputFormats, b.outputFormats);
        if (a.utilizationInfo.empty() && b.utilizationInfo.empty()) {
            return;
        }
        EXPECT_EQ(a.utilizationInfo.size(), b.utilizationInfo.size());
        for (size_t i = 0; i < a.utilizationInfo.size(); ++i) {
            EXPECT_EQ(a.utilizationInfo.at(i), b.utilizationInfo.at(i));
        }
    }

    static std::map<std::string, size_t> GenerateDataMap(const std::vector<std::string> &rowVector) {
        std::map<std::string, size_t> dataMap;
        for (size_t i = 0; i < rowVector.size(); i++) {
            dataMap.emplace(rowVector.at(i), i);
        }
        return dataMap;
    }

    std::string testDataPath;
};

TEST_F(KernelParseTest, GetPyTorchKernelFilesSuccess) {
    std::vector<std::string> paths = {Dic::FileUtil::SplicePath(testDataPath, "test_rank_0")};
    auto result = KernelParse::GetKernelFiles(paths);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0), Dic::FileUtil::SplicePath(paths[0], "ASCEND_PROFILER_OUTPUT", "kernel_details.csv"));
}

class KernelParseConcurrencyEnvironment : public ::testing::Environment {
  public:
    void TearDown() override {
        const auto *tests = ::testing::UnitTest::GetInstance();
        for (int i = 0; i < tests->total_test_suite_count(); ++i) {
            const auto *suite = tests->GetTestSuite(i);
            if (suite->should_run() && std::string(suite->name()) == "KernelParseConcurrencyTest") {
                KernelParse::Instance().threadPool->ShutDown();
                return;
            }
        }
    }
};

[[maybe_unused]] const auto kernelParseConcurrencyEnvironment =
    ::testing::AddGlobalTestEnvironment(new KernelParseConcurrencyEnvironment);

TEST(KernelParseConcurrencyTest, ConcurrentDeviceRanksReuseOneParsedDatabase) {
    const auto folder = std::filesystem::path(::testing::TempDir()) / "kernel_shared_device_parse";
    std::filesystem::create_directories(folder);
    const auto csvPath = folder / "kernel_details.csv";
    const auto dbPath = folder / "mindstudio_insight_data.db";
    for (const std::string suffix : {"", "-wal", "-shm"}) {
        std::filesystem::remove(dbPath.string() + suffix);
    }
    {
        std::ofstream csv(csvPath);
        csv << "Device_id,Name,Type,Accelerator Core,Start Time(us),Duration(us),Wait Time(us),Block Dim\n";
        for (int device = 0; device < 4; ++device) {
            csv << device << ",Add,Add,AI_CORE,100,1,0,1\n";
        }
    }
    auto &manager = DataBaseManager::Instance();
    manager.SetDataType(DataType::TEXT, dbPath.string());
    auto &parser = KernelParse::Instance();
    std::mutex callbacksMutex;
    std::vector<std::string> completed;
    std::function<void(const std::string, const std::string, bool, const std::string)> callback =
        [&](const std::string rank, const std::string file, bool result, const std::string) {
            std::lock_guard lock(callbacksMutex);
            EXPECT_TRUE(result);
            EXPECT_EQ(file, dbPath.string());
            completed.push_back(rank);
        };
    parser.SetParseEndCallBack(callback);
    for (int iteration = 0; iteration < 2; ++iteration) {
        completed.clear();
        std::promise<void> start;
        auto ready = start.get_future().share();
        std::vector<std::future<bool>> tasks;
        for (int device = 0; device < 4; ++device) {
            const std::string rank = "kernel_shared_device_" + std::to_string(device);
            manager.UpdateRankIdToDeviceId(dbPath.string(), rank, std::to_string(device));
            ParserStatusManager::Instance().SetParserStatus(KERNEL_PREFIX + rank, ParserStatus::INIT);
            tasks.emplace_back(parser.threadPool->AddTask(
                [&, ready, rank] {
                    ready.wait();
                    std::string message;
                    return KernelParse::InitParser({csvPath.string()}, rank, dbPath.string(), message);
                },
                ""));
        }
        start.set_value();
        for (auto &task : tasks) {
            EXPECT_TRUE(task.get());
        }
        // Both cold parsing and cache reuse must notify every associated rank.
        for (int device = 0; device < 4; ++device) {
            const std::string rank = "kernel_shared_device_" + std::to_string(device);
            for (int csvDevice = 0; csvDevice < 4; ++csvDevice) {
                EXPECT_EQ(std::count(
                              completed.begin(), completed.end(), "[MSPROF]" + rank + "__" + std::to_string(csvDevice)),
                    1);
            }
            EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(KERNEL_PREFIX + rank), ParserStatus::FINISH);
        }
        auto database = manager.GetSummaryDataBaseByFileId(dbPath.string());
        ASSERT_NE(database, nullptr);
        auto statement = database->CreatPreparedStatement("SELECT COUNT(*) AS row_count FROM kernel_detail");
        ASSERT_NE(statement, nullptr);
        auto rows = statement->ExecuteQuery();
        ASSERT_NE(rows, nullptr);
        ASSERT_TRUE(rows->Next());
        EXPECT_EQ(rows->GetInt64("row_count"), 4);
    }
    callback = nullptr;
    parser.SetParseEndCallBack(callback);
    parser.Reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    std::filesystem::remove(csvPath);
    std::filesystem::remove(dbPath);
    std::filesystem::remove(folder);
}

TEST_F(KernelParseTest, GetMsProfKernelFilesSuccess) {
    std::vector<std::string> paths = {Dic::FileUtil::SplicePath(testDataPath, "msprof", "normal")};
    auto result = KernelParse::GetKernelFiles(paths);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result.at(0),
        Dic::FileUtil::SplicePath(paths[0], "mindstudio_profiler_output", "op_summary_20230919172305.csv"));
}

TEST_F(KernelParseTest, GetMsProfSliceKernelFilesSuccess) {
    std::vector<std::string> paths = {Dic::FileUtil::SplicePath(testDataPath, "msprof", "slice")};
    auto result = KernelParse::GetKernelFiles(paths);
    EXPECT_EQ(result.size(), 2); // 1
    EXPECT_EQ(result.at(0),
        Dic::FileUtil::SplicePath(paths[0], "mindstudio_profiler_output", "op_summary_slice_1_20230919172305.csv"));
    EXPECT_EQ(result.at(1),
        Dic::FileUtil::SplicePath(paths[0], "mindstudio_profiler_output", "op_summary_slice_0_20230919172304.csv"));
}

TEST_F(KernelParseTest, CheckHeaderFieldAndFilterParseFuncFail) {
    std::vector<std::string> rowVector = {
        FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE, FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, false);
    EXPECT_EQ(parseFuncList.size(), 0);
}

TEST_F(KernelParseTest, CheckPyTorchBaseHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE, FIELD_START_TIME,
        FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 2); // 2

    Kernel kernel{};
    std::string fileId = "0";
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48"};
    Kernel ref = {
        fileId, "", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "", "", "", "", "", ""};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, fileId, kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckPyTorchTaskStartTimeHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::STEP_ID, FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE,
        FIELD_TASK_START_TIME, FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 3); // 3

    Kernel kernel{};
    std::string fileId = "0";
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"1", "Ad0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48"};
    Kernel ref = {
        fileId, "1", "", "Ad0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "", "", "", "", "", ""};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, fileId, kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckMsProfBaseHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::DEVICE_ID, FIELD_OP_NAME, FIELD_OP_TYPE, FIELD_TASK_TYPE,
        FIELD_TASK_START_TIME, FIELD_TASK_DURATION, FIELD_TASK_WAIT_TIME, FIELD_BLOCK_DIM};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 2); // 2

    Kernel kernel{};
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"0", "Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48"};
    Kernel ref = {"0", "", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "", "", "", "", "", ""};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, "1", kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckPyTorchWithShapeHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::STEP_ID, FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE,
        FIELD_START_TIME, FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM, FIELD_INPUT_SHAPES, FIELD_INPUT_DATA_TYPES,
        FIELD_INPUT_FORMATS, FIELD_OUTPUT_SHAPES, FIELD_OUTPUT_DATA_TYPES, FIELD_OUTPUT_FORMATS};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 4); // 4

    Kernel kernel{};
    std::string fileId = "0";
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"1", "Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48",
        "2,8193;2;2", "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND"};
    Kernel ref = {fileId, "1", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "2,8193;2;2",
        "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND"};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, fileId, kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckMsProfWithShapeHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::DEVICE_ID, FIELD_OP_NAME, FIELD_OP_TYPE, FIELD_TASK_TYPE,
        FIELD_TASK_START_TIME, FIELD_TASK_DURATION, FIELD_TASK_WAIT_TIME, FIELD_BLOCK_DIM, FIELD_INPUT_SHAPES,
        FIELD_INPUT_DATA_TYPES, FIELD_INPUT_FORMATS, FIELD_OUTPUT_SHAPES, FIELD_OUTPUT_DATA_TYPES,
        FIELD_OUTPUT_FORMATS};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 3); // 3

    Kernel kernel{};
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"0", "Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48",
        "2,8193;2;2", "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND"};
    Kernel ref = {"0", "", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "2,8193;2;2",
        "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND"};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, "1", kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckPyTorchWithPipeUtilizationAICMetricsHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::STEP_ID, FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE,
        FIELD_START_TIME, FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM, FIELD_INPUT_SHAPES, FIELD_INPUT_DATA_TYPES,
        FIELD_INPUT_FORMATS, FIELD_OUTPUT_SHAPES, FIELD_OUTPUT_DATA_TYPES, FIELD_OUTPUT_FORMATS, "aicore_time(us)",
        "total_cycles", "vec_time(us)", "vec_ratio", "mac_time(us)", "mac_ratio", "scalar_time(us)", "scalar_ratio",
        "mte1_time(us)", "mte1_ratio", "mte2_time(us)", "mte2_ratio", "mte3_time(us)", "mte3_ratio", "icache_miss_rate",
        "memory_bound", "cube_utilization(%)"};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 5); // 5

    Kernel kernel{};
    std::string fileId = "0";
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"1", "Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48",
        "2,8193;2;2", "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND", "364.64",
        "11668432", "62.096", "0.17", "0.0", "0.0", "0.851", "0.002", "0.0", "0.0", "327.665", "0.899", "354.137",
        "0.971", "0.005", "5.288", "0"};
    Kernel ref = {fileId, "1", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "2,8193;2;2",
        "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND",
        {"364.64", "11668432", "62.096", "0.17", "0.0", "0.0", "0.851", "0.002", "0.0", "0.0", "327.665", "0.899",
            "354.137", "0.971", "0.005", "5.288", "0"}};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, fileId, kernel);
    }
    CheckKernelData(kernel, ref);
}

TEST_F(KernelParseTest, CheckPyTorchWithArithmeticUtilizationAICMetricsHeaderFieldAndFilterParseFunc) {
    std::vector<std::string> rowVector = {Dic::STEP_ID, FIELD_NAME, FIELD_TYPE, FIELD_ACCELERATOR_CORE,
        FIELD_START_TIME, FIELD_DURATION, FIELD_WAIT_TIME, FIELD_BLOCK_DIM, FIELD_INPUT_SHAPES, FIELD_INPUT_DATA_TYPES,
        FIELD_INPUT_FORMATS, FIELD_OUTPUT_SHAPES, FIELD_OUTPUT_DATA_TYPES, FIELD_OUTPUT_FORMATS, "aicore_time(us)",
        "total_cycles", "mac_fp16_ratio", "mac_int8_ratio", "vec_fp32_ratio", "vec_fp16_ratio", "vec_int32_ratio",
        "vec_misc_ratio", "cube_fops", "vector_fops"};
    std::vector<std::function<void(const std::map<std::string, size_t> &dataMap, const std::vector<std::string> &rows,
        const std::string &fileId, Kernel &kernel)>>
        parseFuncList;
    bool result = CheckHeaderFieldAndFilterParseFunc(rowVector, parseFuncList);
    EXPECT_EQ(result, true);
    EXPECT_EQ(parseFuncList.size(), 5); // 5

    Kernel kernel{};
    std::string fileId = "0";
    std::map<std::string, size_t> dataMap = GenerateDataMap(rowVector);
    std::vector<std::string> dataVector = {"1", "Add0", "Add", "AI_CORE", "1695115378710248", "498.4", "0", "48",
        "2,8193;2;2", "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND", "361.76",
        "11576412", "0.0", "0.0", "0.0", "0.156", "0.0", "0.001", "0.0", "154601472.0"};
    Kernel ref = {fileId, "1", "", "Add0", "Add", "", "AI_CORE", 1695115378710248000, 498.4, 0, 48, "2,8193;2;2",
        "INT32;INT64;INT64", "FORMAT_ND;FORMAT_ND;FORMAT_ND", "2,8192", "INT32", "FORMAT_ND",
        {"361.76", "11576412", "0.0", "0.0", "0.0", "0.156", "0.0", "0.001", "0.0", "154601472.0"}};
    for (const auto &parseFunc : parseFuncList) {
        parseFunc(dataMap, dataVector, fileId, kernel);
    }
    CheckKernelData(kernel, ref);
}
