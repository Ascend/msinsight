/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "Utf8TestUtil.h"
#include <vector>
#include <string>
#include <optional>
#include "gtest/gtest.h"
#include "FileUtil.h"
#include "PythonUtil.h"

using namespace Dic;

class PythonUtilTest : public testing::Test {};

#if defined(__linux__) || defined(__APPLE__)
TEST_F(PythonUtilTest, ExecuteCommandListOnLinuxOrMac) {
    std::string executablePath{"ls"};
    std::vector<std::string> arguments{"-l"};
    int result = PythonUtil::ExecuteCommand(executablePath, arguments, std::nullopt);
    EXPECT_EQ(result, 0);
}
#endif

#ifdef _WIN32
TEST_F(PythonUtilTest, GetPythonCommandReturnsBundledPythonPathOnWindows) {
    std::string expected = FileUtil::SplicePath(FileUtil::GetCurrPath(), "python", "python.exe");

    EXPECT_EQ(PythonUtil::GetPythonCommand(), expected);
}

TEST_F(PythonUtilTest, ExecuteCommandReturnsFailureWhenExecutableMissingOnWindows) {
    std::string executablePath =
        FileUtil::SplicePath(FileUtil::GetCurrPath(), "missing_python_for_python_util_test.exe");

    EXPECT_EQ(PythonUtil::ExecuteCommand(executablePath, {}, std::nullopt), -1);
}
#endif

#ifdef _WIN32
TEST_F(PythonUtilTest, Utf8ExecutableAndRedirectPathSupportUnicodeAndSpaces) {
    Dic::Test::Utf8TempDirectory directory;
    std::vector<wchar_t> system(32768);
    ASSERT_GT(GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size())), 0U);
    const auto source = Dic::StringUtil::WString2String(std::wstring(system.data()) + L"\\cmd.exe");
    const auto executable = directory.Path(u8"练 😀 command.exe");
    ASSERT_TRUE(FileUtil::CopyFileByPath(source, executable));
    const auto logPath = directory.Path(u8"练 😀 output.log");
    // cmd parses its command as one argument. /u explicitly requests UTF-16LE;
    // redirected child bytes must stay unchanged rather than be decoded as UTF-8.
    ASSERT_EQ(PythonUtil::ExecuteCommand(executable, {u8"/u /c echo utf8-path-ok 练 😀"}, logPath), 0);
    std::ifstream input(Dic::Test::Utf8TempDirectory::NativePath(logPath), std::ios::binary);
    ASSERT_TRUE(input.good());
    const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto expected = StringUtil::String2WString(u8"utf8-path-ok 练 😀");
    const std::string expectedBytes(reinterpret_cast<const char *>(expected.data()), expected.size() * sizeof(wchar_t));
    EXPECT_NE(bytes.find(expectedBytes), std::string::npos);
}
#endif
