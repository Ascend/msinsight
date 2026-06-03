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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include "FileUtil.h"
#include "ProjectExplorerManager.h"
#include "GlobalDefs.h"
#include "ProjectParserJson.h"
#include "TestSuit.h"

const int64_t NUMBER_ZERO = 0;
const int64_t NUMBER_THREE = 3;
const int64_t NUMBER_FIVE = 5;
const int64_t NUMBER_SIX = 6;
using namespace Dic::Module::Global;
class ProjectExplorerManagerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() { ProjectExplorerManager::Instance().InitSystemMemoryDbPath(testDataDir); }

    static void TearDownTestSuite() {}

  protected:
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
    std::vector<std::string> tempFiles_;

    void TearDown() override {
        for (const auto &path : tempFiles_) {
            std::remove(path.c_str());
        }
        tempFiles_.clear();
    }

    std::string CreateTempJsonFile(const std::string &content) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string uniqueName = std::string(testInfo->name()) + "_" + std::to_string(std::rand()) + ".json";
        std::string path = Dic::FileUtil::SplicePath(::testing::TempDir(), uniqueName);
        std::ofstream file(path);
        if (!file.is_open()) {
            return "";
        }
        file << content;
        file.close();
        tempFiles_.push_back(path);
        return path;
    }

    ProjectExplorerInfo CreateProjectData(const std::string &projectName, const std::string &fileName,
        const std::string &importType, Dic::ProjectTypeEnum projectType, const std::vector<std::string> dbPath) {
        ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = fileName;
        info.importType = importType;
        info.projectType = static_cast<int64_t>(projectType);
        info.dbPath = dbPath;
        info.accessTime = "2000-01-01 00:00:00.000";
        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = "test";
        parseFileInfo->subId = "test";
        parseFileInfo->type = ParseFileType::RANK;
        info.AddSubParseFileInfo(parseFileInfo);
        return info;
    }
    void InitProjectExplorerData() {
        std::vector<ProjectExplorerInfo> infos;
        ProjectExplorerInfo info = CreateProjectData(
            "testProject", "projectFilePath", "import", Dic::ProjectTypeEnum::TEXT_CLUSTER, std::vector<std::string>());
        infos.push_back(info);
        std::for_each(infos.begin(), infos.end(),
            [](const auto &item) { ProjectExplorerManager::Instance().SaveProjectExplorer(item, false); });
    }

    void ClearProjectExplorerData() {
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProject", std::vector<std::string>());
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("newTestProject", std::vector<std::string>());
    }
};

// 更新项目名成功
TEST_F(ProjectExplorerManagerTest, UpdateProjectNameSuccess) {
    InitProjectExplorerData();
    std::string projectName = "testProject";
    std::string newProjectName = "newTestProject";
    bool updateRes = ProjectExplorerManager::Instance().UpdateProjectName(projectName, newProjectName);
    EXPECT_EQ(updateRes, true);
    std::vector<ProjectExplorerInfo> res =
        ProjectExplorerManager::Instance().QueryProjectExplorer(newProjectName, std::vector<std::string>());
    EXPECT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].importType, "import");
    EXPECT_EQ(res[0].fileName, "projectFilePath");
    EXPECT_EQ(static_cast<Dic::ProjectTypeEnum>(res[0].projectType), Dic::ProjectTypeEnum::TEXT_CLUSTER);
    ClearProjectExplorerData();
}

// 同一个项目下导入文件，存在冲突，并覆盖文件
TEST_F(ProjectExplorerManagerTest, CheckProjectConflictAndCoverData) {
    std::string projectName = "testProject";
    InitProjectExplorerData();
    std::vector<std::string> filePathList;
    Dic::Protocol::ProjectCheckBody body;
    std::string filePath = TestSuit::GetTestDataFile("data.bin");
    filePathList.push_back(filePath);
    Dic::Protocol::ProjectErrorType result =
        ProjectExplorerManager::Instance().CheckProjectConflict(projectName, filePathList[0]);
    bool isConflict = (result == Dic::Protocol::ProjectErrorType::PROJECT_NAME_CONFLICT);
    EXPECT_EQ(isConflict, true);
    std::vector<ProjectExplorerInfo> infos;
    ProjectExplorerInfo info = CreateProjectData(
        projectName, filePathList[0], "import", Dic::ProjectTypeEnum::BIN, std::vector<std::string>());
    infos.push_back(info);
    std::for_each(infos.begin(), infos.end(), [](const auto &item) {
        bool res = ProjectExplorerManager::Instance().SaveProjectExplorer(item, false);
        EXPECT_EQ(res, true);
    });
    std::vector<ProjectExplorerInfo> queryRes =
        ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, std::vector<std::string>());
    EXPECT_EQ(queryRes.size(), 2); // expect size 2
    EXPECT_EQ(queryRes[0].importType, "import");
    EXPECT_EQ(queryRes[0].fileName, filePath);
    EXPECT_EQ(static_cast<Dic::ProjectTypeEnum>(queryRes[0].projectType), Dic::ProjectTypeEnum::BIN);
    ClearProjectExplorerData();
}

TEST_F(ProjectExplorerManagerTest, CheckProjectConflictAllowFtraceJsonWithProfilingDb) {
    ClearProjectExplorerData();
    std::string projectName = "testProject";
    ProjectExplorerInfo info =
        CreateProjectData(projectName, "msprof_1.db", "import", Dic::ProjectTypeEnum::DB, std::vector<std::string>());
    ASSERT_TRUE(ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));

    std::string ftraceJsonPath = CreateTempJsonFile(R"([{"ph": "X", "pid": "Process Scheduling"}])");
    ASSERT_FALSE(ftraceJsonPath.empty());
    Dic::Protocol::ProjectErrorType result =
        ProjectExplorerManager::Instance().CheckProjectConflict(projectName, ftraceJsonPath);
    EXPECT_EQ(result, Dic::Protocol::ProjectErrorType::NO_ERRORS);
    ClearProjectExplorerData();
}

TEST_F(ProjectExplorerManagerTest, CheckProjectConflictAllowFtraceDbWithProfilingText) {
    ClearProjectExplorerData();
    std::string projectName = "testProject";
    ProjectExplorerInfo info = CreateProjectData(
        projectName, "trace_view.json", "import", Dic::ProjectTypeEnum::TRACE, std::vector<std::string>());
    ASSERT_TRUE(ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));

    std::string ftraceDbPath = TestSuit::GetTestDataFile("ftrace_data.db");
    Dic::Protocol::ProjectErrorType result =
        ProjectExplorerManager::Instance().CheckProjectConflict(projectName, ftraceDbPath);
    EXPECT_EQ(result, Dic::Protocol::ProjectErrorType::NO_ERRORS);
    ClearProjectExplorerData();
}

TEST_F(ProjectExplorerManagerTest, CheckProjectConflictAllowFtraceDbWithProfilingDb) {
    ClearProjectExplorerData();
    std::string projectName = "testProject";
    ProjectExplorerInfo info =
        CreateProjectData(projectName, "msprof_1.db", "import", Dic::ProjectTypeEnum::DB, std::vector<std::string>());
    ASSERT_TRUE(ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));

    std::string ftraceDbPath = TestSuit::GetTestDataFile("ftrace_data.db");
    Dic::Protocol::ProjectErrorType result =
        ProjectExplorerManager::Instance().CheckProjectConflict(projectName, ftraceDbPath);
    EXPECT_EQ(result, Dic::Protocol::ProjectErrorType::NO_ERRORS);
    ClearProjectExplorerData();
}

TEST_F(ProjectExplorerManagerTest, CheckProjectConflictKeepsProfilingJsonAndDbConflict) {
    ClearProjectExplorerData();
    std::string projectName = "testProject";
    ProjectExplorerInfo info =
        CreateProjectData(projectName, "msprof_1.db", "import", Dic::ProjectTypeEnum::DB, std::vector<std::string>());
    ASSERT_TRUE(ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));

    std::string profilingJsonPath = CreateTempJsonFile(R"([{"ph": "X", "pid": "Other Scheduling"}])");
    ASSERT_FALSE(profilingJsonPath.empty());
    Dic::Protocol::ProjectErrorType result =
        ProjectExplorerManager::Instance().CheckProjectConflict(projectName, profilingJsonPath);
    EXPECT_EQ(result, Dic::Protocol::ProjectErrorType::PROJECT_NAME_CONFLICT);
    ClearProjectExplorerData();
}

// 清空项目内容
TEST_F(ProjectExplorerManagerTest, ClearProjectExplorerSuccess) {
    ProjectExplorerManager::Instance().ClearProjectExplorer(std::vector<std::string>{});
    InitProjectExplorerData();
    bool result = ProjectExplorerManager::Instance().ClearProjectExplorer(std::vector<std::string>{});
    EXPECT_EQ(result, true);
    std::vector<ProjectExplorerInfo> queryRes =
        ProjectExplorerManager::Instance().QueryProjectExplorer("", std::vector<std::string>());
    EXPECT_EQ(queryRes.size(), 0);
}

TEST_F(ProjectExplorerManagerTest, GetProjectTypeWithOnlyMulitTextType) {
    std::vector<ProjectExplorerInfo> projectInfoList;
    ProjectExplorerInfo info1;
    info1.projectType = NUMBER_FIVE;
    ProjectExplorerInfo info2;
    info2.projectType = NUMBER_THREE;
    projectInfoList.push_back(info1);
    projectInfoList.push_back(info2);
    Dic::ProjectTypeEnum type = ProjectExplorerManager::GetProjectType(projectInfoList);
    EXPECT_EQ(type, Dic::ProjectTypeEnum::TEXT_CLUSTER);
}

TEST_F(ProjectExplorerManagerTest, GetProjectTypeWithOnlyMulitDbType) {
    std::vector<ProjectExplorerInfo> projectInfoList;
    ProjectExplorerInfo info1;
    info1.projectType = NUMBER_ZERO;
    ProjectExplorerInfo info2;
    info2.projectType = NUMBER_SIX;
    projectInfoList.push_back(info1);
    projectInfoList.push_back(info2);
    Dic::ProjectTypeEnum type = ProjectExplorerManager::GetProjectType(projectInfoList);
    EXPECT_EQ(type, Dic::ProjectTypeEnum::DB_CLUSTER);
}

TEST_F(ProjectExplorerManagerTest, DeleteFileNode) {
    std::string importPath = "test_rank_1";
    ProjectExplorerInfo projectInfo;
    projectInfo.fileName = importPath;
    projectInfo.projectName = "testDeleteFile";
    projectInfo.id = 1;
    std::string parsedFilePath = Dic::FileUtil::SplicePath(importPath, "ASCEND_PROFILER_OUTPUT");
    Dic::Module::ProjectParserJson::BuildProjectExploreInfo(projectInfo, {parsedFilePath});
    ProjectExplorerManager::Instance().SaveProjectExplorer(projectInfo, false);
    ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testDeleteFile", {parsedFilePath});
    auto result = ProjectExplorerManager::Instance().QueryProjectExplorer("testDeleteFile", {});
    EXPECT_EQ(result.size(), 0);
}
