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
#include "FileUtil.h"
#include "ParserStatusManager.h"
#include "ServitizationOpenApi.h"
#include "ServitizationContext.h"
using namespace Dic::Module::IE;
class ServitizationOpenApiTest : public ::testing::Test {
  public:
    void SetUp() override { Dic::Module::Timeline::ParserStatusManager::Instance().NotifyStartParse(); }
};

TEST_F(ServitizationOpenApiTest, InitDataBaseTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(folder, "profiler.db");
    std::string tempErrFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "mmmm.text");
    Dic::Server::ServerLog::Info(tempFileName);
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tempErrFile(tempErrFileName, std::ios::out | std::ios::trunc);
    auto res = openApi->ComputeTaskInfo(folder);
    EXPECT_EQ(res.size(), 1); // 1
    openApi->Reset();
}

TEST_F(ServitizationOpenApiTest, ParseTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "profiler.db");
    std::string tempErrFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "mmmm.text");
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tempErrFile(tempErrFileName, std::ios::out | std::ios::trunc);
    std::unordered_map<std::string, std::string> inputs;
    inputs["lll"] = folder;
    auto res = openApi->Parse(inputs);
    EXPECT_EQ(res, false);
    openApi->Reset();
    inputs["lll"] = tempErrFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
    inputs["lll"] = tempFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
}

TEST_F(ServitizationOpenApiTest, ParseMsTest) {
    std::shared_ptr<ServitizationOpenApi> openApi = std::make_shared<ServitizationOpenApi>();
    std::string folder = Dic::FileUtil::GetCurrPath();
    std::string tempFileName = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "ms_service_parsed.db");
    std::string tempFile1Name = Dic::FileUtil::SplicePath(Dic::FileUtil::GetCurrPath(), "ms_service_111.db");
    std::ofstream tempFile(tempFileName, std::ios::out | std::ios::trunc);
    std::ofstream tem2pFile(tempFile1Name, std::ios::out | std::ios::trunc);
    std::unordered_map<std::string, std::string> inputs;
    inputs["lll"] = folder;
    auto res = openApi->Parse(inputs);
    EXPECT_EQ(res, false);
    openApi->Reset();
    inputs["lll"] = tempFileName;
    res = openApi->Parse(inputs);
    EXPECT_EQ(res, true);
    openApi->Reset();
}
