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
#include <iterator>
#include <vector>
#include "FileUtil.h"
#include "LogUtil.h"

using namespace Dic;

TEST(LogUtil, PercentFormattingRules) {
    const std::string originPath = FileUtil::SplicePath(::testing::TempDir(), "log_util_percent.log");
    const std::string expectedPath = FileUtil::SplicePath(::testing::TempDir(), "log_util_percent_244_1.log");
    if (FileUtil::CheckFilePathExist(expectedPath)) {
        ASSERT_TRUE(FileUtil::RemoveFile(expectedPath));
    }

    std::string actualPath;
    {
        LogUtil logger(LogOutType::FILE, originPath, "244");
        logger.LogT(LogLevel::L_INFO, "", "progress=100%%, op=%", "Mat%Mul");
        std::vector<std::string> values = {"progress=100%%, rankId=%, op=%", "244", "Mat%Mul"};
        logger.LogT(LogLevel::L_INFO, std::string(), values);
        actualPath = logger.GetLogFilePath();
    }

    std::ifstream stream(actualPath, std::ios::binary);
    ASSERT_TRUE(stream);
    const std::string log{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    EXPECT_NE(log.find("progress=100%, op=Mat%Mul"), std::string::npos);
    EXPECT_NE(log.find("progress=100%, rankId=244, op=Mat%Mul"), std::string::npos);
    stream.close();
    EXPECT_TRUE(FileUtil::RemoveFile(actualPath));
}
