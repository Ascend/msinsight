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
#include "SourceModule.h"
#include "ProtocolDefs.h"
#include "SourceProtocolRequest.h"

using namespace Dic::Module;

class ComputeSourceModuleTest : public ::testing::Test {};

class SourceModuleTemp : public SourceModule {
  public:
    std::string GetModuleName() { return moduleName; }

    std::map<std::string, std::unique_ptr<ModuleRequestHandler>> &GetRegister() { return requestHandlerMap; }
};

TEST_F(ComputeSourceModuleTest, ConstructorAndDestructorTest) {
    SourceModuleTemp module;
    EXPECT_EQ(Dic::Protocol::MODULE_SOURCE, module.GetModuleName());
}

TEST_F(ComputeSourceModuleTest, RegisterRequestHandlersTest) {
    SourceModuleTemp module;
    module.RegisterRequestHandlers();
    EXPECT_TRUE(!module.GetRegister().empty());
    EXPECT_TRUE(module.GetRegister().find(Dic::Protocol::REQ_RES_SOURCE_CODE_FILE) != module.GetRegister().end());
}

TEST_F(ComputeSourceModuleTest, OnRequestTest) {
    SourceModuleTemp module;
    auto requestPtr = std::make_unique<Dic::Protocol::SourceApiInstrRequest>();
    module.OnRequest(std::move(requestPtr));
}
