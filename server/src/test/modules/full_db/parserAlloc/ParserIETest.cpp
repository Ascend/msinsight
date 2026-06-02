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
#include "DataBaseManager.h"
#include "JsonFileParserManager.h"
#include "ParserIE.h"
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
