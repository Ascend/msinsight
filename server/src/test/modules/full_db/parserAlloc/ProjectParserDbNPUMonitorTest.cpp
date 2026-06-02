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
#include "DataBaseManager.h"
#include "ProjectParserDbNPUMonitor.h"
#include "FileUtil.h"

using namespace Dic;
using namespace Dic::Module;

class ProjectParserDbNPUMonitorTest : public testing::Test {};

TEST_F(ProjectParserDbNPUMonitorTest, GetProjectTypeTest) {
    ProjectParserDbNPUMonitor parser;
    std::string path = "/home/Data/npumonitor";
    ProjectTypeEnum type = parser.GetProjectType(path);
    EXPECT_EQ(type, ProjectTypeEnum::DB_NPUMONITOR);
}

TEST_F(ProjectParserDbNPUMonitorTest, GetParseFileByImportFileTestEmptyFolder) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());

    ProjectParserDbNPUMonitor parser;
    std::string error;
    std::vector<std::string> npuMonitorFiles = parser.GetParseFileByImportFile(folderPath, error);
    EXPECT_FALSE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 1);
    EXPECT_EQ(npuMonitorFiles[0], folderPath);

    Timeline::DataBaseManager::Instance().Clear();
    const std::string rmCommand = "rm -rf " + folderPath;
    system(rmCommand.c_str());
#endif
}

TEST_F(ProjectParserDbNPUMonitorTest, GetParseFileByImportFileTestFolderContainingOneFile) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    ProjectParserDbNPUMonitor parser;
    std::string error;
    std::vector<std::string> npuMonitorFiles = parser.GetParseFileByImportFile(folderPath, error);
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 1);
    EXPECT_EQ(npuMonitorFiles[0], dbPath);

    Timeline::DataBaseManager::Instance().Clear();
    const std::string rmCommand = "rm -rf " + folderPath;
    system(rmCommand.c_str());
#endif
}

TEST_F(ProjectParserDbNPUMonitorTest, GetParseFileByImportFileTestFolderContainingMultipleFiles) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string dbPath1 = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string dbPath2 = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99095_20250901114924895_1.db");
    const std::string dbPath3 = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99098_20250901114924898_-1.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand1 = "touch " + dbPath1;
    system(touchCommand1.c_str());
    const std::string touchCommand2 = "touch " + dbPath2;
    system(touchCommand2.c_str());
    const std::string touchCommand3 = "touch " + dbPath3;
    system(touchCommand3.c_str());

    ProjectParserDbNPUMonitor parser;
    std::string error;
    std::vector<std::string> npuMonitorFiles = parser.GetParseFileByImportFile(folderPath, error);
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 3); // 3
    std::sort(npuMonitorFiles.begin(), npuMonitorFiles.end());
    EXPECT_EQ(npuMonitorFiles[0], dbPath1);
    EXPECT_EQ(npuMonitorFiles[1], dbPath2);
    EXPECT_EQ(npuMonitorFiles[2], dbPath3); // 2

    Timeline::DataBaseManager::Instance().Clear();
    const std::string rmCommand = "rm -rf " + folderPath;
    system(rmCommand.c_str());
#endif
}

TEST_F(ProjectParserDbNPUMonitorTest, GetParseFileByImportFileTestFolderContainingFolderContainingMultipleFiles) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string innerFolderPath = Dic::FileUtil::SplicePath(folderPath, "data");
    const std::string dbPath1 = Dic::FileUtil::SplicePath(innerFolderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string dbPath2 = Dic::FileUtil::SplicePath(innerFolderPath, "msmonitor_99095_20250901114924895_1.db");
    const std::string dbPath3 = Dic::FileUtil::SplicePath(innerFolderPath, "msmonitor_99098_20250901114924898_-1.db");
    const std::string mkdirCommand = "mkdir -p " + innerFolderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand1 = "touch " + dbPath1;
    system(touchCommand1.c_str());
    const std::string touchCommand2 = "touch " + dbPath2;
    system(touchCommand2.c_str());
    const std::string touchCommand3 = "touch " + dbPath3;
    system(touchCommand3.c_str());

    ProjectParserDbNPUMonitor parser;
    std::string error;
    std::vector<std::string> npuMonitorFiles = parser.GetParseFileByImportFile(folderPath, error);
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 3); // 3
    std::sort(npuMonitorFiles.begin(), npuMonitorFiles.end());
    EXPECT_EQ(npuMonitorFiles[0], dbPath1);
    EXPECT_EQ(npuMonitorFiles[1], dbPath2);
    EXPECT_EQ(npuMonitorFiles[2], dbPath3); // 2

    Timeline::DataBaseManager::Instance().Clear();
    const std::string rmCommand = "rm -rf " + folderPath;
    system(rmCommand.c_str());
#endif
}

TEST_F(ProjectParserDbNPUMonitorTest, GetParseFileByImportFileTestSingleFile) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string dbPath1 = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string dbPath2 = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99093_20250901114924895_-1.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand1 = "touch " + dbPath1;
    system(touchCommand1.c_str());
    const std::string touchCommand2 = "touch " + dbPath2;
    system(touchCommand2.c_str());

    ProjectParserDbNPUMonitor parser;
    std::string error;
    std::vector<std::string> npuMonitorFiles = parser.GetParseFileByImportFile(dbPath1, error);
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 1);
    EXPECT_EQ(npuMonitorFiles[0], dbPath1);
    npuMonitorFiles = parser.GetParseFileByImportFile(dbPath2, error);
    EXPECT_TRUE(error.empty());
    ASSERT_EQ(npuMonitorFiles.size(), 1);
    EXPECT_EQ(npuMonitorFiles[0], dbPath2);

    Timeline::DataBaseManager::Instance().Clear();
    const std::string rmCommand = "rm -rf " + folderPath;
    system(rmCommand.c_str());
#endif
}

TEST_F(ProjectParserDbNPUMonitorTest, BuildProjectExploreInfoTest) {
    ProjectExplorerInfo info;
    info.fileName = "/home/Data/npumonitor";
    std::vector<std::string> parsedFiles = {"/home/Data/npumonitor/msmonitor_99092_20250901114924883_0.db",
        "/home/Data/npumonitor/msmonitor_99093_20250901114924876_1.db"};
    ProjectParserDbNPUMonitor parser;
    parser.BuildProjectExploreInfo(info, parsedFiles);
    ASSERT_EQ(info.subParseFileInfo.size(), 2); // 2
    EXPECT_EQ(info.subParseFileInfo[0]->fileId, "/home/Data/npumonitor/msmonitor_99092_20250901114924883_0.db");
    EXPECT_EQ(info.subParseFileInfo[1]->fileId, "/home/Data/npumonitor/msmonitor_99093_20250901114924876_1.db");
}

TEST_F(ProjectParserDbNPUMonitorTest, parse_baseline) {
    ProjectExplorerInfo project;
    project.fileInfoMap.emplace("test", std::make_shared<ParseFileInfo>());
    BaselineInfo baselineInfo;
    baselineInfo.isCluster = true;
    ProjectParserDbNPUMonitor dbParser;
    EXPECT_NO_THROW(dbParser.ParserBaseline(project, baselineInfo));
}
