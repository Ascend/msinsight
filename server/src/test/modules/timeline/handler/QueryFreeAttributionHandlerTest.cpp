/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include "TimelineHandlerTestSupport.h"
#include "QueryFreeAttributionHandler.h"

namespace Dic::Module::Timeline::Test {
class QueryFreeAttributionHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::FreeAttributionSummaryRequest> Summary() {
        auto request = std::make_unique<Protocol::FreeAttributionSummaryRequest>();
        request->id = 71;
        request->params.rankId = "free-rank";
        request->params.dbPath = "missing.db";
        return request;
    }
};

TEST_F(QueryFreeAttributionHandlerTest, RejectsWrongRequestTypesAndNullRequests) {
    QueryFreeAttributionSummaryHandler summary;
    EXPECT_FALSE(summary.HandleRequest(std::make_unique<Protocol::EventsViewRequest>()));
    ExpectFailure<Protocol::FreeAttributionSummaryResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    QueryFreeAttributionDetailsHandler details;
    EXPECT_FALSE(details.HandleRequest(nullptr));
    ExpectFailure<Protocol::FreeAttributionDetailsResponse>(ErrorCode::PARAMS_ERROR);
}

TEST_F(QueryFreeAttributionHandlerTest, ValidatesRangeRankPaginationAndSortBeforeDatabaseQuery) {
    QueryFreeAttributionSummaryHandler handler;
    for (int invalid = 0; invalid < 8; ++invalid) {
        SCOPED_TRACE(invalid);
        auto request = Summary();
        switch (invalid) {
        case 0:
            request->params.rankId.clear();
            break;
        case 1:
            request->params.startTime = 2;
            request->params.endTime = 1;
            break;
        case 2:
            request->params.endTime = UINT64_MAX;
            break;
        case 3:
            request->params.current = 0;
            break;
        case 4:
            request->params.pageSize = 0;
            break;
        case 5:
            request->params.pageSize = 1001;
            break;
        case 6:
            request->params.orderBy = "freeCount;DROP TABLE slice";
            break;
        case 7:
            request->params.order = "unexpected";
            break;
        }
        EXPECT_FALSE(handler.HandleRequest(std::move(request)));
        ExpectFailure<Protocol::FreeAttributionSummaryResponse>(ErrorCode::PARAMS_ERROR);
        const auto *response = session->Get<Protocol::FreeAttributionSummaryResponse>();
        ASSERT_NE(response, nullptr);
        EXPECT_EQ(response->requestId, 71U);
        ResetResponse();
    }
}

TEST_F(QueryFreeAttributionHandlerTest, DetailsRequiresGroupAndRejectsSummarySortFields) {
    QueryFreeAttributionDetailsHandler handler;
    auto request = std::make_unique<Protocol::FreeAttributionDetailsRequest>();
    request->params.rankId = "free-rank";
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::FreeAttributionDetailsResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    request = std::make_unique<Protocol::FreeAttributionDetailsRequest>();
    request->params.rankId = "free-rank";
    request->params.groupId = "matched:aten::add";
    request->params.orderBy = "totalFreeDuration";
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::FreeAttributionDetailsResponse>(ErrorCode::PARAMS_ERROR);
}

TEST_F(QueryFreeAttributionHandlerTest, MissingDatabaseReturnsDedicatedQueryError) {
    QueryFreeAttributionSummaryHandler handler;
    EXPECT_FALSE(handler.HandleRequest(Summary()));
    ExpectFailure<Protocol::FreeAttributionSummaryResponse>(ErrorCode::QUERY_FREE_ATTRIBUTION_FAILED);
}

TEST_F(QueryFreeAttributionHandlerTest, RejectsUnknownRangeModeForSummaryAndDetails) {
    QueryFreeAttributionSummaryHandler summary;
    auto summaryRequest = Summary();
    summaryRequest->params.rangeMode = "unknown";
    EXPECT_FALSE(summary.HandleRequest(std::move(summaryRequest)));
    ExpectFailure<Protocol::FreeAttributionSummaryResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    QueryFreeAttributionDetailsHandler details;
    auto detailsRequest = std::make_unique<Protocol::FreeAttributionDetailsRequest>();
    detailsRequest->params.rankId = "free-rank";
    detailsRequest->params.groupId = "matched:aten::add";
    detailsRequest->params.rangeMode = "unknown";
    EXPECT_FALSE(details.HandleRequest(std::move(detailsRequest)));
    ExpectFailure<Protocol::FreeAttributionDetailsResponse>(ErrorCode::PARAMS_ERROR);
}

TEST_F(QueryFreeAttributionHandlerTest, EmptyRangeReturnsZeroSummaryWithoutLookingUpDatabase) {
    QueryFreeAttributionSummaryHandler handler;
    auto request = Summary();
    request->params.rangeMode = "empty";
    ASSERT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::FreeAttributionSummaryResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    // SetResponseResult also initializes the error object on successful responses.
    ASSERT_TRUE(response->error.has_value());
    EXPECT_TRUE(response->error->message.empty());
    EXPECT_TRUE(response->body.groups.empty());
    EXPECT_EQ(response->body.totalGroups, 0U);
    EXPECT_EQ(response->body.totalFreeCount, 0U);
    EXPECT_EQ(response->body.totalFreeDuration, 0U);
}

TEST_F(QueryFreeAttributionHandlerTest, EmptyRangeReturnsZeroDetailsWithoutLookingUpDatabase) {
    QueryFreeAttributionDetailsHandler handler;
    auto request = std::make_unique<Protocol::FreeAttributionDetailsRequest>();
    request->params.rankId = "free-rank";
    request->params.dbPath = "missing.db";
    request->params.groupId = "matched:aten::add";
    request->params.rangeMode = "empty";
    ASSERT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::FreeAttributionDetailsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    ASSERT_TRUE(response->error.has_value());
    EXPECT_TRUE(response->error->message.empty());
    EXPECT_TRUE(response->body.records.empty());
    EXPECT_EQ(response->body.totalRecords, 0U);
}
}
