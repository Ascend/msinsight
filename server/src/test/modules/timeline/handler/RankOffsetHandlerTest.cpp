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

#include "TimelineHandlerTestSupport.h"
#include "RankOffsetHandler.h"

namespace Dic::Module::Timeline::Test {
class RankOffsetHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::RankOffsetRequest> Request() {
        auto request = std::make_unique<Protocol::RankOffsetRequest>();
        request->params.rankId = "base";
        request->params.fileId = "base-file";
        request->params.pid = "10";
        request->params.sliceName = "MatMul";
        request->params.metaType = "TEXT";
        request->params.alignType = "LEFT";
        request->params.startTime = 120;
        request->params.duration = 20;
        return request;
    }
    void ExpectBodyError(const std::string &message) {
        const auto *response = session->Get<Protocol::RankOffsetResponse>();
        ASSERT_NE(response, nullptr);
        EXPECT_FALSE(response->result);
        EXPECT_EQ(response->body.error.code, 1);
        EXPECT_NE(response->body.error.message.find(message), std::string::npos);
        EXPECT_TRUE(response->body.successList.empty());
    }
};

TEST_F(RankOffsetHandlerTest, AlignsLeftAndRightAnchorsAcrossRanks) {
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    ASSERT_FALSE(AddDatabase("target").empty());
    AddSlices("target");
    Exec("target", "UPDATE slice SET timestamp=170,duration=30,end_time=200 WHERE id=1;");
    RankOffsetHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *response = session->Get<Protocol::RankOffsetResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    ASSERT_EQ(response->body.successList.size(), 1U);
    EXPECT_EQ(response->body.successList[0].rankId, "target");
    EXPECT_EQ(response->body.successList[0].offset, 50);
    EXPECT_EQ(response->body.baseOffset, 0U);
    ResetResponse();
    auto right = Request();
    right->params.alignType = "RIGHT";
    EXPECT_TRUE(handler.HandleRequest(std::move(right)));
    response = session->Get<Protocol::RankOffsetResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->body.successList.size(), 1U);
    EXPECT_EQ(response->body.successList[0].offset, 60);
}

TEST_F(RankOffsetHandlerTest, UsesSortedOrdinalAndReportsPartialFailure) {
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    Exec("base", "UPDATE slice SET name='MatMul' WHERE id=2;");
    ASSERT_FALSE(AddDatabase("target").empty());
    AddSlices("target");
    Exec("target",
        "UPDATE slice SET name='MatMul',timestamp=100,duration=30,end_time=130 WHERE id=2;"
        "UPDATE slice SET timestamp=300,duration=40,end_time=340 WHERE id=1;");
    ASSERT_FALSE(AddDatabase("short").empty());
    AddSlices("short");
    auto request = Request();
    request->params.startTime = 180;
    request->params.duration = 30;
    RankOffsetHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::RankOffsetResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    ASSERT_EQ(response->body.successList.size(), 1U);
    EXPECT_EQ(response->body.successList[0].rankId, "target");
    EXPECT_EQ(response->body.successList[0].offset, 120);
    EXPECT_EQ(response->body.error.code, 1);
    EXPECT_NE(response->body.error.message.find("short"), std::string::npos);
    EXPECT_NE(response->body.error.message.find("Insufficient slices"), std::string::npos);
}

TEST_F(RankOffsetHandlerTest, RejectsUnsupportedMetaTypeMissingBaseAndUnknownSelectedSlice) {
    RankOffsetHandler handler;
    auto request = Request();
    request->params.metaType = "unsupported";
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectBodyError("Unsupported metaType");
    ResetResponse();
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectBodyError("Base rank database not found");
    ResetResponse();
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    request = Request();
    request->params.startTime = 999;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectBodyError("Selected slice not found");
}

TEST_F(RankOffsetHandlerTest, RejectsInvalidParametersNoTargetsAndAllMissingTargetSlices) {
    RankOffsetHandler handler;
    auto request = Request();
    request->params.alignType = "TOP";
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectBodyError("alignType");
    ResetResponse();
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectBodyError("No target ranks");
    ResetResponse();
    ASSERT_FALSE(AddDatabase("empty").empty());
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectBodyError("Slice not found");
}
TEST_F(RankOffsetHandlerTest, SortsTheBaseBeforeSelectingOrdinalAndAddsExistingBaseOffset) {
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    Exec("base",
        "UPDATE slice SET timestamp=180,duration=30,end_time=210 WHERE id=1;"
        "UPDATE slice SET name='MatMul',timestamp=120,duration=20,end_time=140 WHERE id=2;");
    ASSERT_FALSE(AddDatabase("target").empty());
    AddSlices("target");
    Exec("target",
        "UPDATE slice SET timestamp=100,end_time=120 WHERE id=1;"
        "UPDATE slice SET name='MatMul',timestamp=250,duration=40,end_time=290 WHERE id=2;");
    TraceTime::Instance().UpdateCardTimeDuration("base", 150, 1000);
    auto request = Request();
    request->params.startTime = 180;
    request->params.duration = 30;
    RankOffsetHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::RankOffsetResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->body.baseOffset, 50U);
    ASSERT_EQ(response->body.successList.size(), 1U);
    EXPECT_EQ(response->body.successList[0].rankId, "target");
    EXPECT_EQ(response->body.successList[0].offset, 120);
    EXPECT_EQ(response->body.error.code, 0);
}

TEST_F(RankOffsetHandlerTest, ReportsBaseSqlFailureBeforeProcessingOtherRanks) {
    ASSERT_FALSE(AddDatabase("base").empty());
    AddSlices("base");
    ASSERT_FALSE(AddDatabase("target").empty());
    AddSlices("target");
    Exec("base", "DROP TABLE slice;");
    RankOffsetHandler handler;
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectBodyError("Failed to query base slices from database.");
}
} // namespace Dic::Module::Timeline::Test
