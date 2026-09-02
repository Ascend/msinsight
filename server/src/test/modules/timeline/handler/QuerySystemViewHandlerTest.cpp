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

TEST_F(HandlerTest, QuerySystemViewHandlerTestNormal) {
    Dic::Module::Timeline::QuerySystemViewHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::SystemViewRequest>();
    handler.HandleRequest(std::move(requestPtr));
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
