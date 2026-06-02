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
#include "FileUtil.h"
#include "CommunicationGroupParser.h"
#include "ClusterDef.h"
#include "TestSuit.h"

using namespace Dic::Module;
using namespace Dic::Module::Communication;
class CommunicationGroupParserTest : public ::testing::Test {
  protected:
    std::string filePath;

    void SetUp() override { filePath = TestSuit::GetTestDataFile("communication_group"); }
};

/**
 * 测试新的communication_grou.json格式（带有comm_group_parallel_info数据）
 */
TEST_F(CommunicationGroupParserTest, TestNewCommunicationGroup) {
    std::string file = FileUtil::SplicePath(filePath, "communication_group.json");
    std::vector<CommGroupParallelInfo> res = CommunicationGroupParser::ParseCommunicationGroup(file);
    const int exceptSize = 22;
    EXPECT_EQ(res.size(), exceptSize);
}

/**
 * 测试老的communication_grou.json格式（不带comm_group_parallel_info数据）
 */
TEST_F(CommunicationGroupParserTest, TestOldCommunicationGroup) {
    std::string file = FileUtil::SplicePath(filePath, "communication_group_old.json");
    std::vector<CommGroupParallelInfo> res = CommunicationGroupParser::ParseCommunicationGroup(file);
    const int exceptSize = 20;
    EXPECT_EQ(res.size(), exceptSize);
}
