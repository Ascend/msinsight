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
#include <fstream>
#include <chrono>
#include <filesystem>
#include "ProjectExplorerManager.h"
#include "ClusterParseThreadPoolExecutor.h"
#include "DataBaseManager.h"
#include "ImportActionHandler.h"
#include "HandlerTest.cpp"

class ImportActionHandlerTest : HandlerTest {};

TEST_F(HandlerTest, ImportActionHandlerRejectsMissingProject) {
    ResponseCapture capture;
    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    auto requestPtr = std::make_unique<Dic::Protocol::ImportActionRequest>();
    requestPtr->id = 73;
    const auto command = requestPtr->command;
    EXPECT_FALSE(importActionHandler.HandleRequest(std::move(requestPtr)));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    const auto &response = *capture.session->responses.front();
    EXPECT_EQ(response.requestId, 73u);
    EXPECT_EQ(response.command, command);
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Timeline::ErrorCode::PARAMS_ERROR));
    EXPECT_FALSE(response.error->message.empty());
}

TEST_F(HandlerTest, ImportActionHandlerRejectsUnknownProjectAction) {
    ResponseCapture capture;
    Dic::Module::Timeline::ImportActionHandler importActionHandler;
    auto requestPtr = std::make_unique<Dic::Protocol::ImportActionRequest>();
    requestPtr.get()->params.path.emplace_back("jjjj");
    requestPtr->params.projectName = "timeline-ut-project";
    requestPtr->id = 73;
    const auto command = requestPtr->command;
    EXPECT_FALSE(importActionHandler.HandleRequest(std::move(requestPtr)));
    ASSERT_EQ(capture.session->responses.size(), 1u);
    const auto &response = *capture.session->responses.front();
    EXPECT_EQ(response.requestId, 73u);
    EXPECT_EQ(response.command, command);
    EXPECT_FALSE(response.result);
    ASSERT_TRUE(response.error.has_value());
    EXPECT_EQ(response.error->code, static_cast<int>(Dic::Module::Timeline::ErrorCode::PARAMS_ERROR));
    EXPECT_FALSE(response.error->message.empty());
}

namespace {
class ImportClusterDirectory {
  public:
    void Create() {
        std::error_code error;
        const auto temporaryDirectory = std::filesystem::temp_directory_path(error);
        ASSERT_FALSE(error) << error.message();
        projectName =
            "timeline-import-ut-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        directory = temporaryDirectory / projectName;
        directoryCreated = std::filesystem::create_directory(directory, error);
        ASSERT_FALSE(error) << error.message();
        ASSERT_TRUE(directoryCreated);
        clusterDirectory = directory / "cluster_analysis_output";
        ASSERT_TRUE(std::filesystem::create_directory(clusterDirectory, error));
        ASSERT_FALSE(error) << error.message();
        for (const auto *name : {"cluster_communication.json", "cluster_communication_matrix.json",
                 "cluster_step_trace_time.csv", "communication_group.json"}) {
            std::ofstream file(clusterDirectory / name, std::ios::out | std::ios::trunc);
            ASSERT_TRUE(file.is_open()) << name;
            file.close();
            ASSERT_TRUE(file.good()) << name;
        }
        EXPECT_TRUE(
            Dic::Module::Global::ProjectExplorerManager::Instance().QueryProjectExplorer(projectName, {}).empty());
    }
    ~ImportClusterDirectory() {
        if (!directoryCreated) {
            return;
        }
        WaitForImport();
        Dic::Module::ParserFactory::Reset();
        auto &manager = Dic::Module::Timeline::DataBaseManager::Instance();
        for (const auto &path : {directory, clusterDirectory}) {
            if (!manager.HasClusterDatabase(path.string())) {
                continue;
            }
            auto database = manager.GetClusterDatabase(path.string());
            if (database) {
                const auto databasePath = database->GetDbPath();
                database.reset();
                manager.EraseClusterDb(databasePath);
            }
        }
        EXPECT_TRUE(Dic::Module::Global::ProjectExplorerManager::Instance().DeleteProjectAndFilePath(projectName, {}));
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        EXPECT_FALSE(error) << error.message();
    }
    std::unique_ptr<Dic::Protocol::ImportActionRequest> Request(Dic::Protocol::ProjectActionEnum action) const {
        auto request = std::make_unique<Dic::Protocol::ImportActionRequest>();
        request->params.projectName = projectName;
        request->params.projectAction = action;
        request->params.path.emplace_back(directory.string());
        return request;
    }
    static void WaitForImport() {
        Dic::Module::Timeline::ClusterParseThreadPoolExecutor::Instance().GetThreadPool()->WaitForAllTasks();
    }
    std::string projectName;

  private:
    std::filesystem::path directory;
    std::filesystem::path clusterDirectory;
    bool directoryCreated = false;
};
} // namespace

// The parent of cluster_analysis_output is a supported import directory.
TEST_F(HandlerTest, ImportActionHandlerTestParentDirectoryOfClusterAnalysisOutput) {
    ResponseCapture capture;
    ImportClusterDirectory files;
    ASSERT_NO_FATAL_FAILURE(files.Create());
    Dic::Module::Timeline::ImportActionHandler handler;
    EXPECT_TRUE(handler.HandleRequest(files.Request(Dic::Protocol::ProjectActionEnum::ADD_FILE)));
    files.WaitForImport();
    const auto projects =
        Dic::Module::Global::ProjectExplorerManager::Instance().QueryProjectExplorer(files.projectName, {});
    ASSERT_EQ(projects.size(), 1U);
    EXPECT_EQ(projects[0].projectName, files.projectName);
    EXPECT_FALSE(projects[0].subParseFileInfo.empty());
}

TEST_F(HandlerTest, ImportActionHandlerTestParentDirectoryOfClusterAnalysisOutputTransferProject) {
    ResponseCapture capture;
    ImportClusterDirectory files;
    ASSERT_NO_FATAL_FAILURE(files.Create());
    Dic::Module::Timeline::ImportActionHandler handler;
    ASSERT_TRUE(handler.HandleRequest(files.Request(Dic::Protocol::ProjectActionEnum::ADD_FILE)));
    files.WaitForImport();
    const auto projects =
        Dic::Module::Global::ProjectExplorerManager::Instance().QueryProjectExplorer(files.projectName, {});
    ASSERT_EQ(projects.size(), 1U);
    ASSERT_FALSE(projects[0].subParseFileInfo.empty());
    EXPECT_TRUE(handler.HandleRequest(files.Request(Dic::Protocol::ProjectActionEnum::TRANSFER_PROJECT)));
    files.WaitForImport();
    EXPECT_EQ(
        Dic::Module::Global::ProjectExplorerManager::Instance().QueryProjectExplorer(files.projectName, {}).size(), 1U);
}
