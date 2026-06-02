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
#include "SearchSliceHandler.h"
#include "HandlerTest.cpp"

class SearchSliceHandlerTest : HandlerTest {};

TEST_F(HandlerTest, SearchSliceHandlerTestNormal) {
    Dic::Module::Timeline::SearchSliceHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::SearchSliceRequest>();
    EXPECT_EQ(handler.HandleRequest(std::move(requestPtr)), false);
}

TEST_F(HandlerTest, SearchSliceHandlerTestInvalidParam) {
    Dic::Module::Timeline::SearchSliceHandler handler;
    std::unique_ptr<Dic::Protocol::SearchSliceRequest> requestPtr =
        std::make_unique<Dic::Protocol::SearchSliceRequest>();
    requestPtr->params.metadataList.emplace_back(Dic::Protocol::Metadata{.lockStartTime = 2, .lockEndTime = 1});
    EXPECT_EQ(handler.HandleRequest(std::move(requestPtr)), false);
}
