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
#include <gtest/gtest.h>
#include "PluginsManager.h"
#include "Utf8TestUtil.h"

#ifdef _WIN32
namespace Dic::Core {
void AddPathEnv(const std::string &newDir);
}

TEST(PluginsManager, Utf8PluginPathPreservesNativeEnvironmentText) {
    Dic::Test::Utf8TempDirectory directory;
    const DWORD capacity = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    std::vector<wchar_t> previous(capacity > 0 ? capacity : 1);
    if (capacity > 0) {
        ASSERT_GT(GetEnvironmentVariableW(L"PATH", previous.data(), capacity), 0U);
    }
    struct RestorePath {
        bool existed;
        const wchar_t *value;
        ~RestorePath() { SetEnvironmentVariableW(L"PATH", existed ? value : nullptr); }
    } restore{capacity > 0, previous.data()};
    const std::wstring original = L"C:\\original_\u7EC3";
    ASSERT_TRUE(SetEnvironmentVariableW(L"PATH", original.c_str()));
    Dic::Core::AddPathEnv(directory.Path());
    std::vector<wchar_t> current(GetEnvironmentVariableW(L"PATH", nullptr, 0));
    ASSERT_GT(GetEnvironmentVariableW(L"PATH", current.data(), static_cast<DWORD>(current.size())), 0U);
    const std::wstring expected = original + L";" + Dic::StringUtil::String2WString(directory.Path());
    EXPECT_EQ(std::wstring(current.data()), expected);
    Dic::Core::AddPathEnv(directory.Path());
    EXPECT_EQ(GetEnvironmentVariableW(L"PATH", current.data(), static_cast<DWORD>(current.size())), expected.size());
    EXPECT_EQ(std::wstring(current.data()), expected);
}

#endif
