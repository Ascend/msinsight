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
#include "TimelineHandlerSuccessTestSupport.h"

namespace Dic::Module::Timeline {
namespace {
TEST_F(TimelineHandlerSuccessTest, ComputingDetailsSortsFirstPageAndConvertsTimestampsAndDurations) {
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_MATMUL};
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.rankId, rankId);
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_EQ(response->body.currentPage, 1U);
    EXPECT_EQ(response->body.pageSize, 2U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 2U);
    EXPECT_EQ(response->body.sameOperatorsDetails[0].name, "Matmul_100");
    EXPECT_EQ(response->body.sameOperatorsDetails[0].timestamp, 100U);
    EXPECT_EQ(response->body.sameOperatorsDetails[0].duration, 4000U);
    EXPECT_EQ(response->body.sameOperatorsDetails[1].name, "Matmul_200");
    EXPECT_EQ(repository->lastParams.deviceId, "3");
}

TEST_F(TimelineHandlerSuccessTest, ComputingDetailsSortsDurationDescendingBeforeTakingLastPage) {
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_MATMUL};
    request->params.order.orderBy = "duration";
    request->params.order.orderType = "descend";
    request->params.page.current = 2;
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails[0].name, "Matmul_200");
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_EQ(response->body.currentPage, 2U);
}

TEST_F(TimelineHandlerSuccessTest, ComputingDetailsAppliesCategoryAndCaseInsensitiveNameFilter) {
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_MATMUL, OVERALL_CAT_CUBE};
    request->params.name = "MATMUL_300";
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails[0].name, "Matmul_300");
    EXPECT_EQ(response->body.count, 1U);
}

TEST_F(TimelineHandlerSuccessTest, ComputingDetailsReturnsEmptyPageWithoutChangingTotal) {
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_MATMUL};
    request->params.page.current = 3;
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_TRUE(response->body.sameOperatorsDetails.empty());
}

TEST_F(TimelineHandlerSuccessTest, ComputingDetailsReturnsZeroMatchesForAnotherCategory) {
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_CONV};
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 0U);
    EXPECT_TRUE(response->body.sameOperatorsDetails.empty());
}

TEST_F(TimelineHandlerSuccessTest, ComputingDetailsReportsRepositoryFailure) {
    repository->computingSuccess = false;
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {OVERALL_CAT_COMPUTING, OVERALL_CAT_MATMUL};
    EXPECT_FALSE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>(false);
    ASSERT_NE(response, nullptr);
    ASSERT_TRUE(response->error.has_value());
    EXPECT_FALSE(response->error->message.empty());
    EXPECT_EQ(repository->computingQueries, 1U);
}

TEST_F(TimelineHandlerSuccessTest, CommunicationDetailsSortsDescendingAndReturnsLastPage) {
    SeedCommunication();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {COMMUNICATION_TIME, "group-7"};
    request->params.order.orderType = "descend";
    request->params.page.current = 2;
    ASSERT_TRUE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 3U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails[0].name, "AllReduce_10");
    EXPECT_EQ(repository->communicationQueries, 1U);
    EXPECT_EQ(repository->communicationOffset, BASE_TIME);
    EXPECT_EQ(repository->lastParams.categoryList[1], "group-7");
    EXPECT_EQ(repository->lastParams.deviceId, "3");
}

TEST_F(TimelineHandlerSuccessTest, CommunicationDetailsReportsEmptyRepositoryResult) {
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {COMMUNICATION_TIME, "group-7"};
    EXPECT_FALSE(QueryOverallMoreDetailsHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<UnitThreadsOperatorsResponse>(false);
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->body.sameOperatorsDetails.empty());
    EXPECT_EQ(repository->communicationQueries, 1U);
}

TEST_F(TimelineHandlerSuccessTest, OverallCalculatesRatiosAndStoresDefaultResultInCache) {
    ASSERT_TRUE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    auto *response = LastResponse<SystemViewOverallResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->details.size(), 4U);
    EXPECT_EQ(response->details[0].name, COMPUTING_TIME);
    EXPECT_DOUBLE_EQ(response->details[0].totalTime, 60.0);
    EXPECT_DOUBLE_EQ(response->details[0].ratio, 60.0);
    EXPECT_EQ(response->details[1].name, COMMUNICATION_NOT_OVERLAP_TIME);
    EXPECT_DOUBLE_EQ(response->details[1].ratio, 30.0);
    EXPECT_EQ(response->details[2].name, FREE_TIME);
    EXPECT_DOUBLE_EQ(response->details[2].ratio, 10.0);
    EXPECT_EQ(response->details[3].name, E2E_TIME);
    EXPECT_DOUBLE_EQ(response->details[3].totalTime, 100.0);
    EXPECT_DOUBLE_EQ(response->details[3].ratio, 100.0);
    EXPECT_EQ(response->pageParam.total, 4U);
    EXPECT_EQ(response->pageParam.current, 1U);
    EXPECT_EQ(response->pageParam.pageSize, 2U);
    EXPECT_EQ(SystemViewOverallCacheManager::Instance().GetOverallData(fileId).size(), 4U);
    EXPECT_EQ(repository->overlapQueries, 1U);
}

TEST_F(TimelineHandlerSuccessTest, OverallUsesCachedResultWithoutQueryingRepository) {
    SeedCachedOverall();
    repository->overlaps.clear();
    ASSERT_TRUE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    auto *response = LastResponse<SystemViewOverallResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->details.size(), 1U);
    EXPECT_EQ(response->details[0].name, "cached overall");
    EXPECT_DOUBLE_EQ(response->details[0].totalTime, 777.0);
    EXPECT_EQ(response->pageParam.total, 1U);
    EXPECT_EQ(repository->overlapQueries, 0U);
    EXPECT_EQ(repository->computingQueries, 0U);
}

TEST_F(TimelineHandlerSuccessTest, OverallTimeRangeBypassesAndPreservesDefaultCache) {
    SeedCachedOverall();
    auto request = RequestForTest<SystemViewOverallRequest>();
    request->params.startTime = 10;
    request->params.endTime = 100;
    ASSERT_TRUE(QuerySystemViewOverallHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<SystemViewOverallResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->details.size(), 4U);
    EXPECT_EQ(repository->overlapQueries, 1U);
    EXPECT_EQ(repository->lastParams.startTime, 10U);
    EXPECT_EQ(repository->lastParams.endTime, 100U);
    const auto cached = SystemViewOverallCacheManager::Instance().GetOverallData(fileId);
    ASSERT_EQ(cached.size(), 1U);
    EXPECT_EQ(cached.front().name, "cached overall");
}

TEST_F(TimelineHandlerSuccessTest, OverallCustomClassificationBypassesCacheAndReportsUnmatchedKeyword) {
    SeedCachedOverall();
    SeedComputing();
    auto request = RequestForTest<SystemViewOverallRequest>();
    request->params.customClassificationRules = {{"My compute", {"not_in_profile"}, false}};
    ASSERT_TRUE(QuerySystemViewOverallHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<SystemViewOverallResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->details.size(), 4U);
    EXPECT_FALSE(response->details.front().children.empty());
    EXPECT_EQ(response->unmatchedCustomClassificationKeywords, std::vector<std::string>{"not_in_profile"});
    EXPECT_EQ(repository->computingQueries, 1U);
    const auto cached = SystemViewOverallCacheManager::Instance().GetOverallData(fileId);
    ASSERT_EQ(cached.size(), 1U);
    EXPECT_EQ(cached.front().name, "cached overall");
}

TEST_F(TimelineHandlerSuccessTest, OverallReturnsLoadingBeforeReadingCacheWhenAnalysisIsUnfinished) {
    SeedCachedOverall();
    {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
        ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(OVERLAP_ANALYSIS_UNIT, NOT_FINISH_STATUS));
    }
    EXPECT_TRUE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    auto *response = LastResponse<SystemViewOverallResponse>(false);
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->isLoading);
    EXPECT_TRUE(response->details.empty());
    EXPECT_EQ(repository->overlapQueries, 0U);
}

TEST_F(TimelineHandlerSuccessTest, OverallRejectsIncompleteOverlapGroupsWithoutCachingResult) {
    repository->overlaps.pop_back();
    EXPECT_FALSE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    ASSERT_NE(LastResponse<SystemViewOverallResponse>(false), nullptr);
    EXPECT_TRUE(SystemViewOverallCacheManager::Instance().GetOverallData(fileId).empty());
    EXPECT_EQ(repository->computingQueries, 0U);
}

TEST_F(TimelineHandlerSuccessTest, OverallRejectsZeroTotalWithoutDividingByZero) {
    for (auto &overlap : repository->overlaps) {
        overlap.duration = 0.0;
    }
    EXPECT_FALSE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    auto *response = LastResponse<SystemViewOverallResponse>(false);
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->details.empty());
    EXPECT_TRUE(SystemViewOverallCacheManager::Instance().GetOverallData(fileId).empty());
}

TEST_F(TimelineHandlerSuccessTest, OverallKeepsTopLevelStatisticsWhenComputingQueryFails) {
    repository->computingSuccess = false;
    ASSERT_TRUE(QuerySystemViewOverallHandler().HandleRequest(RequestForTest<SystemViewOverallRequest>()));
    auto *response = LastResponse<SystemViewOverallResponse>();
    ASSERT_NE(response, nullptr);
    ASSERT_EQ(response->details.size(), 4U);
    EXPECT_DOUBLE_EQ(response->details[3].totalTime, 100.0);
    EXPECT_TRUE(response->details[0].children.empty());
    EXPECT_EQ(repository->computingQueries, 1U);
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsMapsFieldsAndKeepsTotalBeforePagination) {
    SeedMemcpy();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {""};
    ASSERT_TRUE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<MemcpyDetailResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_EQ(response->body.currentPage, 1U);
    EXPECT_EQ(response->body.pageSize, 2U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 2U);
    const auto &first = response->body.sameOperatorsDetails.front();
    EXPECT_EQ(first.id, "11");
    EXPECT_EQ(first.name, "MEMCPY_ASYNC");
    EXPECT_EQ(first.timestamp, 100U);
    EXPECT_EQ(first.duration, 20U);
    EXPECT_EQ(first.size, 64U);
    EXPECT_EQ(response->body.sameOperatorsDetails.back().id, "12");
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsReturnsPartialLastPageSortedBySizeDescending) {
    SeedMemcpy();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {""};
    request->params.page.current = 2;
    request->params.order.orderBy = "size";
    request->params.order.orderType = "descend";
    ASSERT_TRUE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<MemcpyDetailResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 3U);
    EXPECT_EQ(response->body.currentPage, 2U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails.front().id, "11");
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsCombinesThreadAndOperationFilters) {
    SeedMemcpy();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {"7", "H2D"};
    ASSERT_TRUE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<MemcpyDetailResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 1U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails.front().id, "11");
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsAppliesRelativeTimeRangeToDatabaseQuery) {
    SeedMemcpy();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {""};
    request->params.startTime = 150;
    request->params.endTime = 250;
    ASSERT_TRUE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<MemcpyDetailResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 1U);
    ASSERT_EQ(response->body.sameOperatorsDetails.size(), 1U);
    EXPECT_EQ(response->body.sameOperatorsDetails.front().id, "12");
    EXPECT_EQ(response->body.sameOperatorsDetails.front().timestamp, 200U);
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsDistinguishesEmptyOperationFromUnfilteredOperation) {
    SeedMemcpy();
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {"", ""};
    ASSERT_TRUE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    auto *response = LastResponse<MemcpyDetailResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 0U);
    EXPECT_TRUE(response->body.sameOperatorsDetails.empty());
}

TEST_F(TimelineHandlerSuccessTest, MemcpyDetailsRejectsRecordBeforeTraceStart) {
    SeedMemcpy();
    {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(fileId);
        ASSERT_TRUE(database->ExecSql("UPDATE slice SET timestamp = 900 WHERE id = 11;"));
    }
    auto request = RequestForTest<SystemViewOverallMoreDetailsRequest>();
    request->params.categoryList = {"7", "H2D"};
    EXPECT_FALSE(QueryMemcpyDetailHandler().HandleRequest(std::move(request)));
    Response error;
    ModuleRequestHandler::SetResponseErrorFromRequestContext(error);
    ASSERT_TRUE(error.error.has_value());
    EXPECT_FALSE(error.error->message.empty());
}
} // namespace
} // namespace Dic::Module::Timeline
