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
#include "ExportAffinityAPIAdvice.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "AdvisorErrorManager.h"
#include "TestSuit.h"
#include "ProjectExplorerManager.h"
#include "AffinityAPIAdvisor.h"
#include <filesystem>
#include <fstream>

namespace Dic::Module::Advisor {
using namespace Dic::Protocol;

class ExportAffinityAPIAdviceTest : public ::testing::Test {
  public:
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenRankIdEmpty) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenOrderByIllegal) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.orderBy = "duration--";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenStartTimeGreaterThanEndTime) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.startTime = 100;
    request.endTime = 50;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenNoDatabase) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "nonexistent_rank";
    request.orderBy = "duration";
    request.orderType = "descend";
    request.startTime = 0;
    request.endTime = 0;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

class ExportAffinityAPIIntegrationTest : public TestSuit {
  protected:
    std::filesystem::path exportDir;
    const std::string projectName = "advisor_export_ut";

    void SetUp() override {
        exportDir = std::filesystem::path(TestSuit::GetSrcTestPath()) / "advisor_export_ut";
        std::filesystem::create_directories(exportDir);
        Global::ProjectExplorerManager::Instance().ClearProjectExplorer({projectName});
    }

    void TearDown() override {
        Global::ProjectExplorerManager::Instance().DeleteProjectAndFilePath(projectName, {});
        for (const auto &file : std::filesystem::directory_iterator(exportDir)) {
            if (file.path().extension() == ".csv") {
                std::filesystem::remove(file.path());
            }
        }
    }

    std::unique_ptr<ExportAffinityAPIRequest> RequestForExport() {
        auto request = std::make_unique<ExportAffinityAPIRequest>();
        request->rankId = "0";
        request->orderBy = "duration";
        request->orderType = "descend";
        request->projectName = projectName;
        return request;
    }

    void RegisterProject(const std::string &path) {
        Global::ProjectExplorerInfo info;
        info.projectName = projectName;
        info.fileName = path;
        info.importType = "import";
        info.projectType = static_cast<int64_t>(ProjectTypeEnum::TRACE);
        info.accessTime = "2000-01-01 00:00:00.000";
        // QueryProjectExplorer returns only imports with a parsed-file child.
        auto parsedFile = std::make_shared<Global::ParseFileInfo>();
        parsedFile->parseFilePath = path;
        parsedFile->subId = "advisor_export_ut";
        parsedFile->type = Global::ParseFileType::RANK;
        info.AddSubParseFileInfo(parsedFile);
        ASSERT_TRUE(Global::ProjectExplorerManager::Instance().SaveProjectExplorer(info, false));
    }
};

TEST_F(ExportAffinityAPIIntegrationTest, ExportWritesBomHeadersAndAllMatchingRows) {
    RegisterProject(exportDir.string());
    APITypeParams params;
    params.rankId = "0";
    params.orderBy = "duration";
    params.orderType = "descend";
    std::vector<AffinityAPIData> expected;
    ASSERT_TRUE(AffinityAPIAdvisor::ProcessAll(params, expected));
    ASSERT_FALSE(expected.empty());
    ExportAffinityAPIAdvice handler;
    ASSERT_TRUE(handler.HandleRequest(RequestForExport()));
    std::vector<std::filesystem::path> csvFiles;
    for (const auto &entry : std::filesystem::directory_iterator(exportDir)) {
        if (entry.path().extension() == ".csv") {
            csvFiles.push_back(entry.path());
        }
    }
    ASSERT_EQ(csvFiles.size(), 1);
    std::ifstream input(csvFiles.front(), std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content.substr(0, 3), "\xEF\xBB\xBF");
    EXPECT_NE(content.find("Name,Origin API,Replacement API,Start Time,Duration(ns),Process Id,Thread Id,Notes"),
        std::string::npos);
    EXPECT_NE(content.find(expected.front().name), std::string::npos);
    EXPECT_EQ(std::count(content.begin(), content.end(), '\n'), expected.size() + 1);
}

TEST_F(ExportAffinityAPIIntegrationTest, ExportFailsWhenProjectIsMissing) {
    ExportAffinityAPIAdvice handler;
    EXPECT_FALSE(handler.HandleRequest(RequestForExport()));
}

TEST_F(ExportAffinityAPIIntegrationTest, ExportFailsWhenProjectDirectoryDoesNotExist) {
    RegisterProject((exportDir / "missing").string());
    ExportAffinityAPIAdvice handler;
    EXPECT_FALSE(handler.HandleRequest(RequestForExport()));
}

} // Dic::Module::Advisor
