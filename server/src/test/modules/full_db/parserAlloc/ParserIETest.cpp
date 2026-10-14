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
#include <gmock/gmock.h>
#include "DataBaseManager.h"
#include "JsonFileParserManager.h"
#include "ParserIE.h"
#include "ThreadPool.h"
#include "EventNotifyThreadPoolExecutor.h"
class ParserIETest : public ::testing::Test {};

TEST_F(ParserIETest, TestParser) {
    class MockOpenApi : public Dic::Module::IE::ServitizationOpenApi {
      public:
        std::vector<Dic::Module::IE::TaskInfo> ComputeTaskInfo(const std::string &path) {
            std::vector<Dic::Module::IE::TaskInfo> tasks;
            Dic::Module::IE::TaskInfo task;
            task.fileId = "ll";
            task.filePath = "mmm";
            tasks.emplace_back(task);
            return tasks;
        }
    };
    class MockParserIE : public Dic::Module::ParserIE {
      public:
        void SetOpenApi() { servitizationOpenApi = std::make_shared<MockOpenApi>(); }

        void Reset() { servitizationOpenApi->Reset(); }
    };
    Dic::Module::Timeline::DataBaseManager::Instance().Clear();
    MockParserIE parserIe;
    parserIe.SetOpenApi();
    Dic::Module::Global::ProjectExplorerInfo info;
    std::vector<Dic::Module::Global::ProjectExplorerInfo> projectInfos;
    projectInfos.emplace_back(info);
    ImportActionRequest request;
    ImportActionResponse response;
    request.params.path.emplace_back("");
    parserIe.Parser(projectInfos, request, response);
    EXPECT_TRUE(response.result);
    EXPECT_TRUE(response.body.isIE);
    EXPECT_TRUE(response.body.result.empty());
    // Finish the import and its completion notification before resetting the parser's shared state.
    Dic::ThreadPool::Instance().WaitForAllTasks();
    Dic::Module::Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->WaitForAllTasks();
    parserIe.Reset();
    Module::Timeline::JsonFileParserManager::ResetAll();
    Dic::Module::Timeline::DataBaseManager::Instance().Clear();
}

TEST_F(ParserIETest, TestGetProjectType) {
    Dic::Module::ParserIE parserIe;
    auto res = parserIe.GetProjectType({});
    EXPECT_EQ(res, ProjectTypeEnum::IE);
}

TEST_F(ParserIETest, BuidlProject) {
    Dic::Module::ParserIE parserIE;
    ProjectExplorerInfo projectInfo;
    projectInfo.projectName = "testIe";
    projectInfo.projectType = static_cast<int>(ProjectTypeEnum::IE);
    std::vector<std::string> parseFileList = {"ms_service_parsed_1.db", "ms_service_parsed_2.db"};
    parserIE.BuildProjectExploreInfo(projectInfo, parseFileList);
    EXPECT_EQ(projectInfo.subParseFileInfo.size(), 2); // expect 2
    for (size_t i = 0; i < parseFileList.size(); i++) {
        EXPECT_EQ(projectInfo.subParseFileInfo[i]->parseFilePath, parseFileList[i]);
    }
}

namespace {
class ParserIEDiscoveryUtTest : public ParserIETest {
  protected:
    class MockTaskApi : public Dic::Module::IE::ServitizationOpenApi {
      public:
        MOCK_METHOD(std::vector<Dic::Module::IE::TaskInfo>, ComputeTaskInfo, (const std::string &path), (override));
    };

    class TestableParserIE : public Dic::Module::ParserIE {
      public:
        void SetApi(const std::shared_ptr<Dic::Module::IE::ServitizationOpenApi> &api) { servitizationOpenApi = api; }
    };

    void SetUp() override {
        api = std::make_shared<testing::StrictMock<MockTaskApi>>();
        parser.SetApi(api);
    }

    std::shared_ptr<testing::StrictMock<MockTaskApi>> api;
    TestableParserIE parser;
};

TEST_F(ParserIEDiscoveryUtTest, DirectFileDoesNotQueryTaskApi) {
    const std::string file = Dic::FileUtil::SplicePath(testing::TempDir(), "ie-discovery-ut.json");
    std::string error;
    EXPECT_CALL(*api, ComputeTaskInfo(testing::_)).Times(0);
    EXPECT_EQ(parser.GetParseFileByImportFile(file, error), (std::vector<std::string>{file}));
    EXPECT_TRUE(error.empty());
}

TEST_F(ParserIEDiscoveryUtTest, EmptyTaskListFallsBackToImportedDirectory) {
    const std::string directory = testing::TempDir();
    std::string error;
    EXPECT_CALL(*api, ComputeTaskInfo(directory)).WillOnce(testing::Return(std::vector<Dic::Module::IE::TaskInfo>{}));
    EXPECT_EQ(parser.GetParseFileByImportFile(directory, error), (std::vector<std::string>{directory}));
    EXPECT_TRUE(error.empty());
}

TEST_F(ParserIEDiscoveryUtTest, TasksInSameDirectoryAreDeduplicated) {
    const std::string directory = testing::TempDir();
    const std::string taskDirectory = Dic::FileUtil::SplicePath(directory, "service-a");
    const std::vector<Dic::Module::IE::TaskInfo> tasks = {
        {"rank0", Dic::FileUtil::SplicePath(taskDirectory, "first.db")},
        {"rank1", Dic::FileUtil::SplicePath(taskDirectory, "second.db")}};
    EXPECT_CALL(*api, ComputeTaskInfo(directory)).WillOnce(testing::Return(tasks));
    std::string error;
    EXPECT_EQ(parser.GetParseFileByImportFile(directory, error), (std::vector<std::string>{taskDirectory}));
    EXPECT_TRUE(error.empty());
}

TEST_F(ParserIEDiscoveryUtTest, TasksInDifferentDirectoriesReturnBothParents) {
    const std::string directory = testing::TempDir();
    const std::string first = Dic::FileUtil::SplicePath(directory, "service-a");
    const std::string second = Dic::FileUtil::SplicePath(directory, "service-b");
    const std::vector<Dic::Module::IE::TaskInfo> tasks = {{"rank0", Dic::FileUtil::SplicePath(second, "trace.db")},
        {"rank1", Dic::FileUtil::SplicePath(first, "trace.db")}};
    EXPECT_CALL(*api, ComputeTaskInfo(directory)).WillOnce(testing::Return(tasks));
    std::string error;
    const auto files = parser.GetParseFileByImportFile(directory, error);
    EXPECT_EQ(files, (std::vector<std::string>{first, second}));
    EXPECT_TRUE(error.empty());
}
} // namespace
