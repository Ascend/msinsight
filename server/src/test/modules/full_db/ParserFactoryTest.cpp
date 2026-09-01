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
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "GlobalDefs.h"
#include "ProjectParserFactory.h"
#include "FileUtil.h"
#include "TableDefs.h"

using namespace Dic;
using namespace Dic::Module;

class ParserFactoryTest : public ::testing::Test {};

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
