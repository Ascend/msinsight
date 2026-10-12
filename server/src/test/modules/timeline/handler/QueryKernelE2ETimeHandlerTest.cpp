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
#include "QueryKernelE2ETimeHandler.h"

namespace Dic::Module::Timeline::Test {
class QueryKernelE2ETimeHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::KernelE2ETimeRequest> Request(const std::string &path) {
        auto request = std::make_unique<Protocol::KernelE2ETimeRequest>();
        request->fileId = path;
        request->id = 23;
        request->params.rankId = "e2e";
        request->params.startTime = 0;
        request->params.endTime = 400;
        request->params.current = 1;
        request->params.pageSize = 10;
        return request;
    }
    void AddCompleteAclopChain() {
        Exec("e2e",
            "INSERT INTO process(pid,process_name) VALUES('1','Python'),('2','CANN'),('3','Ascend Hardware');"
            "INSERT INTO thread(track_id,tid,pid) VALUES(1,'11','1'),(2,'22','2'),(3,'33','3');"
            "INSERT INTO slice(id,timestamp,duration,end_time,name,depth,track_id,cat) VALUES"
            "(1,100,160,260,'<built-in method add>',0,1,'python_function'),"
            "(2,140,20,160,'Enqueue',1,1,''),(3,220,140,360,'Dequeue',0,1,''),"
            "(4,240,110,350,'AscendCL@aclopCompileAndExecute',0,2,''),"
            "(5,300,40,340,'Node@launch',1,2,''),(6,350,50,400,'AddKernel',0,3,'');"
            "INSERT INTO flow(flow_id,name,cat,track_id,timestamp,type) VALUES"
            "('queue','queue','async_task_queue',1,140,'s'),('queue','queue','async_task_queue',1,220,'f'),"
            "('launch','launch','HostToDevice',2,300,'s'),('launch','launch','HostToDevice',3,350,'f');");
    }
};

TEST_F(QueryKernelE2ETimeHandlerTest, ReturnsRecoveredChainDurationsAndHighlightCoordinates) {
    const auto path = AddDatabase("e2e");
    ASSERT_FALSE(path.empty());
    AddCompleteAclopChain();
    QueryKernelE2ETimeHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request(path)));
    const auto *response = session->Get<Protocol::KernelE2ETimeResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->requestId, 23U);
    EXPECT_EQ(response->body.totalCount, 1U);
    ASSERT_EQ(response->body.records.size(), 1U);
    const auto &record = response->body.records[0];
    EXPECT_EQ(record.opName, "<built-in method add>");
    EXPECT_EQ(record.pathType, "ACLOP");
    EXPECT_EQ(record.status, "normal");
    ASSERT_TRUE(record.prepareTime.has_value());
    EXPECT_EQ(*record.prepareTime, 40U);
    ASSERT_TRUE(record.queueTime.has_value());
    EXPECT_EQ(*record.queueTime, 60U);
    ASSERT_TRUE(record.endToEndTime.has_value());
    EXPECT_EQ(*record.endToEndTime, 240U);
    EXPECT_EQ(response->body.normalCount, 1U);
    EXPECT_EQ(response->body.incompleteCount, 0U);
    ASSERT_EQ(record.highlightSlices.size(), 6U);
    EXPECT_EQ(record.highlightSlices[0].startTime, 0U);
    EXPECT_EQ(record.highlightSlices[0].role, "PYTHON_CALL");
    EXPECT_EQ(record.highlightSlices[0].tid, "python_stack:text:11");
    EXPECT_EQ(record.highlightSlices[5].role, "HARDWARE_TASK");
    EXPECT_EQ(record.highlightSlices[5].startTime, 250U);
    EXPECT_EQ(record.highlightSlices[5].duration, 50U);
}

TEST_F(QueryKernelE2ETimeHandlerTest, ReturnsEmptyResultAndFiltersExistingChains) {
    const auto path = AddDatabase("e2e");
    ASSERT_FALSE(path.empty());
    QueryKernelE2ETimeHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request(path)));
    const auto *empty = session->Get<Protocol::KernelE2ETimeResponse>();
    ASSERT_NE(empty, nullptr);
    EXPECT_TRUE(empty->result);
    EXPECT_EQ(empty->body.totalCount, 0U);
    EXPECT_TRUE(empty->body.records.empty());
    ResetResponse();
    AddCompleteAclopChain();
    auto request = Request(path);
    request->params.endTime = 401; // A new query identity invalidates the prior empty cache.
    request->params.opName = "missing operator";
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *filtered = session->Get<Protocol::KernelE2ETimeResponse>();
    ASSERT_NE(filtered, nullptr);
    EXPECT_EQ(filtered->body.totalCount, 0U);
    EXPECT_TRUE(filtered->body.records.empty());
}

TEST_F(QueryKernelE2ETimeHandlerTest, RejectsWrongTypeMissingDatabaseAndInvalidParameters) {
    QueryKernelE2ETimeHandler handler;
    EXPECT_FALSE(handler.HandleRequest(std::make_unique<Protocol::EventsViewRequest>()));
    ExpectFailure<Protocol::KernelE2ETimeResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    EXPECT_FALSE(handler.HandleRequest(Request("missing")));
    ExpectFailure<Protocol::KernelE2ETimeResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
    ResetResponse();
    const auto path = AddDatabase("e2e");
    ASSERT_FALSE(path.empty());
    auto invalid = Request(path);
    invalid->params.startTime = 401;
    EXPECT_FALSE(handler.HandleRequest(std::move(invalid)));
    ExpectFailure<Protocol::KernelE2ETimeResponse>(ErrorCode::PARAMS_ERROR);
}
} // namespace Dic::Module::Timeline::Test
