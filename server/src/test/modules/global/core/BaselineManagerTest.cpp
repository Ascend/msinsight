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

#include <algorithm>
#include <gtest/gtest.h>
#include "BaselineManager.h"
#include "BaselineManagerService.h"
#include "ProjectExplorerManager.h"
#include "DataBaseManager.h"
#include "TrackInfoManager.h"
#include "ParserStatusManager.h"
#include "TestSuit.h"
#include "FileUtil.h"

using namespace Dic::Module::Global;
class BaselineManagerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        auto dataEngine = Dic::Module::Timeline::DataEngine::Instance();
        dataEngine->SetRepositoryFactory(Dic::Module::Timeline::RepositoryFactory::Instance());
        Dic::Module::Timeline::RenderEngine::Instance()->SetDataEngineInterface(dataEngine);
        std::string systemDbPath = TestSuit::GetTestDataFile();
        ProjectExplorerManager::Instance().InitSystemMemoryDbPath(systemDbPath);
        InitProjectExplorerData();
    }

    static void TearDownTestSuite() { ClearProjectExplorerData(); }

  protected:
    std::vector<std::string> temporaryProjects;

    void TearDown() override {
        if (temporaryProjects.empty()) {
            return;
        }
        BaselineManagerService::ResetBaseline(true);
        Dic::Module::Timeline::DataBaseManager::Instance().Clear();
        for (const auto &projectName : temporaryProjects) {
            ClearTemporaryProject(projectName);
        }
    }

    static void ClearTemporaryProject(const std::string &projectName) {
        auto &manager = ProjectExplorerManager::Instance();
        if (!manager.QueryProjectExplorer(projectName, {}).empty()) {
            manager.ClearProjectExplorer({projectName});
        }
    }

    bool SaveTemporaryProject(const ProjectExplorerInfo &info) {
        if (std::find(temporaryProjects.begin(), temporaryProjects.end(), info.projectName) ==
            temporaryProjects.end()) {
            ClearTemporaryProject(info.projectName);
            temporaryProjects.push_back(info.projectName);
        }
        return ProjectExplorerManager::Instance().SaveProjectExplorer(info, false);
    }

    static BaselineSettingRequest CreateBaselineRequest(
        const std::string &baselineProject, const std::string &currentProject, const std::string &filePath) {
        BaselineSettingRequest request;
        request.projectName = currentProject;
        request.params.projectName = baselineProject;
        request.params.filePath = filePath;
        request.params.currentClusterPath = COMPARE;
        return request;
    }

    static void ExpectSingleDbBaseline(const BaselineSettingRequest &request) {
        BaselineInfo baselineInfo;
        baselineInfo.parsedFilePath = request.params.filePath;
        EXPECT_TRUE(BaselineManagerService::InitBaselineData(request, baselineInfo));
        EXPECT_TRUE(baselineInfo.errorMessage.empty()) << baselineInfo.errorMessage;
        EXPECT_FALSE(baselineInfo.isCluster);
        EXPECT_EQ(baselineInfo.fileId, request.params.filePath);
        EXPECT_FALSE(baselineInfo.rankId.empty());
        EXPECT_FALSE(baselineInfo.cardId.empty());
        EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.cardId);
        EXPECT_NE(
            Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(baselineInfo.cardId), nullptr);
    }

    static void ExpectBaselineRejected(const BaselineSettingRequest &request) {
        BaselineInfo baselineInfo;
        baselineInfo.parsedFilePath = request.params.filePath;
        EXPECT_TRUE(BaselineManagerService::InitBaselineData(request, baselineInfo));
        EXPECT_FALSE(baselineInfo.errorMessage.empty());
        EXPECT_TRUE(BaselineManager::Instance().GetBaselineId().empty());
    }

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
    Dic::Module::Timeline::ParserStatusManager::Instance().WaitAllFinished(
        {BaselineManager::Instance().GetBaselineId()});
    EXPECT_TRUE(result);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.rankId);
    Dic::Module::Timeline::DataBaseManager::Instance().Clear();
}

TEST_F(BaselineManagerTest, TestBaselineCardIdFallback) {
    BaselineInfo baselineInfo;
    baselineInfo.rankId = "rank0";
    BaselineManager::Instance().SetBaselineInfo(baselineInfo);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.rankId);

    baselineInfo.cardId = "Baseline_rank0";
    BaselineManager::Instance().SetBaselineInfo(baselineInfo);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.cardId);
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
    Dic::Module::Timeline::ParserStatusManager::Instance().WaitAllFinished(
        {BaselineManager::Instance().GetBaselineId()});
    EXPECT_TRUE(result);
    EXPECT_EQ(baselineInfo.rankId.find("Baseline_"), std::string::npos);
    EXPECT_EQ(baselineInfo.cardId.find("Baseline_"), 0);
    EXPECT_NE(
        Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(baselineInfo.cardId), nullptr);
    auto rankList = Dic::Module::Timeline::TrackInfoManager::Instance().GetRankListByFileId(
        baselineInfo.fileId, baselineInfo.cardId);
    ASSERT_FALSE(rankList.empty());
    EXPECT_EQ(rankList[0].rankId, baselineInfo.cardId);
    auto trackId = Dic::Module::Timeline::TrackInfoManager::Instance().GetTrackId(
        baselineInfo.cardId, "Ascend Hardware", rankList[0].deviceId);
    Dic::Module::Timeline::TrackInfo trackInfo;
    ASSERT_TRUE(
        Dic::Module::Timeline::TrackInfoManager::Instance().GetTrackInfo(trackId, trackInfo, baselineInfo.cardId));
    EXPECT_EQ(trackInfo.rankId, rankList[0].rankName);
    EXPECT_EQ(trackInfo.deviceId, rankList[0].deviceId);
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.cardId);
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
    EXPECT_FALSE(baselineInfo.errorMessage.empty());
    EXPECT_TRUE(BaselineManager::Instance().GetBaselineId().empty());
}

TEST_F(BaselineManagerTest, AppendedDbCardUsesSelectedImport) {
    const std::string projectName = "baselineAppendedDb";
    const std::string filePath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    const std::string firstPath = TestSuit::GetTestDataFile("full_db", "0_original_import");
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, firstPath, "import", Dic::ProjectTypeEnum::DB, {firstPath})));
    ASSERT_TRUE(
        SaveTemporaryProject(CreateProjectData(projectName, filePath, "import", Dic::ProjectTypeEnum::DB, {filePath})));
    const auto projectInfos = ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {});
    ASSERT_EQ(projectInfos.size(), 2);
    ASSERT_NE(projectInfos.front().fileName, filePath);

    ExpectSingleDbBaseline(CreateBaselineRequest(projectName, projectName, filePath));
}

TEST_F(BaselineManagerTest, AppendedDbCardInClusterProjectRemainsSingleCard) {
    const std::string projectName = "baselineClusterWithAppendedDb";
    const std::string filePath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    const std::string firstPath = TestSuit::GetTestDataFile("full_db", "0_original_cluster");
    ASSERT_TRUE(
        SaveTemporaryProject(CreateMultiClusterProject(projectName, firstPath, Dic::ProjectTypeEnum::DB_CLUSTER, 1)));
    ASSERT_TRUE(
        SaveTemporaryProject(CreateProjectData(projectName, filePath, "import", Dic::ProjectTypeEnum::DB, {filePath})));
    ASSERT_EQ(ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {}).size(), 2);

    ExpectSingleDbBaseline(CreateBaselineRequest(projectName, projectName, filePath));
}

TEST_F(BaselineManagerTest, CurrentProjectWithMultipleDbImportsSupportsBaseline) {
    const std::string currentProject = "baselineCurrentWithMultipleImports";
    const std::string filePath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(currentProject, "firstImport", "import", Dic::ProjectTypeEnum::DB, {"firstRank"})));
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(currentProject, "secondImport", "import", Dic::ProjectTypeEnum::DB, {"secondRank"})));
    ASSERT_EQ(ProjectExplorerManager::Instance().QueryProjectExplorer(currentProject, {}).size(), 2);

    ExpectSingleDbBaseline(CreateBaselineRequest("testProjectDb", currentProject, filePath));
}

TEST_F(BaselineManagerTest, AppendedTraceCardInTextClusterProjectRemainsSingleCard) {
    const std::string projectName = "baselineTextClusterWithAppendedTrace";
    const std::string filePath = TestSuit::GetTestDataFile("test_rank_0", "ASCEND_PROFILER_OUTPUT");
    ASSERT_TRUE(SaveTemporaryProject(
        CreateMultiClusterProject(projectName, "textClusterImport", Dic::ProjectTypeEnum::TEXT_CLUSTER, 1)));
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, filePath, "import", Dic::ProjectTypeEnum::TRACE, {filePath})));
    ASSERT_EQ(ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {}).size(), 2);

    const auto request = CreateBaselineRequest(projectName, projectName, filePath);
    BaselineInfo baselineInfo;
    baselineInfo.parsedFilePath = filePath;
    EXPECT_TRUE(BaselineManagerService::InitBaselineData(request, baselineInfo));
    EXPECT_TRUE(baselineInfo.errorMessage.empty()) << baselineInfo.errorMessage;
    EXPECT_FALSE(baselineInfo.isCluster);
    EXPECT_EQ(baselineInfo.fileId, Dic::FileUtil::GetDbPath(Dic::FileUtil::SplicePath(filePath, "trace_view.json")));
    EXPECT_FALSE(baselineInfo.rankId.empty());
    EXPECT_EQ(BaselineManager::Instance().GetBaselineId(), baselineInfo.rankId);
}

TEST_F(BaselineManagerTest, MultipleImportsStillRejectMixedTypes) {
    const std::string projectName = "baselineMixedTypes";
    const std::string filePath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    ASSERT_TRUE(
        SaveTemporaryProject(CreateProjectData(projectName, filePath, "import", Dic::ProjectTypeEnum::DB, {filePath})));
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, "traceImport", "import", Dic::ProjectTypeEnum::TRACE, {"traceRank"})));

    ExpectBaselineRejected(CreateBaselineRequest(projectName, "testProjectDb", filePath));
    ExpectBaselineRejected(CreateBaselineRequest("testProjectDb", projectName, filePath));
}

TEST_F(BaselineManagerTest, OverlappingImportsRejectAmbiguousBaselinePath) {
    const std::string projectName = "baselineOverlappingImports";
    const std::string filePath = "sharedRank";
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, "firstImport", "import", Dic::ProjectTypeEnum::DB, {filePath})));
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, "secondImport", "import", Dic::ProjectTypeEnum::DB, {filePath})));

    ExpectBaselineRejected(CreateBaselineRequest(projectName, projectName, filePath));
}

TEST_F(BaselineManagerTest, MultipleImportsStillRejectMemScope) {
    const std::string projectName = "baselineWithMemScope";
    const std::string filePath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
    ASSERT_TRUE(
        SaveTemporaryProject(CreateProjectData(projectName, filePath, "import", Dic::ProjectTypeEnum::DB, {filePath})));
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, "memScopeImport", "import", Dic::ProjectTypeEnum::DB, {"memscope_dump_1.db"})));

    ExpectBaselineRejected(CreateBaselineRequest(projectName, "testProjectDb", filePath));
    ExpectBaselineRejected(CreateBaselineRequest("testProjectDb", projectName, filePath));
}

TEST_F(BaselineManagerTest, AppendedMultiDeviceImportRemainsUnsupported) {
    const std::string projectName = "baselineAppendedMultiDevice";
    const std::string filePath = "multiDeviceRank";
    ASSERT_TRUE(SaveTemporaryProject(
        CreateProjectData(projectName, "firstImport", "import", Dic::ProjectTypeEnum::DB, {"firstRank"})));
    auto multiDeviceInfo = CreateProjectData(projectName, "secondImport", "import", Dic::ProjectTypeEnum::DB, {});
    for (const auto &deviceId : {"0", "1"}) {
        auto deviceInfo = std::make_shared<ParseFileInfo>();
        deviceInfo->parseFilePath = filePath;
        deviceInfo->subId = filePath + deviceId;
        deviceInfo->type = ParseFileType::RANK;
        deviceInfo->deviceId = deviceId;
        multiDeviceInfo.AddSubParseFileInfo(deviceInfo);
    }
    ASSERT_TRUE(SaveTemporaryProject(multiDeviceInfo));

    ExpectBaselineRejected(CreateBaselineRequest(projectName, projectName, filePath));
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
