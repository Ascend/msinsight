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
#include <atomic>
#include <filesystem>
#include "../../defaultMock/MockFileReader.h"
#include "ParserJson_mock_data.h"
#include "ProjectParserJson.h"
#include "ProjectParserPytorchTrace.h"
#include "TestSuit.h"
#include "DataBaseManager.h"
#include "FileUtil.h"
using namespace Dic::Module;
using namespace Dic::Module::ParserJsonMock;
class ParserJsonTest : public ::testing::Test {
  protected:
    std::vector<std::string> tempFiles_;
    std::vector<std::string> tempDirs_;

    inline std::string GetTestDataDir() { return TestSuit::GetTestDataFile(); }

    std::string CreateTempJsonFile(const std::string &content) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string uniqueName = std::string(testInfo->name()) + "_" + std::to_string(std::rand()) + ".json";
        std::string path = Dic::FileUtil::SplicePath(::testing::TempDir(), uniqueName);

        std::ofstream file(path);
        if (file.is_open()) {
            file << content;
            file.close();
            tempFiles_.push_back(path);
            return path;
        }
        return "";
    }

    std::string CreateTempDirectoryWithFiles(const std::vector<std::string> &fileNames) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string uniqueName = std::string(testInfo->name()) + "_" + std::to_string(std::rand());
        std::string path = Dic::FileUtil::SplicePath(::testing::TempDir(), uniqueName);
        if (!std::filesystem::create_directories(path)) {
            return "";
        }
        tempDirs_.push_back(path);
        for (const auto &fileName : fileNames) {
            std::ofstream file(Dic::FileUtil::SplicePath(path, fileName));
            if (!file.is_open()) {
                return "";
            }
            file << R"({"traceEvents":[]})";
        }
        return path;
    }

    std::string CreateStableTempDirectoryWithFiles(const std::vector<std::string> &fileNames) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        static std::atomic<int> seq{0};
        std::string path;
        for (int i = 0; i < 8; ++i) {
            const std::string candidate = Dic::FileUtil::SplicePath(
                ::testing::TempDir(), std::string(testInfo->name()) + "_" + std::to_string(seq.fetch_add(1)));
            std::error_code ec;
            if (std::filesystem::create_directories(candidate, ec)) {
                path = candidate;
                break;
            }
        }
        if (path.empty()) {
            return "";
        }
        tempDirs_.push_back(path);
        for (const auto &fileName : fileNames) {
            std::ofstream file(Dic::FileUtil::SplicePath(path, fileName));
            if (!file.is_open()) {
                return "";
            }
            file << R"({"traceEvents":[]})";
        }
        return path;
    }

    void TearDown() override {
        for (const auto &path : tempFiles_) {
            std::remove(path.c_str());
        }
        tempFiles_.clear();
        for (const auto &path : tempDirs_) {
            std::filesystem::remove_all(path);
        }
        tempDirs_.clear();
    }
};

/**
 * 如果json文件个数为空则校验不通过
 */
TEST_F(ParserJsonTest, TestJsonFileIsEmptyThenReturnFalse) {
    class MockParserJson : public Dic::Module::ProjectParserJson {
      public:
        MockParserJson() : ProjectParserJson(JsonFileParserManager::GetTraceFileParser()) {}
        void SetIFileReader(std::unique_ptr<IFileReader> fileReaderPtr) { fileReader = std::move(fileReaderPtr); }
        bool CheckParseFileInfoSizeTest(
            const std::shared_ptr<Global::ParseFileInfo> parseFileInfo, std::vector<std::string> &jsonFiles) {
            return CheckParseFileInfoSize(parseFileInfo, jsonFiles);
        }
    };
    MockParserJson parserJson;
    std::unique_ptr<IFileReader> fileReaderPtr = std::make_unique<MockFileReader>();
    parserJson.SetIFileReader(std::move(fileReaderPtr));
    auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
    std::vector<std::string> jsonFiles;
    bool result = parserJson.CheckParseFileInfoSizeTest(parseFileInfo, jsonFiles);
    EXPECT_EQ(result, false);
}

/**
 * 如果json文件个数超过100则校验不通过,小于等于100则校验通过
 */
TEST_F(ParserJsonTest, TestJsonFileCountExceed100ThenReturnFalse) {
    class MockParserJson : public Dic::Module::ProjectParserJson {
      public:
        MockParserJson() : ProjectParserJson(JsonFileParserManager::GetTraceFileParser()) {}
        void SetIFileReader(std::unique_ptr<IFileReader> fileReaderPtr) { fileReader = std::move(fileReaderPtr); }
        bool CheckParseFileInfoSizeTest(
            const std::shared_ptr<Global::ParseFileInfo> parseFileInfo, std::vector<std::string> &jsonFiles) {
            return CheckParseFileInfoSize(parseFileInfo, jsonFiles);
        }
    };
    std::unique_ptr<IFileReader> fileReaderPtr = std::make_unique<MockFileReader>();
    MockParserJson parserJson;
    parserJson.SetIFileReader(std::move(fileReaderPtr));
    auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
    std::vector<std::string> jsonFiles;
    const uint8_t fileCount = 100;
    for (int i = 0; i < fileCount; ++i) {
        jsonFiles.emplace_back("kkkkk");
    }
    bool result = parserJson.CheckParseFileInfoSizeTest(parseFileInfo, jsonFiles);
    EXPECT_EQ(result, true);
    jsonFiles.emplace_back("lllll");
    result = parserJson.CheckParseFileInfoSizeTest(parseFileInfo, jsonFiles);
    EXPECT_EQ(result, false);
}

/**
 * 导入50个文件，如果有一个文件大小超过20G就校验不通过
 */
TEST_F(ParserJsonTest, TestCheckParseFileInfoSizeWhenOneFileIs20GThenReturnFalse) {
    class MockParserJsonFileReader : public MockFileReader {
      public:
        virtual int64_t GetFileSize(const std::string &filePath) {
            return CheckParseFileInfoSizeWhenOneFileIs20GThenReturnFalseMock(filePath);
        }
    };
    class MockParserJson : public Dic::Module::ProjectParserJson {
      public:
        MockParserJson() : ProjectParserJson(JsonFileParserManager::GetTraceFileParser()) {}
        void SetIFileReader(std::unique_ptr<IFileReader> fileReaderPtr) { fileReader = std::move(fileReaderPtr); }
        bool CheckParseFileInfoSizeTest(
            const std::shared_ptr<Global::ParseFileInfo> parseFileInfo, std::vector<std::string> &jsonFiles) {
            return CheckParseFileInfoSize(parseFileInfo, jsonFiles);
        }
    };
    std::unique_ptr<IFileReader> fileReaderPtr = std::make_unique<MockParserJsonFileReader>();
    MockParserJson parserJson;
    parserJson.SetIFileReader(std::move(fileReaderPtr));
    auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
    std::vector<std::string> jsonFiles;
    const uint8_t fileCount = 50;
    for (int i = 0; i < fileCount; ++i) {
        jsonFiles.emplace_back("kkkkk");
    }
    bool result = parserJson.CheckParseFileInfoSizeTest(parseFileInfo, jsonFiles);
    EXPECT_EQ(result, false);
}

/**
 * 导入21个文件，前20个文件大小为1G，最后一个为1byte，总大小超过20G,校验不通过
 */
TEST_F(ParserJsonTest, TestCheckParseFileInfoSizeWhenTotalFileSizeExceed20GThenReturnFalse) {
    class MockParserJsonFileReader : public MockFileReader {
      public:
        virtual int64_t GetFileSize(const std::string &filePath) {
            return CheckParseFileInfoSizeWhenTotalFileSizeExceed20GThenReturnFalseMock(filePath);
        }
    };
    class MockParserJson : public Dic::Module::ProjectParserJson {
      public:
        MockParserJson() : ProjectParserJson(JsonFileParserManager::GetTraceFileParser()) {}
        void SetIFileReader(std::unique_ptr<IFileReader> fileReaderPtr) { fileReader = std::move(fileReaderPtr); }
        bool CheckParseFileInfoSizeTest(
            const std::shared_ptr<Global::ParseFileInfo> parseFileInfo, std::vector<std::string> &jsonFiles) {
            return CheckParseFileInfoSize(parseFileInfo, jsonFiles);
        }
    };
    std::unique_ptr<IFileReader> fileReaderPtr = std::make_unique<MockParserJsonFileReader>();
    MockParserJson parserJson;
    parserJson.SetIFileReader(std::move(fileReaderPtr));
    auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
    std::vector<std::string> jsonFiles;
    const uint8_t fileCount = 21;
    for (int i = 0; i < fileCount; ++i) {
        jsonFiles.emplace_back("kkkkk");
    }
    bool result = parserJson.CheckParseFileInfoSizeTest(parseFileInfo, jsonFiles);
    EXPECT_EQ(result, false);
}

TEST_F(ParserJsonTest, TestCheckHasTraceJsonMemoryDataOperatorData) {
    class MockParserJson : public Dic::Module::ProjectParserJson {
      public:
        static void CheckHasTraceJsonMemeoryDataOpDataFailTest() {
            Global::ProjectExplorerInfo projectExplorerInfo;
            std::vector<std::string> parseFileList = {"a.a", "invalid.csv"};
            for (const auto &parseFile : parseFileList) {
                auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
                parseFileInfo->parseFilePath = parseFile;
                projectExplorerInfo.subParseFileInfo.push_back(parseFileInfo);
            }
            auto [hasJson, hasMemory, hasOp] = CheckHasJsonMemoryDataOperatorData({projectExplorerInfo});
            EXPECT_EQ(hasJson, false);
            EXPECT_EQ(hasMemory, false);
            EXPECT_EQ(hasOp, false);
        }

        static void CheckHasTraceJsonMemeoryDataOpDataSuccessTest() {
            Global::ProjectExplorerInfo projectExplorerInfo;
            std::vector<std::string> parseFileList = {"a.json", "memory_record.csv", "kernel_details.csv"};
            for (const auto &parseFile : parseFileList) {
                auto parseFileInfo = std::make_shared<Global::ParseFileInfo>();
                parseFileInfo->parseFilePath = parseFile;
                projectExplorerInfo.subParseFileInfo.push_back(parseFileInfo);
            }
            auto [hasJson, hasMemory, hasOp] = CheckHasJsonMemoryDataOperatorData({projectExplorerInfo});
            EXPECT_EQ(hasJson, true);
            EXPECT_EQ(hasMemory, true);
            EXPECT_EQ(hasOp, true);
        }
    };

    MockParserJson::CheckHasTraceJsonMemeoryDataOpDataFailTest();
    MockParserJson::CheckHasTraceJsonMemeoryDataOpDataSuccessTest();
}

TEST_F(ParserJsonTest, BuildProjectInfoWithSingleFile) {
    ProjectExplorerInfo projectInfo;
    std::string projectPath =
        Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0", "ASCEND_PROFILER_OUTPUT", "trace_view.json");
    projectInfo.fileName = projectPath;
    Dic::Module::ProjectParserJson::BuildProjectExploreInfo(projectInfo, {projectPath});
    EXPECT_EQ(projectInfo.projectFileTree.size(), 1);
    EXPECT_EQ(projectInfo.subParseFileInfo.size(), 1);
    EXPECT_EQ(projectInfo.fileInfoMap.size(), 2); // expect has 2 elements
    auto file = projectInfo.projectFileTree[0];
    EXPECT_EQ(file->type, ParseFileType::PROJECT);
    EXPECT_EQ(file->subId, projectPath);
    EXPECT_EQ(file->subParseFile.size(), 1);
    file = file->subParseFile[0];
    EXPECT_EQ(file->type, ParseFileType::RANK);
    EXPECT_EQ(file->subId, "trace_view.json");
}

TEST_F(ParserJsonTest, BuildProjectInfoWithAscendProfilerOutputDir) {
    ProjectExplorerInfo projectInfo;
    std::string projectPath = Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0", "ASCEND_PROFILER_OUTPUT");
    projectInfo.fileName = projectPath;
    Dic::Module::ProjectParserJson::BuildProjectExploreInfo(projectInfo, {projectPath});
    EXPECT_EQ(projectInfo.projectFileTree.size(), 1);
    EXPECT_EQ(projectInfo.subParseFileInfo.size(), 1);
    EXPECT_EQ(projectInfo.fileInfoMap.size(), 2); // expect has 2 elements
    auto file = projectInfo.projectFileTree[0];
    EXPECT_EQ(file->type, ParseFileType::PROJECT);
    EXPECT_EQ(file->subId, projectPath);
    EXPECT_EQ(file->subParseFile.size(), 1);
    file = file->subParseFile[0];
    EXPECT_EQ(file->type, ParseFileType::RANK);
    EXPECT_EQ(file->subId, "ASCEND_PROFILER_OUTPUT");
}

TEST_F(ParserJsonTest, GetParseFileByImportFile) {
    ProjectParserJson parser(JsonFileParserManager::GetTraceFileParser());
    std::string msg;
    auto files = parser.GetParseFileByImportFile(
        Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0", "ASCEND_PROFILER_OUTPUT", "trace_view.json"), msg);
    EXPECT_EQ(files.size(), 1);
    EXPECT_EQ(files[0],
        Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0", "ASCEND_PROFILER_OUTPUT", "trace_view.json"));
    auto files2 = parser.GetParseFileByImportFile(
        Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0", "ASCEND_PROFILER_OUTPUT"), msg);
    EXPECT_EQ(files2.size(), 1);
}

TEST_F(ParserJsonTest, PytorchTraceDirectFileBuildsDedicatedProjectInfo) {
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string directory = CreateTempDirectoryWithFiles({fileName});
    ASSERT_FALSE(directory.empty());
    const std::string filePath = Dic::FileUtil::SplicePath(directory, fileName);
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(filePath, error);

    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0], filePath);
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(parser.GetProjectType(filePath), ProjectTypeEnum::PYTORCH_TRACE);
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = filePath;
    projectInfo.projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
    ProjectParserPytorchTrace::BuildProjectExploreInfo(projectInfo, files);
    ASSERT_EQ(projectInfo.subParseFileInfo.size(), 1);
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->parseFilePath, filePath);
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->fileId,
        Dic::FileUtil::SplicePath(directory, "msprof_3466812.1787275553016492530_mindstudio_insight_data.db"));
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->projectType, static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE));
}

TEST_F(ParserJsonTest, PytorchTraceNestedDirectoryPlacesDatabaseBesideTraceFile) {
    const std::string outer = CreateTempDirectoryWithFiles({});
    ASSERT_FALSE(outer.empty());
    const std::string inner = Dic::FileUtil::SplicePath(outer, "rank0");
    ASSERT_TRUE(std::filesystem::create_directories(inner));
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string filePath = Dic::FileUtil::SplicePath(inner, fileName);
    {
        std::ofstream file(filePath);
        ASSERT_TRUE(file.is_open());
        file << R"({"traceEvents":[]})";
    }
    ProjectParserPytorchTrace parser;
    std::string error;
    auto files = parser.GetParseFileByImportFile(outer, error);
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0], filePath);
    EXPECT_TRUE(error.empty());
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = outer;
    projectInfo.projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
    ProjectParserPytorchTrace::BuildProjectExploreInfo(projectInfo, files);
    ASSERT_EQ(projectInfo.subParseFileInfo.size(), 1);
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->parseFilePath, filePath);
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->fileId, Dic::FileUtil::SplicePath(inner, "mindstudio_insight_data.db"));
}

TEST_F(ParserJsonTest, PytorchTraceDirectoryPersistsSourceFileAndDirectoryDatabase) {
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string directory = CreateTempDirectoryWithFiles({fileName});
    ASSERT_FALSE(directory.empty());
    const std::string filePath = Dic::FileUtil::SplicePath(directory, fileName);
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(directory, error);

    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0], filePath);
    EXPECT_TRUE(error.empty());
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = directory;
    projectInfo.projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
    ProjectParserPytorchTrace::BuildProjectExploreInfo(projectInfo, files);
    ASSERT_EQ(projectInfo.subParseFileInfo.size(), 1);
    EXPECT_EQ(projectInfo.subParseFileInfo[0]->parseFilePath, filePath);
    EXPECT_EQ(
        projectInfo.subParseFileInfo[0]->fileId, Dic::FileUtil::SplicePath(directory, "mindstudio_insight_data.db"));
}

TEST_F(ParserJsonTest, PytorchTraceBaselineUsesPersistedDirectoryDatabase) {
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string directory = CreateTempDirectoryWithFiles({fileName});
    ASSERT_FALSE(directory.empty());
    const std::string filePath = Dic::FileUtil::SplicePath(directory, fileName);
    const std::string databasePath = Dic::FileUtil::SplicePath(directory, "mindstudio_insight_data.db");
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = directory;
    projectInfo.projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
    ProjectParserPytorchTrace::BuildProjectExploreInfo(projectInfo, {filePath});
    auto &databaseManager = Dic::Module::Timeline::DataBaseManager::Instance();
    databaseManager.Clear();
    ASSERT_TRUE(databaseManager.CreateTraceConnectionPool("existing", databasePath));
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePath;
    ProjectParserPytorchTrace parser;

    parser.ParserBaseline(projectInfo, baselineInfo);

    EXPECT_EQ(baselineInfo.fileId, databasePath);
    databaseManager.Clear();
}

TEST_F(ParserJsonTest, PytorchTraceBaselineFallsBackToDirectFileDatabase) {
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string directory = CreateStableTempDirectoryWithFiles({fileName});
    ASSERT_FALSE(directory.empty());
    const std::string filePath = Dic::FileUtil::SplicePath(directory, fileName);
    const std::string databasePath = ProjectParserPytorchTrace::GetDirectFileDbPath(filePath);
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = filePath;
    projectInfo.projectType = static_cast<int64_t>(ProjectTypeEnum::PYTORCH_TRACE);
    ProjectParserPytorchTrace::BuildProjectExploreInfo(projectInfo, {filePath});
    ASSERT_EQ(projectInfo.subParseFileInfo.size(), 1);
    projectInfo.subParseFileInfo[0]->fileId.clear();
    auto &databaseManager = Dic::Module::Timeline::DataBaseManager::Instance();
    databaseManager.Clear();
    ASSERT_TRUE(databaseManager.CreateTraceConnectionPool("existing", databasePath));
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePath;
    ProjectParserPytorchTrace parser;

    parser.ParserBaseline(projectInfo, baselineInfo);

    EXPECT_EQ(baselineInfo.fileId, databasePath);
    EXPECT_EQ(baselineInfo.fileId,
        Dic::FileUtil::SplicePath(directory, "msprof_3466812.1787275553016492530_mindstudio_insight_data.db"));
    databaseManager.Clear();
}

TEST_F(ParserJsonTest, PytorchTraceDirectoryRejectsMultipleTraceFiles) {
    const std::string directory = CreateTempDirectoryWithFiles(
        {"msprof_3466812.1787275553016492530.pt.trace.json", "msprof_3466812.1787275553016492531.pt.trace.json"});
    ASSERT_FALSE(directory.empty());
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(directory, error);

    EXPECT_TRUE(files.empty());
    EXPECT_FALSE(error.empty());
}

TEST_F(ParserJsonTest, PytorchTraceDirectoryRejectsMixedTraceLayout) {
    const std::string directory =
        CreateTempDirectoryWithFiles({"msprof_3466812.1787275553016492530.pt.trace.json", "trace_view.json"});
    ASSERT_FALSE(directory.empty());
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(directory, error);

    EXPECT_TRUE(files.empty());
    EXPECT_FALSE(error.empty());
}

TEST_F(ParserJsonTest, PytorchTraceRejectsNonPytorchFile) {
    const std::string directory = CreateStableTempDirectoryWithFiles({"trace_view.json"});
    ASSERT_FALSE(directory.empty());
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(Dic::FileUtil::SplicePath(directory, "trace_view.json"), error);

    EXPECT_TRUE(files.empty());
    EXPECT_EQ(error, "The selected file is not a PyTorch trace JSON file");
}

TEST_F(ParserJsonTest, PytorchTraceRejectsEmptyDirectory) {
    const std::string directory = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(directory.empty());
    ProjectParserPytorchTrace parser;
    std::string error;

    auto files = parser.GetParseFileByImportFile(directory, error);

    EXPECT_TRUE(files.empty());
    EXPECT_EQ(error, "No PyTorch trace JSON file found");
}

TEST_F(ParserJsonTest, BuildProjectCluster) {
    ProjectExplorerInfo projectInfo;
    ProjectParserJson::BuildProjectExploreInfo(projectInfo, {GetTestDataDir()});
    EXPECT_EQ(projectInfo.GetClusterInfos().size(), 1);
}

TEST_F(ParserJsonTest, GetDeviceIdFromMemory) {
    std::string parseFolder = Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0");
    auto deviceId = ProjectParserJson::GetDeviceIdFromMemory(parseFolder);
    EXPECT_EQ(deviceId, "0");
}

TEST_F(ParserJsonTest, GetDeviceIdFromOperator) {
    std::string parseFolder = Dic::FileUtil::SplicePath(GetTestDataDir(), "test_rank_0");
    auto deviceId = ProjectParserJson::GetDeviceIdFromKernel(parseFolder);
    EXPECT_EQ(deviceId, "");
}

TEST_F(ParserJsonTest, GetDeviceIdFromPath) {
    std::string parseFolder = Dic::FileUtil::SplicePath(GetTestDataDir(), "msprof", "normal", "PROF_20250620");
    auto deviceId = ProjectParserJson::GetDeviceIdFromPath(parseFolder);
    EXPECT_EQ(deviceId, "");
}

// 测试夹具：管理临时文件生命周期
class ACLGraphDebugJSONTest : public ::testing::Test {
  protected:
    std::vector<std::string> tempFiles_;

    // 创建带指定内容的临时文件，返回路径
    std::string CreateTempFile(const std::string &content) {
        // 使用Google Test提供的安全临时目录
        std::string tempDir = ::testing::TempDir();
        // 生成唯一文件名（含测试名避免冲突）
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string uniqueName = std::string(testInfo->name()) + "_" + std::to_string(std::rand()) + ".json";
        std::string path = Dic::FileUtil::SplicePath(tempDir, uniqueName);

        std::ofstream file(path);
        if (file.is_open()) {
            file << content;
            file.close();
            tempFiles_.push_back(path);
            return path;
        }
        return "";
    }

    void TearDown() override {
        // 自动清理所有创建的临时文件
        for (const auto &path : tempFiles_) {
            std::remove(path.c_str());
        }
    }
};

// ===== 正向测试：严格小写 aclgraph =====
TEST_F(ACLGraphDebugJSONTest, Valid_ExactLowercaseAclgraph) {
    std::vector<std::string> valid_cases = {
        R"([{"pid": "aclGraph"})", // 精确匹配
        R"([{"pid": "xxx aclGraph"})", // 需求示例
    };
    for (const auto &content : valid_cases) {
        std::string path = CreateTempFile(content);
        ASSERT_FALSE(path.empty());
        EXPECT_TRUE(ProjectParserJson::IsACLGraphDebugJSON(path)) << "Failed for valid content: " << content;
        std::remove(path.c_str());
    }
}

// ===== 负向测试：大小写变体应拒绝 =====
TEST_F(ACLGraphDebugJSONTest, Invalid_UppercaseVariantsRejected) {
    std::vector<std::string> invalid_cases = {
        R"([{"pid": "ACLGRAPH"})", // 全大写
        R"([{"pid": "AclGraph"})", // 驼峰（首字母大写）
        R"([{"pid": "ACLgraph"})", // 前缀大写
        R"([{"pid": "aclgrAph"})", // A 大写
        R"([{"pid": "Aclgraph"})", // A 大写
        R"([{"pid": "aclGRAPH"})", // 后缀大写
        R"([{"pid": "Ascend ACLGraph"})", // 混合大小写
        R"([{"pid": "xxx aclGraph debug"})", // 需求示例但带后缀
        R"([{"pid": "xxx ACLgraph"})", // 需求示例但 ACL 大写
        R"([{"pid": "xxx aclgrAPh"})", // 部分大写
        R"([{"pid": "xxx ACLGRAPH"})", // 全大写
        R"([{"pid": "xxx acl-graph"})", // 连字符（非 aclgraph）
        R"([{"pid": "xxx aclgrph"})", // 拼写错误（缺 a）
        R"([{"pid": "xxx aclgrap"})", // 拼写错误（缺 h）
    };
    for (const auto &content : invalid_cases) {
        std::string path = CreateTempFile(content);
        ASSERT_FALSE(path.empty());
        EXPECT_FALSE(ProjectParserJson::IsACLGraphDebugJSON(path)) << "Should reject: " << content;
        std::remove(path.c_str());
    }
}

TEST_F(ACLGraphDebugJSONTest, Invalid_SomeWordsAfterAclgraph) {
    std::string content = R"([{"pid": "xxx_aclGraph_core", "name": "test"}])";
    std::string path = CreateTempFile(content);
    ASSERT_FALSE(path.empty());
    EXPECT_FALSE(ProjectParserJson::IsACLGraphDebugJSON(path));
}

TEST_F(ACLGraphDebugJSONTest, Invalid_MatchOnlyNotArrayRoot) {
    std::string content = "{}\n{}\n{}\n{\"pid\": \"xxx aclGraph\"}";
    std::string path = CreateTempFile(content);
    ASSERT_FALSE(path.empty());
    EXPECT_FALSE(ProjectParserJson::IsACLGraphDebugJSON(path)); // 仅检查前三行
}

TEST_F(ParserJsonTest, FtraceJsonValidWhenFirstXEventPidMatches) {
    std::string content = R"([{"ph": "M", "pid": "Ascend Hardware"}, {"ph": "X", "pid": "CPU Scheduling"}])";
    std::string path = CreateTempJsonFile(content);
    ASSERT_FALSE(path.empty());
    EXPECT_TRUE(ProjectParserJson::IsFtraceJsonData(path));
}

TEST_F(ParserJsonTest, FtraceJsonValidWhenTraceEventsPidMatches) {
    std::string content = R"({"traceEvents": [{"ph": "X", "pid": "Process Scheduling"}]})";
    std::string path = CreateTempJsonFile(content);
    ASSERT_FALSE(path.empty());
    EXPECT_TRUE(ProjectParserJson::IsFtraceJsonData(path));
}

TEST_F(ParserJsonTest, FtraceJsonInvalidWhenFirstXEventPidMismatches) {
    std::string content = R"([{"ph": "X", "pid": "Ascend Hardware"}, {"ph": "X", "pid": "CPU Scheduling"}])";
    std::string path = CreateTempJsonFile(content);
    ASSERT_FALSE(path.empty());
    EXPECT_FALSE(ProjectParserJson::IsFtraceJsonData(path));
}

TEST_F(ParserJsonTest, FtraceJsonInvalidWhenInputIsFolder) {
    EXPECT_FALSE(ProjectParserJson::IsFtraceJsonData(::testing::TempDir()));
}
