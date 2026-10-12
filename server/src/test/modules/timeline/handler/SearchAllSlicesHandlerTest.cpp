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
#include "SearchAllSlicesHandler.h"

namespace Dic::Module::Timeline::Test {
class SearchAllSlicesHandlerTest : public TimelineHandlerDatabaseTest {
  protected:
    std::unique_ptr<Protocol::SearchAllSlicesRequest> Request() {
        auto request = std::make_unique<Protocol::SearchAllSlicesRequest>();
        request->fileId = "request-file";
        request->params.rankId = "search";
        request->params.searchContent = "Mat";
        request->params.orderBy = "timestamp";
        request->params.order = "ascend";
        request->params.current = 1;
        request->params.pageSize = 1;
        return request;
    }
};

TEST_F(SearchAllSlicesHandlerTest, LoadsCacheFiltersAndReturnsPageWithSourceIdentity) {
    const auto path = AddDatabase("search");
    ASSERT_FALSE(path.empty());
    AddSlices("search");
    SearchAllSlicesHandler handler;
    auto request = Request();
    request->params.nameFilter = "GRAD";
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->body.count, 1U);
    EXPECT_EQ(response->body.dbPath, path);
    EXPECT_EQ(response->body.currentPage, 1U);
    ASSERT_EQ(response->body.searchAllSlices.size(), 1U);
    const auto &row = response->body.searchAllSlices[0];
    EXPECT_EQ(row.name, "MatMulGrad");
    EXPECT_EQ(row.id, "2");
    EXPECT_EQ(row.timestamp, 80U);
    EXPECT_EQ(row.fileId, "request-file");
    EXPECT_EQ(row.rankId, "search");
    EXPECT_TRUE(SearchSliceCacheManager::Instance().exists(SearchSliceCacheManager::makeKey("search", "", "Mat")));
}

TEST_F(SearchAllSlicesHandlerTest, SortsBeforePaginationAndUsesExplicitDatabasePath) {
    const auto path = AddDatabase("source");
    ASSERT_FALSE(path.empty());
    AddSlices("source");
    auto request = Request();
    request->params.dbPath = path;
    request->params.orderBy = "duration";
    request->params.order = "descend";
    SearchAllSlicesHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 2U);
    ASSERT_EQ(response->body.searchAllSlices.size(), 1U);
    EXPECT_EQ(response->body.searchAllSlices[0].name, "MatMulGrad");
    EXPECT_EQ(response->body.dbPath, path);
}

TEST_F(SearchAllSlicesHandlerTest, EmptySearchFallsBackToSqlAndReturnsZeroCount) {
    ASSERT_FALSE(AddDatabase("search").empty());
    AddSlices("search");
    auto request = Request();
    request->params.searchContent = "absent";
    SearchAllSlicesHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->body.count, 0U);
    EXPECT_TRUE(response->body.searchAllSlices.empty());
}

TEST_F(SearchAllSlicesHandlerTest, LockRangeLimitsSearchAndBypassesTheCache) {
    ASSERT_FALSE(AddDatabase("search").empty());
    AddSlices("search");
    auto request = Request();
    Protocol::Metadata metadata;
    metadata.rankId = "search";
    metadata.pid = "10";
    metadata.tid = "11";
    metadata.metaType = "TEXT";
    metadata.lockStartTime = 10;
    metadata.lockEndTime = 45;
    request->params.metadataList = {metadata};
    SearchAllSlicesHandler handler;
    EXPECT_TRUE(handler.HandleRequest(std::move(request)));
    const auto *response = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.count, 1U);
    ASSERT_EQ(response->body.searchAllSlices.size(), 1U);
    EXPECT_EQ(response->body.searchAllSlices[0].name, "MatMul");
    EXPECT_EQ(SearchSliceCacheManager::Instance().size(), 0U);
}

TEST_F(SearchAllSlicesHandlerTest, ReportsValidationConnectionAndSqlFallbackErrors) {
    SearchAllSlicesHandler handler;
    auto request = Request();
    request->params.pageSize = 0;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::SearchAllSlicesResponse>(ErrorCode::PARAMS_ERROR);
    ResetResponse();
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectFailure<Protocol::SearchAllSlicesResponse>(ErrorCode::CONNECT_DATABASE_FAILED);
    ResetResponse();
    ASSERT_FALSE(AddDatabase("search").empty());
    Exec("search", "DROP TABLE slice;");
    EXPECT_FALSE(handler.HandleRequest(Request()));
    ExpectFailure<Protocol::SearchAllSlicesResponse>(ErrorCode::QUERY_SLICE_DETAIL_FAILED);
}

TEST_F(SearchAllSlicesHandlerTest, RefreshesExistingCacheWhenTheSourceChangesAndKeepsPartialResultCount) {
    ASSERT_FALSE(AddDatabase("search").empty());
    AddSlices("search");
    SearchAllSlicesHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *first = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->body.count, 2U);
    ResetResponse();
    Exec("search", "UPDATE slice SET name='Renamed' WHERE id=2;");
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *refreshed = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(refreshed, nullptr);
    EXPECT_EQ(refreshed->body.count, 1U);
    ASSERT_EQ(refreshed->body.searchAllSlices.size(), 1U);
    EXPECT_EQ(refreshed->body.searchAllSlices[0].name, "MatMul");
    ResetResponse();
    Exec("search", "DROP TABLE thread;");
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *partial = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(partial, nullptr);
    EXPECT_TRUE(partial->result);
    EXPECT_EQ(partial->body.count, 1U);
    EXPECT_TRUE(partial->body.searchAllSlices.empty());
}
TEST_F(SearchAllSlicesHandlerTest, ReturnsSqlRowsWhenTheCacheCannotReadRowIds) {
    const auto path = AddDatabase("search");
    ASSERT_FALSE(path.empty());
    AddSlices("search");
    Exec("search",
        "ALTER TABLE slice RENAME TO original_slice;"
        "CREATE TABLE slice(id INTEGER PRIMARY KEY,name TEXT,timestamp INTEGER,duration INTEGER,"
        "end_time INTEGER,depth INTEGER,track_id INTEGER) WITHOUT ROWID;"
        "INSERT INTO slice SELECT id,name,timestamp,duration,end_time,depth,track_id FROM original_slice;");
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId("search");
    ASSERT_NE(database, nullptr);
    LightSliceCache unavailableCache;
    EXPECT_FALSE(database->LoadSliceCache(unavailableCache, Request()->params, 100));
    SearchAllSlicesHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request()));
    const auto *response = session->Get<Protocol::SearchAllSlicesResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->body.count, 2U);
    EXPECT_EQ(response->body.currentPage, 1U);
    EXPECT_EQ(response->body.pageSize, 1U);
    EXPECT_EQ(response->body.dbPath, path);
    ASSERT_EQ(response->body.searchAllSlices.size(), 1U);
    EXPECT_EQ(response->body.searchAllSlices[0].id, "1");
    EXPECT_EQ(response->body.searchAllSlices[0].name, "MatMul");
    EXPECT_EQ(response->body.searchAllSlices[0].timestamp, 20U);
    EXPECT_EQ(response->body.searchAllSlices[0].fileId, "request-file");
}

TEST_F(SearchAllSlicesHandlerTest, ReportsLockRangeSqlFailureWithoutCreatingCache) {
    ASSERT_FALSE(AddDatabase("search").empty());
    AddSlices("search");
    Exec("search", "DROP TABLE slice;");
    auto request = Request();
    Protocol::Metadata metadata;
    metadata.rankId = "search";
    metadata.pid = "10";
    metadata.tid = "11";
    metadata.metaType = "TEXT";
    metadata.lockStartTime = 10;
    metadata.lockEndTime = 45;
    request->params.metadataList = {metadata};
    SearchAllSlicesHandler handler;
    EXPECT_FALSE(handler.HandleRequest(std::move(request)));
    ExpectFailure<Protocol::SearchAllSlicesResponse>(ErrorCode::QUERY_SLICE_DETAIL_FAILED);
    EXPECT_EQ(SearchSliceCacheManager::Instance().size(), 0U);
}
} // namespace Dic::Module::Timeline::Test
