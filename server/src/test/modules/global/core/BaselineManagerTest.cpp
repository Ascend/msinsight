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
#include "BaselineManager.h"
#include "BaselineManagerService.h"
#include "ProjectExplorerManager.h"
#include "DataBaseManager.h"
#include "ParserStatusManager.h"
#include "TestSuit.h"
#include "FileUtil.h"

using namespace Dic::Module::Global;
class BaselineManagerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string systemDbPath = TestSuit::GetTestDataFile();
        ProjectExplorerManager::Instance().InitSystemMemoryDbPath(systemDbPath);
        InitProjectExplorerData();
    }

    static void TearDownTestSuite() { ClearProjectExplorerData(); }

  protected:
    inline static int retry = 5;
    static ProjectExplorerInfo CreateProjectData(const std::string &projectName, const std::string &fileName,
        const std::string &importType, Dic::ProjectTypeEnum projectType, const std::vector<std::string> parseFileList) {
        ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = fileName;
        info.importType = importType;
        info.projectType = static_cast<int64_t>(projectType);
        for (const auto &item : parseFileList) {
            auto parseFileInfo = std::make_shared<ParseFileInfo>();
            parseFileInfo->parseFilePath = item;
            parseFileInfo->subId = item;
            parseFileInfo->type = ParseFileType::RANK;
            info.AddSubParseFileInfo(parseFileInfo);
        }
        return info;
    }

    static ProjectExplorerInfo CreateMultiClusterProject(const std::string &projectName, const std::string &fileName,
        Dic::ProjectTypeEnum projectType, int clusterCount) {
        ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = fileName;
        info.importType = "import";
        info.projectType = static_cast<int>(projectType);
        for (int i = 0; i < clusterCount; i++) {
            auto cluster = std::make_shared<ParseFileInfo>();
            std::string clusterName = "cluster_" + std::to_string(i);
            cluster->parseFilePath = clusterName;
            cluster->clusterId = clusterName;
            cluster->type = ParseFileType::CLUSTER;
            cluster->subId = Dic::FileUtil::SplicePath(projectName, clusterName);
            info.AddSubParseFileInfo(cluster);
        }
        return info;
    }

    static void InitProjectExplorerData() {
        std::string filePathText = TestSuit::GetTestDataFile("test_rank_0", "ASCEND_PROFILER_OUTPUT");
        std::string filePathDb = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
        std::vector<ProjectExplorerInfo> infos;
        std::vector<std::string> parseFileList{filePathText};
        ProjectExplorerInfo info = CreateProjectData(
            "testProject", "projectFilePath", "import", Dic::ProjectTypeEnum::TEXT_CLUSTER, parseFileList);
        ProjectExplorerManager::Instance().SaveProjectExplorer(info, false);

        std::vector<std::string> parseDbFileList{filePathDb};
        ProjectExplorerInfo dbInfo = CreateProjectData(
            "testProjectDb", "projectFilePathDb", "import", Dic::ProjectTypeEnum::DB, parseDbFileList);
        ProjectExplorerManager::Instance().SaveProjectExplorer(dbInfo, false);
        ProjectExplorerInfo multiClusterInfo =
            CreateMultiClusterProject("multiCluster", "projectFilePath", Dic::ProjectTypeEnum::DB_CLUSTER,
                4); // generator 4 cluster
        ProjectExplorerManager::Instance().SaveProjectExplorer(multiClusterInfo, false);
    }

    static void ClearProjectExplorerData() {
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProject", std::vector<std::string>());
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProjectDb", std::vector<std::string>());
    }
};

// 测试text数据baseline设置正常情况
TEST_F(BaselineManagerTest, TestText) {
    std::string filePathText = TestSuit::GetTestDataFile("test_rank_0", "ASCEND_PROFILER_OUTPUT");
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePathText;
    BaselineSettingRequest request;
    request.projectName = "testProject";
    request.params.projectName = "testProject";
    request.params.filePath = filePathText;
    request.params.currentClusterPath = COMPARE;
    bool result = BaselineManagerService::InitBaselineData(request, baselineInfo);
    std::string notFinishTask = "";
    int index = 0;
    while (index < retry && !Dic::Module::Timeline::ParserStatusManager::Instance().IsAllFinished(notFinishTask)) {
        const int sleepTime = 2000;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        index++;
    }
    EXPECT_TRUE(result);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.rankId);
    Dic::Module::Timeline::DataBaseManager::Instance().Clear();
}

// 测试db数据baseline设置正常情况
TEST_F(BaselineManagerTest, TestDb) {
    std::string filePathDb = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePathDb;
    BaselineSettingRequest request;
    request.projectName = "testProjectDb";
    request.params.projectName = "testProjectDb";
    request.params.filePath = filePathDb;
    request.params.currentClusterPath = COMPARE;
    bool result = BaselineManagerService::InitBaselineData(request, baselineInfo);
    std::string notFinishTask = "";
    int index = 0;
    while (index < retry && !Dic::Module::Timeline::ParserStatusManager::Instance().IsAllFinished(notFinishTask)) {
        const int sleepTime = 2000;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        index++;
    }
    EXPECT_TRUE(result);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.rankId);
    Dic::Module::Timeline::DataBaseManager::Instance().Clear();
}

// 测试db不存在的场景
TEST_F(BaselineManagerTest, TestFileNotExist) {
    std::string filePathDb = "noData";
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePathDb;
    BaselineSettingRequest request;
    request.projectName = "testProjectDb";
    request.params.projectName = "testProjectDb";
    request.params.filePath = filePathDb;
    request.params.currentClusterPath = COMPARE;
    bool result = BaselineManagerService::InitBaselineData(request, baselineInfo);
    EXPECT_TRUE(result);
    EXPECT_EQ(baselineInfo.errorMessage, "");
}

TEST_F(BaselineManagerTest, SetGetBaselineClusterPath) {
    BaselineManager::Instance().SetBaselineClusterPath("baseline");
    EXPECT_EQ(BaselineManager::Instance().GetBaseLineClusterPath(), "baseline");
}

TEST_F(BaselineManagerTest, GetCompareClusterPath) {
    BaselineManager::Instance().SetCompareClusterPath("compare");
    EXPECT_EQ(BaselineManager::Instance().GetCompareClusterPath(), "compare");
}

TEST_F(BaselineManagerTest, MultiCluster) {
    std::string filePath = "cluster_2";
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePath;
    BaselineSettingRequest request;
    request.projectName = "multiCluster";
    request.params.projectName = "multiCluster";
    request.params.filePath = filePath;
    request.params.currentClusterPath = COMPARE;
    bool result = BaselineManagerService::InitBaselineData(request, baselineInfo);
    EXPECT_TRUE(result);
    EXPECT_EQ(baselineInfo.isCluster, true);
}
