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
#include "QueryMemcpyDetailHandler.h"
#include "HandlerTest.cpp"
#include "DataBaseManager.h"

using namespace Dic::Module::Timeline;
TEST_F(HandlerTest, QueryMemcpyDetailHandlerTestNormal) {
    Dic::Module::Timeline::QueryMemcpyDetailHandler handler;
    auto requestPtr = std::make_unique<Dic::Protocol::SystemViewOverallMoreDetailsRequest>();
    requestPtr->params.rankId = "0";
    DataBaseManager::Instance().CreateTraceConnectionPool("0", "test");
    handler.HandleRequest(std::move(requestPtr));
    DataBaseManager::Instance().Clear();
}
