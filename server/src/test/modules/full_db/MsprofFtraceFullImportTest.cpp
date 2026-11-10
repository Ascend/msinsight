/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include "BaselineManager.h"
#include "BaselineManagerService.h"
#include "DataBaseManager.h"
#include "DataEngine.h"
#include "FullDbParser.h"
#include "EventNotifyThreadPoolExecutor.h"
#include "MsprofFtraceImportService.h"
#include "ParserStatusManager.h"
#include "ProjectParserDb.h"
#include "RepositoryFactory.h"
#define private public
#include "RenderEngine.h"
#include "ProjectExplorerManager.h"
#undef private
#include "../timeline/handler/TimelineHandlerTestSupport.h"
#include "sqlite3.h"

namespace Dic::Module::FullDb {
namespace {
namespace fs = std::filesystem;

class MsprofFtraceFullImportTest : public testing::Test {
  protected:
    void SetUp() override {
        MsprofFtraceImportService::Instance().Reset();
        FullDbParser::Instance().Reset();
        directory = fs::temp_directory_path() /
            ("msprof-full-import-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(fs::create_directory(directory));
        original =
            fs::path(__FILE__).parent_path().parent_path().parent_path() / "test_data" / "full_db" / "msprof_0.db";
        ASSERT_TRUE(fs::exists(original));
        source = directory / "msprof_0.db";
        const auto staging = directory / "snapshot.db";
        for (const auto &suffix : {"", "-wal", "-shm"}) {
            const auto path = fs::path(original.string() + suffix);
            originalBytes[suffix] = fs::exists(path) ? Bytes(path) : std::string{};
            originalExists[suffix] = fs::exists(path);
            if (originalExists[suffix]) {
                fs::copy_file(path, staging.string() + suffix);
            }
        }
        sqlite3 *input = nullptr;
        sqlite3 *output = nullptr;
        ASSERT_EQ(sqlite3_open(staging.string().c_str(), &input), SQLITE_OK);
        ASSERT_EQ(sqlite3_open(source.string().c_str(), &output), SQLITE_OK);
        auto backup = sqlite3_backup_init(output, "main", input, "main");
        ASSERT_NE(backup, nullptr);
        ASSERT_EQ(sqlite3_backup_step(backup, -1), SQLITE_DONE);
        ASSERT_EQ(sqlite3_backup_finish(backup), SQLITE_OK);
        sqlite3_close(input);
        sqlite3_close(output);
        epoch = Scalar(source.string(), "SELECT MIN(startNs) FROM CANN_API");
        ASSERT_GT(epoch, 0);
        ASSERT_TRUE(InjectFtrace());

        auto &projects = Global::ProjectExplorerManager::Instance();
        previousProjectDb = std::move(projects.db);
        previousProjectDbPath = projects.systemMemoryDbPath;
        projects.InitSystemMemoryDbPath(directory.string());
        projectStateInstalled = true;
        previousEngine = Timeline::RenderEngine::Instance()->dataEngine;
        auto engine = std::make_shared<Timeline::DataEngine>();
        engine->SetRepositoryFactory(std::make_shared<Timeline::RepositoryFactory>());
        Timeline::RenderEngine::Instance()->SetDataEngineInterface(engine);
        session = std::make_shared<Timeline::Test::CapturingTimelineSession>();
        {
            auto &sessions = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            previousSession = std::move(sessions.session);
            sessions.session = session;
        }
        sessionInstalled = true;
    }

    void TearDown() override {
        Timeline::ParserStatusManager::Instance().NotifyStartParse();
        Global::BaselineManagerService::ResetBaseline(true);
        FullDbParser::Instance().Reset();
        MsprofFtraceImportService::Instance().Reset();
        // ProjectParserDb also schedules ParsePostProcess on a separate notification pool.
        // Drain it while this fixture's session, engine and project database are still alive;
        // otherwise it can first initialize JsonParseMemPool during Windows static destruction.
        Timeline::EventNotifyThreadPoolExecutor::Instance().GetThreadPool()->WaitForAllTasks();
        if (sessionInstalled) {
            auto &sessions = Server::WsSessionManager::Instance();
            std::lock_guard<std::mutex> lock(sessions.sessionMutex);
            sessions.session = std::move(previousSession);
            Timeline::RenderEngine::Instance()->SetDataEngineInterface(previousEngine);
        }
        if (projectStateInstalled) {
            auto &projects = Global::ProjectExplorerManager::Instance();
            projects.db.reset();
            projects.db = std::move(previousProjectDb);
            projects.systemMemoryDbPath = previousProjectDbPath;
        }
        for (const auto &entry : originalBytes) {
            const auto path = fs::path(original.string() + entry.first);
            EXPECT_EQ(fs::exists(path), originalExists[entry.first]);
            if (originalExists[entry.first]) {
                EXPECT_EQ(Bytes(path), entry.second);
            }
        }
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }

    static std::string Bytes(const fs::path &path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    static int64_t Scalar(const std::string &path, const std::string &sql) {
        sqlite3 *db = nullptr;
        if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            if (db) {
                sqlite3_close(db);
            }
            return -1;
        }
        sqlite3_stmt *statement = nullptr;
        int64_t result = -1;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK &&
            sqlite3_step(statement) == SQLITE_ROW) {
            result = sqlite3_column_int64(statement, 0);
        }
        sqlite3_finalize(statement);
        sqlite3_close(db);
        return result;
    }

    bool InjectFtrace() {
        sqlite3 *db = nullptr;
        if (sqlite3_open(source.string().c_str(), &db) != SQLITE_OK) {
            return false;
        }
        const std::string sql = R"(
CREATE TABLE META_DATA(name TEXT,value TEXT);
INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace',NULL),('ftrace_events_complete','1');
CREATE TABLE GENERIC_EVENT_SOURCES(sourceId INTEGER PRIMARY KEY,nameId INTEGER,timeSourceId INTEGER,
 sourceGroupId INTEGER,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPES(typeId INTEGER PRIMARY KEY,sourceId INTEGER,nameId INTEGER,ftraceFormat TEXT,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPE_FIELDS(typeId INTEGER,fieldIdx INTEGER,fieldNameId INTEGER,offset INTEGER,
 size INTEGER,type INTEGER,ftracePrefix TEXT,ftraceSuffix TEXT,PRIMARY KEY(typeId,fieldIdx));
CREATE TABLE GENERIC_EVENTS(genericEventId INTEGER PRIMARY KEY,rawTimestamp INTEGER,timestamp INTEGER,
 typeId INTEGER,globalTid INTEGER,cpuId INTEGER,flags INTEGER);
CREATE TABLE GENERIC_EVENT_DATA(genericEventId INTEGER,fieldIdx INTEGER,intVal INTEGER,uintVal INTEGER,
 floatVal REAL,doubleVal REAL,strId INTEGER,PRIMARY KEY(genericEventId,fieldIdx));
INSERT INTO STRING_IDS VALUES(4000001,'sched'),(4000002,'irq'),(4000003,'sched_switch'),
 (4000004,'softirq_entry'),(4000005,'softirq_exit'),(4000006,'swapper/8'),
 (4000007,'full_import_worker_complete_name'),(4000010,'common_pid'),(4000011,'prev_pid'),
 (4000012,'next_pid'),(4000013,'prev_comm'),(4000014,'next_comm'),(4000015,'prev_state'),(4000016,'vec');
INSERT INTO GENERIC_EVENT_SOURCES VALUES(7,4000001,3,1,NULL),(8,4000002,3,1,NULL);
INSERT INTO GENERIC_EVENT_TYPES VALUES(1000,7,4000003,NULL,NULL),(1001,8,4000004,NULL,NULL),
 (1002,8,4000005,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES
 (1000,0,4000010,4,4,0,NULL,NULL),(1000,1,4000011,8,4,0,NULL,NULL),
 (1000,2,4000012,12,4,0,NULL,NULL),(1000,3,4000013,16,16,0,NULL,NULL),
 (1000,4,4000014,32,16,0,NULL,NULL),(1000,5,4000015,48,8,0,NULL,NULL),
 (1001,0,4000010,4,4,0,NULL,NULL),(1001,1,4000016,8,4,1,NULL,NULL),
 (1002,0,4000010,4,4,0,NULL,NULL),(1002,1,4000016,8,4,1,NULL,NULL);
INSERT INTO GENERIC_EVENT_DATA VALUES
 (1,0,0,0,0,0,0),(1,1,0,0,0,0,0),(1,2,42,0,0,0,0),
 (1,3,0,0,0,0,4000006),(1,4,0,0,0,0,4000007),(1,5,0,0,0,0,0),
 (2,0,42,0,0,0,0),(2,1,0,3,0,0,0),
 (3,0,42,0,0,0,0),(3,1,0,3,0,0,0),
 (4,0,42,0,0,0,0),(4,1,42,0,0,0,0),(4,2,0,0,0,0,0),
 (4,3,0,0,0,0,4000007),(4,4,0,0,0,0,4000006),(4,5,1,0,0,0,0);
)" + std::string("INSERT INTO GENERIC_EVENTS VALUES(1,1000,") +
            std::to_string(epoch + 1000) + ",1000,NULL,8,0),(2,2000," + std::to_string(epoch + 2000) +
            ",1001,180388626474,8,0),(3,3000," + std::to_string(epoch + 3000) + ",1002,180388626474,8,0),(4,4000," +
            std::to_string(epoch + 4000) + ",1000,180388626474,8,0);";
        char *error = nullptr;
        const bool result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) == SQLITE_OK;
        if (!result) {
            ADD_FAILURE() << (error ? error : "Cannot inject ftrace fixture");
        }
        sqlite3_free(error);
        sqlite3_close(db);
        return result;
    }

    static bool SaveFtraceProject(const std::string &name, const fs::path &path,
        const std::string &host = "baseline-host", const std::string &cluster = "baseline-cluster") {
        Global::ProjectExplorerInfo project;
        project.projectName = name;
        project.fileName = path.u8string();
        project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB);
        project.importType = "import";
        auto root = std::make_shared<Global::ParseFileInfo>();
        root->parseFilePath = project.fileName;
        root->subId = project.fileName;
        root->type = Global::ParseFileType::PROJECT;
        root->projectType = project.projectType;
        project.AddSubParseFileInfo(root);
        auto main = std::make_shared<Global::ParseFileInfo>();
        main->parseFilePath = project.fileName;
        main->fileId = project.fileName;
        main->subId = project.fileName;
        main->type = Global::ParseFileType::RANK;
        main->projectType = project.projectType;
        project.AddSubParseFileInfo(root->subId, root->type, main);
        auto derived = std::make_shared<Global::ParseFileInfo>();
        derived->fileId = MsprofFtraceImportService::ResourceId(project.fileName);
        derived->parseFilePath = derived->fileId;
        derived->rankId = derived->fileId;
        derived->subId = derived->fileId;
        derived->host = host;
        derived->clusterId = cluster;
        derived->type = Global::ParseFileType::RANK;
        derived->projectType = static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE);
        project.AddSubParseFileInfo(root->subId, root->type, derived);
        return Global::ProjectExplorerManager::Instance().SaveProjectExplorer(project, false);
    }

    static Protocol::BaselineSettingRequest FtraceBaselineRequest(const fs::path &path) {
        Protocol::BaselineSettingRequest request;
        request.projectName = "current-project";
        request.params.projectName = "baseline-project";
        request.params.filePath = MsprofFtraceImportService::ResourceId(path.u8string());
        return request;
    }

    static bool Execute(const fs::path &path, const std::string &sql) {
        sqlite3 *db = nullptr;
        if (sqlite3_open(path.u8string().c_str(), &db) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
        const bool success = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
        sqlite3_close(db);
        return success;
    }

    fs::path directory, original, source;
    int64_t epoch = 0;
    std::map<std::string, std::string> originalBytes;
    std::map<std::string, bool> originalExists;
    std::shared_ptr<Timeline::DataEngineInterface> previousEngine;
    std::shared_ptr<Timeline::Test::CapturingTimelineSession> session;
    std::shared_ptr<Server::WsSession> previousSession;
    std::unique_ptr<Global::SystemMemoryDatabase> previousProjectDb;
    std::string previousProjectDbPath;
    bool projectStateInstalled = false, sessionInstalled = false;
};

TEST_F(MsprofFtraceFullImportTest, NormalProfilingImportAlsoPublishesIndependentFtraceTextResource) {
    Global::ProjectExplorerInfo project;
    project.projectName = "msprof-full-import-test";
    project.fileName = source.string();
    project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB);
    project.importType = "import";
    ProjectParserDb::BuildProjectExploreInfo(project, {source.string()});
    ASSERT_TRUE(Global::ProjectExplorerManager::Instance().SaveProjectExplorer(project, false));
    auto projects = Global::ProjectExplorerManager::Instance().QueryProjectExplorer(project.projectName, {});
    ASSERT_EQ(projects.size(), 1);
    Protocol::ImportActionRequest request;
    request.params.projectName = project.projectName;
    Protocol::ImportActionResponse response;
    ProjectParserDb parser;
    std::string warning;
    parser.GetParseFileByImportFile(source.string(), warning);
    parser.Parser(projects, request, response);
    const auto resourceId = MsprofFtraceImportService::ResourceId(source.string());
    std::vector<std::string> mainRanks;
    bool hasFtraceCard = false;
    for (const auto &action : response.body.result) {
        if (action.fileId == source.string()) {
            mainRanks.push_back(action.rankId);
        }
        if (action.fileId == resourceId) {
            hasFtraceCard = action.cardName == "Ftrace";
        }
    }
    ASSERT_FALSE(mainRanks.empty());
    ASSERT_TRUE(hasFtraceCard);
    Timeline::ParserStatusManager::Instance().NotifyStartParse();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    bool mainReady = false, ftraceReady = false;
    do {
        mainReady = std::all_of(mainRanks.begin(), mainRanks.end(), [](const auto &rank) {
            return Timeline::ParserStatusManager::Instance().GetParserStatus(rank) ==
                Timeline::ParserStatus::FINISH_ALL;
        });
        const auto resource = MsprofFtraceImportService::Instance().GetResource(resourceId);
        ftraceReady = resource && resource->ready;
        if (mainReady && ftraceReady) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } while (std::chrono::steady_clock::now() < deadline);
    ASSERT_TRUE(mainReady);
    ASSERT_TRUE(ftraceReady);
    auto &manager = Timeline::DataBaseManager::Instance();
    EXPECT_EQ(manager.GetDataType(source.string()), Timeline::DataType::DB);
    EXPECT_EQ(manager.GetDataType(resourceId), Timeline::DataType::TEXT);
    auto main = manager.GetTraceDatabaseByFileId(source.string());
    ASSERT_NE(main, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<DbTraceDataBase>(main), nullptr);
    EXPECT_TRUE(main->CheckTableExist("CANN_API"));
    EXPECT_TRUE(main->CheckTableExist("NPU_INFO"));
    EXPECT_EQ(Scalar(source.string(), "SELECT COUNT(*) FROM CANN_API"), 4);
    EXPECT_EQ(Scalar(source.string(), "SELECT COUNT(*) FROM NPU_INFO"), 8);
    EXPECT_EQ(Scalar(source.string(), "SELECT COUNT(*) FROM sqlite_master WHERE name='slice'"), 0);
    auto derived = manager.GetTraceDatabaseByRankId(resourceId);
    ASSERT_NE(derived, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<Timeline::TextTraceDatabase>(derived), nullptr);
    uint64_t min = 0, max = 0;
    EXPECT_TRUE(derived->QueryExtremumTimestamp(min, max));
    EXPECT_EQ(min, static_cast<uint64_t>(epoch + 1000));
    const auto resource = MsprofFtraceImportService::Instance().GetResource(resourceId);
    ASSERT_TRUE(resource.has_value());
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='softirq'"), 1);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT COUNT(*) FROM process"), 2);
    EXPECT_EQ(Scalar(resource->cachePath,
                  "SELECT COUNT(*) FROM process WHERE process_name NOT IN ('CPU Scheduling','Process Scheduling')"),
        0);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT SUM(duration) FROM slice WHERE name='Running'"), 3000);
    EXPECT_EQ(Scalar(resource->cachePath, "SELECT SUM(duration) FROM slice WHERE name='softirq'"), 1000);
    EXPECT_GT(
        Scalar(resource->cachePath, "SELECT COUNT(*) FROM slice WHERE args LIKE '%full_import_worker_complete_name%'"),
        0);
}

TEST_F(MsprofFtraceFullImportTest, ReopensPersistedFtraceBaselineAfterSwitchingProjects) {
    const auto current = directory / "current.db";
    ASSERT_TRUE(fs::copy_file(source, current));
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", current, "current-host", "current-cluster"));
    const auto sourceBytes = Bytes(source);
    auto &service = MsprofFtraceImportService::Instance();
    auto &manager = Timeline::DataBaseManager::Instance();
    ASSERT_TRUE(service.Discover(source.u8string(), "baseline-host", "baseline-cluster", "baseline-project"));
    std::string error;
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto sourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto retainedPath = service.GetResource(sourceId)->cachePath;
    ASSERT_TRUE(Execute(
        fs::u8path(retainedPath), "CREATE TABLE reuse_probe(value INTEGER); INSERT INTO reuse_probe VALUES(91)"));
    manager.Clear(); // A project switch releases live resources but keeps the persisted project tree.
    ASSERT_FALSE(service.GetResource(sourceId).has_value());
    ASSERT_TRUE(fs::exists(fs::u8path(retainedPath)));
    ASSERT_TRUE(service.Discover(current.u8string(), "current-host", "current-cluster", "current-project"));
    ASSERT_TRUE(service.Import(current.u8string(), error, false)) << error;
    const auto currentId = MsprofFtraceImportService::ResourceId(current.u8string());
    const auto currentResource = service.GetResource(currentId);
    ASSERT_TRUE(currentResource.has_value());

    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(source), baseline));
    ASSERT_TRUE(baseline.errorMessage.empty()) << baseline.errorMessage;
    ASSERT_FALSE(baseline.fileId.empty());
    EXPECT_TRUE(baseline.isFtrace);
    EXPECT_EQ(baseline.host, "baseline-host");
    EXPECT_EQ(baseline.clusterBaseLine, "baseline-cluster");
    EXPECT_EQ(manager.GetDataType(baseline.fileId), Timeline::DataType::TEXT);
    const auto alias = service.GetResource(baseline.fileId);
    ASSERT_TRUE(alias.has_value());
    EXPECT_EQ(alias->projectName, "baseline-project");
    EXPECT_FALSE(service.GetResource(sourceId).has_value());
    EXPECT_TRUE(fs::exists(fs::u8path(alias->cachePath)));
    EXPECT_EQ(alias->cachePath, retainedPath);
    EXPECT_EQ(Scalar(alias->cachePath, "SELECT value FROM reuse_probe"), 91);
    EXPECT_EQ(Scalar(alias->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
    EXPECT_EQ(Scalar(alias->cachePath, "SELECT COUNT(*) FROM slice WHERE name='softirq'"), 1);
    {
        auto trace = manager.GetTraceDatabaseByRankId(baseline.rankId);
        ASSERT_NE(trace, nullptr);
        EXPECT_NE(std::dynamic_pointer_cast<Timeline::TextTraceDatabase>(trace), nullptr);
        uint64_t min = 0, max = 0;
        ASSERT_TRUE(trace->QueryExtremumTimestamp(min, max));
        EXPECT_EQ(min, static_cast<uint64_t>(epoch + 1000));
        EXPECT_GT(max, min);
    }
    EXPECT_EQ(service.GetResource(currentId)->cachePath, currentResource->cachePath);
    EXPECT_EQ(Bytes(source), sourceBytes);

    Global::BaselineManagerService::ResetBaseline(true);
    EXPECT_FALSE(service.GetResource(baseline.fileId).has_value());
    EXPECT_TRUE(fs::exists(fs::u8path(alias->cachePath)));
    EXPECT_TRUE(service.GetResource(currentId)->ready);
    EXPECT_EQ(Scalar(currentResource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);

    Global::BaselineInfo repeated;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(source), repeated));
    ASSERT_TRUE(repeated.errorMessage.empty()) << repeated.errorMessage;
    const auto reused = service.GetResource(repeated.fileId);
    ASSERT_TRUE(reused.has_value());
    EXPECT_EQ(reused->cachePath, retainedPath);
    EXPECT_EQ(Scalar(reused->cachePath, "SELECT value FROM reuse_probe"), 91);
    Global::BaselineManagerService::ResetBaseline(true);
    EXPECT_TRUE(fs::exists(fs::u8path(retainedPath)));

    ASSERT_TRUE(Global::ProjectExplorerManager::Instance().DeleteProjectAndFilePath("baseline-project", {sourceId}));
    EXPECT_FALSE(fs::exists(fs::u8path(retainedPath)));
    EXPECT_TRUE(fs::exists(source));
    EXPECT_TRUE(service.GetResource(currentId)->ready);
}

TEST_F(MsprofFtraceFullImportTest, DeletingInactiveProjectDiscardsItsRetainedCacheOnly) {
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(service.Discover(source.u8string(), "baseline-host", "baseline-cluster", "baseline-project"));
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto path = service.GetResource(id)->cachePath;
    Timeline::DataBaseManager::Instance().Clear();
    ASSERT_FALSE(service.GetResource(id).has_value());
    ASSERT_TRUE(fs::exists(fs::u8path(path)));

    ASSERT_TRUE(Global::ProjectExplorerManager::Instance().DeleteProjectAndFilePath("baseline-project", {}));
    EXPECT_FALSE(fs::exists(fs::u8path(path)));
    EXPECT_TRUE(fs::exists(source));
}

TEST_F(MsprofFtraceFullImportTest, SameProjectFtraceBaselineReusesSourceAcrossCancelAndReplacement) {
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    auto &service = MsprofFtraceImportService::Instance();
    auto &manager = Timeline::DataBaseManager::Instance();
    ASSERT_TRUE(service.Discover(source.u8string(), "current-host", "current-cluster", "current-project"));
    std::string error;
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto sourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto originalResource = service.GetResource(sourceId);
    ASSERT_TRUE(originalResource.has_value());
    const auto sourceToken = manager.GetTraceDatabasePoolToken(sourceId);
    const auto cacheBytes = Bytes(fs::u8path(originalResource->cachePath));
    auto request = FtraceBaselineRequest(source);
    request.params.projectName = request.projectName;

    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(request, baseline));
    ASSERT_TRUE(baseline.errorMessage.empty()) << baseline.errorMessage;
    EXPECT_EQ(baseline.fileId, sourceId);
    EXPECT_EQ(baseline.rankId, sourceId);
    EXPECT_EQ(baseline.cardId, sourceId);
    EXPECT_EQ(baseline.cardName, "Ftrace");
    EXPECT_EQ(Global::BaselineManager::Instance().GetBaselineId(), sourceId);

    Global::BaselineManagerService::ResetBaseline(false);
    EXPECT_FALSE(sourceToken.expired());
    ASSERT_TRUE(service.GetResource(sourceId).has_value());
    EXPECT_EQ(service.GetResource(sourceId)->cachePath, originalResource->cachePath);

    Global::BaselineInfo repeated;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(request, repeated));
    ASSERT_TRUE(repeated.errorMessage.empty()) << repeated.errorMessage;
    EXPECT_EQ(repeated.cardId, sourceId);

    // Switching baselines requests forced cleanup of the previous one, but must not release the visible source.
    const auto other = directory / "another-project.db";
    ASSERT_TRUE(fs::copy_file(source, other));
    ASSERT_TRUE(SaveFtraceProject("baseline-project", other));
    Global::BaselineInfo external;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(other), external));
    ASSERT_TRUE(external.errorMessage.empty()) << external.errorMessage;
    EXPECT_NE(external.cardId, sourceId);
    EXPECT_EQ(external.cardName, "Baseline_Ftrace");
    EXPECT_FALSE(sourceToken.expired());
    ASSERT_TRUE(service.GetResource(sourceId).has_value());
    EXPECT_TRUE(service.GetResource(sourceId)->ready);
    EXPECT_EQ(Bytes(fs::u8path(originalResource->cachePath)), cacheBytes);
    EXPECT_EQ(Scalar(originalResource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);

    Global::BaselineManagerService::ResetBaseline(true);
    Global::BaselineInfo currentAgain;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(request, currentAgain));
    ASSERT_TRUE(currentAgain.errorMessage.empty()) << currentAgain.errorMessage;
    EXPECT_EQ(currentAgain.cardId, sourceId);
    Global::BaselineManagerService::ResetBaseline(true);
    EXPECT_FALSE(sourceToken.expired());
    ASSERT_TRUE(service.GetResource(sourceId).has_value());
    EXPECT_TRUE(service.GetResource(sourceId)->ready);
}

TEST_F(MsprofFtraceFullImportTest, RestoredSameProjectBaselineKeepsItsVisibleSourceResource) {
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    auto request = FtraceBaselineRequest(source);
    request.params.projectName = request.projectName;
    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(request, baseline));
    ASSERT_TRUE(baseline.errorMessage.empty()) << baseline.errorMessage;
    const auto sourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    EXPECT_EQ(baseline.cardId, sourceId);
    EXPECT_EQ(baseline.cardName, "Ftrace");
    const auto resource = MsprofFtraceImportService::Instance().GetResource(sourceId);
    ASSERT_TRUE(resource.has_value());
    EXPECT_TRUE(resource->ready);
    Global::BaselineManagerService::ResetBaseline(true);
    ASSERT_TRUE(MsprofFtraceImportService::Instance().GetResource(sourceId).has_value());
    EXPECT_TRUE(MsprofFtraceImportService::Instance().GetResource(sourceId)->ready);
}

TEST_F(MsprofFtraceFullImportTest, ActiveFtraceBaselineKeepsItsSourceResource) {
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    auto &service = MsprofFtraceImportService::Instance();
    ASSERT_TRUE(service.Discover(source.u8string(), "baseline-host", "baseline-cluster", "baseline-project"));
    std::string error;
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto sourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto originalResource = service.GetResource(sourceId);
    ASSERT_TRUE(originalResource.has_value());
    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(source), baseline));
    ASSERT_TRUE(baseline.errorMessage.empty()) << baseline.errorMessage;
    EXPECT_NE(baseline.fileId, sourceId);
    EXPECT_EQ(baseline.cardName, "Baseline_Ftrace");
    ASSERT_TRUE(service.GetResource(sourceId).has_value());
    EXPECT_EQ(service.GetResource(sourceId)->cachePath, originalResource->cachePath);
    Global::BaselineManagerService::ResetBaseline(true);
    EXPECT_TRUE(service.GetResource(sourceId)->ready);
    EXPECT_EQ(Scalar(originalResource->cachePath, "SELECT COUNT(*) FROM slice WHERE name='sched_switch'"), 2);
}

TEST_F(MsprofFtraceFullImportTest, DeletingOneProjectKeepsSharedFtraceResourceAndIdleCache) {
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    auto &service = MsprofFtraceImportService::Instance();
    ASSERT_TRUE(service.Discover(source.u8string(), "current-host", "current-cluster", "current-project"));
    std::string error;
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto id = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto path = service.GetResource(id)->cachePath;
    ASSERT_TRUE(
        Execute(fs::u8path(path), "CREATE TABLE reuse_probe(value INTEGER); INSERT INTO reuse_probe VALUES(35)"));

    auto &projects = Global::ProjectExplorerManager::Instance();
    ASSERT_TRUE(projects.DeleteProjectAndFilePath("baseline-project", {}));
    ASSERT_TRUE(service.GetResource(id).has_value());
    EXPECT_TRUE(service.GetResource(id)->ready);
    EXPECT_EQ(Scalar(path, "SELECT value FROM reuse_probe"), 35);

    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    Timeline::DataBaseManager::Instance().Clear();
    ASSERT_FALSE(service.GetResource(id).has_value());
    ASSERT_TRUE(projects.DeleteProjectAndFilePath("baseline-project", {}));
    EXPECT_TRUE(fs::exists(fs::u8path(path)));
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    EXPECT_EQ(Scalar(path, "SELECT value FROM reuse_probe"), 35);

    ASSERT_TRUE(projects.DeleteProjectAndFilePath("current-project", {}));
    EXPECT_FALSE(service.GetResource(id).has_value());
    EXPECT_FALSE(fs::exists(fs::u8path(path)));
    EXPECT_TRUE(fs::exists(source));
}

TEST_F(MsprofFtraceFullImportTest, ClearingProjectsDiscardsIdleCachesOnlyAfterLastOwnerIsRemoved) {
    const auto other = directory / "other-project.db";
    ASSERT_TRUE(fs::copy_file(source, other));
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    ASSERT_TRUE(SaveFtraceProject("other-project", other));
    auto &service = MsprofFtraceImportService::Instance();
    std::string error;
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    const auto sourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    const auto sourceCache = service.GetResource(sourceId)->cachePath;
    ASSERT_TRUE(service.Remove(sourceId, true));
    ASSERT_TRUE(service.Import(other.u8string(), error, false)) << error;
    const auto otherId = MsprofFtraceImportService::ResourceId(other.u8string());
    const auto otherCache = service.GetResource(otherId)->cachePath;
    ASSERT_TRUE(service.Remove(otherId, true));

    auto &projects = Global::ProjectExplorerManager::Instance();
    ASSERT_TRUE(projects.ClearProjectExplorer({"baseline-project"}));
    EXPECT_TRUE(fs::exists(fs::u8path(sourceCache)));
    EXPECT_TRUE(fs::exists(fs::u8path(otherCache)));
    ASSERT_TRUE(projects.ClearProjectExplorer({"current-project"}));
    EXPECT_FALSE(fs::exists(fs::u8path(sourceCache)));
    EXPECT_TRUE(fs::exists(fs::u8path(otherCache)));
    ASSERT_TRUE(projects.ClearProjectExplorer({}));
    EXPECT_FALSE(fs::exists(fs::u8path(otherCache)));
    EXPECT_TRUE(fs::exists(source));
    EXPECT_TRUE(fs::exists(other));
}

TEST_F(MsprofFtraceFullImportTest, MissingPersistedFtraceSourceReportsFailureInsteadOfEmptyBaseline) {
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    fs::rename(source, directory / "removed.db");
    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(source), baseline));
    EXPECT_FALSE(baseline.errorMessage.empty());
    EXPECT_TRUE(baseline.fileId.empty());
    EXPECT_TRUE(Global::BaselineManager::Instance().GetBaselineId().empty());
    EXPECT_FALSE(MsprofFtraceImportService::Instance()
                     .GetResource(MsprofFtraceImportService::ResourceId(source.u8string()))
                     .has_value());
    EXPECT_FALSE(fs::exists(source));
}

TEST_F(MsprofFtraceFullImportTest, InvalidPersistedFtraceSourceReportsFailureAndRemovesTemporaryResource) {
    ASSERT_TRUE(SaveFtraceProject("baseline-project", source));
    ASSERT_TRUE(SaveFtraceProject("current-project", source));
    ASSERT_TRUE(Execute(source, "UPDATE META_DATA SET value='1.3.0' WHERE name='SCHEMA_VERSION'"));
    const auto sourceBytes = Bytes(source);
    Global::BaselineInfo baseline;
    ASSERT_TRUE(Global::BaselineManagerService::InitBaselineData(FtraceBaselineRequest(source), baseline));
    EXPECT_FALSE(baseline.errorMessage.empty());
    EXPECT_TRUE(baseline.fileId.empty());
    EXPECT_TRUE(Global::BaselineManager::Instance().GetBaselineId().empty());
    EXPECT_FALSE(MsprofFtraceImportService::Instance()
                     .GetResource(MsprofFtraceImportService::ResourceId(source.u8string()))
                     .has_value());
    EXPECT_EQ(Bytes(source), sourceBytes);
}
}
}
