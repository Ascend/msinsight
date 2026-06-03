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
#include "../../defaultMock/MockFileReader.h"
#include "ParserJson_mock_data.h"
#include "ProjectParserJson.h"
#include "TestSuit.h"
#include "FileUtil.h"
using namespace Dic::Module;
using namespace Dic::Module::ParserJsonMock;
class ParserJsonTest : public ::testing::Test {
  protected:
    std::vector<std::string> tempFiles_;

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

    void TearDown() override {
        for (const auto &path : tempFiles_) {
            std::remove(path.c_str());
        }
        tempFiles_.clear();
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
