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
#include "BaselineManagerService.h"
#include "DataBaseManager.h"
#include "QueryOpCategoryInfoHandler.h"
#include "QueryOpDetailInfoHandler.h"
#include "ParamsParser.h"
#include "ProjectExplorerManager.h"
#include "FileUtil.h"
#include "../../../FullDbTestSuit.cpp"
#include "../../../TestSuit.h"

using namespace Dic::Server;
using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::Global;
class OperatorDetailRequestHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() { SetBaseLineManager(); }

    static void TearDownTestSuite() { ClearProjectExplorerData(); }

    static void InitBaseLineManager() {
        ProjectExplorerManager::Instance().InitSystemMemoryDbPath(testDataDir);
        InitProjectExplorerData();
    }

    static bool SetBaseLineManager() {
        InitBaseLineManager();
        // 创建DB场景的baseline基线manager
        std::string filePathText = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0", "ASCEND_PROFILER_OUTPUT");
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
        return result;
    }

    static void ClearProjectExplorerData() {
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProject", std::vector<std::string>());
        ProjectExplorerManager::Instance().DeleteProjectAndFilePath("testProjectDb", std::vector<std::string>());
    }

  protected:
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
    inline static int retry = 2;
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

    static void InitProjectExplorerData() {
        std::string filePathText = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0", "ASCEND_PROFILER_OUTPUT");
        std::string filePathDb = Dic::FileUtil::SplicePath(testDataDir, "full_db", "ascend_pytorch_profiler.db");
        std::vector<ProjectExplorerInfo> infos;
        std::vector<std::string> parseFileList{filePathText};
        ProjectExplorerInfo info = CreateProjectData(
            "testProject", "projectFilePath", "import", Dic::ProjectTypeEnum::TEXT_CLUSTER, parseFileList);
        infos.push_back(info);
        std::for_each(infos.begin(), infos.end(),
            [](const auto &item) { ProjectExplorerManager::Instance().SaveProjectExplorer(item, false); });

        std::vector<ProjectExplorerInfo> dbInfos;
        std::vector<std::string> parseDbFileList{filePathDb};
        ProjectExplorerInfo dbInfo = CreateProjectData(
            "testProjectDb", "projectFilePathDb", "import", Dic::ProjectTypeEnum::DB, parseDbFileList);
        dbInfos.push_back(dbInfo);
        std::for_each(dbInfos.begin(), dbInfos.end(),
            [](const auto &item) { ProjectExplorerManager::Instance().SaveProjectExplorer(item, false); });
    }
};

TEST_F(OperatorDetailRequestHandlerTest, QueryOpDetailInfoHandlerFailedWhenBsesLineIsSetAndCompareIsTrue) {
    Dic::Module::Operator::QueryOpDetailInfoHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::OperatorDetailInfoRequest>();
    requestPtr->params.rankId = "1";
    requestPtr->params.group = "Operator";
    // topK给一个极大值
    requestPtr->params.topK = 10000000; // 10000000表示topK是一个极大值
    requestPtr->params.isCompare = true;
    requestPtr->params.orderBy = "count";
    requestPtr->params.order = "descend";
    // 10 表示分页最小是10条
    requestPtr->params.pageSize = 10;
    requestPtr->params.current = 1;

    EXPECT_TRUE(SetBaseLineManager());
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
}
