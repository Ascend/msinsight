/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#include <algorithm>
#include <filesystem>
#include <sqlite3.h>
#include "QueryPmuCounterHandler.h"
#include "DataBaseManager.h"
#include "RankLaneMergeCoordinator.h"
#include "TraceTime.h"
#include "HandlerTest.cpp"

using namespace Dic::Module::Timeline;
using namespace Dic::Protocol;

class QueryPmuCounterHandlerTest : public HandlerTest {
  protected:
    void SetUp() override {
        DataBaseManager::Instance().Clear();
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(1000000000, 1000001000);
        path = (std::filesystem::temp_directory_path() / "insight-pmu-handler-test.db").string();
        std::filesystem::remove(path);
        sqlite3 *db = nullptr;
        ASSERT_EQ(sqlite3_open(path.c_str(), &db), SQLITE_OK);
        ASSERT_EQ(
            sqlite3_exec(db,
                "CREATE TABLE HOST_PMU_EVENTS(eventId INTEGER PRIMARY KEY,name TEXT,source INTEGER,unit TEXT);"
                "CREATE TABLE HOST_PMU_COUNTER_INFO(counterId INTEGER PRIMARY KEY,eventId INTEGER,pid INTEGER,cpu "
                "INTEGER,intervalNs INTEGER);"
                "CREATE TABLE HOST_PMU_COUNTER_DATA(counterId INTEGER,timestamp INTEGER,countValue REAL,rawValue "
                "INTEGER,"
                "timeEnabled INTEGER,timeRunning INTEGER,valid INTEGER);"
                "INSERT INTO HOST_PMU_EVENTS VALUES(1,'cycles',2,'count');"
                "INSERT INTO HOST_PMU_COUNTER_INFO VALUES(1,1,-1,3,100),(2,1,-1,7,100);"
                "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,1000000000,10,10,100,100,1),"
                "(1,1000000100,30,30,200,200,1);",
                nullptr, nullptr, nullptr),
            SQLITE_OK);
        sqlite3_close(db);
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(DataType::DB, path);
        ASSERT_TRUE(manager.CreateTraceConnectionPool("rank0", path));
    }
    void TearDown() override {
        DataBaseManager::Instance().Clear();
        TraceTime::Instance().Reset();
        std::filesystem::remove(path);
    }
    std::unique_ptr<PmuCounterRequest> Request(const std::string &id = "1") {
        auto request = std::make_unique<PmuCounterRequest>();
        request->id = 123;
        request->params = {path, "rank0", id, 0, 100, 32};
        return request;
    }
    std::string path;
};

TEST_F(QueryPmuCounterHandlerTest, QueriesOnlyRegisteredAssociatedSource) {
    ResponseCapture capture;
    QueryPmuCounterHandler handler;
    ASSERT_TRUE(handler.HandleRequest(Request()));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    auto *response = dynamic_cast<PmuCounterResponse *>(capture.session->responses[0].get());
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->result);
    EXPECT_EQ(response->requestId, 123u);
    ASSERT_EQ(response->body.segments.size(), 1u);
    EXPECT_EQ(response->body.segments[0].startTime, 0u);
    EXPECT_EQ(response->body.segments[0].endTime, 100u);
    ASSERT_TRUE(response->body.segments[0].value.has_value());
    EXPECT_DOUBLE_EQ(*response->body.segments[0].value, 20.0);
}

TEST_F(QueryPmuCounterHandlerTest, RejectsForeignRankAndArbitraryHostFallback) {
    ResponseCapture capture;
    QueryPmuCounterHandler handler;
    auto foreign = Request();
    foreign->params.rankId = "unrelated-rank";
    EXPECT_FALSE(handler.HandleRequest(std::move(foreign)));
    auto host = Request();
    host->params.rankId = "other-Host";
    EXPECT_FALSE(handler.HandleRequest(std::move(host)));
    auto source = Request();
    source->params.dbPath += ".unregistered";
    EXPECT_FALSE(handler.HandleRequest(std::move(source)));
}

TEST_F(QueryPmuCounterHandlerTest, DistinguishesEmptyCounterFromMissingCounter) {
    ResponseCapture capture;
    QueryPmuCounterHandler handler;
    EXPECT_TRUE(handler.HandleRequest(Request("2")));
    auto *response = dynamic_cast<PmuCounterResponse *>(capture.session->responses[0].get());
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.status, "empty");
    EXPECT_FALSE(handler.HandleRequest(Request("999")));
}

TEST_F(QueryPmuCounterHandlerTest, MetadataPreservesPhysicalCpuAndTrueSource) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    std::vector<std::unique_ptr<UnitTrack>> metadata;
    db->QueryUnitsMetadata("rank0", metadata);
    const UnitTrack *metrics = nullptr;
    for (const auto &track : metadata) {
        if (track->metaData.processName == "CPU Metrics") {
            metrics = track.get();
        }
    }
    ASSERT_NE(metrics, nullptr);
    ASSERT_EQ(metrics->children.size(), 1u);
    const auto &group = *metrics->children[0];
    EXPECT_EQ(group.metaData.processName, "PMU");
    ASSERT_TRUE(group.metaData.pmu.has_value());
    EXPECT_EQ(group.metaData.pmu->kind, "group");
    EXPECT_EQ(group.metaData.dbPath, path);
    EXPECT_NE(group.metaData.pmu->sourceKey, "rank0");
    ASSERT_EQ(group.children.size(), 2u);
    EXPECT_EQ(group.children[0]->metaData.processName, "CPU 3");
    const auto &leaf = *group.children[0]->children[0];
    EXPECT_EQ(leaf.metaData.metaType, "PMU_COUNTER");
    EXPECT_EQ(leaf.metaData.dbPath, path);
    ASSERT_TRUE(leaf.metaData.pmu.has_value());
    EXPECT_EQ(leaf.metaData.pmu->counterId, "1");
    EXPECT_EQ(leaf.metaData.pmu->unit, "count");
    EXPECT_EQ(leaf.metaData.pmu->hasSamples, true);
    EXPECT_EQ(group.children[1]->children[0]->metaData.pmu->hasSamples, false);
}

TEST_F(QueryPmuCounterHandlerTest, PmuOnlyDatabaseContributesTimelineBoundaries) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 1000000000u);
    EXPECT_EQ(max, 1000000100u);
    TraceTime::Instance().Reset();
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 1000000000u);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 100u);
}

TEST_F(QueryPmuCounterHandlerTest, TimelineBoundariesUnionSessionAndPmuIgnoringInvalidTimestampStorage) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(1000000020,1000000080);"
                            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,-10,0,0,0,0,1),(1,'bad-time',0,0,0,0,1),"
                            "(1,1000000000.5,0,0,0,0,1);"));
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 1000000000u);
    EXPECT_EQ(max, 1000000100u);
    ASSERT_TRUE(db->ExecSql("UPDATE SESSION_TIME_INFO SET startTimeNs=999999900,endTimeNs=1000000200;"));
    min = UINT64_MAX;
    max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 999999900u);
    EXPECT_EQ(max, 1000000200u);
}

TEST_F(QueryPmuCounterHandlerTest, EmptyThreadingProcessTableDoesNotReplacePmuEpochWithZero) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("CREATE TABLE HOST_CORE_PROCESS(id INTEGER,start_ts INTEGER,end_ts INTEGER);"
                            "CREATE TABLE HOST_CORE_THREAD(id INTEGER);CREATE TABLE HOST_CORE_METRIC(id INTEGER);"
                            "CREATE TABLE HOST_CORE_METRIC_DESC(id INTEGER);"));
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 1000000000u);
    EXPECT_EQ(max, 1000000100u);
}

TEST_F(QueryPmuCounterHandlerTest, ReimportReevaluatesSourceCapabilities) {
    ResponseCapture capture;
    QueryPmuCounterHandler handler;
    ASSERT_TRUE(handler.HandleRequest(Request()));
    auto &manager = DataBaseManager::Instance();
    manager.Clear();
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open(path.c_str(), &db), SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(db,
            "DROP TABLE HOST_PMU_COUNTER_DATA;"
            "CREATE TABLE HOST_PMU_COUNTER_DATA(counterId INTEGER,timestamp INTEGER,countValue REAL,valid INTEGER);"
            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,1000000000,10,1),(1,1000000100,30,1);",
            nullptr, nullptr, nullptr),
        SQLITE_OK);
    sqlite3_close(db);
    manager.SetDataType(DataType::DB, path);
    ASSERT_TRUE(manager.CreateTraceConnectionPool("rank0", path));
    TraceTime::Instance().UpdateTime(1000000000, 1000001000);
    ASSERT_TRUE(handler.HandleRequest(Request()));
    auto *response = dynamic_cast<PmuCounterResponse *>(capture.session->responses.back().get());
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.status, "unsupported");
    EXPECT_TRUE(response->body.segments.empty());
}

TEST_F(QueryPmuCounterHandlerTest, BrokenPmuMetadataCannotExpandSessionBoundaries) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(999999900,1000000200);"
                            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(999,1,0,0,0,0,0),(999,8000000000,0,0,0,0,1);"
                            "INSERT INTO HOST_PMU_COUNTER_INFO VALUES(3,1,-1,-2,100);"
                            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(3,8000000001,0,0,0,0,1);"));
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 999999900u);
    EXPECT_EQ(max, 1000000200u);
    ASSERT_TRUE(db->ExecSql("DROP TABLE HOST_PMU_EVENTS;"
                            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,9000000000,0,0,0,0,1);"));
    min = UINT64_MAX;
    max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 999999900u);
    EXPECT_EQ(max, 1000000200u);
}

TEST_F(QueryPmuCounterHandlerTest, QueryTimeRangeDoesNotAccumulateCallerBounds) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    const std::vector<std::pair<uint64_t, uint64_t>> initialBounds = {
        {0, 0}, {7, 9000000000ULL}, {UINT64_MAX, 0}, {1000000070, 1000000080}};
    for (const auto &initial : initialBounds) {
        SCOPED_TRACE(::testing::Message() << "input: " << initial.first << ", " << initial.second);
        uint64_t min = initial.first;
        uint64_t max = initial.second;
        ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
        EXPECT_EQ(min, 1000000000u);
        EXPECT_EQ(max, 1000000100u);
    }
}

TEST_F(QueryPmuCounterHandlerTest, ZeroEpochIsAValidTimelineStart) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA;"
                            "INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,0,10,10,100,100,1),"
                            "(1,100,30,30,200,200,1);"));
    uint64_t min = 25;
    uint64_t max = 900;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 0u);
    EXPECT_EQ(max, 100u);
    ASSERT_TRUE(db->ExecSql("CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(0,200);"));
    min = 25;
    max = 900;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 0u);
    EXPECT_EQ(max, 200u);
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    TraceTime::Instance().Reset();
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 0u);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 200u);
    const auto cards = TraceTime::Instance().ComputeCardMinTimeInfo();
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("rank0"), uint64_t{0})), cards.end());
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("Host"), uint64_t{0})), cards.end());
}

TEST_F(QueryPmuCounterHandlerTest, SessionRangeExcludesTheWholeRowForEitherNegativeEndpoint) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA;"
                            "CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(999999900,1000000050),"
                            "(1000000200,1000000300),(-1,9000000000),(1,-2);"));
    uint64_t min = 5;
    uint64_t max = 7000000000ULL;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 999999900u);
    EXPECT_EQ(max, 1000000300u);
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    TraceTime::Instance().Reset();
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 999999900u);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 150u);
}

TEST_F(QueryPmuCounterHandlerTest, EmptyPmuRetainsValidSessionAndRegistersCardAndHost) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA;"
                            "CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(6000,9000);"));
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 6000u);
    EXPECT_EQ(max, 9000u);
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    auto &time = TraceTime::Instance();
    time.Reset();
    time.UpdateTime(1000, 2000);
    time.UpdateCardMinTimestamp("earlier-rank", 1000);
    time.UpdateCardTimeDuration("earlier-rank", 1000, 2000);
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(time.GetStartTime(), 1000u);
    EXPECT_EQ(time.GetDuration(), 3000u);
    EXPECT_EQ(time.GetOffsetByFileIdUsingMinTimestamp("rank0"), 5000u);
    const auto cards = time.ComputeCardMinTimeInfo();
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("rank0"), uint64_t{5000})), cards.end());
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("Host"), uint64_t{5000})), cards.end());
}

TEST_F(QueryPmuCounterHandlerTest, DisjointSessionsKeepTheirGapWhenPmuExtendsOnlyTheFirstSession) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(999999900,1000000050),(1000000200,1000000300);"));
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    auto &time = TraceTime::Instance();
    time.Reset();
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(time.GetStartTime(), 999999900u);
    EXPECT_EQ(time.GetDuration(), 200u);
    const auto cards = time.ComputeCardMinTimeInfo();
    EXPECT_EQ(cards.size(), 2u);
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("rank0"), uint64_t{0})), cards.end());
    EXPECT_NE(std::find(cards.begin(), cards.end(), std::make_pair(std::string("Host"), uint64_t{0})), cards.end());
    time.UpdateCardTimeDuration("later-session-probe", 1000000220, 1000000240);
    EXPECT_EQ(time.GetOffsetByFileId("later-session-probe"), 300u);
    EXPECT_EQ(time.GetDuration(), 200u);
}

TEST_F(QueryPmuCounterHandlerTest, DisjointSessionsKeepTheirGapWithNoPmuSamples) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA;"
                            "CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(999999900,1000000050),(1000000200,1000000300);"));
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    auto &time = TraceTime::Instance();
    time.Reset();
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(time.GetStartTime(), 999999900u);
    EXPECT_EQ(time.GetDuration(), 150u);
    time.UpdateCardTimeDuration("later-session-probe", 1000000220, 1000000240);
    EXPECT_EQ(time.GetOffsetByFileId("later-session-probe"), 300u);
    EXPECT_EQ(time.GetDuration(), 150u);
}

TEST_F(QueryPmuCounterHandlerTest, MissingSessionTableRetainsApiRangeFallback) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql(
        "DROP TABLE IF EXISTS temp.PYTORCH_API;"
        "CREATE TABLE main.PYTORCH_API(startNs INTEGER,endNs INTEGER);"
        "INSERT INTO main.PYTORCH_API(startNs,endNs) VALUES(999999900,1000000050),(1000000200,1000000300);"));
    ASSERT_FALSE(db->CheckTableExist("SESSION_TIME_INFO"));
    ASSERT_TRUE(db->CheckTableExist("PYTORCH_API"));
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    ASSERT_TRUE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 999999900u);
    EXPECT_EQ(max, 1000000300u);
    auto fullDb = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(db);
    ASSERT_NE(fullDb, nullptr);
    TraceTime::Instance().Reset();
    fullDb->UpdateStartTime("rank0");
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 999999900u);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 400u);
}

TEST_F(QueryPmuCounterHandlerTest, MissingValidRangeDoesNotChangeCallerBounds) {
    auto db = DataBaseManager::Instance().GetTraceDatabaseByFileId(path);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA;"
                            "CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                            "INSERT INTO SESSION_TIME_INFO VALUES(-1,9000),(1,-2);"));
    uint64_t min = 42;
    uint64_t max = 99;
    EXPECT_FALSE(db->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, 42u);
    EXPECT_EQ(max, 99u);
}
