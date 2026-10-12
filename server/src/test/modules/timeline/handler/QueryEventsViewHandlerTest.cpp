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
#include "QueryEventsViewHandler.h"

namespace Dic::Module::Timeline::Test {
class QueryEventsViewHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::EventsViewRequest> Request() {
        auto request = std::make_unique<Protocol::EventsViewRequest>();
        request->id = 42;
        request->params.rankId = "events";
        request->params.pid = "10";
        request->params.processName = "Python";
        request->params.metaType = "TEXT";
        request->params.currentPage = 1;
        request->params.pageSize = 2;
        return request;
    }
};

TEST_F(QueryEventsViewHandlerTest, ReturnsRowsPaginationAndRelativeTimestamps) {
    ASSERT_FALSE(AddDatabase("events").empty());
    AddSlices("events");
    QueryEventsViewHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *response = session->Get<Protocol::EventsViewResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->requestId, 42U);
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_EQ(response->body.currentPage, 1U);
    EXPECT_EQ(response->body.pageSize, 2U);
    ASSERT_EQ(response->body.eventDetailList.size(), 2U);
    EXPECT_EQ(response->body.eventDetailList[0]->name, "MatMul");
    EXPECT_EQ(response->body.eventDetailList[0]->startTime, 20U);
    EXPECT_EQ(response->body.eventDetailList[0]->duration, 20U);
    EXPECT_EQ(response->body.eventDetailList[1]->name, "MatMulGrad");
    EXPECT_FALSE(response->body.columnList.empty());
}

TEST_F(QueryEventsViewHandlerTest, SelectsExplicitSourceAndRestoresPythonStackThreadId) {
    const auto path = AddDatabase("source");
    ASSERT_FALSE(path.empty());
    AddSlices("source");
    auto request = Request();
    request->params.dbPath = path;
    request->params.tid = "python_stack:text:11";
    request->params.metaType = "PYTORCH_API_PYTHON_STACK";
    request->params.currentPage = 2;
    QueryEventsViewHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::EventsViewResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 3U);
    ASSERT_EQ(response->body.eventDetailList.size(), 1U);
    EXPECT_EQ(response->body.eventDetailList[0]->name, "Add");
    EXPECT_EQ(response->body.eventDetailList[0]->threadId, "11");
}

TEST_F(QueryEventsViewHandlerTest, ReturnsEmptyResultForAnUnmatchedProcess) {
    ASSERT_FALSE(AddDatabase("events").empty());
    AddSlices("events");
    auto request = Request();
    request->params.pid = "missing";
    QueryEventsViewHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::EventsViewResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->body.count, 0U);
    EXPECT_TRUE(response->body.eventDetailList.empty());
}

TEST_F(QueryEventsViewHandlerTest, DistinguishesInvalidParamsMissingConnectionAndQueryFailure) {
    QueryEventsViewHandler handler;
    auto invalid = Request();
    invalid->params.startTime = 2;
    invalid->params.endTime = 1;
    EXPECT_FALSE(handler.HandleRequest(std::move(invalid)));
    ExpectFailure<Protocol::EventsViewResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectFailure<Protocol::EventsViewResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
    ResetResponse();
    ASSERT_FALSE(AddDatabase("events").empty());
    Exec("events", "DROP TABLE slice;");
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectFailure<Protocol::EventsViewResponse>(ErrorCode::QUERY_EVENTS_VIEW_DATA_FAILED);
}
} // namespace Dic::Module::Timeline::Test
