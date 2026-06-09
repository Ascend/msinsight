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
#include "ProjectParserFtrace.h"
#include "FileUtil.h"

using namespace Dic::Module;
using namespace Dic::Module::Global;

class ProjectParserTest : public testing::Test {};

TEST_F(ProjectParserTest, BaseIsParseFile) { EXPECT_EQ(false, ProjectParserBase().IsParsedFile("test")); }

TEST_F(ProjectParserTest, GetParentFolders) {
    std::string path = "/home/user/name/profiling/cluster1/ASCEND_PROFILER_OUTPUT";
    std::string prefix = "/home/user/name/profiling";
    EXPECT_EQ(ProjectParserBase().GetParentFileList(prefix, path).size(), 1); // expect parent folders of path is  2
}

TEST_F(ProjectParserTest, GetParentFoldersPathEqualPrefix) {
    EXPECT_EQ(ProjectParserBase().GetParentFileList("/home/user", "/home/user").empty(), true);
}

TEST_F(ProjectParserTest, GetClusterInfo) {
    std::vector<std::string> folders = ProjectParserBase().GetParentFileList(
        "/home/user/name/profiling", "/home/user/name/profiling/cluster1/host_ascend_pt/ASCEND_PROFILER_OUTPUT");
    auto [cluster, clusterPrefix] = ProjectParserBase().GetClusterInfo(folders);
    EXPECT_EQ(FileUtil::GetFileName(cluster), "cluster1");
    EXPECT_EQ(clusterPrefix, "/home/user/name/profiling/cluster1");
}

TEST_F(ProjectParserTest, GetClusterInfoEmpty) {
    std::vector<std::string> folders = {};
    auto [cluster, clusterPrefix] = ProjectParserBase().GetClusterInfo(folders);
    EXPECT_EQ(cluster, "");
    EXPECT_EQ(clusterPrefix, "");
}

TEST_F(ProjectParserTest, ParserBaseParser) {
    ProjectParserBase parser;
    ProjectExplorerInfo info;
    ImportActionRequest request;
    ImportActionResponse response;
    parser.Parser({info}, request, response);
}

TEST_F(ProjectParserTest, ParserBaseline) {
    ProjectParserBase parser;
    ProjectExplorerInfo info;
    BaselineInfo baselineInfo;
    parser.ParserBaseline(info, baselineInfo);
}

TEST_F(ProjectParserTest, FtraceProjectTypeSupportsBaselineCompare) {
    EXPECT_TRUE(IsSupportCompareType(ProjectTypeEnum::DB_FTRACE));
    EXPECT_TRUE(IsComparable(ProjectTypeEnum::DB_FTRACE, ProjectTypeEnum::DB_FTRACE));
}

TEST_F(ProjectParserTest, GetParseFileByImportFile) {
    ProjectParserBase parser;
    std::string error;
    auto res = parser.GetParseFileByImportFile("test", error);
    EXPECT_EQ(res.size(), 1); // expect size 1
    EXPECT_EQ(res[0], "test");
}

TEST_F(ProjectParserTest, ParseProgressCallBack) {
    ProjectParserBase parser;
    parser.ParseProgressCallBack("0", 40, 100, 40); // set 40/100, percent=40
}
