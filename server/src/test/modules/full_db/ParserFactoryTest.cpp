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

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <utility>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "GlobalDefs.h"
#include "ProjectParserFactory.h"
#include "ProjectParserPytorchTrace.h"
#include "ProjectParserBin.h"
#include "ProjectParserDb.h"
#include "ProjectParserDbNPUMonitor.h"
#include "ProjectParserFtrace.h"
#include "ProjectParserJson.h"
#include "ProjectParserMemScope.h"
#include "ProjectParserMemSnapshot.h"
#include "ProjectParserTriton.h"
#include "ParserIE.h"
#include "MemSnapshotParser.h"
#include "../timeline/handler/TimelineHandlerTestSupport.h"
#include "FileUtil.h"
#include "TableDefs.h"
#include "TraceTime.h"
#include "TestSuit.h"

using namespace Dic;
using namespace Dic::Module;

class ParserFactoryTest : public ::testing::Test {
  protected:
    std::vector<std::string> tempDirectories;

    std::string CreateTempDirectoryWithFiles(const std::vector<std::string> &fileNames) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string directory = Dic::FileUtil::SplicePath(
            ::testing::TempDir(), std::string(testInfo->name()) + "_" + std::to_string(std::rand()));
        if (!std::filesystem::create_directories(directory)) {
            return "";
        }
        tempDirectories.push_back(directory);
        for (const auto &fileName : fileNames) {
            std::ofstream file(Dic::FileUtil::SplicePath(directory, fileName));
            if (!file.is_open()) {
                return "";
            }
            file << R"({"traceEvents":[]})";
        }
        return directory;
    }

    std::string CreateStableTempDirectoryWithFiles(const std::vector<std::string> &fileNames) {
        const ::testing::TestInfo *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string testName = testInfo->name();
        std::replace(testName.begin(), testName.end(), '/', '_');
        static std::atomic<int> seq{0};
        std::string directory;
        for (int i = 0; i < 8; ++i) {
            const std::string candidate =
                Dic::FileUtil::SplicePath(::testing::TempDir(), testName + "_" + std::to_string(seq.fetch_add(1)));
            std::error_code ec;
            if (std::filesystem::create_directories(candidate, ec)) {
                directory = candidate;
                break;
            }
        }
        if (directory.empty()) {
            return "";
        }
        tempDirectories.push_back(directory);
        for (const auto &fileName : fileNames) {
            std::ofstream file(Dic::FileUtil::SplicePath(directory, fileName));
            if (!file.is_open()) {
                return "";
            }
            file << R"({"traceEvents":[]})";
        }
        return directory;
    }

    void TearDown() override {
        for (const auto &directory : tempDirectories) {
            std::filesystem::remove_all(directory);
        }
        tempDirectories.clear();
    }
};

TEST_F(ParserFactoryTest, GetImportTypeStandardDbFileWithoutHostCoreTables) {
    const auto uniqueId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path =
        FileUtil::SplicePath(::testing::TempDir(), "ascend_pytorch_profiler_" + uniqueId.substr(0, 12) + ".db");
    sqlite3 *database = nullptr;
    ASSERT_EQ(sqlite3_open(path.c_str(), &database), SQLITE_OK);
    EXPECT_EQ(
        sqlite3_exec(database, "CREATE TABLE PYTORCH_API (globalTid INTEGER);", nullptr, nullptr, nullptr), SQLITE_OK);
    EXPECT_EQ(sqlite3_close(database), SQLITE_OK);
    const auto result = ParserFactory::GetImportType(path);
    EXPECT_EQ(result.first, path);
    EXPECT_EQ(result.second, ParserType::DB);
    std::remove(path.c_str());
}

class PlatformParseSuccessEventTest : public ::testing::Test {
  protected:
    class TestableProjectParserBase : public ProjectParserBase {
      public:
        using ProjectParserBase::SendPlatformParseSuccessEvent;
    };

    void SetUp() override {
        databasePath = FileUtil::SplicePath(::testing::TempDir(), "platform-parse-success-event.db");
        std::remove(databasePath.c_str());
        sqlite3 *database = nullptr;
        ASSERT_EQ(sqlite3_open(databasePath.c_str(), &database), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(database,
                      "CREATE TABLE NUMA_TITLES_NAMES (name TEXT, description TEXT, summary_flag INTEGER, "
                      "measurement_unit TEXT, unique_id INTEGER);"
                      "CREATE TABLE NUMA_LEVELS_HIERARCHY_NAMES "
                      "(title0_id INTEGER, title1_id INTEGER, title2_id INTEGER);"
                      "CREATE TABLE NUMA_METRICS (ts INTEGER, value REAL, levels_id INTEGER);"
                      "CREATE TABLE NUMA_SCALING_VALUES (id INTEGER PRIMARY KEY, level_id INTEGER, max_value REAL);"
                      "INSERT INTO NUMA_TITLES_NAMES VALUES ('Metric', 'desc', 1, 'Ratio', 1);"
                      "INSERT INTO NUMA_LEVELS_HIERARCHY_NAMES VALUES (1, 0, 0);"
                      "INSERT INTO NUMA_METRICS VALUES (100, 50.0, 1);"
                      "INSERT INTO NUMA_METRICS VALUES (30100, 60.0, 1);"
                      "INSERT INTO NUMA_SCALING_VALUES VALUES (1, 1, 100.0);",
                      nullptr, nullptr, nullptr),
            SQLITE_OK);
        ASSERT_EQ(sqlite3_close(database), SQLITE_OK);

        auto platform = Timeline::DataBaseManager::Instance().CreatePlatformDataBase(rankId, databasePath);
        ASSERT_NE(platform, nullptr);
        ASSERT_TRUE(platform->OpenDb(databasePath, false));
        Timeline::TraceTime::Instance().Reset();
        Timeline::TraceTime::Instance().UpdateTime(10000, 20000);
        Timeline::TraceTime::Instance().UpdateCardTimeDuration("trace-rank", 10000, 20000);
    }

    void TearDown() override {
        Timeline::DataBaseManager::Instance().Clear();
        Timeline::TraceTime::Instance().Reset();
        std::remove(databasePath.c_str());
    }

    const std::string rankId = "trace-rank#platform";
    std::string databasePath;
};

TEST_F(PlatformParseSuccessEventTest, DoesNotApplyNumaClockToGlobalTraceTime) {
    PlatformParseSuccessEventTest::TestableProjectParserBase::SendPlatformParseSuccessEvent(rankId, databasePath);

    EXPECT_EQ(Timeline::TraceTime::Instance().GetStartTime(), 10000U);
    EXPECT_EQ(Timeline::TraceTime::Instance().GetDuration(), 10000U);
}

class SearchGroupedAscendHardwareThreadsTest : public ::testing::Test {
  protected:
    class TestableProjectParserBase : public ProjectParserBase {
      public:
        using ProjectParserBase::SearchGroupedAscendHardwareThreads;
    };

    void SetUp() override {
        const auto uniqueId = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        rankId = "parser_factory_multi_group_" + uniqueId;
        databasePath = FileUtil::SplicePath(::testing::TempDir(), rankId + ".db");

        sqlite3 *rawDatabase = nullptr;
        ASSERT_EQ(sqlite3_open(databasePath.c_str(), &rawDatabase), SQLITE_OK);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(rawDatabase, sqlite3_close);
        ASSERT_TRUE(ExecuteSql(database.get(),
            "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, thread_name TEXT, "
            "thread_sort_index INTEGER);"));
        ASSERT_TRUE(ExecuteSql(database.get(), "CREATE TABLE slice (track_id INTEGER, args TEXT);"));
        ASSERT_TRUE(ExecuteSql(database.get(),
            "INSERT INTO thread (track_id, tid, pid, thread_name, thread_sort_index) VALUES "
            "(1, '1', '300', 'Stream 1', 1), (2, '2', '300', 'Stream 2', 2), "
            "(5, '5', '300', 'Stream 5', 5), (6, '6', '300', 'Stream 6', 6);"));
        ASSERT_TRUE(ExecuteSql(database.get(),
            "INSERT INTO slice (track_id, args) VALUES (1, '{\"Model Id\":\"101\"}'), "
            "(2, '{\"Model Id\":\"101\"}'), (5, '{\"Model Id\":\"505\"}'), "
            "(6, '{\"Model Id\":\"505\"}');"));
        ASSERT_TRUE(ExecuteSql(database.get(), "PRAGMA user_version = " + std::to_string(DATABASE_VERSION) + ";"));
        database.reset();

        auto &databaseManager = Timeline::DataBaseManager::Instance();
        databaseManager.SetDataType(Timeline::DataType::TEXT, databasePath);
        ASSERT_TRUE(databaseManager.CreateTraceConnectionPool(rankId, databasePath));
        databaseRegistered = true;
    }

    void TearDown() override {
        if (databaseRegistered) {
            Timeline::DataBaseManager::Instance().ReleaseDatabaseByFileId(databasePath);
        }
        for (const auto &suffix : {"", "-shm", "-wal"}) {
            std::remove((databasePath + suffix).c_str());
        }
    }

    static ::testing::AssertionResult ExecuteSql(sqlite3 *database, const std::string &sql) {
        char *errorMessage = nullptr;
        const int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &errorMessage);
        if (result != SQLITE_OK) {
            const std::string message = errorMessage == nullptr ? "Unknown SQLite error" : errorMessage;
            sqlite3_free(errorMessage);
            return ::testing::AssertionFailure() << message;
        }
        sqlite3_free(errorMessage);
        return ::testing::AssertionSuccess();
    }

    static Protocol::Unit BuildUnit() {
        Protocol::Unit unit;
        unit.metadata.cardId = "card_0";
        auto process = std::make_unique<Protocol::UnitTrack>();
        process->type = "process";
        process->metaData.processId = "300";
        for (const auto &threadId : std::vector<std::string>{"1", "2", "5", "6"}) {
            auto thread = std::make_unique<Protocol::UnitTrack>();
            thread->type = "thread";
            thread->metaData.threadId = threadId;
            process->children.emplace_back(std::move(thread));
        }
        unit.children.emplace_back(std::move(process));
        return unit;
    }

    std::string rankId;
    std::string databasePath;
    bool databaseRegistered = false;
};

TEST_F(SearchGroupedAscendHardwareThreadsTest, UpdatesAllGroupsInSameProcess) {
    Protocol::Unit unit = BuildUnit();
    std::vector<Protocol::ThreadGroup> groupedThreads;

    TestableProjectParserBase::SearchGroupedAscendHardwareThreads(databasePath, unit, groupedThreads);

    ASSERT_EQ(groupedThreads.size(), 2U);
    std::set<std::set<std::string>> actualGroups;
    for (const auto &group : groupedThreads) {
        actualGroups.emplace(group.threadIds.begin(), group.threadIds.end());
        EXPECT_EQ(group.processId, "300");
        EXPECT_EQ(group.cardId, "card_0");
    }
    const std::set<std::set<std::string>> expectedGroups = {{"1", "2"}, {"5", "6"}};
    EXPECT_EQ(actualGroups, expectedGroups);
}

TEST_F(ParserFactoryTest, GetImportTypeBinTest) {
    std::pair<std::string, ParserType> result = ParserFactory::GetImportType("/home/user/data/visualize_data.bin");
    std::pair<std::string, ParserType> expect{"/home/user/data/visualize_data.bin", ParserType::BIN};
    EXPECT_EQ(result, expect);
}

TEST_F(ParserFactoryTest, GetImportTypeDbTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(
        currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt", "ASCEND_PROFILER_OUTPUT");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "ascend_pytorch_profiler_0.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::DB};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeDbClusterTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "cluster_analysis.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB};
    EXPECT_EQ(result1, expect1);
    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::DB};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList2;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeDbNPUMonitorTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB_NPUMONITOR};
    EXPECT_EQ(result1, expect1);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeACLGraphDebugTextTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "aclgraph_debug", "graph_debug.json")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::ACLGRPAH_DEBUG_JSON};
    EXPECT_EQ(result1, expect1);
}

TEST_F(ParserFactoryTest, GetImportTypeTextTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(
        currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt", "ASCEND_PROFILER_OUTPUT");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "trace_view.json");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::JSON};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::JSON};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypePytorchTraceSampleFileAndDirectoryTest) {
    const std::string filePath =
        TestSuit::GetTestDataFile("torchnpu", "msprof_3466812.1787275553016492530.pt.trace.json");
    const std::string directory = TestSuit::GetTestDataFile("torchnpu");
    EXPECT_EQ(ParserFactory::GetImportType(filePath).second, ParserType::PYTORCH_TRACE_JSON);
    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::PYTORCH_TRACE_JSON);
    ProjectParserPytorchTrace parser;
    std::string error;
    auto files = parser.GetParseFileByImportFile(directory, error);
    ASSERT_EQ(files.size(), 1);
    EXPECT_EQ(files[0], filePath);
    EXPECT_TRUE(error.empty());
}

TEST_F(ParserFactoryTest, GetImportTypePytorchTraceFileTest) {
    const std::string fileName = "msprof_3466812.1787275553016492530.pt.trace.json";
    const std::string directory = CreateTempDirectoryWithFiles({fileName});
    ASSERT_FALSE(directory.empty());
    const std::string filePath = Dic::FileUtil::SplicePath(directory, fileName);

    EXPECT_EQ(ParserFactory::GetImportType(filePath).second, ParserType::PYTORCH_TRACE_JSON);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserPytorchTrace>(
                  ParserFactory::GetProjectParser(ParserType::PYTORCH_TRACE_JSON)),
        nullptr);
}

TEST_F(ParserFactoryTest, GetImportTypePytorchTraceDirectoryTest) {
    const std::string directory = CreateTempDirectoryWithFiles({"msprof_3466812.1787275553016492530.pt.trace.json"});
    ASSERT_FALSE(directory.empty());

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::PYTORCH_TRACE_JSON);
}

TEST_F(ParserFactoryTest, MixedPytorchTraceAndJsonRoutesToGenericJsonTest) {
    const std::string directory =
        CreateTempDirectoryWithFiles({"first.pt.trace.json", "second.pt.trace.json", "trace_view.json"});
    ASSERT_FALSE(directory.empty());

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::JSON);
}

TEST_F(ParserFactoryTest, GetImportTypeInsightDbBesidePytorchTraceStillRoutesToPytorchTest) {
    const std::string directory =
        CreateStableTempDirectoryWithFiles({"trace.pt.trace.json", "mindstudio_insight_data.db"});
    ASSERT_FALSE(directory.empty());

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::PYTORCH_TRACE_JSON);
}

TEST_F(ParserFactoryTest, GetImportTypeProfilerDbWinsOverPytorchTraceTest) {
    const std::string directory = CreateStableTempDirectoryWithFiles({"trace.pt.trace.json", "msprof_1.db"});
    ASSERT_FALSE(directory.empty());

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::DB);
}

TEST_F(ParserFactoryTest, GetImportTypeMultiplePytorchTraceOnlyRoutesToOtherTest) {
    const std::string directory = CreateTempDirectoryWithFiles({"first.pt.trace.json", "second.pt.trace.json"});
    ASSERT_FALSE(directory.empty());

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::OTHER);
}

TEST_F(ParserFactoryTest, GetImportTypePytorchTraceNestedFileTest) {
    const std::string directory = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(directory.empty());
    const std::string nested = Dic::FileUtil::SplicePath(directory, "rank_0");
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::create_directories(nested, ec));
    std::ofstream file(Dic::FileUtil::SplicePath(nested, "trace.pt.trace.json"));
    ASSERT_TRUE(file.is_open());
    file << R"({"traceEvents":[]})";
    file.close();

    EXPECT_EQ(ParserFactory::GetImportType(directory).second, ParserType::PYTORCH_TRACE_JSON);
}

TEST_F(ParserFactoryTest, GetImportTypeTextClusterTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "cluster_communication.json");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::JSON};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::JSON};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList2;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeOtherTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar", "scalar_data");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "tf.event.out.1");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::OTHER};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar_data")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::OTHER};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, ParserBaseSendFail) {
    ProjectParserBase parser;
    EXPECT_EQ(parser.GetSubId("test", ParseFileType::RANK), "test");
    EXPECT_EQ(parser.GetRankIdFromPath("test", "test/rank0"), "test");
    EXPECT_EQ(ProjectParserBase::GetDbPath("test", 0), "test_0.db");
}

namespace {
namespace Global = Dic::Module::Global;
class CapturingImportSession : public Dic::Module::Timeline::Test::CapturingTimelineSession {
  public:
    void OnEvent(std::unique_ptr<Protocol::Event> event) override { events.emplace_back(std::move(event)); }
    std::vector<std::unique_ptr<Protocol::Event>> events;
};

class FullDbImportUtTest : public ParserFactoryTest {
  protected:
    class TestableParserBase : public ProjectParserBase {
      public:
        using ProjectParserBase::IsNeedReset;
        using ProjectParserBase::MergeFileTree;
    };

    void SetUp() override {
        capture = std::make_shared<CapturingImportSession>();
        previousSnapshotState = MemSnapshotParser::Instance().GetParseContext().GetState();
        auto &sessions = Server::WsSessionManager::Instance();
        std::lock_guard<std::mutex> lock(sessions.sessionMutex);
        previousSession = std::exchange(sessions.session, capture);
    }

    void TearDown() override {
        MemSnapshotParser::Instance().GetParseContext().SetState(previousSnapshotState);
        {
            auto &sessions = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            sessions.session = std::move(previousSession);
        }
        ParserFactoryTest::TearDown();
    }

    std::shared_ptr<Global::ParseFileInfo> Node(const std::string &subId, Global::ParseFileType type) {
        auto node = std::make_shared<Global::ParseFileInfo>();
        node->subId = subId;
        node->type = type;
        return node;
    }

    template <typename Parser>
    void ExpectProjectTree(ProjectTypeEnum type, const std::vector<std::string> &files, bool hasFileId) {
        Global::ProjectExplorerInfo info{};
        info.projectName = "full-db-import-ut";
        info.fileName = CreateStableTempDirectoryWithFiles({});
        ASSERT_FALSE(info.fileName.empty());
        info.projectType = static_cast<int64_t>(type);
        std::vector<std::string> parsedFiles;
        for (const auto &file : files) {
            parsedFiles.push_back(FileUtil::SplicePath(info.fileName, file));
        }
        Parser::BuildProjectExploreInfo(info, parsedFiles);
        ASSERT_EQ(info.projectFileTree.size(), 1U);
        const auto &root = info.projectFileTree.front();
        EXPECT_EQ(root->type, Global::ParseFileType::PROJECT);
        EXPECT_EQ(root->parseFilePath, info.fileName);
        EXPECT_EQ(root->projectType, static_cast<int64_t>(type));
        ASSERT_EQ(root->subParseFile.size(), files.size());
        for (size_t i = 0; i < files.size(); ++i) {
            const auto &child = root->subParseFile[i];
            EXPECT_EQ(child->parseFilePath, parsedFiles[i]);
            EXPECT_EQ(child->type, Global::ParseFileType::RANK);
            EXPECT_EQ(child->curDirName, FileUtil::GetFileName(files[i]));
            if (type == ProjectTypeEnum::DB_FTRACE) {
                EXPECT_EQ(child->subId, FileUtil::GetFileName(files[i]));
                EXPECT_EQ(child->projectType, static_cast<int64_t>(type));
            } else {
                EXPECT_EQ(child->subId, parsedFiles[i]);
                EXPECT_EQ(child->rankId, parsedFiles[i]);
            }
            if (hasFileId) {
                EXPECT_EQ(child->fileId, parsedFiles[i]);
            }
            EXPECT_EQ(info.GetSubParseFileInfo(child->subId, Global::ParseFileType::RANK), child);
        }
    }

    std::shared_ptr<CapturingImportSession> capture;
    std::shared_ptr<Server::WsSession> previousSession;
    ParserState previousSnapshotState = ParserState::INIT;
};

TEST_F(FullDbImportUtTest, FactoryConstructsEachSupportedParserType) {
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserDb>(ParserFactory::GetProjectParser(ParserType::DB)), nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserBin>(ParserFactory::GetProjectParser(ParserType::BIN)), nullptr);
    EXPECT_NE(
        std::dynamic_pointer_cast<ProjectParserJson>(ParserFactory::GetProjectParser(ParserType::ACLGRPAH_DEBUG_JSON)),
        nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserJson>(ParserFactory::GetProjectParser(ParserType::JSON)), nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ParserIE>(ParserFactory::GetProjectParser(ParserType::IE)), nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserDbNPUMonitor>(
                  ParserFactory::GetProjectParser(ParserType::DB_NPUMONITOR)),
        nullptr);
    EXPECT_NE(
        std::dynamic_pointer_cast<ProjectParserMemScope>(ParserFactory::GetProjectParser(ParserType::DB_MEMSCOPE)),
        nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserMemSnapshot>(
                  ParserFactory::GetProjectParser(ParserType::PKL_MEM_SNAPSHOT)),
        nullptr);
    EXPECT_NE(
        std::dynamic_pointer_cast<ProjectParserTriton>(ParserFactory::GetProjectParser(ParserType::TRITON_MEMORY)),
        nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserFtrace>(ParserFactory::GetProjectParser(ParserType::DB_FTRACE)),
        nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<ProjectParserPytorchTrace>(
                  ParserFactory::GetProjectParser(ParserType::PYTORCH_TRACE_JSON)),
        nullptr);
    auto other = ParserFactory::GetProjectParser(ParserType::OTHER);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(typeid(*other), typeid(ProjectParserBase));
    EXPECT_EQ(other->GetProjectType("unused"), ProjectTypeEnum::OTHER);
}

TEST_F(FullDbImportUtTest, ResetDependsOnTransferConflictAndSelectedProject) {
    ImportActionRequest request;
    request.params.projectName = "target";
    request.params.projectAction = ProjectActionEnum::TRANSFER_PROJECT;
    EXPECT_TRUE(TestableParserBase::IsNeedReset(request));
    request.params.projectAction = ProjectActionEnum::ADD_FILE;
    request.params.isConflict = true;
    EXPECT_TRUE(TestableParserBase::IsNeedReset(request));
    request.params.isConflict = false;
    request.projectName = "other";
    EXPECT_TRUE(TestableParserBase::IsNeedReset(request));
    request.projectName = "target";
    EXPECT_FALSE(TestableParserBase::IsNeedReset(request));
    request.projectName.clear();
    EXPECT_FALSE(TestableParserBase::IsNeedReset(request));
}

TEST_F(FullDbImportUtTest, DbPathReplacesExtensionAndAddsDuplicateSuffix) {
    EXPECT_EQ(ProjectParserBase::GetDbPath("trace.json", 1), "trace.db");
    EXPECT_EQ(ProjectParserBase::GetDbPath("trace.json", 2), "trace_2.db");
    EXPECT_EQ(ProjectParserBase::GetDbPath("trace", 1), "trace.db");
    EXPECT_EQ(ProjectParserBase::GetDbPath("trace", 2), "trace_2.db");
}

TEST_F(FullDbImportUtTest, SubIdHandlesEmptyPathDirectoryAndFile) {
    const std::string directory = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(directory.empty());
    EXPECT_TRUE(ProjectParserBase::GetSubId("", Global::ParseFileType::RANK).empty());
    EXPECT_EQ(ProjectParserBase::GetSubId(directory, Global::ParseFileType::PROJECT), directory);
    EXPECT_EQ(ProjectParserBase::GetSubId(FileUtil::SplicePath(directory, "trace.json"), Global::ParseFileType::RANK),
        "trace.json");
}

TEST_F(FullDbImportUtTest, MergeTreePreservesChildOrderAndAppendsDifferentRoots) {
    auto root = Node("same", Global::ParseFileType::PROJECT);
    auto first = Node("first", Global::ParseFileType::RANK);
    auto second = Node("second", Global::ParseFileType::RANK);
    root->subParseFile = {first};
    auto sameRoot = Node("same", Global::ParseFileType::PROJECT);
    sameRoot->subParseFile = {second};
    std::vector<std::shared_ptr<Global::ParseFileInfo>> roots = {root};
    TestableParserBase::MergeFileTree(roots, {sameRoot});
    ASSERT_EQ(roots.size(), 1U);
    EXPECT_EQ(root->subParseFile, (std::vector<std::shared_ptr<Global::ParseFileInfo>>{first, second}));
    auto otherRoot = Node("other", Global::ParseFileType::PROJECT);
    TestableParserBase::MergeFileTree(roots, {otherRoot});
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(roots.back(), otherRoot);
    TestableParserBase::MergeFileTree(roots, {});
    EXPECT_EQ(roots.size(), 2U);
    std::vector<std::shared_ptr<Global::ParseFileInfo>> empty;
    TestableParserBase::MergeFileTree(empty, roots);
    EXPECT_EQ(empty, roots);
}

TEST_F(FullDbImportUtTest, ImportResponseSortsByCardNameLengthThenLexically) {
    auto response = std::make_unique<ImportActionResponse>();
    for (const auto &name : {"rank10", "b", "rank2", "a", "rank1"}) {
        Protocol::Action action{};
        action.cardName = name;
        response->body.result.push_back(action);
    }
    ProjectParserBase::SendImportActionRes(std::move(response));
    const auto *sent = capture->Get<ImportActionResponse>();
    ASSERT_NE(sent, nullptr);
    ASSERT_TRUE(sent->result);
    std::vector<std::string> names;
    for (const auto &action : sent->body.result) {
        names.push_back(action.cardName);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"a", "b", "rank1", "rank2", "rank10"}));
}

TEST_F(FullDbImportUtTest, FtraceFindsOnlyMatchingFilesAndReportsEmptyDirectory) {
    ProjectParserFtrace parser;
    std::string error;
    EXPECT_EQ(parser.GetParseFileByImportFile("ftrace_data.db", error), (std::vector<std::string>{"ftrace_data.db"}));
    EXPECT_TRUE(error.empty());
    const std::string empty = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(empty.empty());
    EXPECT_EQ(parser.GetParseFileByImportFile(empty, error), (std::vector<std::string>{empty}));
    EXPECT_EQ(error, "No ftrace db files found");
    const std::string directory = CreateStableTempDirectoryWithFiles(
        {"ftrace_data.db", "custom_ftrace_data.db", "ftrace_data.json", "ftrace_data.db.bak"});
    ASSERT_FALSE(directory.empty());
    error.clear();
    const auto result = parser.GetParseFileByImportFile(directory, error);
    // The shared finder keeps the lexically greatest matching non-sliced filename in each directory.
    EXPECT_EQ(result, (std::vector<std::string>{FileUtil::SplicePath(directory, "ftrace_data.db")}));
    EXPECT_TRUE(error.empty());
}

TEST_F(FullDbImportUtTest, FtraceDiscoversMatchingFilesAcrossSubdirectories) {
    const std::string directory = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(directory.empty());
    std::set<std::string> expected;
    for (const auto &rank : {"rank0", "rank1"}) {
        const auto child = FileUtil::SplicePath(directory, rank);
        ASSERT_TRUE(std::filesystem::create_directory(child));
        const auto filePath = FileUtil::SplicePath(child, "ftrace_data.db");
        std::ofstream file(filePath);
        ASSERT_TRUE(file.is_open());
        expected.insert(filePath);
        std::ofstream noise(FileUtil::SplicePath(child, "ftrace_data.json"));
        ASSERT_TRUE(noise.is_open());
    }
    ProjectParserFtrace parser;
    std::string error;
    const auto result = parser.GetParseFileByImportFile(directory, error);
    EXPECT_EQ(std::set<std::string>(result.begin(), result.end()), expected);
    EXPECT_TRUE(error.empty());
}

TEST_F(FullDbImportUtTest, MemScopeBuildsEmptySingleAndMultipleFileTrees) {
    ExpectProjectTree<ProjectParserMemScope>(ProjectTypeEnum::DB_MEMSCOPE, {}, true);
    ExpectProjectTree<ProjectParserMemScope>(ProjectTypeEnum::DB_MEMSCOPE, {"first.db"}, true);
    ExpectProjectTree<ProjectParserMemScope>(ProjectTypeEnum::DB_MEMSCOPE, {"first.db", "second.db"}, true);
}

TEST_F(FullDbImportUtTest, SnapshotBuildsEmptySingleAndMultipleFileTrees) {
    ExpectProjectTree<ProjectParserMemSnapshot>(ProjectTypeEnum::PKL_MEM_SNAPSHOT, {}, true);
    ExpectProjectTree<ProjectParserMemSnapshot>(ProjectTypeEnum::PKL_MEM_SNAPSHOT, {"first.pkl"}, true);
    ExpectProjectTree<ProjectParserMemSnapshot>(ProjectTypeEnum::PKL_MEM_SNAPSHOT, {"first.pkl", "second.pkl"}, true);
}

TEST_F(FullDbImportUtTest, TritonBuildsEmptySingleAndMultipleFileTrees) {
    ExpectProjectTree<ProjectParserTriton>(ProjectTypeEnum::TRITON, {}, false);
    ExpectProjectTree<ProjectParserTriton>(ProjectTypeEnum::TRITON, {"first_memory_info.json"}, false);
    ExpectProjectTree<ProjectParserTriton>(
        ProjectTypeEnum::TRITON, {"first_memory_info.json", "second_memory_info.json"}, false);
}

TEST_F(FullDbImportUtTest, FtraceBuildsEmptySingleAndMultipleFileTrees) {
    ExpectProjectTree<ProjectParserFtrace>(ProjectTypeEnum::DB_FTRACE, {}, false);
    ExpectProjectTree<ProjectParserFtrace>(ProjectTypeEnum::DB_FTRACE, {"ftrace_data.db"}, false);
    ExpectProjectTree<ProjectParserFtrace>(
        ProjectTypeEnum::DB_FTRACE, {"ftrace_data.db", "custom_ftrace_data.db"}, false);
}

class FullDbParserValidationUtTest : public FullDbImportUtTest, public testing::WithParamInterface<ParserType> {};

TEST_P(FullDbParserValidationUtTest, EmptyProjectSendsFailureEventWithoutImportActions) {
    auto parser = ParserFactory::GetProjectParser(GetParam());
    ImportActionRequest request;
    ImportActionResponse response;
    parser->Parser({}, request, response);
    EXPECT_TRUE(response.body.result.empty());
    ASSERT_EQ(capture->events.size(), 1U);
    const auto *event = dynamic_cast<ParseFailEvent *>(capture->events.front().get());
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->body.error, "Project explorer info is not existed.");
    EXPECT_TRUE(event->body.rankId.empty());
    EXPECT_TRUE(event->body.dbPath.empty());
}

TEST_P(FullDbParserValidationUtTest, RejectsDirectoryAndReturnsSingleSupportedFile) {
    auto parser = ParserFactory::GetProjectParser(GetParam());
    const std::string directory = CreateStableTempDirectoryWithFiles({});
    ASSERT_FALSE(directory.empty());
    std::string error;
    EXPECT_TRUE(parser->GetParseFileByImportFile(directory, error).empty());
    if (GetParam() != ParserType::TRITON_MEMORY) {
        EXPECT_FALSE(error.empty());
    }
    const std::string file = FileUtil::SplicePath(directory, "memory_info.json");
    error.clear();
    EXPECT_EQ(parser->GetParseFileByImportFile(file, error), (std::vector<std::string>{file}));
    EXPECT_TRUE(error.empty());
    const auto expectedType = GetParam() == ParserType::DB_MEMSCOPE ? ProjectTypeEnum::DB_MEMSCOPE
        : GetParam() == ParserType::PKL_MEM_SNAPSHOT                ? ProjectTypeEnum::PKL_MEM_SNAPSHOT
                                                                    : ProjectTypeEnum::TRITON;
    EXPECT_EQ(parser->GetProjectType(file), expectedType);
}

TEST_P(FullDbParserValidationUtTest, FailedProjectUpdateStopsBeforeAsyncParsing) {
    auto parser = ParserFactory::GetProjectParser(GetParam());
    auto &context = MemSnapshotParser::Instance().GetParseContext();
    const ParserState previousState = context.GetState();
    context.SetState(ParserState::INIT);
    Global::ProjectExplorerInfo project{};
    project.projectType = static_cast<int64_t>(ProjectTypeEnum::OTHER);
    auto root = Node("project", Global::ParseFileType::PROJECT);
    auto rank = Node("rank", Global::ParseFileType::RANK);
    rank->parseFilePath = "synthetic";
    root->subParseFile = {rank};
    project.projectFileTree = {root};
    ImportActionRequest request;
    ImportActionResponse response;
    parser->Parser({project}, request, response);
    context.SetState(previousState);
    EXPECT_FALSE(response.result);
    EXPECT_TRUE(response.body.result.empty());
    EXPECT_TRUE(response.body.reset);
    EXPECT_EQ(response.moduleName, MODULE_TIMELINE);
    if (GetParam() == ParserType::TRITON_MEMORY) {
        EXPECT_TRUE(response.body.isTriton);
        ASSERT_EQ(capture->events.size(), 1U);
        const auto *event = dynamic_cast<ParseFailEvent *>(capture->events.front().get());
        ASSERT_NE(event, nullptr);
        EXPECT_EQ(event->body.error, "Failed to update project in parsing");
    } else {
        EXPECT_TRUE(response.body.isLeaks);
        EXPECT_EQ(rank->rankId, rank->parseFilePath);
    }
}

INSTANTIATE_TEST_SUITE_P(ImportTypes, FullDbParserValidationUtTest,
    testing::Values(ParserType::DB_MEMSCOPE, ParserType::PKL_MEM_SNAPSHOT, ParserType::TRITON_MEMORY));

TEST_F(FullDbImportUtTest, SnapshotRejectsLoadingAndProcessingContexts) {
    ProjectParserMemSnapshot parser;
    auto &context = MemSnapshotParser::Instance().GetParseContext();
    const ParserState previousState = context.GetState();
    for (const auto state : {ParserState::Loading, ParserState::Processing}) {
        context.SetState(state);
        Global::ProjectExplorerInfo project{};
        project.projectType = static_cast<int64_t>(ProjectTypeEnum::PKL_MEM_SNAPSHOT);
        ImportActionRequest request;
        ImportActionResponse response;
        parser.Parser({project}, request, response);
        EXPECT_FALSE(response.result);
        EXPECT_TRUE(response.body.result.empty());
        EXPECT_TRUE(response.body.isLeaks);
        EXPECT_TRUE(capture->events.empty());
        EXPECT_EQ(context.GetState(), state);
    }
    context.SetState(previousState);
}
} // namespace
