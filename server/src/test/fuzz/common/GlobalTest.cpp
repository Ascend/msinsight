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

#include <string>
#include <gtest/gtest.h>
#include "../FuzzDefs.h"
#include "CheckProjectValidHandler.h"
#include "SetBaselineHandler.h"

using namespace Dic;
using namespace Module;

TEST(GlobalTest, CheckProjectValidHandler) {
    char testApi[] = "test_check_project_valid";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        char *fuzzProjectName = DT_SetGetString(&g_Element[0], 18, UINT64_MAX, "/home/pytorchdata");
        auto requestPtr = std::make_unique<Dic::Protocol::ProjectCheckValidRequest>();
        requestPtr->params.projectName = fuzzProjectName;
        Dic::Module::CheckProjectValidHandler handler;
        handler.HandleRequest(std::move(requestPtr));
    }
    DT_FUZZ_END()
}

TEST(GlobalTest, SetBaselineHandler) {
    char testApi[] = "test_set_baseline";
    DT_FUZZ_START(0, FUZZ_RUN_TIMES, testApi, 0) {
        char *fuzzProjectName = DT_SetGetString(&g_Element[0], 18, UINT64_MAX, "/home/pytorchdata");
        char *fuzzFilePath = DT_SetGetString(&g_Element[0], 18, UINT64_MAX, "/home/data");
        auto requestPtr = std::make_unique<Dic::Protocol::BaselineSettingRequest>();
        requestPtr->params.projectName = fuzzProjectName;
        requestPtr->params.filePath = fuzzFilePath;
        Dic::Module::SetBaselineHandler handler;
        handler.HandleRequest(std::move(requestPtr));
    }
    DT_FUZZ_END()
}
