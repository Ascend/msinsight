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
#include "QuerySystemViewHandler.h"
#include "RankOverlapStore.h"
#include "SystemViewDatabaseResolver.h"
#include "HandlerTest.cpp"

class QuerySystemViewHandlerTest : public HandlerTest {
  protected:
    void TearDown() override { Dic::Module::Timeline::RankOverlapStore::Instance().Clear(); }
};

TEST_F(HandlerTest, QuerySystemViewHandlerRejectsInvalidLayer) {
    ResponseCapture capture;
    Dic::Module::Timeline::QuerySystemViewHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::SystemViewRequest>();
    requestPtr->id = 73;
    const auto command = requestPtr->command;
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    const auto &response = *capture.session->responses.front();
    EXPECT_EQ(response.requestId, 73u);
    EXPECT_EQ(response.command, command);
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Timeline::ErrorCode::PARAMS_ERROR));
    EXPECT_FALSE(response.error->message.empty());
}

TEST_F(QuerySystemViewHandlerTest, OverlapAnalysisUsesDerivedRankDatabase) {
    auto &store = Dic::Module::Timeline::RankOverlapStore::Instance();
    const std::string source = store.Publish("system-view-rank", "0", {{10, 20, 0}});
    ASSERT_FALSE(source.empty());
    Dic::Protocol::SystemViewParams params;
    params.rankId = "system-view-rank";
    params.dbPath = "representative.db";
    params.layer = "Overlap Analysis";

    auto database = Dic::Module::Timeline::ResolveSystemViewDatabase(params);

    ASSERT_NE(database, nullptr);
    EXPECT_EQ(database->GetDbPath(), source);
}
