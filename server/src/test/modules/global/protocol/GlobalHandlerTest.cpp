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
#include <fstream>
#include "ProtocolDefs.h"
#include "GlobalProtocolRequest.h"
#include "CancelBaselineHandler.h"
#include "FilesGetHandler.h"
#define private public
#include "CheckProjectValidHandler.h"
#undef private
#include "ClearProjectExplorerHandler.h"
#include "DeleteProjectExplorerInfoHandler.h"
#include "GetModuleConfigHandler.h"
#include "HeartCheckHandler.h"
#include "UpdateProjectExplorerInfoHandler.h"

#include "WsSession.h"
#include "ServerDefs.h"
#include "TestSuit.h"
#include "WsSessionManager.h"
#include "WsSessionImpl.h"
#include "FileUtil.h"

using namespace Dic::Server;
using namespace Dic::Module::Global;
using namespace Dic::Module;
using namespace Dic::Protocol;
class GlobalHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

  protected:
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
};

TEST_F(GlobalHandlerTest, TestCancelBaselineHandler) {
    std::unique_ptr<Request> requestPtr = std::make_unique<BaselineCancelRequest>();
    CancelBaselineHandler cancelBaselineHandler;
    ASSERT_TRUE(cancelBaselineHandler.HandleRequest(std::move(requestPtr)));
}

TEST_F(GlobalHandlerTest, TestCheckProjectValidHandler) {
    auto requestPtr = std::make_unique<ProjectCheckValidRequest>();
    requestPtr->params.projectName = "";
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0");
    requestPtr->params.dataPath = {path};
    CheckProjectValidHandler checkProjectValidHandler;
    ASSERT_TRUE(checkProjectValidHandler.HandleRequest(std::move(requestPtr)));
}

#ifndef _WIN32
TEST_F(GlobalHandlerTest, TestCheckProjectValidHandlerCollectsPathSecurityErrorsByLayer) {
    const std::string deleteFolderPath = Dic::FileUtil::SplicePath(
        Dic::FileUtil::GetCurrPath(), "../../../../test/data/check_project_valid_security_test");
    const std::string nestedFolderPath = Dic::FileUtil::SplicePath(deleteFolderPath, "nested");
    const std::string removeCommand = "rm -rf " + deleteFolderPath;
    system(removeCommand.c_str());
    const std::string mkdirCommand = "mkdir -p " + nestedFolderPath;
    system(mkdirCommand.c_str());

    const std::string safeFile = Dic::FileUtil::SplicePath(deleteFolderPath, "trace_view.json");
    const std::string firstLayerInvalidFile = Dic::FileUtil::SplicePath(deleteFolderPath, "bad\nfile.json");
    const std::string secondLayerInvalidFile = Dic::FileUtil::SplicePath(nestedFolderPath, "bad\tfile.json");
    std::ofstream outfile;
    outfile.open(safeFile, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(firstLayerInvalidFile, std::ios::out | std::ios::trunc);
    outfile.close();
    outfile.open(secondLayerInvalidFile, std::ios::out | std::ios::trunc);
    outfile.close();

    ProjectCheckParams params;
    params.projectName = "test";
    params.dataPath = {deleteFolderPath};
    ProjectErrorType error = ProjectErrorType::NO_ERRORS;
    std::vector<ProjectCheckBody::ErrorDetail> errors;

    EXPECT_FALSE(CheckProjectValidHandler::CheckRequestParamsValid(params, error, errors));
    ASSERT_EQ(2, errors.size());
    EXPECT_EQ(ProjectErrorType::IS_UNSAFE_PATH, error);
    EXPECT_EQ(1, errors[0].layer);
    EXPECT_EQ(2, errors[1].layer);
    EXPECT_EQ(static_cast<int>(ProjectErrorType::IS_UNSAFE_PATH), errors[0].error);
    EXPECT_EQ(Dic::StringUtil::GetPrintAbleString(firstLayerInvalidFile), errors[0].path);
    EXPECT_EQ(Dic::StringUtil::GetPrintAbleString(secondLayerInvalidFile), errors[1].path);

    system(removeCommand.c_str());
}
#endif

TEST_F(GlobalHandlerTest, TestDeleteProjectExplorerHandler) {
    auto requestPtr = std::make_unique<ProjectExplorerInfoDeleteRequest>();
    requestPtr->params.projectName = "";
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0");
    requestPtr->params.dataPath = {path};
    DeleteProjectExplorerInfoHandler deleteProjectExplorerInfoHandler;
    ASSERT_TRUE(deleteProjectExplorerInfoHandler.HandleRequest(std::move(requestPtr)));
}

TEST_F(GlobalHandlerTest, TestFilesNotExistGetHandler) {
    auto requestPtr = std::make_unique<FilesGetRequest>();
    requestPtr->params.path = "";
    FilesGetHandler filesGetHandler;
    ASSERT_TRUE(filesGetHandler.HandleRequest(std::move(requestPtr)));
}

TEST_F(GlobalHandlerTest, TestCheckfilesGetHandler) {
    auto requestPtr = std::make_unique<FilesGetRequest>();
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0");
    requestPtr->params.path = path;
    FilesGetHandler filesGetHandler;
    ASSERT_TRUE(filesGetHandler.HandleRequest(std::move(requestPtr)));
}

TEST_F(GlobalHandlerTest, TestHandleRequest) {
    auto requestPtr = std::make_unique<HeartCheckRequest>();
    HeartCheckHandler heartCheckHandler;
    ASSERT_TRUE(heartCheckHandler.HandleRequest(std::move(requestPtr)));
}

TEST_F(GlobalHandlerTest, TestUpdateProjectExplorerInfoHandler) {
    auto requestPtr = std::make_unique<ProjectExplorerInfoUpdateRequest>();
    requestPtr->params.newProjectName = "";
    requestPtr->params.oldProjectName = "";
    UpdateProjectExplorerInfoHandler updateProjectExplorerInfoHandler;
    ASSERT_FALSE(updateProjectExplorerInfoHandler.HandleRequest(std::move(requestPtr)));
}
