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
#include "SafeFile.h"

class SafeFileTest : public ::testing::Test {};

TEST_F(SafeFileTest, testOpenReadFileSafelyWhenFileCanNotBeWrittenByOthers) {
    std::string path = "FileCanNotBeWrittenByOthers.tmp";
    std::string content = "test";
    std::ofstream out(path);
    out.write(content.c_str(), content.size());
    out.close();
    // 设置仅owner拥有写权限
    fs::permissions(path, fs::perms::owner_all);

    // 读取文件并比较内容
    auto in = Dic::OpenReadFileSafely(path, std::ios::in);
    EXPECT_TRUE(in);
    std::string readLine;
    readLine.resize(content.length());
    in.read(&readLine[0], content.length());
    in.close();
    EXPECT_EQ(content, readLine);

    // 删除文件
    std::remove(path.c_str());
}

TEST_F(SafeFileTest, testOpenWithReadMode) {
    std::string path = "FileCanNotBeWrittenByOthers.tmp";
    std::string content = "test";
    std::ofstream out(path);
    out.write(content.c_str(), content.size());
    out.close();
    // 设置仅owner拥有写权限
    fs::permissions(path, fs::perms::owner_all);

    // 读取文件并比较内容
    auto in = Dic::OpenReadFileSafely(path, std::ios::out);
    EXPECT_FALSE(in);
}
