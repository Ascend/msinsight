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
#include <gmock/gmock.h>
#include <chrono>
#include <filesystem>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "TrackInfoManager.h"
#include "DataEngine.h"
#include "RepositoryFactory.h"

using namespace Dic::Module::Timeline;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Ref;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;

namespace {
using ThreadInfo = std::unordered_map<uint64_t, std::pair<std::string, std::string>>;

class BaseSliceRepoMock : public IBaseSliceRepo {
  public:
    MOCK_METHOD(
        void, QuerySimpleSliceWithOutNameByTrackId, (const SliceQuery &, std::vector<SliceDomain> &), (override));
    MOCK_METHOD(void, QueryCompeteSliceByIds,
        (const SliceQuery &, const std::vector<uint64_t> &, std::vector<CompeteSliceDomain> &), (override));
    MOCK_METHOD(bool, QuerySliceDetailInfo, (const SliceQuery &, CompeteSliceDomain &), (override));
};

class SliceRepoMock : public BaseSliceRepoMock,
                      public IPythonFuncSlice,
                      public ITextSlice,
                      public IFindSliceByTimepointAndName {
  public:
    MOCK_METHOD(void, QuerySliceIdsByCat, (const SliceQuery &, std::vector<uint64_t> &), (override));
    MOCK_METHOD(uint64_t, QueryPythonFunctionCountByTrackId, (const SliceQuery &), (override));
    MOCK_METHOD(void, QueryCompeteSliceVecByTimeRangeAndTrackId,
        (const SliceQuery &, std::vector<CompeteSliceDomain> &), (override));
    MOCK_METHOD(void, QueryAllThreadInfo, (const ThreadQuery &, ThreadInfo &), (override));
    MOCK_METHOD(bool, QuerySliceByTimepointAndName, (const SliceQuery &, CompeteSliceDomain &), (override));
};

class FlowRepoMock : public FlowRepoInterface {
  public:
    MOCK_METHOD(void, QueryFlowPointByCategory, (const FlowQuery &, std::vector<FlowPoint> &), (override));
    MOCK_METHOD(void, QueryFlowPointByTimeRange, (const FlowQuery &, std::vector<FlowPoint> &), (override));
    MOCK_METHOD(void, QueryFlowPointByFlowId, (const FlowQuery &, std::vector<FlowPoint> &), (override));
};

class FlagRepoMock : public SimulationSliceRepoInterface {
  public:
    MOCK_METHOD(void, QueryAllFlagSlice, (const SliceQuery &, std::vector<CompeteSliceDomain> &), (override));
};

class FactoryMock : public RepositoryFactoryInterface {
  public:
    MOCK_METHOD(std::shared_ptr<IBaseSliceRepo>, GetSliceRespo, (PROCESS_TYPE), (override));
    MOCK_METHOD(std::shared_ptr<SimulationSliceRepoInterface>, GetSimulationSliceRespo, (PROCESS_TYPE), (override));
    MOCK_METHOD(std::shared_ptr<CounterRepoInterface>, GetCounterRespo, (PROCESS_TYPE), (override));
    MOCK_METHOD(std::shared_ptr<FlowRepoInterface>, GetFlowRespo, (PROCESS_TYPE), (override));
};
}

class DataEngineTest : public ::testing::Test {
  protected:
    void SetUp() override {
        query.metaType = PROCESS_TYPE::API;
        query.rankId = "rank-7";
        query.dbPath = "data-source.db";
        query.trackId = 17;
        query.startTime = 100;
        query.endTime = 200;
        query.minTimestamp = 1000;
        query.sliceId = "42";
        query.cat = "python_function";
        query.name = "operator";
        query.timePoint = 150;
        flowQuery.metaType = PROCESS_TYPE::DB;
        flowQuery.fileId = "data-source.db";
        flowQuery.trackId = 17;
        flowQuery.flowId = "flow-42";
        flowQuery.cat = "async";
        flowQuery.startTime = 100;
        flowQuery.endTime = 200;
        flowQuery.minTimestamp = 1000;
        flowQuery.flowIds = {"flow-42", "flow-43"};
    }

    DataEngine engine;
    SliceQuery query;
    FlowQuery flowQuery;
    std::shared_ptr<StrictMock<FactoryMock>> factory = std::make_shared<StrictMock<FactoryMock>>();
    std::shared_ptr<StrictMock<SliceRepoMock>> repo = std::make_shared<StrictMock<SliceRepoMock>>();
    std::shared_ptr<StrictMock<FlowRepoMock>> flowRepo = std::make_shared<StrictMock<FlowRepoMock>>();
};

TEST_F(DataEngineTest, QueryAllThreadInfoNormalTest) {
    ThreadQuery threadQuery;
    threadQuery.metaType = PROCESS_TYPE::TEXT;
    threadQuery.fileId = "data-source.db";
    ThreadInfo output{{9, {"sentinel", "sentinel"}}};
    engine.QueryAllThreadInfo(threadQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.at(9).first, "sentinel");
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(nullptr));
    engine.QueryAllThreadInfo(threadQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.at(9).first, "sentinel");
    const ThreadInfo expected{{17, {"worker", "process"}}};
    EXPECT_CALL(*factory, GetSliceRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QueryAllThreadInfo(Ref(threadQuery), Ref(output))).WillOnce(SetArgReferee<1>(expected));
    engine.QueryAllThreadInfo(threadQuery, output);
    EXPECT_EQ(output, expected);
}

TEST_F(DataEngineTest, QuerySimpleSliceWithOutNameByTrackIdTestWithOutFactory) {
    SliceDomain sentinel;
    sentinel.id = 99;
    std::vector<SliceDomain> output{sentinel};
    engine.QuerySimpleSliceWithOutNameByTrackId(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    engine.QuerySimpleSliceWithOutNameByTrackId(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
}

TEST_F(DataEngineTest, QueryPythonFunctionCountByTrackIdTestWithOutFactory) {
    EXPECT_EQ(engine.QueryPythonFunctionCountByTrackId(query), 0u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    EXPECT_EQ(engine.QueryPythonFunctionCountByTrackId(query), 0u);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QueryPythonFunctionCountByTrackId(Ref(query))).WillOnce(Return(37));
    EXPECT_EQ(engine.QueryPythonFunctionCountByTrackId(query), 37u);
}

TEST_F(DataEngineTest, QuerySliceIdsByCatTestWithOutFactory) {
    std::vector<uint64_t> output{99};
    engine.QuerySliceIdsByCat(query, output);
    EXPECT_EQ(output, std::vector<uint64_t>{99});
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    engine.QuerySliceIdsByCat(query, output);
    EXPECT_EQ(output, std::vector<uint64_t>{99});
    const std::vector<uint64_t> expected{7, 13};
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySliceIdsByCat(Ref(query), Ref(output))).WillOnce(SetArgReferee<1>(expected));
    engine.QuerySliceIdsByCat(query, output);
    EXPECT_EQ(output, expected);
}

TEST_F(DataEngineTest, QueryCompeteSliceVecByTimeRangeAndTrackIdTestWithOutFactory) {
    CompeteSliceDomain sentinel;
    sentinel.id = 99;
    std::vector<CompeteSliceDomain> output{sentinel};
    engine.QueryCompeteSliceVecByTimeRangeAndTrackId(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    engine.QueryCompeteSliceVecByTimeRangeAndTrackId(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    CompeteSliceDomain expected;
    expected.id = 42;
    expected.name = "operator";
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QueryCompeteSliceVecByTimeRangeAndTrackId(Ref(query), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<CompeteSliceDomain>{expected}));
    engine.QueryCompeteSliceVecByTimeRangeAndTrackId(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 42u);
    EXPECT_EQ(output.front().name, "operator");
}

TEST_F(DataEngineTest, QueryFlowPointByTimeRangeTestWithOutFactory) {
    FlowPoint sentinel;
    sentinel.flowId = "unchanged";
    std::vector<FlowPoint> output{sentinel};
    engine.QueryFlowPointByTimeRange(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flowId, "unchanged");
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetFlowRespo(flowQuery.metaType)).WillOnce(Return(nullptr));
    engine.QueryFlowPointByTimeRange(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flowId, "unchanged");
    FlowPoint point;
    point.flowId = "flow-42";
    point.timestamp = 150;
    EXPECT_CALL(*factory, GetFlowRespo(flowQuery.metaType)).WillOnce(Return(flowRepo));
    EXPECT_CALL(*flowRepo, QueryFlowPointByTimeRange(Ref(flowQuery), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<FlowPoint>{point}));
    engine.QueryFlowPointByTimeRange(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flowId, "flow-42");
    EXPECT_EQ(output.front().timestamp, 150u);
}

TEST_F(DataEngineTest, QueryFlowPointByFlowIdTestWithOutFactory) {
    FlowPoint sentinel;
    sentinel.id = 99;
    std::vector<FlowPoint> output{sentinel};
    engine.QueryFlowPointByFlowId(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetFlowRespo(flowQuery.metaType)).WillOnce(Return(nullptr));
    engine.QueryFlowPointByFlowId(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    FlowPoint point;
    point.id = 42;
    point.flowId = "flow-42";
    EXPECT_CALL(*factory, GetFlowRespo(flowQuery.metaType)).WillOnce(Return(flowRepo));
    EXPECT_CALL(*flowRepo, QueryFlowPointByFlowId(Ref(flowQuery), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<FlowPoint>{point}));
    engine.QueryFlowPointByFlowId(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 42u);
    EXPECT_EQ(output.front().flowId, "flow-42");
}

TEST_F(DataEngineTest, QueryCompeteSliceByIdsTestWithOutFactory) {
    const std::vector<uint64_t> ids{42, 43};
    CompeteSliceDomain sentinel;
    sentinel.id = 99;
    std::vector<CompeteSliceDomain> output{sentinel};
    engine.QueryCompeteSliceByIds(query, ids, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    engine.QueryCompeteSliceByIds(query, ids, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 99u);
    CompeteSliceDomain expected;
    expected.id = 42;
    expected.name = "operator";
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QueryCompeteSliceByIds(Ref(query), Ref(ids), Ref(output)))
        .WillOnce(SetArgReferee<2>(std::vector<CompeteSliceDomain>{expected}));
    engine.QueryCompeteSliceByIds(query, ids, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().id, 42u);
    EXPECT_EQ(output.front().name, "operator");
}

TEST_F(DataEngineTest, QuerySliceDetailInfoTestWithOutFactory) {
    CompeteSliceDomain output;
    output.id = 99;
    EXPECT_FALSE(engine.QuerySliceDetailInfo(query, output));
    EXPECT_EQ(output.id, 99u);
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    EXPECT_FALSE(engine.QuerySliceDetailInfo(query, output));
    EXPECT_EQ(output.id, 99u);
    CompeteSliceDomain expected;
    expected.id = 42;
    expected.name = "operator";
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySliceDetailInfo(Ref(query), Ref(output)))
        .WillOnce(DoAll(SetArgReferee<1>(expected), Return(true)));
    EXPECT_TRUE(engine.QuerySliceDetailInfo(query, output));
    EXPECT_EQ(output.id, 42u);
    EXPECT_EQ(output.name, "operator");
}

TEST_F(DataEngineTest, QuerySliceDetailInfoFailTest) {
    query.sliceId = "\n\t\bAA\v";
    CompeteSliceDomain output;
    output.name = "unchanged";
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySliceDetailInfo(Ref(query), Ref(output))).WillOnce(Return(false));
    EXPECT_FALSE(engine.QuerySliceDetailInfo(query, output));
    EXPECT_EQ(output.name, "unchanged");
}

TEST_F(DataEngineTest, QueryAllFlagSliceTestWithOutFactory) {
    std::vector<CompeteSliceDomain> output;
    engine.QueryAllFlagSlice(query, output);
    EXPECT_TRUE(output.empty());
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetSimulationSliceRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(nullptr));
    engine.QueryAllFlagSlice(query, output);
    EXPECT_TRUE(output.empty());
    auto flagRepo = std::make_shared<StrictMock<FlagRepoMock>>();
    CompeteSliceDomain expected;
    expected.flagId = "flag-7";
    EXPECT_CALL(*factory, GetSimulationSliceRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(flagRepo));
    EXPECT_CALL(*flagRepo, QueryAllFlagSlice(Ref(query), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<CompeteSliceDomain>{expected}));
    engine.QueryAllFlagSlice(query, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flagId, "flag-7");
}

TEST_F(DataEngineTest, QueryFlowPointByCategoryTestWithOutFactory) {
    std::vector<FlowPoint> output;
    engine.QueryFlowPointByCategory(flowQuery, output);
    EXPECT_TRUE(output.empty());
    engine.SetRepositoryFactory(factory);
    EXPECT_CALL(*factory, GetFlowRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(nullptr));
    engine.QueryFlowPointByCategory(flowQuery, output);
    EXPECT_TRUE(output.empty());
    auto textRepo = std::make_shared<StrictMock<FlowRepoMock>>();
    FlowPoint expected;
    expected.flowId = "db-fallback";
    ::testing::InSequence order;
    EXPECT_CALL(*factory, GetFlowRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(textRepo));
    EXPECT_CALL(*textRepo, QueryFlowPointByCategory(Ref(flowQuery), Ref(output)));
    EXPECT_CALL(*factory, GetFlowRespo(PROCESS_TYPE::DB)).WillOnce(Return(flowRepo));
    EXPECT_CALL(*flowRepo, QueryFlowPointByCategory(Ref(flowQuery), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<FlowPoint>{expected}));
    engine.QueryFlowPointByCategory(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flowId, "db-fallback");
}

TEST_F(DataEngineTest, QueryFlowPointByCategoryKeepsTextResultWithoutDbFallback) {
    engine.SetRepositoryFactory(factory);
    std::vector<FlowPoint> output;
    FlowPoint expected;
    expected.flowId = "text-result";
    EXPECT_CALL(*factory, GetFlowRespo(PROCESS_TYPE::TEXT)).WillOnce(Return(flowRepo));
    EXPECT_CALL(*factory, GetFlowRespo(PROCESS_TYPE::DB)).Times(0);
    EXPECT_CALL(*flowRepo, QueryFlowPointByCategory(Ref(flowQuery), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<FlowPoint>{expected}));
    engine.QueryFlowPointByCategory(flowQuery, output);
    ASSERT_EQ(output.size(), 1u);
    EXPECT_EQ(output.front().flowId, "text-result");
}

TEST_F(DataEngineTest, QuerySliceByTimepointAndNameTestWithOutFactory) {
    CompeteSliceDomain output;
    output.id = 99;
    EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, output));
    EXPECT_EQ(output.id, 99u);
    engine.SetRepositoryFactory(factory);
    query.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
    EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, output));
    EXPECT_EQ(output.id, 99u); // No repository is requested for an unsupported process type.
    query.metaType = PROCESS_TYPE::API;
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(nullptr));
    EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, output));
    CompeteSliceDomain expected;
    expected.id = 42;
    expected.name = "operator";
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySliceByTimepointAndName(Ref(query), Ref(output)))
        .WillOnce(DoAll(SetArgReferee<1>(expected), Return(true)));
    EXPECT_TRUE(engine.QuerySliceByTimepointAndName(query, output));
    EXPECT_EQ(output.id, 42u);
    EXPECT_EQ(output.name, "operator");
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySliceByTimepointAndName(Ref(query), Ref(output))).WillOnce(Return(false));
    EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, output));
}

TEST_F(DataEngineTest, QuerySimpleSliceWithOutNameByTrackId) {
    engine.SetRepositoryFactory(factory);
    std::vector<SliceDomain> output;
    SliceDomain first;
    first.id = 10;
    first.timestamp = 10;
    SliceDomain second;
    second.id = 11;
    second.timestamp = 7;
    SliceDomain third;
    third.id = 12;
    third.timestamp = 3;
    third.depth = 1;
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).WillOnce(Return(repo));
    EXPECT_CALL(*repo, QuerySimpleSliceWithOutNameByTrackId(Ref(query), Ref(output)))
        .WillOnce(SetArgReferee<1>(std::vector<SliceDomain>{third, first, second}));
    engine.QuerySimpleSliceWithOutNameByTrackId(query, output);
    ASSERT_EQ(output.size(), 3u);
    EXPECT_EQ(output[0].id, 11u);
    EXPECT_EQ(output[1].id, 10u);
    EXPECT_EQ(output[2].id, 12u);
    EXPECT_EQ(output[0].timestamp, 7u);
    EXPECT_EQ(output[2].depth, 1u);
}

TEST_F(DataEngineTest, UnsupportedSliceInterfacesDoNotMutateOutput) {
    engine.SetRepositoryFactory(factory);
    auto baseOnly = std::make_shared<StrictMock<BaseSliceRepoMock>>();
    EXPECT_CALL(*factory, GetSliceRespo(query.metaType)).Times(5).WillRepeatedly(Return(baseOnly));
    std::vector<uint64_t> ids{99};
    engine.QuerySliceIdsByCat(query, ids);
    EXPECT_EQ(ids, std::vector<uint64_t>{99});
    EXPECT_EQ(engine.QueryPythonFunctionCountByTrackId(query), 0u);
    CompeteSliceDomain sentinel;
    sentinel.id = 99;
    std::vector<CompeteSliceDomain> slices{sentinel};
    engine.QueryCompeteSliceVecByTimeRangeAndTrackId(query, slices);
    ASSERT_EQ(slices.size(), 1u);
    EXPECT_EQ(slices.front().id, 99u);
    EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, sentinel));
    EXPECT_EQ(sentinel.id, 99u);
    ThreadQuery threadQuery;
    threadQuery.metaType = query.metaType;
    ThreadInfo threadInfo{{99, {"sentinel", "process"}}};
    engine.QueryAllThreadInfo(threadQuery, threadInfo);
    ASSERT_EQ(threadInfo.size(), 1u);
    EXPECT_EQ(threadInfo.at(99).first, "sentinel");
}

class DataEngineDatabaseTest : public ::testing::Test {
  protected:
    void SetUp() override {
        TrackInfoManager::Instance().Reset();
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path = (std::filesystem::temp_directory_path() / ("timeline-data-engine-" + std::to_string(suffix) + ".db"))
                   .string();
        sqlite3 *raw = nullptr;
        ASSERT_EQ(sqlite3_open(path.c_str(), &raw), SQLITE_OK);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        ASSERT_EQ(sqlite3_exec(raw, R"SQL(
            CREATE TABLE PYTORCH_API(startNs INTEGER, endNs INTEGER, globalTid INTEGER, type INTEGER);
            CREATE TABLE ENUM_API_TYPE(id INTEGER PRIMARY KEY, name TEXT);
            INSERT INTO ENUM_API_TYPE VALUES(50003,'trace'),(50004,'other');
            INSERT INTO PYTORCH_API VALUES(100,150,123,50003),(100,150,456,50003),
                (100,150,123,50004),(300,350,123,50003);
        )SQL",
                      nullptr, nullptr, nullptr),
            SQLITE_OK);
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(DataType::DB, path);
        ASSERT_TRUE(manager.CreateTraceConnectionPool(rank, path));
        manager.SetDbPathMapping(rank, path, "");
    }

    void TearDown() override {
        DataBaseManager::Instance().Clear(DatabaseType::TRACE);
        TrackInfoManager::Instance().Reset();
        std::error_code error;
        std::filesystem::remove(path, error);
        EXPECT_FALSE(error) << error.message();
    }

    std::string path;
    const std::string rank = "timeline-data-engine-sqlite";
};

// Retain the original positive SQL path as well as the dispatch-only contract tests above.
TEST_F(DataEngineDatabaseTest, QuerySliceIdsByCatFiltersThreadCategoryAndTimeRange) {
    DataEngine engine;
    engine.SetRepositoryFactory(std::make_shared<RepositoryFactory>());
    SliceQuery query;
    query.metaType = PROCESS_TYPE::API;
    query.rankId = rank;
    query.trackId = TrackInfoManager::Instance().GetTrackId(rank, "123", "pytorch");
    query.startTime = 90;
    query.endTime = 200;
    std::vector<uint64_t> ids;
    engine.QuerySliceIdsByCat(query, ids);
    EXPECT_EQ(ids, std::vector<uint64_t>{1});
    query.startTime = 300;
    query.endTime = 350;
    ids.clear();
    engine.QuerySliceIdsByCat(query, ids);
    EXPECT_EQ(ids, std::vector<uint64_t>{4});
}
