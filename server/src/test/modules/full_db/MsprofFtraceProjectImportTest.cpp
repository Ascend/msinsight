/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include "DataBaseManager.h"
#include "MsprofFtraceImportService.h"
#include "ProjectParserDb.h"
#include "ProjectExplorerManager.h"
#include "sqlite3.h"

namespace Dic::Module {
namespace {
class ExposedFtraceProjectParser : public ProjectParserDb {
  public:
    using ProjectParserDb::GetReportFiles;
    using ProjectParserDb::SetHostInfo;
};

class MsprofFtraceProjectImportTest : public testing::Test {
  protected:
    void SetUp() override {
        static std::atomic<unsigned> sequence{0};
        directory = std::filesystem::temp_directory_path() /
            ("msprof-ftrace-project-" + std::to_string(sequence++) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
        source = (directory / "source.db").string();
        sqlite3 *db = nullptr;
        ASSERT_EQ(sqlite3_open(source.c_str(), &db), SQLITE_OK);
        const int result = sqlite3_exec(db,
            "CREATE TABLE META_DATA(name TEXT,value TEXT);"
            "INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace',NULL);",
            nullptr, nullptr, nullptr);
        sqlite3_close(db);
        ASSERT_EQ(result, SQLITE_OK);
        // Invalid/incomplete additional metadata still creates its independent diagnostic resource.
        ASSERT_TRUE(FullDb::MsprofFtraceImportService::Instance().Discover(source, "host_", "cluster", "project"));
    }

    void TearDown() override {
        FullDb::MsprofFtraceImportService::Instance().Remove(FullDb::MsprofFtraceImportService::ResourceId(source));
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    std::filesystem::path directory;
    std::string source;

    Global::ProjectExplorerInfo ProjectWithDerivedNode() {
        Global::ProjectExplorerInfo project;
        project.projectName = "project";
        project.fileName = directory.parent_path().string();
        project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB);
        project.importType = "import";
        auto root = std::make_shared<Global::ParseFileInfo>();
        root->parseFilePath = project.fileName;
        root->subId = root->parseFilePath;
        root->type = Global::ParseFileType::PROJECT;
        root->projectType = project.projectType;
        project.AddSubParseFileInfo(root);
        auto main = std::make_shared<Global::ParseFileInfo>();
        main->parseFilePath = directory.string();
        main->subId = main->parseFilePath;
        main->fileId = source;
        main->type = Global::ParseFileType::RANK;
        main->projectType = project.projectType;
        project.AddSubParseFileInfo(root->subId, root->type, main);
        auto derived = std::make_shared<Global::ParseFileInfo>();
        derived->parseFilePath = FullDb::MsprofFtraceImportService::ResourceId(source);
        derived->subId = FileUtil::SplicePath(main->subId, "kernel-resource");
        derived->fileId = derived->parseFilePath;
        derived->rankId = derived->fileId;
        derived->host = "host_";
        derived->clusterId = "cluster";
        derived->curDirName = "Kernel Trace";
        derived->type = Global::ParseFileType::RANK;
        derived->projectType = static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE);
        project.AddSubParseFileInfo(root->subId, root->type, derived);
        return project;
    }
};

TEST_F(MsprofFtraceProjectImportTest, AddsOneIndependentFtraceCardForMultiRankSource) {
    const auto resourceId = FullDb::MsprofFtraceImportService::ResourceId(source);
    std::map<std::string, HostInfo> hosts{{"host_", {{source, {"0", "1", "2"}}}}};
    Protocol::ImportActionResponse response;
    ExposedFtraceProjectParser::SetHostInfo(hosts, response, static_cast<int64_t>(ProjectTypeEnum::DB));
    ASSERT_EQ(response.body.result.size(), 4);
    size_t ftraceCount = 0;
    for (const auto &action : response.body.result) {
        if (action.fileId == resourceId) {
            ++ftraceCount;
            EXPECT_EQ(action.cardName, "Ftrace");
            EXPECT_EQ(action.rankId, resourceId);
            EXPECT_EQ(action.host, "host");
            EXPECT_EQ(action.cluster, "cluster");
            EXPECT_EQ(action.projectType, static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE));
            EXPECT_NE(std::find(action.dataPathList.begin(), action.dataPathList.end(), resourceId),
                action.dataPathList.end());
            EXPECT_NE(
                std::find(action.dataPathList.begin(), action.dataPathList.end(), source), action.dataPathList.end());
            EXPECT_NE(std::find(action.dataPathList.begin(), action.dataPathList.end(), directory.string()),
                action.dataPathList.end());
        } else {
            EXPECT_EQ(action.fileId, source);
            EXPECT_EQ(action.projectType, static_cast<int64_t>(ProjectTypeEnum::DB));
        }
    }
    EXPECT_EQ(ftraceCount, 1);
}

TEST_F(MsprofFtraceProjectImportTest, RetainsHostFtraceCardWithoutAcceleratorRanks) {
    std::map<std::string, HostInfo> hosts{{"host_", {{source, {}}}}};
    Protocol::ImportActionResponse response;
    ExposedFtraceProjectParser::SetHostInfo(hosts, response, static_cast<int64_t>(ProjectTypeEnum::DB));
    ASSERT_EQ(response.body.result.size(), 1);
    EXPECT_EQ(response.body.result.front().cardName, "Ftrace");
    EXPECT_EQ(response.body.result.front().rankId, FullDb::MsprofFtraceImportService::ResourceId(source));
    EXPECT_EQ(response.body.result.front().host, "host");
    EXPECT_EQ(response.body.result.front().cluster, "cluster");
}

TEST_F(MsprofFtraceProjectImportTest, ReopenedLogicalResourceUsesFtraceNameAndRetainsOwningHost) {
    Global::ProjectExplorerManager manager;
    manager.InitSystemMemoryDbPath(directory.string());
    ASSERT_TRUE(manager.SaveProjectExplorer(ProjectWithDerivedNode(), false));
    const auto reopened = manager.QueryProjectExplorer("project", {});
    ASSERT_EQ(reopened.size(), 1);
    const auto id = FullDb::MsprofFtraceImportService::ResourceId(source);
    const auto derived = std::find_if(reopened.front().subParseFileInfo.begin(),
        reopened.front().subParseFileInfo.end(), [&id](const auto &node) { return node->fileId == id; });
    ASSERT_NE(derived, reopened.front().subParseFileInfo.end());
    EXPECT_EQ((*derived)->curDirName, "Ftrace");
    EXPECT_EQ((*derived)->parseFilePath, id);
    EXPECT_EQ((*derived)->rankId, id);
    EXPECT_EQ((*derived)->host, "host_");
    EXPECT_EQ((*derived)->clusterId, "cluster");
}

TEST_F(MsprofFtraceProjectImportTest, PersistedLogicalResourceIsNeverReopenedAsPhysicalDatabase) {
    const auto resourceId = FullDb::MsprofFtraceImportService::ResourceId(source);
    ASSERT_TRUE(FullDb::MsprofFtraceImportService::Instance().Remove(resourceId));
    auto node = std::make_shared<Global::ParseFileInfo>();
    node->type = Global::ParseFileType::RANK;
    node->projectType = static_cast<int64_t>(ProjectTypeEnum::DB_FTRACE);
    node->parseFilePath = resourceId;
    node->fileId = resourceId;
    Global::ProjectExplorerInfo project;
    project.projectName = "project";
    project.projectType = static_cast<int64_t>(ProjectTypeEnum::DB);
    project.subParseFileInfo.push_back(node);
    Timeline::DataBaseManager::Instance().SetDataType(Timeline::DataType::TEXT, resourceId);
    ExposedFtraceProjectParser parser;
    EXPECT_TRUE(parser.GetReportFiles({project}).empty());
    EXPECT_EQ(Timeline::DataBaseManager::Instance().GetDataType(resourceId), Timeline::DataType::TEXT);
    EXPECT_FALSE(FullDb::MsprofFtraceImportService::Instance().IsResource(resourceId));
}

TEST_F(MsprofFtraceProjectImportTest, DeletingDerivedNodePreservesOwningMainNodeAndProjectType) {
    Global::ProjectExplorerManager manager;
    manager.InitSystemMemoryDbPath(directory.string());
    ASSERT_TRUE(manager.SaveProjectExplorer(ProjectWithDerivedNode(), false));
    const auto id = FullDb::MsprofFtraceImportService::ResourceId(source);
    ASSERT_TRUE(FullDb::MsprofFtraceImportService::Instance().Remove(id));
    ASSERT_TRUE(manager.DeleteProjectAndFilePath("project", {id}));
    const auto remaining = manager.QueryProjectExplorer("project", {});
    ASSERT_EQ(remaining.size(), 1);
    ASSERT_EQ(remaining.front().subParseFileInfo.size(), 1);
    EXPECT_EQ(remaining.front().subParseFileInfo.front()->fileId, source);
    EXPECT_EQ(Global::ProjectExplorerManager::GetProjectType(remaining), ProjectTypeEnum::DB);
    EXPECT_TRUE(std::filesystem::exists(source));
}

TEST_F(MsprofFtraceProjectImportTest, DeletingSourceAlsoRemovesItsPersistedDerivedNode) {
    Global::ProjectExplorerManager manager;
    manager.InitSystemMemoryDbPath(directory.string());
    ASSERT_TRUE(manager.SaveProjectExplorer(ProjectWithDerivedNode(), false));
    ASSERT_TRUE(manager.DeleteProjectAndFilePath("project", {directory.string()}));
    EXPECT_TRUE(manager.QueryProjectExplorer("project", {}).empty());
    EXPECT_TRUE(std::filesystem::exists(source));
}

TEST_F(MsprofFtraceProjectImportTest, UnknownResourceDeletionDoesNotClearAllProjectFiles) {
    Global::ProjectExplorerManager manager;
    manager.InitSystemMemoryDbPath(directory.string());
    ASSERT_TRUE(manager.SaveProjectExplorer(ProjectWithDerivedNode(), false));
    ASSERT_TRUE(manager.DeleteProjectAndFilePath("project", {"missing::msprof_ftrace"}));
    const auto remaining = manager.QueryProjectExplorer("project", {});
    ASSERT_EQ(remaining.size(), 1);
    EXPECT_EQ(remaining.front().subParseFileInfo.size(), 2);
}
}
}
