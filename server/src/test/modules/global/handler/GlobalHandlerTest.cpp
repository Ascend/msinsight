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
#include <chrono>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include "GlobalProtocolResponse.h"
#include "SystemMemoryDatabase.h"
#define private public
#include "ProjectExplorerManager.h"
#include "TraceTime.h"
#undef private
#include "../../timeline/handler/HandlerTest.cpp"
#include "BaselineManager.h"
#include "BaselineManagerService.h"
#include "DataBaseManager.h"
#include "RankLaneMergeCoordinator.h"
#include "TestSuit.h"
#include "GlobalProtocolRequest.h"
#include "GetProjectExplorerInfoHandler.h"
#include "SetBaselineHandler.h"
#include "GetModuleConfigHandler.h"
#include "ClearProjectExplorerHandler.h"

using namespace Dic::Module;

namespace {
class ScopedHandlerError {
  public:
    ScopedHandlerError() {
        // Use the handler API so the DLL and the test share the same request context on Windows.
        Dic::Protocol::Response previousResponse;
        ModuleRequestHandler::SetResponseErrorFromRequestContext(previousResponse);
        previousError = previousResponse.error.value_or(Dic::Protocol::ErrorMessage{});
        ModuleRequestHandler::ResetRequestContextError();
    }
    ~ScopedHandlerError() { ModuleRequestHandler::SetRequestContextError(previousError); }

  private:
    Dic::Protocol::ErrorMessage previousError;
};

class ScopedTraceTime {
  public:
    ScopedTraceTime() {
        auto &time = Timeline::TraceTime::Instance();
        std::lock_guard<std::mutex> lock(time.mutex);
        minTimestamp = time.minTimestamp;
        maxTimestamp = time.maxTimestamp;
        cardTimeDurationMap = time.cardTimeDurationMap;
        rankMinTimestampMap = time.rankMinTimestampMap;
        cardGroupTimeDurations = time.cardGroupTimeDurations;
        isSimulation = time.isSimulation;
    }
    ~ScopedTraceTime() {
        auto &time = Timeline::TraceTime::Instance();
        std::lock_guard<std::mutex> lock(time.mutex);
        time.minTimestamp = minTimestamp;
        time.maxTimestamp = maxTimestamp;
        time.cardTimeDurationMap = std::move(cardTimeDurationMap);
        time.rankMinTimestampMap = std::move(rankMinTimestampMap);
        time.cardGroupTimeDurations = std::move(cardGroupTimeDurations);
        time.isSimulation = isSimulation;
    }

  private:
    uint64_t minTimestamp;
    uint64_t maxTimestamp;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> cardTimeDurationMap;
    std::unordered_map<std::string, uint64_t> rankMinTimestampMap;
    std::vector<Timeline::CardGroup> cardGroupTimeDurations;
    bool isSimulation;
};

class BaselineProject {
  public:
    void Create() {
        std::error_code error;
        const auto temporaryDirectory = std::filesystem::temp_directory_path(error);
        ASSERT_FALSE(error) << error.message();
        projectName =
            "set-baseline-handler-ut-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        directory = temporaryDirectory / projectName;
        directoryCreated = std::filesystem::create_directory(directory, error);
        ASSERT_FALSE(error) << error.message();
        ASSERT_TRUE(directoryCreated);
        filePath = (directory / "ascend_pytorch_profiler.db").string();
        auto source = std::filesystem::path(TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db"));
        ASSERT_TRUE(std::filesystem::copy_file(source.make_preferred(), filePath, error)) << error.message();
        ASSERT_FALSE(error) << error.message();

        auto &projects = Global::ProjectExplorerManager::Instance();
        {
            // Do not depend on another suite having initialized the project store or log path.
            std::lock_guard<std::recursive_mutex> lock(projects.mutex);
            previousProjectDatabase = std::move(projects.db);
            previousProjectDatabasePath = projects.systemMemoryDbPath;
            projects.db = std::make_unique<Global::SystemMemoryDatabase>(projects.mutex);
            projects.InitSystemMemoryDbPath(directory.string());
            projectStoreInstalled = true;
        }
        ASSERT_TRUE(projects.QueryProjectExplorer(projectName, {}).empty());
        Global::ProjectExplorerInfo project;
        project.projectName = projectName;
        project.fileName = filePath;
        project.importType = "import";
        project.projectType = static_cast<int64_t>(Dic::ProjectTypeEnum::DB);
        auto rank = std::make_shared<Global::ParseFileInfo>();
        rank->parseFilePath = filePath;
        rank->subId = filePath;
        rank->type = Global::ParseFileType::RANK;
        project.AddSubParseFileInfo(rank);
        ASSERT_TRUE(projects.SaveProjectExplorer(project, false));
        ASSERT_EQ(projects.QueryProjectExplorer(projectName, {}).size(), 1U);

        // Setting an already imported database as baseline does not need to start another parser.
        auto &databases = Timeline::DataBaseManager::Instance();
        databases.SetDataType(Timeline::DataType::DB, filePath);
        databases.SetFileType(Timeline::FileType::PYTORCH, filePath);
        ASSERT_TRUE(databases.CreateTraceConnectionPool(filePath, filePath));
        auto database = databases.GetTraceDatabaseByFileId(filePath);
        ASSERT_NE(database, nullptr);
        // Keep rank/host mappings distinct from tests using the original sample database.
        ASSERT_TRUE(database->ExecSql("CREATE TABLE IF NOT EXISTS HOST_INFO(hostName TEXT, hostUid TEXT);"));
        ASSERT_TRUE(database->ExecSql("DELETE FROM HOST_INFO;"));
        ASSERT_TRUE(database->ExecSql("INSERT INTO HOST_INFO(hostName, hostUid) VALUES ('" + projectName + "', '');"));
    }

    ~BaselineProject() {
        if (!directoryCreated) {
            return;
        }
        if (baselineRequested) {
            // ResetBaseline removes the baseline alias; also remove the original rank's source.
            const auto baselineId = Global::BaselineManager::Instance().GetBaselineId();
            const std::string prefix = "Baseline_";
            if (baselineId.compare(0, prefix.size(), prefix) == 0) {
                Timeline::RankLaneMergeCoordinator::Instance().RemoveSource(baselineId.substr(prefix.size()), filePath);
            }
            Global::BaselineManagerService::ResetBaseline(true);
        }
        Timeline::DataBaseManager::Instance().ReleaseDatabaseByFileId(filePath);
        if (projectStoreInstalled) {
            auto &projects = Global::ProjectExplorerManager::Instance();
            EXPECT_TRUE(projects.DeleteProjectAndFilePath(projectName, {}));
            EXPECT_TRUE(projects.QueryProjectExplorer(projectName, {}).empty());
            std::lock_guard<std::recursive_mutex> lock(projects.mutex);
            projects.db = std::move(previousProjectDatabase);
            projects.systemMemoryDbPath = std::move(previousProjectDatabasePath);
        }
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        EXPECT_FALSE(error) << error.message();
    }

    std::string projectName;
    std::string filePath;
    bool baselineRequested = false;

  private:
    ScopedTraceTime traceTime;
    std::unique_ptr<Global::SystemMemoryDatabase> previousProjectDatabase;
    std::string previousProjectDatabasePath;
    bool projectStoreInstalled = false;
    std::filesystem::path directory;
    bool directoryCreated = false;
};
} // namespace

TEST_F(HandlerTest, GetProjectExplorerInfoWithEmptyResult) {
    auto request = std::make_unique<Dic::Module::ProjectExplorerInfoGetRequest>();
    Dic::Module::GetProjectExplorerInfoHandler handler;
    bool result = handler.HandleRequest(std::move(request));
    EXPECT_EQ(result, true);
}

TEST_F(HandlerTest, SetBaselineHandler) {
    ResponseCapture capture;
    ScopedHandlerError requestError;
    BaselineProject project;
    ASSERT_NO_FATAL_FAILURE(project.Create());
    auto request = std::make_unique<BaselineSettingRequest>();
    request->id = 73;
    request->projectName = project.projectName;
    request->params.projectName = project.projectName;
    request->params.filePath = project.filePath;
    request->fileId = project.filePath;
    const auto command = request->command;
    SetBaselineHandler handler;
    project.baselineRequested = true;
    ASSERT_TRUE(handler.HandleRequest(std::move(request)));
    ASSERT_EQ(capture.session->responses.size(), 1U);
    const auto *response =
        dynamic_cast<const Dic::Protocol::BaselineSettingResponse *>(capture.session->responses.front().get());
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->requestId, 73U);
    EXPECT_EQ(response->command, command);
    EXPECT_TRUE(response->result);
    // SendResponse populates the optional error object even for a successful response.
    ASSERT_TRUE(response->error.has_value());
    EXPECT_TRUE(response->error->message.empty()) << response->error->message;
    EXPECT_TRUE(response->body.errorMessage.empty()) << response->body.errorMessage;
    EXPECT_FALSE(response->body.isCluster);
    EXPECT_EQ(response->body.fileId, project.filePath);
    EXPECT_NE(response->body.host.find(project.projectName), std::string::npos);
    ASSERT_FALSE(response->body.rankId.empty());
    EXPECT_EQ(response->body.cardId, "Baseline_" + response->body.rankId);
    EXPECT_EQ(Global::BaselineManager::Instance().GetBaselineId(), response->body.cardId);
    EXPECT_NE(Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(response->body.cardId), nullptr);
}

TEST_F(HandlerTest, GetModuleConfigHandler) {
    auto request = std::make_unique<BaselineSettingRequest>();
    GetModuleConfigHandler handler;
    EXPECT_EQ(handler.HandleRequest(std::move(request)), true);
}

TEST_F(HandlerTest, ClearProjectExplorerHandler) {
    auto request = std::make_unique<ProjectExplorerInfoClearRequest>();
    ClearProjectExplorerHandler handler;
    EXPECT_EQ(handler.HandleRequest(std::move(request)), true);
}
