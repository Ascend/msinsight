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
#include "GlobalDefs.h"
#include "ProjectParserFactory.h"
#include "FileUtil.h"

using namespace Dic;
using namespace Dic::Module;

class ParserFactoryTest : public ::testing::Test {};

TEST_F(ParserFactoryTest, GetImportTypeBinTest) {
    std::pair<std::string, ParserType> result = ParserFactory::GetImportType("/home/user/data/visualize_data.bin");
    std::pair<std::string, ParserType> expect{"/home/user/data/visualize_data.bin", ParserType::BIN};
    EXPECT_EQ(result, expect);
}

TEST_F(ParserFactoryTest, GetImportTypeDbTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(
        currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt", "ASCEND_PROFILER_OUTPUT");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "ascend_pytorch_profiler_0.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::DB};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeDbClusterTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "cluster_analysis.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB};
    EXPECT_EQ(result1, expect1);
    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::DB};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList2;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeDbNPUMonitorTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "msmonitor_99092_20250901114924883_0.db");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "npumonitor")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::DB_NPUMONITOR};
    EXPECT_EQ(result1, expect1);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeACLGraphDebugTextTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "aclgraph_debug", "graph_debug.json")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::ACLGRPAH_DEBUG_JSON};
    EXPECT_EQ(result1, expect1);
}

TEST_F(ParserFactoryTest, GetImportTypeTextTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath = Dic::FileUtil::SplicePath(
        currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt", "ASCEND_PROFILER_OUTPUT");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "trace_view.json");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::JSON};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "test", "ubuntu_ascend_pt")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::JSON};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeTextClusterTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "cluster_communication.json");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster", "cluster_analysis_output")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::JSON};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "cluster")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::JSON};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList2;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, GetImportTypeOtherTest) {
#ifdef __linux__
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string folderPath =
        Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar", "scalar_data");
    const std::string dbPath = Dic::FileUtil::SplicePath(folderPath, "tf.event.out.1");
    const std::string mkdirCommand = "mkdir -p " + folderPath;
    system(mkdirCommand.c_str());
    const std::string touchCommand = "touch " + dbPath;
    system(touchCommand.c_str());

    std::string pathList1{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar")};
    std::pair<std::string, ParserType> result1 = ParserFactory::GetImportType(pathList1);
    std::pair<std::string, ParserType> expect1{pathList1, ParserType::OTHER};
    EXPECT_EQ(result1, expect1);

    std::string pathList2{Dic::FileUtil::SplicePath(currPath.substr(0, index), "test", "data", "scalar_data")};
    std::pair<std::string, ParserType> result2 = ParserFactory::GetImportType(pathList2);
    std::pair<std::string, ParserType> expect2{pathList2, ParserType::OTHER};
    EXPECT_EQ(result2, expect2);

    const std::string rmCommand = "rm -rf " + pathList1;
    system(rmCommand.c_str());
#endif
}

TEST_F(ParserFactoryTest, ParserBaseSendFail) {
    ProjectParserBase parser;
    EXPECT_EQ(parser.GetSubId("test", ParseFileType::RANK), "test");
    EXPECT_EQ(parser.GetRankIdFromPath("test", "test/rank0"), "test");
    EXPECT_EQ(ProjectParserBase::GetDbPath("test", 0), "test_0.db");
}
