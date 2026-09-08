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
#include <vector>
#include "ProjectParserFactory.h"
#include "ProjectParserDb.h"
#include "DataBaseManager.h"
#include "DbPlatformDataBase.h"
#include "FileUtil.h"
#include "../../../DatabaseTestCaseMockUtil.h"
using namespace Dic::Module;
using namespace Dic::Module::Global;
using namespace Dic::Global::PROFILER::MockUtil;

class ProjectParserDbTest : public testing::Test {
  protected:
    void SetUp() override {
        FullDb::DataBaseManager::Instance().Clear();
        Timeline::DataBaseManager::Instance().Clear();
    }

    class ProjectParserDbTestHelper : public ProjectParserDb {
      public:
        void SetRankDeviceMapHelper(std::shared_ptr<ParseFileInfo> parseFileInfo,
            std::unordered_map<std::string, std::string> &rankDeviceMap, const std::string &deviceIdInMem,
            const std::string &rank) {
            SetRankDeviceMap(parseFileInfo, rankDeviceMap, deviceIdInMem, rank);
        }

        void SetHostInfoHelper(
            std::map<std::string, HostInfo> &hostInfoMap, ImportActionResponse &response, int64_t projectType) {
            SetHostInfo(hostInfoMap, response, projectType);
        }

        std::vector<std::string> GetDbFilesInDirHelper(const std::string &filePath) {
            return GetDbFilesInDir(filePath);
        }
    };
    std::string GetMultiDeviceTestDataPath() {
        std::string current = Dic::FileUtil::GetCurrPath();
        auto pos = current.find("server");
        return Dic::FileUtil::SplicePath(
            current.substr(0, pos + 6), "src", "test", "test_data", "multiDevice", "msprof_db");
    }

    void DataPrepare() {
        std::string path = GetMultiDeviceTestDataPath();
        std::string profDir = FileUtil::SplicePath(path, "PROF_000001_20250722180243900_BNJAGJGRKECIQHIA");
        for (int i = 0; i < 4; i++) {
            fs::create_directories(FileUtil::SplicePath(profDir, "device_" + std::to_string(i)));
        }
        std::string profilerDbPath =
            FileUtil::SplicePath(path, "ASCEND_PROFILER_OUTPUT", "ascend_pytorch_profiler.dat");
        std::string newProfilerDbPath =
            FileUtil::SplicePath(path, "ASCEND_PROFILER_OUTPUT", "ascend_pytorch_profiler.db");
        fs::rename(profilerDbPath, newProfilerDbPath);
        std::string analysisDbPath = FileUtil::SplicePath(path, "ASCEND_PROFILER_OUTPUT", "analysis.dat");
        std::string newAnalysisDbPath = FileUtil::SplicePath(path, "ASCEND_PROFILER_OUTPUT", "analysis.db");
        fs::rename(analysisDbPath, newAnalysisDbPath);
    }
    void TearDown() override {
        FullDb::DataBaseManager::Instance().Clear();
        Timeline::DataBaseManager::Instance().Clear();
        for (const auto &path : temporaryFiles_) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }

    class DbParserTestHelper : public ProjectParserDb {
      public:
        std::map<std::string, HostInfo> GetReportFileTestHelper(std::vector<ProjectExplorerInfo> &projectInfos) {
            return GetReportFiles(projectInfos);
        }
    };

    fs::path CreatePlatformDatabase(const std::string &name) {
        const fs::path path = fs::temp_directory_path() / name;
        temporaryFiles_.push_back(path);
        std::error_code error;
        fs::remove(path, error);
        sqlite3 *database = nullptr;
        EXPECT_EQ(sqlite3_open(path.string().c_str(), &database), SQLITE_OK);
        DatabaseTestCaseMockUtil::CreateTable(database,
            "CREATE TABLE NUMA_TITLES_NAMES (name TEXT, description TEXT, summary_flag INTEGER, "
            "measurement_unit TEXT, unique_id INTEGER);"
            "CREATE TABLE NUMA_LEVELS_HIERARCHY_NAMES (title0_id INTEGER, title1_id INTEGER, title2_id INTEGER);"
            "CREATE TABLE NUMA_METRICS (ts INTEGER, value REAL, levels_id INTEGER);"
            "CREATE TABLE NUMA_SCALING_VALUES (id INTEGER PRIMARY KEY, level_id INTEGER, max_value REAL);");
        EXPECT_EQ(sqlite3_close(database), SQLITE_OK);
        auto &manager = Timeline::DataBaseManager::Instance();
        manager.SetDataType(Timeline::DataType::DB, path.string());
        EXPECT_TRUE(manager.CreateTraceConnectionPool(path.string(), path.string()));
        return path;
    }

    ImportActionResponse SetHostInfo(const fs::path &path) {
        std::map<std::string, HostInfo> hosts = {{"msprof0_", {{path.string(), {"-1"}}}}};
        ImportActionResponse response;
        ProjectParserDbTestHelper().SetHostInfoHelper(hosts, response, static_cast<int64_t>(ProjectTypeEnum::DB));
        return response;
    }

    std::vector<fs::path> temporaryFiles_;
};

TEST_F(ProjectParserDbTest, multiDeivce) {
    std::string path = GetMultiDeviceTestDataPath();
    DataPrepare();
    ProjectExplorerInfo projectInfo;
    projectInfo.projectName = "multiDeviceTest";
    projectInfo.projectType = static_cast<int64_t>(Dic::ProjectTypeEnum::DB);
    DbParserTestHelper parser;
    std::string error;
    auto parseFileList = parser.GetParseFileByImportFile(path, error);
    EXPECT_EQ(parseFileList.size(), 1); // expect 1
    ProjectParserDb::BuildProjectExploreInfo(projectInfo, parseFileList);
    EXPECT_EQ(projectInfo.subParseFileInfo.size(), 4); // expect 4
    auto isAllDevice = std::all_of(projectInfo.subParseFileInfo.begin(), projectInfo.subParseFileInfo.end(),
        [](const auto &info) { return info->type == ParseFileType::DEVICE_CHIP; });
    EXPECT_EQ(isAllDevice, true);
    std::vector<ProjectExplorerInfo> vec;
    vec.push_back(projectInfo);
    parser.GetReportFileTestHelper(vec);
    std::set<std::string> expectDeviceIds = {"0", "1", "2", "3"};
    std::set<std::string> deviceIds;
    std::for_each(projectInfo.subParseFileInfo.begin(), projectInfo.subParseFileInfo.end(),
        [&expectDeviceIds, &deviceIds](const auto &info) {
            EXPECT_EQ(expectDeviceIds.count(info->deviceId), 1); // expect 1
            deviceIds.insert(info->deviceId);
        });
    EXPECT_EQ(expectDeviceIds, deviceIds);
}

TEST_F(ProjectParserDbTest, parse_baseline_info_emty_cluster) {
    ProjectExplorerInfo project;
    project.fileInfoMap.emplace("test", std::make_shared<ParseFileInfo>());
    BaselineInfo baselineInfo;
    baselineInfo.isCluster = true;
    ProjectParserDb dbParser;
    EXPECT_NO_THROW(dbParser.ParserBaseline(project, baselineInfo));
}

TEST_F(ProjectParserDbTest, set_rank_device_map_multi_device) {
    ProjectParserDbTestHelper dbParser;
    auto fileInfo = std::make_shared<ParseFileInfo>();
    fileInfo->type = DEVICE_CHIP;
    fileInfo->rankId = "test_rankId";
    fileInfo->deviceId = "test_deviceId";
    std::unordered_map<std::string, std::string> rankDeviceMap;
    dbParser.SetRankDeviceMapHelper(fileInfo, rankDeviceMap, "", "");
    EXPECT_EQ(rankDeviceMap[fileInfo->rankId], fileInfo->deviceId);
}

TEST_F(ProjectParserDbTest, SetHostInfoAddsPlatformCardForMergedDatabase) {
    const auto merged = CreatePlatformDatabase("msinsight_merged_platform_timeline_test.db");
    const auto mergedResponse = SetHostInfo(merged);
    ASSERT_EQ(mergedResponse.body.result.size(), 2);
    EXPECT_EQ(mergedResponse.body.result[1].cardName, "Platform Metrics");
    EXPECT_EQ(mergedResponse.body.result[1].rankId, FullDb::BuildEmbeddedPlatformRankId("msprof0_-1"));
    EXPECT_EQ(mergedResponse.body.result[1].fileId, merged.string());
}

TEST_F(ProjectParserDbTest, GetDbFilesInDirIgnoresStandalonePlatformDatabase) {
    const fs::path directory = fs::temp_directory_path() / "msinsight-platform-discovery-test";
    temporaryFiles_.push_back(directory);
    fs::create_directories(directory);
    const fs::path merged = directory / "ascend_pytorch_profiler.db";
    const fs::path standalone = directory / "platform.db";
    sqlite3 *database = nullptr;
    ASSERT_EQ(sqlite3_open(merged.string().c_str(), &database), SQLITE_OK);
    ASSERT_EQ(sqlite3_close(database), SQLITE_OK);
    ASSERT_EQ(sqlite3_open(standalone.string().c_str(), &database), SQLITE_OK);
    ASSERT_EQ(sqlite3_close(database), SQLITE_OK);

    const auto files = ProjectParserDbTestHelper().GetDbFilesInDirHelper(directory.string());
    ASSERT_EQ(files.size(), 1U);
    EXPECT_EQ(files.front(), merged.string());
    EXPECT_TRUE(ProjectParserDbTestHelper().GetDbFilesInDirHelper(standalone.string()).empty());
}

TEST_F(ProjectParserDbTest, SetHostInfoEmitsOneActionForSameLogicalRank) {
    ProjectParserDbTestHelper parser;
    std::map<std::string, HostInfo> hostInfoMap = {{"host", {{"z.db", {"0"}}, {"a.db", {"0"}}, {"b.db", {"1"}}}}};
    ImportActionResponse response;

    parser.SetHostInfoHelper(hostInfoMap, response, static_cast<int64_t>(ProjectTypeEnum::DB));

    ASSERT_EQ(response.body.result.size(), 2U);
    EXPECT_EQ(response.body.result[0].rankId, "host0");
    EXPECT_EQ(response.body.result[0].fileId, "a.db");
    EXPECT_EQ(response.body.result[1].rankId, "host1");
    EXPECT_EQ(hostInfoMap["host"]["z.db"][0], "host0");
    EXPECT_EQ(hostInfoMap["host"]["a.db"][0], "host0");
}

TEST_F(ProjectParserDbTest, SetHostInfoEmitsOnePlatformActionForSameLogicalRank) {
    const auto first = CreatePlatformDatabase("msinsight-platform-source-a.db");
    const auto second = CreatePlatformDatabase("msinsight-platform-source-b.db");
    ProjectParserDbTestHelper parser;
    std::map<std::string, HostInfo> hostInfoMap = {
        {"host", {{second.string(), {"0"}}, {first.string(), {"0"}}}},
    };
    ImportActionResponse response;

    parser.SetHostInfoHelper(hostInfoMap, response, static_cast<int64_t>(ProjectTypeEnum::DB));

    ASSERT_EQ(response.body.result.size(), 2U);
    EXPECT_EQ(response.body.result[0].rankId, "host0");
    EXPECT_EQ(response.body.result[1].rankId, FullDb::BuildEmbeddedPlatformRankId("host0"));
    EXPECT_EQ(response.body.result[1].fileId, first.string());
}
