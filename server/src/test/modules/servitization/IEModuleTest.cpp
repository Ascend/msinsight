/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "IEModule.h"
#include "ProtocolDefs.h"

namespace {
class TestableIEModule : public Dic::Module::IEModule {
  public:
    const std::string &Name() const { return moduleName; }
    const auto &Handlers() const { return requestHandlerMap; }
};
}

TEST(IEModuleTest, RegistersAllRequestHandlers) {
    TestableIEModule module;
    EXPECT_EQ(module.Name(), Dic::Protocol::MODULE_IE);

    module.RegisterRequestHandlers();
    EXPECT_EQ(module.Handlers().size(), 3);
    EXPECT_NE(module.Handlers().find(Dic::Protocol::REQ_RES_IE_VIEW), module.Handlers().end());
    EXPECT_NE(module.Handlers().find(Dic::Protocol::REQ_RES_IE_TABLE_VIEW), module.Handlers().end());
    EXPECT_NE(module.Handlers().find(Dic::Protocol::REQ_RES_IE_DATA_GROUP), module.Handlers().end());

    module.RegisterRequestHandlers();
    EXPECT_EQ(module.Handlers().size(), 3);
}
