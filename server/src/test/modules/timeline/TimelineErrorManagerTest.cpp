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
#include "TimelineErrorManager.h"

using namespace Dic::Module::Timeline;
class TimelineErrorManagerTest : public testing::Test {
  protected:
    void SetUp() override { Dic::Module::ModuleRequestHandler::ResetRequestContextError(); }
    void TearDown() override { Dic::Module::ModuleRequestHandler::ResetRequestContextError(); }
};

TEST_F(TimelineErrorManagerTest, KnownCodesAndUnknownFallbackHaveStableMessages) {
    EXPECT_EQ(GetErrorMessage(ErrorCode::RESET_ERROR), "");
    EXPECT_EQ(GetErrorMessage(ErrorCode::PARAMS_ERROR), "Request parameter exception");
    EXPECT_EQ(GetErrorMessage(ErrorCode::CONNECT_DATABASE_FAILED), "Failed to connect to database");
    EXPECT_EQ(
        GetErrorMessage(ErrorCode::QUERY_EVENTS_VIEW_DATA_FAILED), "Failed to get events view table response data");
    EXPECT_EQ(GetErrorMessage(static_cast<ErrorCode>(-1)), "Unknown error code");
    EXPECT_EQ(GetErrorMessage(static_cast<ErrorCode>(99999)), "Unknown error code");
    for (const auto &[code, message] : errorMessages) {
        if (code != ErrorCode::RESET_ERROR) {
            EXPECT_FALSE(message.empty());
        }
        EXPECT_NE(GetErrorMessage(code), "Unknown error code");
    }
}

TEST_F(TimelineErrorManagerTest, SetsReplacesAndResetsRequestContextError) {
    Dic::Protocol::Response response("test");
    SetTimelineError(ErrorCode::PARAMS_ERROR);
    Dic::Module::ModuleRequestHandler::SetResponseErrorFromRequestContext(response);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, 1101);
    EXPECT_EQ(response.error->message, "Request parameter exception");
    SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
    Dic::Protocol::Response replacement("test");
    Dic::Module::ModuleRequestHandler::SetResponseErrorFromRequestContext(replacement);
    ASSERT_TRUE(replacement.error.has_value());
    EXPECT_EQ(replacement.error->code, 3001);
    EXPECT_EQ(replacement.error->message, "Failed to connect to database");
    Dic::Module::ModuleRequestHandler::ResetRequestContextError();
    Dic::Protocol::Response clean("test");
    Dic::Module::ModuleRequestHandler::SetResponseErrorFromRequestContext(clean);
    EXPECT_FALSE(clean.error.has_value());
}
