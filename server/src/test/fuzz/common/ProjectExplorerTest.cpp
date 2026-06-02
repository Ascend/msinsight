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

#include <fstream>
#include "FuzzDefs.h"
#include "FuzzFileUtil.h"
#include "UpdateProjectExplorerInfoHandler.h"
#include "JsonUtil.h"

using namespace Dic::Module;

TEST(TestProjectExplorer, GetProjectExplorerInfoHandler) {
    char testApi[] = "test_get_project_explorer_info";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        auto request = std::make_unique<Dic::Module::ProjectExplorerInfoGetRequest>();
        Dic::Module::GetProjectExplorerInfoHandler handler;
        bool result = handler.HandleRequest(std::move(request));
        EXPECT_EQ(result, true);
    }
    DT_FUZZ_END()
}

TEST(TestProjectExplorer, UpdateProjectExplorerInfoHandler) {
    char testApi[] = "test_update_project_explorer_info";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        char *newProjectName = DT_SetGetString(&g_Element[0], 5, UINT32_MAX, "path");
        char *oldProjectName = DT_SetGetString(&g_Element[0], 5, UINT32_MAX, "path");
        auto requestPtr = std::make_unique<ProjectExplorerInfoUpdateRequest>();
        requestPtr->params.newProjectName = newProjectName;
        requestPtr->params.oldProjectName = oldProjectName;
        UpdateProjectExplorerInfoHandler updateProjectExplorerInfoHandler;
        updateProjectExplorerInfoHandler.HandleRequest(std::move(requestPtr));
    }
    DT_FUZZ_END()
}
