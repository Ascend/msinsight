/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include "BaselineManager.h"
#include "DataBaseManager.h"
#include "BaselineManagerService.h"
#include "GlobalProtocolRequest.h"
#include "GlobalProtocolResponse.h"
#include "SetBaselineHandler.h"
#include "SystemMemoryDatabase.h"
#include "SystemMemoryDatabaseDef.h"
#include "ParserStatusManager.h"
#include "DataEngine.h"
#include "RepositoryFactory.h"
#include "TrackInfoManager.h"
#define private public
#include "MsprofFtraceImportService.h"
#include "ProjectExplorerManager.h"
#include "RenderEngine.h"
#undef private
#include "../timeline/handler/TimelineHandlerTestSupport.h"
#include "sqlite3.h"

namespace Dic::Module::FullDb {
namespace {
namespace fs = std::filesystem;
constexpr int64_t IMPORT_EPOCH_NS = 1793654244074239046;

class BaselineResponseSession : public Timeline::Test::CapturingTimelineSession {
  public:
    void OnResponse(std::unique_ptr<Protocol::Response> value) override {
        Timeline::Test::CapturingTimelineSession::OnResponse(std::move(value));
        responseSent = true;
    }
    void SendBaseResponse(std::unique_ptr<Protocol::Response> value) override { OnResponse(std::move(value)); }
    void OnEvent(std::unique_ptr<Protocol::Event> event) override {
        if (event->event == Protocol::EVENT_ALL_SUCCESS) {
            ++allPagesSuccess;
            if (!responseSent) {
                completionBeforeResponse = true;
            }
        }
    }
    std::atomic<unsigned> allPagesSuccess{0};
    std::atomic<bool> responseSent{false};
    std::atomic<bool> completionBeforeResponse{false};
};

class ScopedBaselineResponseSession {
  public:
    ScopedBaselineResponseSession() : session(std::make_shared<BaselineResponseSession>()) {
        Protocol::Response previousResponse;
        ModuleRequestHandler::SetResponseErrorFromRequestContext(previousResponse);
        previousError = previousResponse.error.value_or(Protocol::ErrorMessage{});
        ModuleRequestHandler::ResetRequestContextError();
        auto &manager = Server::WsSessionManager::Instance();
        std::lock_guard<std::mutex> lock(manager.sessionMutex);
        previous = std::move(manager.session);
        manager.session = session;
    }
    ~ScopedBaselineResponseSession() {
        auto &manager = Server::WsSessionManager::Instance();
        std::lock_guard<std::mutex> lock(manager.sessionMutex);
        manager.session = std::move(previous);
        ModuleRequestHandler::SetRequestContextError(previousError);
    }
    std::shared_ptr<BaselineResponseSession> session;

  private:
    std::shared_ptr<Server::WsSession> previous;
    Protocol::ErrorMessage previousError;
};

#ifdef _WIN32
class ScopedUnicodeTemporaryDirectory {
  public:
    explicit ScopedUnicodeTemporaryDirectory(const fs::path &path)
        : previousTmp(Read(L"TMP")), previousTemp(Read(L"TEMP")) {
        applied = SetEnvironmentVariableW(L"TMP", path.c_str()) && SetEnvironmentVariableW(L"TEMP", path.c_str());
    }
    ~ScopedUnicodeTemporaryDirectory() {
        EXPECT_TRUE(SetEnvironmentVariableW(L"TMP", previousTmp ? previousTmp->c_str() : nullptr));
        EXPECT_TRUE(SetEnvironmentVariableW(L"TEMP", previousTemp ? previousTemp->c_str() : nullptr));
    }
    bool IsApplied() const { return applied; }

  private:
    static std::optional<std::wstring> Read(const wchar_t *name) {
        const DWORD capacity = GetEnvironmentVariableW(name, nullptr, 0);
        if (capacity == 0) {
            return std::nullopt;
        }
        std::wstring value(capacity, L'\0');
        const DWORD length = GetEnvironmentVariableW(name, value.data(), capacity);
        value.resize(length);
        return value;
    }
    std::optional<std::wstring> previousTmp;
    std::optional<std::wstring> previousTemp;
    bool applied = false;
};

struct ResetBeforeTemporaryDirectoryRestore {
    ~ResetBeforeTemporaryDirectoryRestore() { MsprofFtraceImportService::Instance().Reset(); }
};
#endif

class MsprofFtraceImportServiceTest : public testing::Test {
  protected:
    void SetUp() override {
        previousDataEngine = Timeline::RenderEngine::Instance()->dataEngine;
        auto engine = std::make_shared<Timeline::DataEngine>();
        engine->SetRepositoryFactory(std::make_shared<Timeline::RepositoryFactory>());
        Timeline::RenderEngine::Instance()->SetDataEngineInterface(engine);
        static std::atomic<unsigned> sequence{0};
        directory = fs::temp_directory_path() /
            ("msprof-ftrace-import-" + std::to_string(sequence++) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(fs::create_directory(directory));
        source = directory / "source.db";
        ASSERT_TRUE(Write(R"(
CREATE TABLE META_DATA(name TEXT,value TEXT);
INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace',NULL),('ftrace_events_complete','1');
CREATE TABLE STRING_IDS(id INTEGER PRIMARY KEY,value TEXT);
CREATE TABLE GENERIC_EVENT_SOURCES(sourceId INTEGER PRIMARY KEY,nameId INTEGER,timeSourceId INTEGER,
 sourceGroupId INTEGER,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPES(typeId INTEGER PRIMARY KEY,sourceId INTEGER,nameId INTEGER,ftraceFormat TEXT,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPE_FIELDS(typeId INTEGER,fieldIdx INTEGER,fieldNameId INTEGER,offset INTEGER,
 size INTEGER,type INTEGER,ftracePrefix TEXT,ftraceSuffix TEXT,PRIMARY KEY(typeId,fieldIdx));
CREATE TABLE GENERIC_EVENTS(genericEventId INTEGER PRIMARY KEY,rawTimestamp INTEGER,timestamp INTEGER,
 typeId INTEGER,globalTid INTEGER,cpuId INTEGER,flags INTEGER);
CREATE TABLE GENERIC_EVENT_DATA(genericEventId INTEGER,fieldIdx INTEGER,intVal INTEGER,uintVal INTEGER,
 floatVal REAL,doubleVal REAL,strId INTEGER,PRIMARY KEY(genericEventId,fieldIdx));
CREATE INDEX GenericEventTsIndex ON GENERIC_EVENTS(timestamp);
INSERT INTO STRING_IDS VALUES(1000,'worker_thread_1'),(1,'swapper/8'),(10,'sched'),(11,'irq'),(12,'power'),
 (20,'sched_switch'),(21,'softirq_entry'),(22,'softirq_exit'),(23,'cpu_frequency'),
 (100,'common_pid'),(101,'prev_pid'),(102,'next_pid'),(103,'prev_comm'),(104,'next_comm'),
 (105,'prev_state'),(106,'vec'),(107,'state'),(108,'cpu_id');
INSERT INTO GENERIC_EVENT_SOURCES VALUES(7,10,3,1,NULL),(11,11,3,1,NULL),(13,12,3,1,NULL);
INSERT INTO GENERIC_EVENT_TYPES VALUES(1000,7,20,NULL,NULL),(1100,11,21,NULL,NULL),
 (1101,11,22,NULL,NULL),(1300,13,23,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES
 (1000,0,100,4,4,0,NULL,NULL),(1000,2,101,8,4,0,NULL,NULL),(1000,4,102,12,4,0,NULL,NULL),
 (1000,8,103,16,16,0,NULL,NULL),(1000,9,104,32,16,0,NULL,NULL),(1000,11,105,48,8,0,NULL,NULL),
 (1100,0,100,4,4,0,NULL,NULL),(1100,3,106,8,4,1,NULL,NULL),
 (1101,0,100,4,4,0,NULL,NULL),(1101,3,106,8,4,1,NULL,NULL),
 (1300,0,107,4,4,1,NULL,NULL),(1300,1,108,8,4,1,NULL,NULL);
INSERT INTO GENERIC_EVENTS VALUES
 (1,5000,1793654244074239046,1000,NULL,8,0),
 (2,5003,1793654244074239049,1100,180388626474,8,1),
 (3,5005,1793654244074239051,1101,180388626474,8,0),
 (4,5010,1793654244074239056,1000,180388626474,8,0),
 (5,5011,1793654244074239057,1300,NULL,8,0);
INSERT INTO GENERIC_EVENT_DATA VALUES
 (1,0,0,0,0,0,0),(1,2,0,0,0,0,0),(1,4,42,0,0,0,0),
 (1,8,0,0,0,0,1),(1,9,0,0,0,0,1000),(1,11,0,0,0,0,0),
 (2,0,42,0,0,0,0),(2,3,0,3,0,0,0),
 (3,0,42,0,0,0,0),(3,3,0,3,0,0,0),
 (4,0,42,0,0,0,0),(4,2,42,0,0,0,0),(4,4,0,0,0,0,0),
 (4,8,0,0,0,0,1000),(4,9,0,0,0,0,1),(4,11,1,0,0,0,0),
 (5,0,0,2400000,0,0,0),(5,1,0,8,0,0,0);
)"));
    }

    void TearDown() override {
        auto &service = MsprofFtraceImportService::Instance();
        if (!alias.empty()) {
            service.Remove(alias);
        }
        service.Remove(MsprofFtraceImportService::ResourceId(source.u8string()));
        service.Reset();
        Timeline::RenderEngine::Instance()->SetDataEngineInterface(previousDataEngine);
        if (createdBaseline) {
            Global::BaselineManager::Instance().Reset();
        }
        if (projectStoreInstalled) {
            auto &projects = Global::ProjectExplorerManager::Instance();
            std::lock_guard<std::recursive_mutex> lock(projects.mutex);
            projects.db.reset();
            projects.db = std::move(previousProjectDb);
            projects.systemMemoryDbPath = previousProjectDbPath;
        }
        std::error_code error;
        for (const auto &path : publishedCaches) {
            for (const auto &suffix : {"", "-wal", "-shm"}) {
                fs::remove(fs::u8path(path + suffix), error);
            }
        }
        fs::remove_all(directory, error);
    }

    bool Import(std::string &error) {
        auto &service = MsprofFtraceImportService::Instance();
        const bool result = service.Import(source.u8string(), error, false);
        const auto resource = service.GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
        if (resource.has_value() && !resource->cachePath.empty()) {
            publishedCaches.insert(resource->cachePath);
        }
        return result;
    }

    bool Write(const std::string &sql) { return WriteTo(source.u8string(), sql); }

    static bool WriteTo(const std::string &path, const std::string &sql) {
        sqlite3 *db = nullptr;
        const int opened = sqlite3_open(path.c_str(), &db);
        if (opened != SQLITE_OK) {
            if (db != nullptr) {
                sqlite3_close(db);
            }
            return false;
        }
        char *error = nullptr;
        const bool result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) == SQLITE_OK;
        if (!result) {
            ADD_FAILURE() << (error == nullptr ? "SQLite fixture update failed" : error);
        }
        sqlite3_free(error);
        sqlite3_close(db);
        return result;
    }

    bool AddHardirqEvents() {
        return Write(R"(
INSERT INTO STRING_IDS VALUES(24,'irq_handler_entry'),(25,'irq_handler_exit'),(109,'irq'),(110,'name'),(111,'arch_timer');
INSERT INTO GENERIC_EVENT_TYPES VALUES(1200,11,24,NULL,NULL),(1201,11,25,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES
 (1200,0,100,4,4,0,NULL,NULL),(1200,3,109,8,4,1,NULL,NULL),(1200,4,110,12,32,0,NULL,NULL),
 (1201,0,100,4,4,0,NULL,NULL),(1201,3,109,8,4,1,NULL,NULL);
INSERT INTO GENERIC_EVENTS VALUES
 (6,5006,1793654244074239052,1200,NULL,8,0),(7,5008,1793654244074239054,1201,NULL,8,0);
INSERT INTO GENERIC_EVENT_DATA VALUES
 (6,0,42,0,0,0,0),(6,3,0,27,0,0,0),(6,4,0,0,0,0,111),
 (7,0,42,0,0,0,0),(7,3,0,27,0,0,0);
)");
    }

    static int64_t Scalar(const std::string &path, const std::string &sql) {
        sqlite3 *db = nullptr;
        EXPECT_EQ(sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK);
        if (db == nullptr) {
            return -1;
        }
        sqlite3_stmt *statement = nullptr;
        const int prepared = sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr);
        EXPECT_EQ(prepared, SQLITE_OK) << sqlite3_errmsg(db);
        int64_t result = -1;
        if (prepared == SQLITE_OK) {
            EXPECT_EQ(sqlite3_step(statement), SQLITE_ROW);
            result = sqlite3_column_int64(statement, 0);
        }
        sqlite3_finalize(statement);
        sqlite3_close(db);
        return result;
    }

    static std::string Bytes(const fs::path &path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    static std::string Bytes(const std::string &path) { return Bytes(fs::u8path(path)); }

    bool SaveBaselineProject() {
        auto &projects = Global::ProjectExplorerManager::Instance();
        {
            std::lock_guard<std::recursive_mutex> lock(projects.mutex);
            previousProjectDb = std::move(projects.db);
            previousProjectDbPath = projects.systemMemoryDbPath;
            projects.InitSystemMemoryDbPath(directory.u8string());
            projectStoreInstalled = true;
        }
        Global::ProjectExplorerInfo project;
        project.projectName = "baseline-readiness-project";
        project.fileName = source.u8string();
        project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB);
        project.importType = "import";
        auto root = std::make_shared<Global::ParseFileInfo>();
        root->parseFilePath = project.fileName;
        root->subId = project.fileName;
        root->type = Global::ParseFileType::PROJECT;
        root->projectType = project.projectType;
        project.AddSubParseFileInfo(root);
        auto derived = std::make_shared<Global::ParseFileInfo>();
        derived->fileId = MsprofFtraceImportService::ResourceId(project.fileName);
        derived->parseFilePath = derived->fileId;
        derived->rankId = derived->fileId;
        derived->subId = derived->fileId;
        derived->type = Global::ParseFileType::RANK;
        derived->projectType = static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE);
        project.AddSubParseFileInfo(root->subId, root->type, derived);
        return projects.SaveProjectExplorer(project, false);
    }

    std::unique_ptr<Protocol::BaselineSettingRequest> BaselineRequest() {
        auto request = std::make_unique<Protocol::BaselineSettingRequest>();
        request->projectName = "baseline-readiness-project";
        request->params.projectName = request->projectName;
        request->params.filePath = MsprofFtraceImportService::ResourceId(source.u8string());
        return request;
    }

    void ExpectBusyBaselineAttempt(const std::string &id) {
        auto &service = MsprofFtraceImportService::Instance();
        const auto entry = service.FindEntry(id);
        ASSERT_NE(entry, nullptr);
        std::unique_lock<std::mutex> building(entry->buildMutex);
        createdBaseline = true; // Also clean up if a regression unexpectedly publishes the baseline.
        auto attempt = std::async(std::launch::async, [&] {
            std::string error;
            const bool ready = service.CheckBaselineReady(id, error);
            Global::BaselineInfo info;
            const bool created = service.CreateBaseline(id, info, true);
            return std::make_tuple(ready, error, created, info.errorMessage);
        });
        const auto status = attempt.wait_for(std::chrono::seconds(1));
        // Release before checking the result: a blocking regression must still be able to finish.
        building.unlock();
        const auto result = attempt.get();
        EXPECT_EQ(status, std::future_status::ready);
        EXPECT_FALSE(std::get<0>(result));
        EXPECT_EQ(std::get<1>(result), "Ftrace must finish parsing before selecting a baseline");
        EXPECT_FALSE(std::get<2>(result));
        EXPECT_EQ(std::get<3>(result), "Ftrace must finish parsing before selecting a baseline");
    }

    fs::path directory;
    fs::path source;
    std::string alias;
    bool createdBaseline = false;
    std::set<std::string> publishedCaches;
    std::shared_ptr<Timeline::DataEngineInterface> previousDataEngine;
    bool projectStoreInstalled = false;
    std::unique_ptr<Global::SystemMemoryDatabase> previousProjectDb;
    std::string previousProjectDbPath;
};

TEST_F(MsprofFtraceImportServiceTest, ImportsPrivateTextCacheWithoutRegisteringOrModifyingMainSource) {
    auto &service = MsprofFtraceImportService::Instance();
    const auto sourceBytes = Bytes(source);
    const auto discovered = service.Discover(source.u8string(), "host-1", "cluster-1", "project-1");
    ASSERT_TRUE(discovered.has_value());
    EXPECT_FALSE(discovered->ready);
    EXPECT_NE(discovered->fileId, source.u8string());
    EXPECT_EQ(discovered->sourceFileId, source.u8string());
    EXPECT_EQ(discovered->host, "host-1");
    EXPECT_EQ(discovered->cluster, "cluster-1");
    EXPECT_EQ(discovered->projectName, "project-1");
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource = service.GetResource(discovered->fileId);
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready) << resource->error;
    ASSERT_FALSE(resource->cachePath.empty());
    const auto ranks = Timeline::TrackInfoManager::Instance().GetRankListByFileId(resource->fileId, resource->rankId);
    ASSERT_EQ(ranks.size(), 1);
    EXPECT_EQ(ranks.front().rankName, "Ftrace");
    EXPECT_EQ(ranks.front().host, "host-1");
    EXPECT_EQ(ranks.front().cluster, "cluster-1");
    EXPECT_NE(resource->cachePath, source.u8string());
    EXPECT_EQ(Bytes(source), sourceBytes);
    EXPECT_FALSE(fs::exists(fs::u8path(source.u8string() + "-wal")));
    EXPECT_FALSE(fs::exists(fs::u8path(source.u8string() + "-shm")));
    auto &manager = Timeline::DataBaseManager::Instance();
    EXPECT_EQ(manager.GetDataType(resource->fileId), Timeline::DataType::TEXT);
    auto trace = manager.GetTraceDatabaseByFileId(resource->fileId);
    ASSERT_NE(trace, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<Timeline::TextTraceDatabase>(trace), nullptr);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT COUNT(*) FROM process WHERE process_name IN ('CPU Scheduling','Process Scheduling')"),
        2);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM process"), 2);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT COUNT(*) FROM msprof_ftrace_cache WHERE version='msprof-ftrace-v8'"), 1);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT SUM(duration) FROM slice WHERE track_id IN (SELECT track_id FROM thread WHERE pid='Process "
                  "Scheduling') AND name='Running'"),
        10);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT SUM(duration) FROM slice WHERE name='softirq'"), 2);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT running_ns FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 10);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT soft_irq_duration FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 2);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT cs_count FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 1);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT MIN(timestamp) FROM slice"), IMPORT_EPOCH_NS);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM counter"), 0);
    EXPECT_EQ(Scalar(source.u8string(), "SELECT COUNT(*) FROM sqlite_master WHERE name='slice'"), 0);
    EXPECT_EQ(service.GetCapabilities(resource->fileId), (std::vector<std::string>{"scheduling", "softirq"}));
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, HardirqOnlySourcePublishesStatisticsCapabilityAndPreservesItOnCacheHit) {
    ASSERT_TRUE(Write("DELETE FROM GENERIC_EVENT_DATA; DELETE FROM GENERIC_EVENTS;"));
    ASSERT_TRUE(AddHardirqEvents());
    const auto sourceBytes = Bytes(source);
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto resource = service.GetResource(id);
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready);
    EXPECT_EQ(resource->capabilities, (std::vector<std::string>{"hardirq"}));
    EXPECT_EQ(service.GetCapabilities(id), (std::vector<std::string>{"hardirq"}));
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='irq'"), 1);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT hard_irq_count FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 1);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT hard_irq_duration FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 2);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT COUNT(*) FROM msprof_ftrace_cache WHERE capabilities='hardirq' || char(10)"),
        1);
    const auto cachedBytes = Bytes(resource->cachePath);
    ASSERT_TRUE(Import(error)) << error;
    const auto reused = service.GetResource(id);
    ASSERT_TRUE(reused.has_value());
    EXPECT_EQ(reused->cachePath, resource->cachePath);
    EXPECT_EQ(reused->capabilities, (std::vector<std::string>{"hardirq"}));
    EXPECT_EQ(Bytes(reused->cachePath), cachedBytes);
    EXPECT_EQ(Bytes(source), sourceBytes);
}

TEST_F(MsprofFtraceImportServiceTest, MixedInterruptSourcePublishesBothCapabilitiesAndStatistics) {
    ASSERT_TRUE(AddHardirqEvents());
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource =
        MsprofFtraceImportService::Instance().GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    EXPECT_EQ(resource->capabilities, (std::vector<std::string>{"scheduling", "softirq", "hardirq"}));
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT soft_irq_count FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 1);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT hard_irq_count FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 1);
    EXPECT_EQ(
        Scalar(resource->cachePath, "SELECT hard_irq_duration FROM trace_task_summary WHERE pid=42 AND cpu_id=8"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, RebuildsLegacyCacheWithoutHardirqCapability) {
    ASSERT_TRUE(AddHardirqEvents());
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto original = service.GetResource(id);
    ASSERT_TRUE(original.has_value());
    ASSERT_TRUE(WriteTo(original->cachePath,
        "UPDATE msprof_ftrace_cache SET version='msprof-ftrace-v7',capabilities='scheduling' || char(10) || "
        "'softirq' || char(10)"));
    ASSERT_TRUE(Import(error)) << error;
    const auto rebuilt = service.GetResource(id);
    ASSERT_TRUE(rebuilt.has_value());
    EXPECT_TRUE(rebuilt->ready);
    EXPECT_NE(rebuilt->cachePath, original->cachePath);
    EXPECT_EQ(rebuilt->capabilities, (std::vector<std::string>{"scheduling", "softirq", "hardirq"}));
    EXPECT_EQ(
        Scalar(rebuilt->cachePath, "SELECT COUNT(*) FROM msprof_ftrace_cache WHERE version='msprof-ftrace-v8'"), 1);
}

TEST_F(MsprofFtraceImportServiceTest, ArbitraryMarkerTextDoesNotDetermineReadiness) {
    ASSERT_TRUE(Write("UPDATE META_DATA SET value='{\"events\":[\"sched/sched_switch\"]}' WHERE name='ftrace'"));
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    auto resource = service.GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, BaselineSelectionRejectsAnUnpublishedResourceWithoutWaiting) {
    auto &service = MsprofFtraceImportService::Instance();
    const auto discovered = service.Discover(source.u8string());
    ASSERT_TRUE(discovered.has_value());
    ASSERT_FALSE(discovered->ready);
    ExpectBusyBaselineAttempt(discovered->fileId);
    std::string error;
    EXPECT_FALSE(service.CheckBaselineReady(discovered->fileId, error));
    Global::BaselineInfo info;
    EXPECT_FALSE(service.CreateBaseline(discovered->fileId, info, true));
    EXPECT_EQ(info.errorMessage, "Ftrace must finish parsing before selecting a baseline");
}

TEST_F(MsprofFtraceImportServiceTest, BaselineSelectionRejectsRefreshAndAcceptsThePublishedResourceAfterRelease) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    ASSERT_TRUE(service.GetResource(id)->ready);
    ExpectBusyBaselineAttempt(id);
    error = "previous attempt was busy";
    EXPECT_TRUE(service.CheckBaselineReady(id, error));
    EXPECT_TRUE(error.empty());
    Global::BaselineInfo info;
    ASSERT_TRUE(service.CreateBaseline(id, info, true));
    EXPECT_EQ(info.fileId, id);
    EXPECT_EQ(info.cardName, "Ftrace");
}

TEST_F(MsprofFtraceImportServiceTest, BaselineSelectionRechecksCancelledAndUnpublishedState) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto entry = service.FindEntry(id);
    ASSERT_NE(entry, nullptr);
    ASSERT_TRUE(service.CheckBaselineReady(id, error));
    entry->cancelled = true; // The earlier preflight is not a reservation.
    Global::BaselineInfo cancelled;
    EXPECT_FALSE(service.CreateBaseline(id, cancelled, true));
    EXPECT_FALSE(service.CheckBaselineReady(id, error));
    entry->cancelled = false;
    {
        std::lock_guard<std::mutex> lock(service.mutex_);
        entry->resource.ready = false;
    }
    Global::BaselineInfo unpublished;
    EXPECT_FALSE(service.CreateBaseline(id, unpublished, true));
    EXPECT_EQ(unpublished.errorMessage, "Ftrace must finish parsing before selecting a baseline");
}

TEST_F(MsprofFtraceImportServiceTest, BusyBaselineRequestPreservesTheCurrentBaselineAndSkipsCompletionWait) {
    auto &service = MsprofFtraceImportService::Instance();
    ASSERT_TRUE(service.Discover(source.u8string()).has_value());
    ASSERT_TRUE(SaveBaselineProject());
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto entry = service.FindEntry(id);
    ASSERT_NE(entry, nullptr);
    Global::BaselineInfo previous;
    previous.cardId = "existing-ready-baseline";
    Global::BaselineManager::Instance().SetBaselineInfo(previous);
    createdBaseline = true;
    auto &statusManager = Timeline::ParserStatusManager::Instance();
    statusManager.NotifyStartParse();
    statusManager.SetParserStatus(previous.cardId, Timeline::ParserStatus::FINISH);
    statusManager.SetParserStatus(id, Timeline::ParserStatus::RUNNING);
    ScopedBaselineResponseSession capture;
    std::unique_lock<std::mutex> building(entry->buildMutex);
    auto attempt = std::async(std::launch::async, [&] {
        SetBaselineHandler handler;
        return handler.HandleRequest(BaselineRequest());
    });
    const auto status = attempt.wait_for(std::chrono::seconds(1));
    // A regression must not leave a future blocked in CreateBaseline or SendAllParseSuccess.
    building.unlock();
    statusManager.SetParserStatus(id, Timeline::ParserStatus::FINISH);
    const bool handled = attempt.get();
    statusManager.ClearParserStatus(previous.cardId);
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(handled);
    const auto *response = capture.session->Get<Protocol::BaselineSettingResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body.errorMessage, "Ftrace must finish parsing before selecting a baseline");
    EXPECT_EQ(Global::BaselineManager::Instance().GetBaselineId(), previous.cardId);
    EXPECT_EQ(capture.session->allPagesSuccess.load(), 0);
    EXPECT_EQ(service.GetResource(id)->fileId, id);
}

TEST_F(MsprofFtraceImportServiceTest, SuccessfulBaselineRequestStillSendsCompletionAfterItsResponse) {
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    ASSERT_TRUE(SaveBaselineProject());
    Timeline::ParserStatusManager::Instance().NotifyStartParse();
    ScopedBaselineResponseSession capture;
    createdBaseline = true;
    SetBaselineHandler handler;
    ASSERT_TRUE(handler.HandleRequest(BaselineRequest()));
    const auto *response = capture.session->Get<Protocol::BaselineSettingResponse>();
    ASSERT_NE(response, nullptr);
    EXPECT_TRUE(response->body.errorMessage.empty());
    EXPECT_EQ(response->body.fileId, MsprofFtraceImportService::ResourceId(source.u8string()));
    EXPECT_EQ(capture.session->allPagesSuccess.load(), 1);
    EXPECT_FALSE(capture.session->completionBeforeResponse);
}

TEST_F(MsprofFtraceImportServiceTest, BaselinePinsOldGenerationAcrossRebuildAndSourceRemoval) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto oldPath = service.GetResource(id)->cachePath;
    Global::BaselineInfo baseline;
    ASSERT_TRUE(service.CreateBaseline(id, baseline));
    alias = baseline.fileId;
    EXPECT_EQ(baseline.cardName, "Baseline_Ftrace");
    EXPECT_EQ(baseline.host, service.GetResource(id)->host);
    ASSERT_TRUE(Write("UPDATE STRING_IDS SET value='next_generation_worker' WHERE id=1000"));
    ASSERT_TRUE(Import(error)) << error;
    const auto newPath = service.GetResource(id)->cachePath;
    EXPECT_NE(oldPath, newPath);
    EXPECT_TRUE(fs::exists(fs::u8path(oldPath)));
    EXPECT_EQ(Scalar(oldPath, "SELECT COUNT(*) FROM slice WHERE args LIKE '%next_generation_worker%'"), 0);
    ASSERT_TRUE(service.Remove(id));
    EXPECT_FALSE(fs::exists(fs::u8path(newPath)));
    EXPECT_TRUE(fs::exists(fs::u8path(oldPath)));
    EXPECT_NE(Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(alias), nullptr);
    EXPECT_EQ(Scalar(oldPath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
    ASSERT_TRUE(service.Remove(alias));
    alias.clear();
    EXPECT_FALSE(fs::exists(fs::u8path(oldPath)));
}

TEST_F(MsprofFtraceImportServiceTest, CurrentProjectBaselineReusesThePublishedSourceIdentity) {
    auto &service = MsprofFtraceImportService::Instance();
    auto &manager = Timeline::DataBaseManager::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto resource = service.GetResource(id);
    ASSERT_TRUE(resource.has_value());
    const auto cacheBytes = Bytes(resource->cachePath);
    auto sourceToken = manager.GetTraceDatabasePoolToken(id);
    Timeline::ParserStatusManager::Instance().SetParserStatus(id, Timeline::ParserStatus::FINISH_ALL);

    Global::BaselineInfo baseline;
    ASSERT_TRUE(service.CreateBaseline(id, baseline, true));
    createdBaseline = true;
    EXPECT_EQ(baseline.fileId, id);
    EXPECT_EQ(baseline.rankId, id);
    EXPECT_EQ(baseline.cardId, id);
    EXPECT_EQ(baseline.cardName, "Ftrace");
    EXPECT_TRUE(baseline.isFtrace);
    EXPECT_EQ(Global::BaselineManager::Instance().GetBaselineId(), id);
    EXPECT_EQ(Timeline::ParserStatusManager::Instance().GetParserStatus(id), Timeline::ParserStatus::FINISH_ALL);
    EXPECT_FALSE(sourceToken.expired());
    EXPECT_EQ(service.GetResource(id)->cachePath, resource->cachePath);
    EXPECT_EQ(Bytes(resource->cachePath), cacheBytes);
}

TEST_F(MsprofFtraceImportServiceTest, ResetCancelsWorkerWaitingForImportResponse) {
    auto &service = MsprofFtraceImportService::Instance();
    ASSERT_TRUE(service.Discover(source.u8string()).has_value());
    Timeline::ParserStatusManager::Instance().ResetParse();
    service.Schedule(source.u8string());
    service.Reset();
    EXPECT_FALSE(service.IsResource(MsprofFtraceImportService::ResourceId(source.u8string())));
    EXPECT_NE(Timeline::ParserStatusManager::Instance().GetParserStatus(
                  MsprofFtraceImportService::ResourceId(source.u8string())),
        Timeline::ParserStatus::INIT);
    Timeline::ParserStatusManager::Instance().NotifyStartParse();
}

TEST_F(MsprofFtraceImportServiceTest, ProjectClearRetainsCompletedCacheWithoutKeepingLiveResources) {
    auto &service = MsprofFtraceImportService::Instance();
    auto &manager = Timeline::DataBaseManager::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto path = service.GetResource(id)->cachePath;
    ASSERT_TRUE(WriteTo(path, "CREATE TABLE reuse_probe(value INTEGER); INSERT INTO reuse_probe VALUES(77)"));

    manager.Clear();
    EXPECT_FALSE(service.GetResource(id).has_value());
    EXPECT_TRUE(manager.GetTraceDatabasePoolToken(id).expired());
    EXPECT_TRUE(fs::exists(fs::u8path(path)));
    ASSERT_TRUE(Import(error)) << error;
    EXPECT_EQ(service.GetResource(id)->cachePath, path);
    // An extra table survives reuse, but would disappear even if a rebuild reused the same filename.
    EXPECT_EQ(Scalar(path, "SELECT value FROM reuse_probe"), 77);
    EXPECT_EQ(Scalar(path, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, RetainedCacheStillValidatesSourceAndRetiresObsoleteGeneration) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto previous = service.GetResource(id)->cachePath;
    ASSERT_TRUE(WriteTo(previous, "CREATE TABLE reuse_probe(value INTEGER)"));
    ASSERT_TRUE(service.Remove(id, true));
    ASSERT_TRUE(fs::exists(fs::u8path(previous)));

    ASSERT_TRUE(Write("UPDATE STRING_IDS SET value='updated_retained_worker' WHERE id=1000"));
    ASSERT_TRUE(Import(error)) << error;
    const auto current = service.GetResource(id)->cachePath;
    EXPECT_NE(current, previous);
    EXPECT_FALSE(fs::exists(fs::u8path(previous)));
    EXPECT_EQ(Scalar(current, "SELECT COUNT(*) FROM sqlite_master WHERE name='reuse_probe'"), 0);
    EXPECT_GT(Scalar(current, "SELECT COUNT(*) FROM slice WHERE args LIKE '%updated_retained_worker%'"), 0);
}

TEST_F(MsprofFtraceImportServiceTest, ExplicitRemovalAndResetDiscardInactiveCompletedCaches) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto first = service.GetResource(id)->cachePath;
    ASSERT_TRUE(service.Remove(id, true));
    ASSERT_TRUE(fs::exists(fs::u8path(first)));
    ASSERT_TRUE(service.Remove(id));
    EXPECT_FALSE(fs::exists(fs::u8path(first)));

    ASSERT_TRUE(Import(error)) << error;
    const auto second = service.GetResource(id)->cachePath;
    ASSERT_TRUE(service.Remove(id, true));
    ASSERT_TRUE(fs::exists(fs::u8path(second)));
    service.Reset();
    EXPECT_FALSE(fs::exists(fs::u8path(second)));
    EXPECT_TRUE(fs::exists(source));
}

TEST_F(MsprofFtraceImportServiceTest, IdleCacheLimitEvictsOldestSourceWithoutRemovingPinnedBaseline) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto pinned = service.GetResource(id)->cachePath;
    Global::BaselineInfo baseline;
    ASSERT_TRUE(service.CreateBaseline(id, baseline));
    alias = baseline.fileId;
    createdBaseline = true;
    ASSERT_TRUE(service.Remove(id, true));

    std::vector<std::string> idlePaths;
    for (unsigned index = 0; index < 5; ++index) {
        const auto copy = directory / ("idle-" + std::to_string(index) + ".db");
        ASSERT_TRUE(fs::copy_file(source, copy));
        ASSERT_TRUE(service.Import(copy.u8string(), error, false)) << error;
        const auto copyId = MsprofFtraceImportService::ResourceId(copy.u8string());
        const auto path = service.GetResource(copyId)->cachePath;
        publishedCaches.insert(path);
        idlePaths.push_back(path);
        ASSERT_TRUE(service.Remove(copyId, true));
    }
    // Only four idle source entries are retained. The pinned generation outlives LRU eviction.
    EXPECT_FALSE(fs::exists(fs::u8path(idlePaths.front())));
    for (size_t index = 1; index < idlePaths.size(); ++index) {
        EXPECT_TRUE(fs::exists(fs::u8path(idlePaths[index])));
    }
    EXPECT_TRUE(fs::exists(fs::u8path(pinned)));
    EXPECT_EQ(Scalar(pinned, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
    ASSERT_TRUE(service.Remove(alias));
    alias.clear();
    EXPECT_FALSE(fs::exists(fs::u8path(pinned)));
}

TEST_F(MsprofFtraceImportServiceTest, CacheHitDoesNotWaitForAnExistingQueryLease) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    auto lease = Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(id);
    ASSERT_NE(lease, nullptr);
    auto result = std::async(std::launch::async, [&] {
        std::string importError;
        return service.Import(source.u8string(), importError, false);
    });
    const auto status = result.wait_for(std::chrono::seconds(5));
    lease.reset(); // Always release before asserting, so a regression cannot hang test teardown.
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(result.get());
}

TEST_F(MsprofFtraceImportServiceTest, RebuildsAfterSourceContentChangesAndPreservesSourceBytes) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto previous = service.GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(previous.has_value());
    ASSERT_TRUE(Import(error)) << error;
    EXPECT_EQ(service.GetResource(previous->fileId)->cachePath, previous->cachePath);
    ASSERT_TRUE(Write("UPDATE STRING_IDS SET value='renamed_worker_with_complete_name' WHERE id=1000"));
    const auto changedSourceBytes = Bytes(source);
    ASSERT_TRUE(Import(error)) << error;
    const auto resource = service.GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready);
    EXPECT_EQ(Bytes(source), changedSourceBytes);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT COUNT(*) FROM slice WHERE track_id IN (SELECT track_id FROM thread WHERE pid='Process "
                  "Scheduling') AND name='Running' "
                  "AND args LIKE '%renamed_worker_with_complete_name%'"),
        1);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, DecodeFailureFlagInvalidatesCacheWithoutChangingSourceEvents) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto original = service.GetResource(id);
    ASSERT_TRUE(original.has_value());
    ASSERT_TRUE(Write("UPDATE GENERIC_EVENTS SET flags=2 WHERE genericEventId=4"));
    const auto sourceBytes = Bytes(source);
    ASSERT_TRUE(Import(error)) << error;
    const auto changed = service.GetResource(id);
    ASSERT_TRUE(changed.has_value());
    EXPECT_NE(changed->cachePath, original->cachePath);
    EXPECT_EQ(Scalar(changed->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 1);
    EXPECT_EQ(Scalar(changed->cachePath, "SELECT COUNT(*) FROM slice WHERE name='Running'"), 0);
    EXPECT_EQ(Scalar(changed->cachePath, "SELECT COUNT(*) FROM slice WHERE name='softirq'"), 1);
    EXPECT_EQ(Scalar(source.u8string(), "SELECT COUNT(*) FROM GENERIC_EVENTS"), 5);
    EXPECT_EQ(Bytes(source), sourceBytes);
    ASSERT_TRUE(Import(error)) << error;
    EXPECT_EQ(service.GetResource(id)->cachePath, changed->cachePath);
    ASSERT_TRUE(Write("UPDATE GENERIC_EVENTS SET flags=0 WHERE genericEventId=4"));
    ASSERT_TRUE(Import(error)) << error;
    EXPECT_EQ(Scalar(service.GetResource(id)->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceImportServiceTest, FailedRowsWithInvalidStringPlaceholdersDoNotManufactureTracksOrCapabilities) {
    ASSERT_TRUE(Write("UPDATE GENERIC_EVENTS SET flags=flags|2;"
                      "UPDATE GENERIC_EVENT_DATA SET strId=999999"));
    const auto sourceBytes = Bytes(source);
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource =
        MsprofFtraceImportService::Instance().GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    EXPECT_TRUE(resource->ready);
    EXPECT_TRUE(resource->capabilities.empty());
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice"), 0);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM thread"), 0);
    EXPECT_EQ(Scalar(source.u8string(), "SELECT COUNT(*) FROM GENERIC_EVENTS"), 5);
    EXPECT_EQ(Bytes(source), sourceBytes);
}

TEST_F(MsprofFtraceImportServiceTest, ImportsConcreteRealSchemaProfileWithCompositeUniqueIndexes) {
    ASSERT_TRUE(Write(R"(
UPDATE META_DATA SET value='1.2.0' WHERE name='SCHEMA_VERSION';
ALTER TABLE GENERIC_EVENT_TYPE_FIELDS RENAME TO old_fields;
CREATE TABLE GENERIC_EVENT_TYPE_FIELDS AS SELECT * FROM old_fields;
DROP TABLE old_fields;
CREATE UNIQUE INDEX UxGenericEventTypeFields ON GENERIC_EVENT_TYPE_FIELDS(typeId,fieldIdx);
ALTER TABLE GENERIC_EVENT_DATA RENAME TO old_data;
CREATE TABLE GENERIC_EVENT_DATA AS SELECT * FROM old_data;
DROP TABLE old_data;
CREATE UNIQUE INDEX UxGenericEventData ON GENERIC_EVENT_DATA(genericEventId,fieldIdx);
)"));
    const auto sourceBytes = Bytes(source);
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource =
        MsprofFtraceImportService::Instance().GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    EXPECT_TRUE(resource->ready);
    EXPECT_EQ(resource->inspection.schemaVersion, "1.2.0");
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
    EXPECT_EQ(Bytes(source), sourceBytes);
}

TEST_F(MsprofFtraceImportServiceTest, PowerSourceRecordsDoNotAddDisplayGroupsOrChangeMainFrequency) {
    ASSERT_TRUE(Write("CREATE TABLE CPU_FREQ(cpuId INTEGER,timestamp INTEGER,frequency INTEGER);"
                      "INSERT INTO CPU_FREQ VALUES(8,1793654244074239046,2400000)"));
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource = service.GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM counter"), 0);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT COUNT(*) FROM process WHERE process_name NOT IN ('CPU Scheduling','Process Scheduling')"),
        0);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='power:cpu_frequency'"), 0);
    EXPECT_EQ(resource->capabilities, (std::vector<std::string>{"scheduling", "softirq"}));
    EXPECT_EQ(Scalar(source.u8string(), "SELECT COUNT(*) FROM CPU_FREQ"), 1);
    EXPECT_EQ(Scalar(source.u8string(), "SELECT COUNT(*) FROM GENERIC_EVENTS WHERE typeId=1300"), 1);
}

TEST_F(MsprofFtraceImportServiceTest, UnsupportedEventChangesDoNotInvalidateSchedulingCache) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto original = service.GetResource(id);
    ASSERT_TRUE(original.has_value());
    const auto cacheBytes = Bytes(original->cachePath);
    ASSERT_TRUE(Write(R"(
UPDATE GENERIC_EVENT_DATA SET uintVal=3.5,intVal=1,strId=999999 WHERE genericEventId=5;
UPDATE GENERIC_EVENT_TYPE_FIELDS SET type=999,fieldNameId=999999 WHERE typeId=1300;
UPDATE GENERIC_EVENTS SET timestamp='unused time',cpuId=-1 WHERE genericEventId=5;
INSERT INTO STRING_IDS VALUES(500,'sched_migrate_task');
INSERT INTO GENERIC_EVENT_TYPES VALUES(1500,7,500,'unused format',NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES(1500,0,999999,-1,-1,999,NULL,NULL);
INSERT INTO GENERIC_EVENTS VALUES(6,-1,'unused time',1500,NULL,NULL,0);
INSERT INTO GENERIC_EVENT_DATA VALUES(6,0,1,2,0,0,999999);
)"));
    const auto changedSource = Bytes(source);
    ASSERT_TRUE(Import(error)) << error;
    const auto reused = service.GetResource(id);
    ASSERT_TRUE(reused.has_value());
    EXPECT_EQ(reused->cachePath, original->cachePath);
    EXPECT_EQ(Bytes(reused->cachePath), cacheBytes);
    EXPECT_TRUE(reused->inspection.IsReady());
    EXPECT_EQ(reused->capabilities, (std::vector<std::string>{"scheduling", "softirq"}));
    EXPECT_EQ(Bytes(source), changedSource);
}

TEST_F(MsprofFtraceImportServiceTest, PowerOnlySourceRemainsValidWithoutManufacturingSchedulingTracks) {
    ASSERT_TRUE(Write("DELETE FROM GENERIC_EVENT_DATA WHERE genericEventId<>5;"
                      "DELETE FROM GENERIC_EVENTS WHERE genericEventId<>5;"));
    const auto sourceBytes = Bytes(source);
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto resource =
        MsprofFtraceImportService::Instance().GetResource(MsprofFtraceImportService::ResourceId(source.u8string()));
    ASSERT_TRUE(resource.has_value());
    EXPECT_TRUE(resource->ready);
    EXPECT_TRUE(resource->inspection.IsReady());
    EXPECT_TRUE(resource->capabilities.empty());
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM process"), 0);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice"), 0);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM counter"), 0);
    EXPECT_EQ(Bytes(source), sourceBytes);
}

TEST_F(MsprofFtraceImportServiceTest, FailedRebuildDoesNotReplacePreviouslyPublishedCache) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const std::string id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto original = service.GetResource(id);
    ASSERT_TRUE(original.has_value());
    ASSERT_TRUE(original->ready);
    const auto oldCacheBytes = Bytes(original->cachePath);
    ASSERT_TRUE(Write("UPDATE GENERIC_EVENT_DATA SET strId=987654321 WHERE genericEventId=1 AND fieldIdx=9"));
    const auto invalidSourceBytes = Bytes(source);
    EXPECT_FALSE(Import(error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(fs::exists(fs::u8path(original->cachePath)));
    EXPECT_EQ(Bytes(original->cachePath), oldCacheBytes);
    EXPECT_EQ(Bytes(source), invalidSourceBytes);
    EXPECT_EQ(Scalar(original->cachePath,
                  "SELECT SUM(duration) FROM slice WHERE track_id IN (SELECT track_id FROM thread WHERE pid='Process "
                  "Scheduling') AND name='Running'"),
        10);
}

TEST_F(MsprofFtraceImportServiceTest, RemovingBaselineAliasKeepsMainResourceAndPublishedTextConnection) {
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const std::string id = MsprofFtraceImportService::ResourceId(source.u8string());
    Global::BaselineInfo info;
    ASSERT_TRUE(service.CreateBaseline(id, info));
    createdBaseline = true;
    ASSERT_TRUE(info.isFtrace);
    EXPECT_EQ(info.cardName, "Baseline_Ftrace");
    ASSERT_FALSE(info.fileId.empty());
    EXPECT_NE(info.fileId, id);
    alias = info.fileId;
    auto &manager = Timeline::DataBaseManager::Instance();
    ASSERT_NE(manager.GetTraceDatabaseByFileId(info.fileId), nullptr);
    ASSERT_TRUE(service.Remove(info.fileId));
    alias.clear();
    EXPECT_TRUE(service.IsResource(id));
    const auto resource = service.GetResource(id);
    ASSERT_TRUE(resource.has_value());
    EXPECT_TRUE(resource->ready);
    EXPECT_NE(manager.GetTraceDatabaseByFileId(id), nullptr);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

#ifdef _WIN32
TEST_F(MsprofFtraceImportServiceTest, ChineseTemporaryDirectorySupportsCacheReuseFailureAndBaselineCleanup) {
    const auto temporary = directory / fs::path(L"\u4e2d\u6587\u4e34\u65f6\u76ee\u5f55");
    ASSERT_TRUE(fs::create_directory(temporary));
    ScopedUnicodeTemporaryDirectory environment(temporary);
    ASSERT_TRUE(environment.IsApplied());
    ResetBeforeTemporaryDirectoryRestore reset;
    ASSERT_TRUE(fs::equivalent(fs::temp_directory_path(), temporary));
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(Import(error)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto original = service.GetResource(id);
    ASSERT_TRUE(original.has_value());
    const auto oldPath = fs::u8path(original->cachePath);
    ASSERT_TRUE(fs::exists(oldPath));
    EXPECT_EQ(oldPath.parent_path().parent_path().parent_path(), temporary);
    const auto originalBytes = Bytes(oldPath);
    ASSERT_TRUE(Import(error)) << error;
    EXPECT_EQ(service.GetResource(id)->cachePath, original->cachePath);
    EXPECT_EQ(Bytes(oldPath), originalBytes);

    const auto filesInCache = [&]() {
        std::set<fs::path> files;
        for (const auto &entry : fs::directory_iterator(oldPath.parent_path())) {
            files.insert(entry.path().filename());
        }
        return files;
    };
    const auto beforeFailure = filesInCache();
    ASSERT_TRUE(Write("UPDATE GENERIC_EVENT_DATA SET strId=987654321 WHERE genericEventId=1 AND fieldIdx=9"));
    EXPECT_FALSE(Import(error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(filesInCache(), beforeFailure);
    EXPECT_EQ(Bytes(oldPath), originalBytes);
    EXPECT_TRUE(service.GetResource(id)->ready);

    ASSERT_TRUE(Write("UPDATE GENERIC_EVENT_DATA SET strId=1000 WHERE genericEventId=1 AND fieldIdx=9"));
    Global::BaselineInfo baseline;
    ASSERT_TRUE(service.CreateBaseline(id, baseline));
    alias = baseline.fileId;
    createdBaseline = true;
    ASSERT_TRUE(Write("UPDATE STRING_IDS SET value='next_generation_worker' WHERE id=1000"));
    ASSERT_TRUE(Import(error)) << error;
    const auto newPath = fs::u8path(service.GetResource(id)->cachePath);
    EXPECT_NE(oldPath, newPath);
    EXPECT_TRUE(fs::exists(oldPath));
    ASSERT_TRUE(service.Remove(id));
    EXPECT_FALSE(fs::exists(newPath));
    EXPECT_TRUE(fs::exists(oldPath));
    ASSERT_TRUE(service.Remove(alias));
    alias.clear();
    EXPECT_FALSE(fs::exists(oldPath));
    EXPECT_TRUE(fs::is_empty(oldPath.parent_path()));
}
#endif

}
}
