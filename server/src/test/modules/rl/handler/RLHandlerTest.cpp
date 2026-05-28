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
#include "RLProtocolRequest.h"
#include "RLPipelineHandler.h"
#include "RLModule.h"
#include "../../timeline/handler/HandlerTest.cpp"

using namespace Dic::Module;

// 强化学习流水线接口请求成功
TEST_F(HandlerTest, RLHandlerSuccess) {
    auto request = std::make_unique<RLPipelineRequest>();
    Dic::Module::RL::RLPipelineHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, RLHandlerOnRequest) {
    auto module = RLModule();
    auto request = std::make_unique<Request>(std::string_view("unknown"));
    EXPECT_NO_THROW(module.OnRequest(std::move(request)));
}
