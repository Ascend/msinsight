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
#include "ProtocolDefs.h"
#include "GlobalProtocolRequest.h"
#include "CancelBaselineHandler.h"
#include "FilesGetHandler.h"
#include "CheckProjectValidHandler.h"
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
