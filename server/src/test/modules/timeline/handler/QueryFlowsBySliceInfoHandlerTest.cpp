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
#include "QueryFlowsBySliceInfoHandler.h"
#include "TraceDatabaseHelper.h"

namespace Dic::Module::Timeline::Test {
class QueryFlowsBySliceInfoHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::UnitFlowsRequest> Request() {
        auto request = std::make_unique<Protocol::UnitFlowsRequest>();
        request->id = 17;
        request->params.rankId = "flows";
        request->params.pid = "10";
        request->params.tid = "11";
        request->params.id = "1";
        request->params.startTime = 0;
        request->params.endTime = 200;
        request->params.metaType = "TEXT";
        return request;
    }
};

TEST_F(QueryFlowsBySliceInfoHandlerTest, ReturnsSimulationFlowEndpointsWithRelativeTimeAndDepth) {
    ASSERT_FALSE(AddDatabase("flows").empty());
    AddSlices("flows");
    Exec("flows",
        "UPDATE slice SET flag_id='flag' WHERE id=1;"
        "INSERT INTO thread(track_id,tid,pid,thread_name,thread_sort_index) VALUES(2,'12','10','other',1);"
        "INSERT INTO slice(id,timestamp,duration,end_time,name,depth,track_id,cat,flag_id)"
        "VALUES(4,220,20,240,'Wait',2,2,'','flag');"
        "INSERT INTO flow(flow_id,name,cat,track_id,timestamp,type) VALUES"
        "('flag','dependency','sync',1,140,'s'),('flag','dependency','sync',2,220,'f');");
    auto request = Request();
    request->params.isSimulation = true;
    QueryFlowsBySliceInfoHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::UnitFlowsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->requestId, 17U);
    ASSERT_EQ(response->body.unitAllFlows.size(), 1U);
    const auto &category = response->body.unitAllFlows[0];
    EXPECT_EQ(category.cat, "sync");
    ASSERT_EQ(category.flows.size(), 1U);
    EXPECT_EQ(category.flows[0].id, "flag");
    EXPECT_EQ(category.flows[0].title, "dependency");
    EXPECT_EQ(category.flows[0].from.timestamp, 40U);
    EXPECT_EQ(category.flows[0].to.timestamp, 120U);
    EXPECT_EQ(category.flows[0].from.tid, "11");
    EXPECT_EQ(category.flows[0].to.tid, "12");
    EXPECT_EQ(category.flows[0].to.depth, 2U);
}

TEST_F(QueryFlowsBySliceInfoHandlerTest, EmptyOrdinaryFlowQueryAndExplicitSourceReturnSuccessfulEmptyBody) {
    const auto path = AddDatabase("flows");
    ASSERT_FALSE(path.empty());
    AddSlices("flows");
    auto request = Request();
    request->params.dbPath = path;
    request->params.tid = "python_stack:text:11";
    request->params.metaType = "PYTORCH_API_PYTHON_STACK";
    QueryFlowsBySliceInfoHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::UnitFlowsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_TRUE(response->body.unitAllFlows.empty());
}

TEST_F(QueryFlowsBySliceInfoHandlerTest, RejectsReversedTimeAndMissingExplicitSourceWithDifferentErrors) {
    QueryFlowsBySliceInfoHandler handler;
    auto request = Request();
    request->params.startTime = 201;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::UnitFlowsResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    ASSERT_FALSE(AddDatabase("flows").empty());
    request = Request();
    request->params.dbPath = "missing-source";
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::UnitFlowsResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
}
TEST_F(QueryFlowsBySliceInfoHandlerTest, ReturnsOrdinaryFlowWithSliceIdentityAndRelativeCoordinates) {
    ASSERT_FALSE(AddDatabase("flows").empty());
    AddSlices("flows");
    Exec("flows",
        "UPDATE slice SET duration=100,end_time=220 WHERE id=1;"
        "INSERT INTO flow(flow_id,name,cat,track_id,timestamp,type) VALUES"
        "('ordinary','dependency','queue',1,130,'s'),('ordinary','dependency','queue',1,180,'f');");
    auto request = Request();
    request->params.id = "2";
    request->params.startTime = 50;
    request->params.endTime = 120;
    QueryFlowsBySliceInfoHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::UnitFlowsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    ASSERT_EQ(response->body.unitAllFlows.size(), 1U);
    const auto &category = response->body.unitAllFlows[0];
    EXPECT_EQ(category.cat, "queue");
    ASSERT_EQ(category.flows.size(), 1U);
    const auto &flow = category.flows[0];
    EXPECT_EQ(flow.cat, "queue");
    EXPECT_EQ(flow.from.id, "1");
    EXPECT_EQ(flow.to.id, "2");
    EXPECT_EQ(flow.from.pid, "10");
    EXPECT_EQ(flow.to.pid, "10");
    EXPECT_EQ(flow.from.rankId, "flows");
    EXPECT_EQ(flow.to.rankId, "flows");
    EXPECT_EQ(flow.from.duration, 100U);
    EXPECT_EQ(flow.to.duration, 30U);
    EXPECT_EQ(flow.from.timestamp, 30U);
    EXPECT_EQ(flow.to.timestamp, 80U);
    EXPECT_EQ(flow.from.tid, "11");
    EXPECT_EQ(flow.to.tid, "11");
    EXPECT_EQ(flow.from.metaType, "TEXT");
    EXPECT_EQ(flow.to.metaType, "TEXT");
    EXPECT_EQ(flow.from.depth, 0U);
    EXPECT_EQ(flow.to.depth, 1U);
}

TEST_F(QueryFlowsBySliceInfoHandlerTest, MapsDatabaseExceptionToUnitFlowErrorResponse) {
    const auto path = AddDatabase("flows", DataType::DB);
    ASSERT_FALSE(path.empty());
    auto request = Request();
    request->params.dbPath = path;
    request->params.metaType = "Ascend Hardware";
    {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
        ASSERT_NE(std::dynamic_pointer_cast<DbTraceDataBase>(database), nullptr);
        // Opening a DB trace creates an empty temporary TASK table; remove it to force SQL preparation failure.
        ASSERT_TRUE(database->ExecSql("DROP TABLE TASK;"));
        auto statement = database->CreatPreparedStatement();
        ASSERT_THROW(TraceDatabaseHelper::QueryConnectionId(statement, request->params), DatabaseException);
    }
    QueryFlowsBySliceInfoHandler handler;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::UnitFlowsResponse>(ErrorCode::QUERY_UNIT_FLOWS_FAILED);
    const auto *response = session->Get<Protocol::UnitFlowsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->requestId, 17U);
    EXPECT_TRUE(response->body.unitAllFlows.empty());
}
} // namespace Dic::Module::Timeline::Test
