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
#include "ProjectAnalyze.h"
#include "ProjectParserBin.h"
#include "ProjectParserDb.h"
#include "ProjectParserJson.h"

using namespace Dic::Module;
using namespace Dic;

class ProjectAnalyzeTest : public testing::Test {};

TEST_F(ProjectAnalyzeTest, ProjectAnalyzeRegister) {
    ProjectAnalyzeRegister<ProjectParserBin> reg(ParserType::BIN);
    ProjectAnalyzeRegister<ProjectParserDb> reg2(ParserType::DB);
    ProjectAnalyzeRegister<ProjectParserJson> reg3(ParserType::JSON);
    ProjectAnalyzeRegister<ProjectParserJson> reg4(ParserType::ACLGRPAH_DEBUG_JSON);
    std::vector<ParserType> types = {
        ParserType::BIN, ParserType::DB, ParserType::JSON, ParserType::ACLGRPAH_DEBUG_JSON};
    for (auto type : types) {
        ProjectExplorerInfo projectInfo;
        ProjectAnalyze::Instance().ProjectExportInfoBuild(type, {"test"}, projectInfo);
    }
}
