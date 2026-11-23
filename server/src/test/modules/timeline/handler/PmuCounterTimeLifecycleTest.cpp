/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#include "TimelineHandlerTestSupport.h"
#include "DbTraceDataBase.h"
#include "ParserStatusManager.h"
#include "ProjectParserFactory.h"
#include "RankLaneMergeCoordinator.h"
#include "RemoteDeleteHandler.h"

using namespace Dic;
using namespace Dic::Module;
using namespace Dic::Module::Timeline;

class PmuCounterTimeLifecycleTest : public testing::Test {
  protected:
    class EventSession : public Timeline::Test::CapturingTimelineSession {
      public:
        void OnEvent(std::unique_ptr<Protocol::Event> value) override { event = std::move(value); }
        std::unique_ptr<Protocol::Event> event;
    };
    class Parser : public ProjectParserBase {
      public:
        using ProjectParserBase::SendParseSuccessEvent;
    };

    void SetUp() override {
        DataBaseManager::Instance().Clear();
        RankLaneMergeCoordinator::Instance().Reset();
        TraceTime::Instance().Reset();
        ParserStatusManager::Instance().ClearAllParserStatus();
        directory = std::filesystem::temp_directory_path() /
            ("pmu-time-lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        session = std::make_shared<EventSession>();
        auto &manager = Server::WsSessionManager::Instance();
        std::lock_guard<std::mutex> lock(manager.sessionMutex);
        previousSession = std::exchange(manager.session, session);
    }

    void TearDown() override {
        DataBaseManager::Instance().Clear();
        RankLaneMergeCoordinator::Instance().Reset();
        TraceTime::Instance().Reset();
        ParserStatusManager::Instance().ClearAllParserStatus();
        {
            auto &manager = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(manager.sessionMutex);
            manager.session = std::move(previousSession);
        }
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        EXPECT_FALSE(error) << error.message();
    }

    std::string AddSource(const std::string &name, const std::string &sessionRows, bool pmu = true) {
        const auto file = (directory / (name + ".db")).string();
        sqlite3 *database = nullptr;
        EXPECT_EQ(sqlite3_open(file.c_str(), &database), SQLITE_OK);
        std::string sql = "CREATE TABLE SESSION_TIME_INFO(startTimeNs INTEGER,endTimeNs INTEGER);"
                          "INSERT INTO SESSION_TIME_INFO VALUES " +
            sessionRows + ";";
        if (pmu) {
            sql +=
                "CREATE TABLE HOST_PMU_EVENTS(eventId INTEGER PRIMARY KEY,name TEXT,source INTEGER,unit TEXT);"
                "CREATE TABLE HOST_PMU_COUNTER_INFO(counterId INTEGER PRIMARY KEY,eventId INTEGER,pid INTEGER,cpu "
                "INTEGER,intervalNs INTEGER);"
                "CREATE TABLE HOST_PMU_COUNTER_DATA(counterId INTEGER,timestamp INTEGER,valid INTEGER,rawValue INTEGER,"
                "timeEnabled INTEGER,timeRunning INTEGER);"
                "INSERT INTO HOST_PMU_EVENTS VALUES(1,'cycles',2,'count');"
                "INSERT INTO HOST_PMU_COUNTER_INFO VALUES(1,1,-1,0,10);"
                "INSERT INTO HOST_PMU_COUNTER_DATA SELECT 1,startTimeNs+10,1,0,10,10 FROM SESSION_TIME_INFO;";
        }
        EXPECT_EQ(sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
        EXPECT_EQ(sqlite3_close(database), SQLITE_OK);
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(DataType::DB, file);
        EXPECT_TRUE(manager.CreateTraceConnectionPool(rankId, file));
        RankLaneMergeCoordinator::Instance().RegisterSource(rankId, file);
        RankLaneMergeCoordinator::Instance().MarkSourceSucceeded(rankId, file);
        auto fullDb = std::dynamic_pointer_cast<FullDb::DbTraceDataBase>(manager.GetTraceDatabaseByFileId(file));
        EXPECT_NE(fullDb, nullptr);
        if (fullDb != nullptr) {
            fullDb->UpdateStartTime(rankId);
        }
        return file;
    }

    const Protocol::ParseSuccessEvent *ParseSuccess(const std::string &file) {
        Parser::SendParseSuccessEvent(rankId, file);
        return dynamic_cast<const Protocol::ParseSuccessEvent *>(session->event.get());
    }

    const Protocol::RemoteDeleteResponse *RebuildAfterDelete() {
        ParserStatusManager::Instance().SetParserStatus(rankId, ParserStatus::FINISH_ALL);
        RemoteDeleteHandler handler;
        auto request = std::make_unique<Protocol::RemoteDeleteRequest>();
        EXPECT_TRUE(handler.HandleRequest(std::move(request)));
        return session->Get<Protocol::RemoteDeleteResponse>();
    }

    const std::string rankId = "pmu-rank";
    std::filesystem::path directory;
    std::shared_ptr<EventSession> session;
    std::shared_ptr<Server::WsSession> previousSession;
};

TEST_F(PmuCounterTimeLifecycleTest, ParseSuccessDoesNotBridgeSeparateSessionIntervals) {
    const auto source = AddSource("pmu", "(1000,1100),(2000,2100)");
    // Keep PMU inside one session; its data must not manufacture a bridge between sessions.
    auto database = DataBaseManager::Instance().GetTraceDatabaseByFileId(source);
    ASSERT_TRUE(database->ExecSql("DELETE FROM HOST_PMU_COUNTER_DATA WHERE timestamp>1100"));
    TraceTime::Instance().Reset();
    std::dynamic_pointer_cast<FullDb::DbTraceDataBase>(database)->UpdateStartTime(rankId);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 100u);
    const auto *event = ParseSuccess(source);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->body.startTime, 1000u);
    EXPECT_EQ(event->body.maxTimeStamp, 100u);
    EXPECT_EQ(TraceTime::Instance().GetDuration(), 100u);
}

TEST_F(PmuCounterTimeLifecycleTest, ParseSuccessAndDeleteRebuildEveryPmuSourceWithoutRankEnvelope) {
    const auto first = AddSource("a-pmu", "(1000,1100)");
    AddSource("b-pmu", "(5000,5300)");
    const auto *event = ParseSuccess(first);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->body.maxTimeStamp, 300u);
    const auto *response = RebuildAfterDelete();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->body.startTimeUpdated);
    EXPECT_EQ(response->body.maxTimeStamp, 300u);
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 1000u);
}

TEST_F(PmuCounterTimeLifecycleTest, MixedRankRetainsOrdinaryFullDbDuringDeleteRebuild) {
    const auto first = AddSource("a-pmu", "(1000,1100)");
    AddSource("b-standard", "(5000,5400)", false);
    const auto *event = ParseSuccess(first);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->body.maxTimeStamp, 400u);
    const auto *response = RebuildAfterDelete();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->body.startTimeUpdated);
    EXPECT_EQ(response->body.maxTimeStamp, 400u);
    EXPECT_EQ(TraceTime::Instance().GetStartTime(), 1000u);
}
