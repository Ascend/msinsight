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
#include "../../timeline/handler/HandlerTest.cpp"
#include "GlobalProtocolRequest.h"
#include "GetProjectExplorerInfoHandler.h"
#include "SetBaselineHandler.h"
#include "GetModuleConfigHandler.h"
#include "ClearProjectExplorerHandler.h"

using namespace Dic::Module;
TEST_F(HandlerTest, GetProjectExplorerInfoWithEmptyResult) {
    auto request = std::make_unique<Dic::Module::ProjectExplorerInfoGetRequest>();
    Dic::Module::GetProjectExplorerInfoHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, SetBaselineHandler) {
    auto request = std::make_unique<BaselineSettingRequest>();
    request->params.baselineClusterPath = "COMPARE";
    request->params.projectName = "test";
    request->params.filePath = "COMPARE";
    request->params.currentClusterPath = "Baseline";
    request->fileId = "COMPARE";
    SetBaselineHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, GetModuleConfigHandler) {
    auto request = std::make_unique<BaselineSettingRequest>();
    GetModuleConfigHandler handler;
    EXPECT_EQ(handler.HandleRequest(std::move(request)), true);
}

TEST_F(HandlerTest, ClearProjectExplorerHandler) {
    auto request = std::make_unique<ProjectExplorerInfoClearRequest>();
    ClearProjectExplorerHandler handler;
    EXPECT_EQ(handler.HandleRequest(std::move(request)), true);
}
