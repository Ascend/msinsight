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

#ifndef PROFILER_SERVER_UTF8_TEST_UTIL_H
#define PROFILER_SERVER_UTF8_TEST_UTIL_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "FileUtil.h"

namespace Dic::Test {
class Utf8TempDirectory {
  public:
    Utf8TempDirectory() {
        const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#ifdef _WIN32
        std::vector<wchar_t> temp(32768);
        DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
        if (length == 0 || length >= temp.size()) {
            throw std::runtime_error("Cannot retrieve native temporary directory");
        }
        root = StringUtil::WString2String(std::wstring(temp.data(), length));
#else
        root = std::filesystem::temp_directory_path().u8string();
#endif
        root = FileUtil::SplicePath(root, "msinsight_utf8_" + unique + u8"_练_😀 space");
        if (!std::filesystem::create_directory(NativePath(root))) {
            throw std::runtime_error("Cannot create Unicode temporary directory");
        }
#ifndef _WIN32
        std::filesystem::permissions(NativePath(root), std::filesystem::perms::owner_all);
#endif
    }

    ~Utf8TempDirectory() {
#ifdef _WIN32
        RemoveNativeDirectory(FileUtil::ConvertToLongPathW(root));
#else
        std::error_code error;
        std::filesystem::remove_all(NativePath(root), error);
#endif
    }
    Utf8TempDirectory(const Utf8TempDirectory &) = delete;
    Utf8TempDirectory &operator=(const Utf8TempDirectory &) = delete;

    std::string Path(const std::string &name = "") const { return FileUtil::SplicePath(root, name); }

    static std::filesystem::path NativePath(const std::string &path) {
#ifdef _WIN32
        return std::filesystem::path(FileUtil::ConvertToLongPathW(path));
#else
        return std::filesystem::u8path(path);
#endif
    }

    static bool Write(const std::string &path, const std::string &bytes) {
        std::ofstream stream(NativePath(path), std::ios::binary | std::ios::trunc);
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return stream.good();
    }

  private:
#ifdef _WIN32
    // MinGW's remove_all can loop on extended-length paths. Clean only this
    // instance's unique directory, without following directory reparse points.
    static void RemoveNativeDirectory(const std::wstring &path) {
        WIN32_FIND_DATAW entry{};
        HANDLE search = FindFirstFileW((path + L"\\*").c_str(), &entry);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                const std::wstring name(entry.cFileName);
                if (name == L"." || name == L"..") {
                    continue;
                }
                const auto child = path + L"\\" + name;
                if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    if ((entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                        RemoveNativeDirectory(child);
                    } else {
                        RemoveDirectoryW(child.c_str());
                    }
                } else {
                    DeleteFileW(child.c_str());
                }
            } while (FindNextFileW(search, &entry));
            FindClose(search);
        }
        RemoveDirectoryW(path.c_str());
    }
#endif
    std::string root;
};
} // namespace Dic::Test
#endif // PROFILER_SERVER_UTF8_TEST_UTIL_H
