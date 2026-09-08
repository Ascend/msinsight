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
#include "DbPlatformDataBase.h"
#include "HandlerTest.cpp"
#include "ParseCardsHandler.h"
#include "ParserStatusManager.h"

class ParseCardsHandlerTest : public HandlerTest {};

TEST_F(ParseCardsHandlerTest, ParseCardsHandlerTestCardFilePathIsEmpty) {
    Dic::Module::Timeline::ParseCardsHandler parseCardsHandler;
    auto requestPtr = std::make_unique<Dic::Protocol::ParseCardsRequest>();
    requestPtr->params.cards = {"card1", "card2"};
    requestPtr->params.fileIds = {"fileId1", "fileId2"};
    Module::Timeline::ParserStatusManager::Instance().SetPendingStatus(
        "card1", std::make_pair<ProjectTypeEnum, std::vector<std::string>>(ProjectTypeEnum::DB, {}));
    Module::Timeline::ParserStatusManager::Instance().SetPendingStatus("card2",
        std::make_pair<ProjectTypeEnum, std::vector<std::string>>(ProjectTypeEnum::TRACE, {"invalid filePath"}));
    EXPECT_EQ(parseCardsHandler.HandleRequest(std::move(requestPtr)), true);
}

TEST_F(ParseCardsHandlerTest, EmbeddedPlatformCardStartsPendingTraceDatabaseParse) {
    auto &statusManager = Module::Timeline::ParserStatusManager::Instance();
    statusManager.ClearAllParserStatus();
    const std::string traceRankId = "rank0";
    const std::string platformRankId = Dic::Module::FullDb::BuildEmbeddedPlatformRankId(traceRankId);
    statusManager.SetPendingStatus(traceRankId, {ProjectTypeEnum::DB, {"missing.db"}});

    auto requestPtr = std::make_unique<Dic::Protocol::ParseCardsRequest>();
    requestPtr->params.cards = {platformRankId};
    requestPtr->params.fileIds = {"missing.db"};

    Dic::Module::Timeline::ParseCardsHandler parseCardsHandler;
    EXPECT_TRUE(parseCardsHandler.HandleRequest(std::move(requestPtr)));
    EXPECT_EQ(statusManager.GetParserStatus(traceRankId), Module::Timeline::ParserStatus::INIT);
    EXPECT_EQ(statusManager.GetParserStatus(Dic::Module::FullDb::BuildEmbeddedPlatformRankId(platformRankId)),
        Module::Timeline::ParserStatus::UN_KNOW);
    statusManager.ClearAllParserStatus();
}
