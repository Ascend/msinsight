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
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "ProjectParserFtrace.h"
#include "FileUtil.h"
#include "BaselineManager.h"
#include "ConstantDefs.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "JsonFileParserManager.h"
#include "TestSuit.h"
#include "../../timeline/handler/TimelineHandlerTestSupport.h"

using namespace Dic::Module;
using namespace Dic::Module::Global;

class ProjectParserFtraceTest : public testing::Test {};

TEST_F(ProjectParserFtraceTest, FtraceProjectTypeSupportsBaselineCompare) {
    EXPECT_TRUE(IsSupportCompareType(ProjectTypeEnum::DB_FTRACE));
    EXPECT_TRUE(IsComparable(ProjectTypeEnum::DB_FTRACE, ProjectTypeEnum::DB_FTRACE));
}

TEST_F(ProjectParserFtraceTest, FtraceDbFileNameSupportsContainingKeyword) {
    EXPECT_TRUE(ProjectParserFtrace::IsFtraceDbFile("ftrace_data.db"));
    EXPECT_TRUE(ProjectParserFtrace::IsFtraceDbFile("ftrace_data123.db"));
    EXPECT_TRUE(ProjectParserFtrace::IsFtraceDbFile("custom_ftrace_data.db"));
    EXPECT_TRUE(ProjectParserFtrace::IsFtraceDbFile("ftrace_data_custom.db"));

    EXPECT_FALSE(ProjectParserFtrace::IsFtraceDbFile("ftrace_data.json"));
    EXPECT_FALSE(ProjectParserFtrace::IsFtraceDbFile("ftrace.db"));
    EXPECT_FALSE(ProjectParserFtrace::IsFtraceDbFile("ftrace_data.db.bak"));
}

namespace {
using namespace Dic;
using namespace Dic::Module::Timeline;

class FtraceEventSession : public Test::CapturingTimelineSession {
  public:
    void OnEvent(std::unique_ptr<Protocol::Event> event) override {
        std::lock_guard<std::mutex> lock(mutex);
        events.emplace_back(std::move(event));
        condition.notify_all();
    }

    template <typename T> const T *GetEvent() {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto &event : events) {
            if (const auto *result = dynamic_cast<const T *>(event.get())) {
                return result;
            }
        }
        return nullptr;
    }

    template <typename T> bool WaitForEvent() {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, std::chrono::seconds(30), [this]() {
            return std::any_of(events.begin(), events.end(),
                [](const auto &event) { return dynamic_cast<const T *>(event.get()) != nullptr; });
        });
    }

    bool WaitForAllSuccess() { return WaitForEvent<Protocol::AllSuccessEvent>(); }

  private:
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::unique_ptr<Protocol::Event>> events;
};

class FtraceProjectParserTest : public Dic::Module::Timeline::Test::TimelineHandlerDatabaseTest {
  protected:
    void SetUp() override {
        Dic::Module::Timeline::Test::TimelineHandlerDatabaseTest::SetUp();
        ASSERT_FALSE(HasFatalFailure());
        capture = std::make_shared<FtraceEventSession>();
        session = capture;
        {
            auto &sessions = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            sessions.session = capture;
        }
        TraceTime::Instance().Reset();
        ParserStatusManager::Instance().ClearAllParserStatus();
        ParserStatusManager::Instance().NotifyStartParse();
        BaselineManager::Instance().Reset();
        TestSuit::GetRenderEngine();
    }

    void TearDown() override {
        ThreadPool::Instance().WaitForAllTasks();
        // Release the all-success worker even when a pending-case assertion fails.
        ParserStatusManager::Instance().ClearAllParserStatus();
        ParserStatusManager::Instance().NotifyStartParse();
        JsonFileParserManager::ResetAll();
        EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->WaitForAllTasks();
        std::function<void(const std::string, const std::string, bool, const std::string)> callback;
        JsonFileParserManager::GetTraceFileParser().SetParseEndCallBack(callback);
        BaselineManager::Instance().Reset();
        Dic::Module::Timeline::Test::TimelineHandlerDatabaseTest::TearDown();
    }

    std::string CopySample() {
        const auto destination = directory / "ftrace_data.db";
        std::error_code error;
        const bool copied =
            std::filesystem::copy_file(TestSuit::GetTestDataFile("ftrace", "ftrace_data.db"), destination, error);
        EXPECT_FALSE(error) << error.message();
        EXPECT_TRUE(copied);
        if (copied) {
            // Baseline import expects an already parsed database for the current schema version.
            std::recursive_mutex sqlMutex;
            Dic::Module::Database database(sqlMutex);
            EXPECT_TRUE(database.OpenDb(destination.string(), false));
            EXPECT_TRUE(database.SetDataBaseVersion());
        }
        return destination.string();
    }

    void Import(const std::vector<std::string> &files, ImportActionResponse &response) {
        ProjectExplorerInfo project{};
        project.fileName = directory.string();
        project.projectName = "ftrace-parser-ut";
        project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE);
        ProjectParserFtrace::BuildProjectExploreInfo(project, files);
        ImportActionRequest request;
        request.params.path = {directory.string()};
        parser.Parser({project}, request, response);
    }

    int64_t QueryInteger(const std::string &path, const std::string &sql) {
        sqlite3 *raw = nullptr;
        const int opened = sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        EXPECT_EQ(opened, SQLITE_OK);
        if (opened != SQLITE_OK) {
            return -1;
        }
        sqlite3_stmt *rawStatement = nullptr;
        const int prepared = sqlite3_prepare_v2(raw, sql.c_str(), -1, &rawStatement, nullptr);
        std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(rawStatement, sqlite3_finalize);
        EXPECT_EQ(prepared, SQLITE_OK) << sqlite3_errmsg(raw);
        if (prepared != SQLITE_OK) {
            return -1;
        }
        const int step = sqlite3_step(rawStatement);
        EXPECT_EQ(step, SQLITE_ROW) << sqlite3_errmsg(raw);
        return step == SQLITE_ROW ? sqlite3_column_int64(rawStatement, 0) : -1;
    }

    void ExpectParsedSample(const std::string &file) {
        ASSERT_TRUE(capture->WaitForEvent<Protocol::ParseSuccessEvent>()) << "Timed out waiting for Ftrace result";
        ASSERT_TRUE(capture->WaitForAllSuccess()) << "Timed out waiting for Ftrace parsing";
        const auto *event = capture->GetEvent<Protocol::ParseSuccessEvent>();
        ASSERT_NE(event, nullptr);
        EXPECT_TRUE(event->result);
        EXPECT_TRUE(event->body.isFtrace);
        EXPECT_FALSE(event->body.isFullDb);
        EXPECT_EQ(event->body.fileId, file);
        EXPECT_EQ(event->body.unit.metadata.cardId, file);
        EXPECT_EQ(event->body.startTime, 5262008677705000ULL);
        EXPECT_EQ(event->body.maxTimeStamp, 20849111000ULL);
        EXPECT_EQ(event->body.unit.children.size(), 2U);
        EXPECT_EQ(capture->GetEvent<Protocol::ParseFailEvent>(), nullptr);
        EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(file), ParserStatus::FINISH);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM slice"), 244794);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM thread"), 1144);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM slice WHERE name = 'Running'"), 62497);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM slice WHERE name = 'Runnable'"), 61112);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM slice WHERE name = 'irq'"), 1601);
        EXPECT_EQ(QueryInteger(file, "SELECT COUNT(*) FROM slice WHERE name = 'softirq'"), 54633);
    }

    void ParseBaseline(const std::string &importPath, const std::string &file) {
        BaselineInfo baseline{};
        baseline.parsedFilePath = importPath;
        parser.ParserBaseline({}, baseline);
        EXPECT_TRUE(baseline.errorMessage.empty());
        EXPECT_EQ(baseline.rankId, file);
        EXPECT_EQ(baseline.fileId, file);
        EXPECT_EQ(baseline.cardName, "Baseline_" + file);
        EXPECT_TRUE(baseline.isFtrace);
        EXPECT_TRUE(BaselineManager::Instance().IsBaselineRankId(file));
        ExpectParsedSample(file);
    }

    ProjectParserFtrace parser;
    std::shared_ptr<FtraceEventSession> capture;
};

TEST_F(FtraceProjectParserTest, ImportsRealDatabaseAndPreservesSchedulingData) {
    const std::string file = CopySample();
    ASSERT_FALSE(HasFailure());
    ImportActionResponse response{};
    Import({file}, response);

    EXPECT_TRUE(response.result);
    EXPECT_FALSE(response.body.isPending);
    EXPECT_EQ(response.command, Protocol::REQ_RES_IMPORT_ACTION);
    EXPECT_EQ(response.moduleName, MODULE_TIMELINE);
    ASSERT_EQ(response.body.result.size(), 1U);
    EXPECT_EQ(response.body.result[0].rankId, file);
    EXPECT_EQ(response.body.result[0].fileId, file);
    EXPECT_EQ(response.body.result[0].projectType, static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE));
    ASSERT_EQ(response.body.projectFileTree.size(), 1U);
    ASSERT_EQ(response.body.projectFileTree[0]->subParseFile.size(), 1U);
    EXPECT_EQ(response.body.projectFileTree[0]->subParseFile[0]->rankId, file);
    ExpectParsedSample(file);
}

TEST_F(FtraceProjectParserTest, DefersAllFilesAtPendingThreshold) {
    std::vector<std::string> files;
    for (int i = 0; i < PENDIND_CRITICAL_VALUE; ++i) {
        const auto path = directory / ("ftrace_data_" + std::to_string(i) + ".db");
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open());
        files.emplace_back(path.string());
    }
    ImportActionResponse response{};
    Import(files, response);
    ThreadPool::Instance().WaitForAllTasks();

    EXPECT_TRUE(response.result);
    EXPECT_TRUE(response.body.isPending);
    EXPECT_EQ(response.body.result.size(), files.size());
    for (const auto &file : files) {
        const auto pending = ParserStatusManager::Instance().QueryPendingFilePath(file);
        EXPECT_EQ(pending.first, ProjectTypeEnum::DB_FTRACE);
        EXPECT_EQ(pending.second, (std::vector<std::string>{file}));
        EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(file), ParserStatus::UN_KNOW);
    }
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
    ParserStatusManager::Instance().ClearAllParserStatus();
    EXPECT_TRUE(capture->WaitForAllSuccess());
}

TEST_F(FtraceProjectParserTest, SkipsNonFtraceFiles) {
    ImportActionResponse response{};
    Import({(directory / "trace.json").string()}, response);
    EXPECT_TRUE(response.result);
    EXPECT_TRUE(response.body.result.empty());
    ASSERT_TRUE(capture->WaitForAllSuccess());
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
}

TEST_F(FtraceProjectParserTest, SkipsFileWhenConnectionPoolAlreadyExists) {
    const std::string file = CopySample();
    ASSERT_FALSE(HasFailure());
    DataBaseManager::Instance().SetDataType(DataType::TEXT, file);
    ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(file, file));
    ImportActionResponse response{};
    Import({file}, response);
    EXPECT_TRUE(response.result);
    EXPECT_TRUE(response.body.result.empty());
    ASSERT_TRUE(capture->WaitForAllSuccess());
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
}

TEST_F(FtraceProjectParserTest, SkipsFileWhenDatabaseCannotBeOpened) {
    ImportActionResponse response{};
    Import({(directory / "missing" / "ftrace_data.db").string()}, response);
    EXPECT_TRUE(response.result);
    EXPECT_TRUE(response.body.result.empty());
    ASSERT_TRUE(capture->WaitForAllSuccess());
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
}

TEST_F(FtraceProjectParserTest, ParsesBaselineFromRealDatabaseFile) {
    const std::string file = CopySample();
    ASSERT_FALSE(HasFailure());
    ParseBaseline(file, file);
}

TEST_F(FtraceProjectParserTest, FindsAndParsesBaselineInsideDirectory) {
    const std::string file = CopySample();
    ASSERT_FALSE(HasFailure());
    ParseBaseline(directory.string(), file);
}

TEST_F(FtraceProjectParserTest, MissingBaselineReportsErrorWithoutStartingParse) {
    for (const auto &path : {directory.string(), (directory / "trace.json").string()}) {
        BaselineInfo baseline{};
        baseline.parsedFilePath = path;
        parser.ParserBaseline({}, baseline);
        EXPECT_EQ(baseline.errorMessage, "No ftrace db files found");
        EXPECT_TRUE(baseline.rankId.empty());
        EXPECT_TRUE(baseline.fileId.empty());
    }
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
    EXPECT_EQ(capture->GetEvent<Protocol::AllSuccessEvent>(), nullptr);
}

TEST_F(FtraceProjectParserTest, ExistingBaselinePoolStopsBeforeParsing) {
    const std::string file = CopySample();
    ASSERT_FALSE(HasFailure());
    DataBaseManager::Instance().SetDataType(DataType::TEXT, file);
    ASSERT_TRUE(DataBaseManager::Instance().CreateTraceConnectionPool(file, file));
    BaselineInfo baseline{};
    baseline.parsedFilePath = file;
    parser.ParserBaseline({}, baseline);
    EXPECT_EQ(baseline.rankId, file);
    EXPECT_TRUE(baseline.isFtrace);
    EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(file), ParserStatus::UN_KNOW);
    EXPECT_EQ(capture->GetEvent<Protocol::ParseSuccessEvent>(), nullptr);
    EXPECT_EQ(capture->GetEvent<Protocol::AllSuccessEvent>(), nullptr);
}
} // namespace
