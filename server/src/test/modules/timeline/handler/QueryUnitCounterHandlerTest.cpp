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
#include "QueryUnitCounterHandler.h"
#include "DataBaseManager.h"
#include "HandlerTest.cpp"

namespace Dic::Protocol {
using namespace Dic::Module::Timeline;
}

class QueryUnitCounterHandlerTest : HandlerTest {};

TEST_F(HandlerTest, QueryUnitCounterHandlerTestNormal) {
    Dic::Module::Timeline::QueryUnitCounterHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::UnitCounterRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(HandlerTest, QueryUnitCounterHandlerTestPlatformRouting) {
    Dic::Protocol::DataBaseManager::Instance().Clear();
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PLATFORM, ":memory:platform_test");
    std::string rankId = "platform_test_rank";
    Dic::Protocol::DataBaseManager::Instance().CreatePlatformDataBase(rankId, ":memory:platform_test");

    Dic::Module::Timeline::QueryUnitCounterHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::UnitCounterRequest>();
    requestPtr->params.rankId = rankId;
    requestPtr->params.threadName = "cpu_usage";
    requestPtr->params.threadId = "0";
    handler.HandleRequest(std::move(requestPtr));

    Dic::Protocol::DataBaseManager::Instance().Clear();
}
