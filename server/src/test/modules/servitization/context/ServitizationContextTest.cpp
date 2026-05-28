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
#include "IEContext.h"
#include "FileUtil.h"
#include "ServitizationContext.h"
using namespace Dic::Module::IE;
class ServitizationContextTest : public ::testing::Test {
    void SetUp() override { IEContext::Instance().Reset(); }

    void TearDown() override { IEContext::Instance().Reset(); }
};

TEST_F(ServitizationContextTest, InitDataBaseTest) {
    std::shared_ptr<ServitizationContext> context = std::make_shared<ServitizationContext>();
    auto res = context->InitDataBase("llllllll", ":memory:");
    EXPECT_EQ(res, true);
    auto res2 = context->InitDataBase("llllllll", ":memory:");
    EXPECT_EQ(res2, false);
}

TEST_F(ServitizationContextTest, ComputeFileIdByFolderTest) {
    std::shared_ptr<ServitizationContext> context = std::make_shared<ServitizationContext>();
    std::string folder = R"(/src/test/test_data)";
    auto res = context->ComputeFileIdByFolder(folder);
    EXPECT_EQ(res, "test_data_0");
    auto res2 = context->ComputeFileIdByFolder(folder);
    EXPECT_EQ(res2, "test_data_0");
}

TEST_F(ServitizationContextTest, GetDatabaseTest) {
    std::shared_ptr<ServitizationContext> context = std::make_shared<ServitizationContext>();
    auto res = context->GetDatabase("kk");
    EXPECT_EQ(res, nullptr);
}

TEST_F(ServitizationContextTest, ExecuteScriptTest) {
    std::shared_ptr<ServitizationContext> context = std::make_shared<ServitizationContext>();
    auto res = context->ExecuteScript("mmmmmm", "lll");
    context->Reset();
    EXPECT_EQ(res, false);
}
