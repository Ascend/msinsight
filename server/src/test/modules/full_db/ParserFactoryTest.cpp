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
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "GlobalDefs.h"
#include "ProjectParserFactory.h"
#include "ProjectParserPytorchTrace.h"
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
        static std::atomic<int> seq{0};
        std::string directory;
        for (int i = 0; i < 8; ++i) {
            const std::string candidate = Dic::FileUtil::SplicePath(
                ::testing::TempDir(), std::string(testInfo->name()) + "_" + std::to_string(seq.fetch_add(1)));
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
