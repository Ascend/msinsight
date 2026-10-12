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
#include "QuerySystemViewFtraceStatHandler.h"

namespace Dic::Module::Timeline::Test {
class QuerySystemViewFtraceStatHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::SystemViewFtraceStatRequest> Request(const std::string &path) {
        auto request = std::make_unique<Protocol::SystemViewFtraceStatRequest>();
        request->fileId = path;
        request->params.current = 1;
        request->params.pageSize = 1;
        request->params.orderBy = "pid";
        request->params.order = "ascend";
        return request;
    }
    void Populate(const std::string &rank) {
        auto database =
            std::dynamic_pointer_cast<TextTraceDatabase>(DataBaseManager::Instance().GetTraceDatabaseByRankId(rank));
        ASSERT_NE(database, nullptr);
        EXPECT_TRUE(database->CreateTraceTaskSummaryTable());
        ASSERT_TRUE(
            database->ExecSql("INSERT INTO trace_task_summary"
                              "(comm,pid,cpu_id,runnable_ns,running_ns,sleeping_ns,cs_count,cs_involuntary_count,"
                              "soft_irq_count,soft_irq_duration,hard_irq_count,hard_irq_duration) VALUES"
                              "('worker',10,2,11,22,33,44,0,55,66,77,88),('idle',20,3,0,0,0,0,0,0,0,0,0);"));
    }
};

TEST_F(QuerySystemViewFtraceStatHandlerTest, ConvertsAllStatisticsFieldsAndPaginates) {
    const auto path = AddDatabase("ftrace");
    ASSERT_FALSE(path.empty());
    Populate("ftrace");
    QuerySystemViewFtraceStatHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request(path)));
    const auto *response = session->Get<Protocol::SystemViewFtraceStatResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->pageParam.total, 2U);
    ASSERT_EQ(response->data.size(), 1U);
    const std::unordered_map<std::string, std::string> expected = {{"comm", "worker"}, {"pid", "10"}, {"cpu", "2"},
        {"runnable", "11"}, {"running", "22"}, {"sleeping", "33"}, {"context_switch_count", "44"},
        {"soft_irq_count", "55"}, {"soft_irq_duration", "66"}, {"hard_irq_count", "77"}, {"hard_irq_duration", "88"}};
    EXPECT_EQ(response->data[0], expected);
    ResetResponse();
    auto second = Request(path);
    second->params.current = 2;
    EXPECT_TRUE(handler.HandleRequest(std::move(second)));
    response = session->Get<Protocol::SystemViewFtraceStatResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->pageParam.current, 2U);
    EXPECT_EQ(response->pageParam.pageSize, 1U);
    ASSERT_EQ(response->data.size(), 1U);
    EXPECT_EQ(response->data[0].at("comm"), "idle");
    EXPECT_EQ(response->data[0].at("running"), "0");
}

TEST_F(QuerySystemViewFtraceStatHandlerTest, EmptyPagePreservesTotalAndInvalidPageReturnsError) {
    const auto path = AddDatabase("ftrace");
    ASSERT_FALSE(path.empty());
    Populate("ftrace");
    QuerySystemViewFtraceStatHandler handler;
    auto request = Request(path);
    request->params.current = 3;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::SystemViewFtraceStatResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->data.empty());
    EXPECT_EQ(response->pageParam.total, 2U);
    ResetResponse();
    request = Request(path);
    request->params.pageSize = 0;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::SystemViewFtraceStatResponse>(ErrorCode::PARAMS_ERROR);
}

TEST_F(QuerySystemViewFtraceStatHandlerTest, RejectsMissingDatabaseAndNonTextDatabase) {
    QuerySystemViewFtraceStatHandler handler;
    EXPECT_FALSE(handler.HandleRequest(Request("missing")));
    ExpectFailure<Protocol::SystemViewFtraceStatResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
    ResetResponse();
    const auto path = AddDatabase("db", DataType::DB);
    ASSERT_FALSE(path.empty());
    EXPECT_FALSE(handler.HandleRequest(Request(path)));
    ExpectFailure<Protocol::SystemViewFtraceStatResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
}
} // namespace Dic::Module::Timeline::Test
