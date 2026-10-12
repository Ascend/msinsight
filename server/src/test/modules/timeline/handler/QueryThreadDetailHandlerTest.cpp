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
#include "QueryThreadDetailHandler.h"
#include "HandlerTest.cpp"

class QueryThreadDetailHandlerTest : HandlerTest {};

TEST_F(HandlerTest, QueryThreadDetailHandlerRejectsMissingRenderEngine) {
    ResponseCapture capture;
    Dic::Module::Timeline::QueryThreadDetailHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::ThreadDetailRequest>();
    requestPtr->id = 73;
    const auto command = requestPtr->command;
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    const auto &response = *capture.session->responses.front();
    EXPECT_EQ(response.requestId, 73u);
    EXPECT_EQ(response.command, command);
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Timeline::ErrorCode::QUERY_THREAD_DETAIL_FAILED));
    EXPECT_FALSE(response.error->message.empty());
}

TEST_F(HandlerTest, QueryThreadDetailHandlerTestIdInvalid) {
    Dic::Module::Timeline::QueryThreadDetailHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::ThreadDetailRequest>();
    requestPtr->params.id = " where 1=1; DROP TABLES;";
    EXPECT_EQ(handler.HandleRequest(std::move(requestPtr)), false);
}
