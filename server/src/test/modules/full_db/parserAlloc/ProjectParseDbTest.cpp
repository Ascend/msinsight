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
#include "FileUtil.h"
using namespace Dic::Module;
using namespace Dic::Module::Global;

class ProjectParserDbTest : public testing::Test {
  protected:
    class ProjectParserDbTestHelper : public ProjectParserDb {
      public:
        void SetRankDeviceMapHelper(std::shared_ptr<ParseFileInfo> parseFileInfo,
            std::unordered_map<std::string, std::string> &rankDeviceMap, const std::string &deviceIdInMem,
            const std::string &rank) {
            SetRankDeviceMap(parseFileInfo, rankDeviceMap, deviceIdInMem, rank);
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
    void TearDown() override { FullDb::DataBaseManager::Instance().Clear(); }

    class DbParserTestHelper : public ProjectParserDb {
      public:
        std::map<std::string, HostInfo> GetReportFileTestHelper(std::vector<ProjectExplorerInfo> &projectInfos) {
            return GetReportFiles(projectInfos);
        }
    };
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
