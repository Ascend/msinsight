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
#include "Utf8TestUtil.h"

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

TEST_F(SafeFileTest, ReadsUtf8PathAsUnmodifiedBinaryBytes) {
    Dic::Test::Utf8TempDirectory directory;
    const auto path = directory.Path(u8"练 😀.json");
    const std::string bytes = std::string(u8"{\"name\":\"练 😀\"}\r\n") + std::string("a\0b", 3);
    ASSERT_TRUE(Dic::Test::Utf8TempDirectory::Write(path, bytes));
    auto input = Dic::OpenReadFileSafely(path, std::ios::binary);
    ASSERT_TRUE(input.good());
    const std::string actual{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    EXPECT_EQ(actual, bytes);
    EXPECT_FALSE(Dic::OpenReadFileSafely(directory.Path("missing.json")));
}
