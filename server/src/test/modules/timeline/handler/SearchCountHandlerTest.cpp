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
#include "SearchCountHandler.h"
#include "HandlerTest.cpp"

class SearchCountHandlerTest : HandlerTest {};

TEST_F(HandlerTest, SearchCountHandlerReturnsZeroForMissingDatabase) {
    ResponseCapture capture;
    Dic::Module::Timeline::SearchCountHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::SearchCountRequest>();
    requestPtr->params.rankId = "timeline-ut-missing-database";
    requestPtr->id = 73;
    const auto command = requestPtr->command;
    EXPECT_TRUE(handler.HandleRequest(std::move(requestPtr)));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    const auto &response = *capture.session->responses.front();
    EXPECT_EQ(response.requestId, 73u);
    EXPECT_EQ(response.command, command);
    EXPECT_TRUE(response.result);
    const auto *count = dynamic_cast<const Dic::Protocol::SearchCountResponse *>(&response);
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->body.totalCount, 0u);
    ASSERT_EQ(count->body.countList.size(), 1u);
    EXPECT_EQ(count->body.countList.front().rankId, "timeline-ut-missing-database");
    EXPECT_EQ(count->body.countList.front().count, 0u);
}
