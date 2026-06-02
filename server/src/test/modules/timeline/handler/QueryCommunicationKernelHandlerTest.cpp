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
#include "QueryCommunicationKernelHandler.h"
#include "HandlerTest.cpp"

class QueryCommunicationKernelHandlerTest : HandlerTest {};

TEST_F(HandlerTest, QueryCommunicationKernelHandlerTestParamIsEmpty) {
    Dic::Module::Timeline::QueryCommunicationKernelHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::CommunicationKernelRequest>();
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}
