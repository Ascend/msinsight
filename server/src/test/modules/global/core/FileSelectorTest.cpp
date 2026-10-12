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
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#endif
#include "BaselineManagerService.h"
#include "ParserStatusManager.h"
#include "FileSelector.h"
#include "GlobalProtocolResponse.h"
#include "ProtocolDefs.h"
#include "FileUtil.h"
#include "TestSuit.h"

using namespace Dic::Module::Global;
using namespace Dic::Protocol;
class FileSelectorTest : public ::testing::Test {
  protected:
    inline static std::string testDataDir = TestSuit::GetTestDataFile();
};

TEST_F(FileSelectorTest, TestPathNotExist) {
    std::string path = "xxx";
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_FALSE(exist);
    ASSERT_TRUE(childrenFolders.empty());
    ASSERT_TRUE(childrenFiles.empty());
}

TEST_F(FileSelectorTest, TestContainFolder) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "msprof");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<Folder>> realChildrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    std::vector<std::string> folders = {"normal", "slice"};
    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = Dic::FileUtil::SplicePath(path, folder);
        realChildrenFolders.emplace_back(std::move(folderPtr));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < childrenFolders.size(); i++) {
        ASSERT_EQ(childrenFolders[i]->name, realChildrenFolders[i]->name);
    }
    ASSERT_TRUE(childrenFiles.empty());
}

TEST_F(FileSelectorTest, TestContainFiles) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    std::vector<std::unique_ptr<File>> realChildrenFiles;
    bool exist = true;
    std::vector<std::string> files = {
        "ascend_pytorch_profiler.db", "cluster_analysis.db", "leaks_dump_20250806.dat", "msprof_0.db"};
    for (const auto &file : files) {
        std::string tmpPath = Dic::FileUtil::SplicePath(path, file);
        realChildrenFiles.emplace_back(std::make_unique<File>(file, tmpPath));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < realChildrenFiles.size(); i++) {
        ASSERT_EQ(realChildrenFiles[i]->name, childrenFiles[i]->name);
    }
    ASSERT_TRUE(childrenFolders.empty());
}

TEST_F(FileSelectorTest, TestContainFolderandFiles) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "test_rank_0");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<Folder>> realChildrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    std::vector<std::unique_ptr<File>> realChildrenFiles;
    bool exist = true;
    std::vector<std::string> folders = {"ASCEND_PROFILER_OUTPUT"};
    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = Dic::FileUtil::SplicePath(path, folder);
        realChildrenFolders.emplace_back(std::move(folderPtr));
    }
    std::vector<std::string> files = {"profiler_info_0.json"};
    for (const auto &file : files) {
        std::string tmpPath = Dic::FileUtil::SplicePath(path, file);
        realChildrenFiles.emplace_back(std::make_unique<File>(file, tmpPath));
    }
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist);
    EXPECT_TRUE(exist);
    for (size_t i = 0; i < childrenFolders.size(); i++) {
        ASSERT_EQ(childrenFolders[i]->name, realChildrenFolders[i]->name);
    }
    for (size_t i = 0; i < realChildrenFiles.size(); i++) {
        ASSERT_EQ(realChildrenFiles[i]->name, childrenFiles[i]->name);
    }
}

TEST_F(FileSelectorTest, TestSortByNameDesc) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist, "name", "desc");
    EXPECT_TRUE(exist);
    ASSERT_GE(childrenFiles.size(), 2U);
    for (size_t i = 1; i < childrenFiles.size(); ++i) {
        std::string left = childrenFiles[i - 1]->name;
        std::string right = childrenFiles[i]->name;
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char c) { return std::tolower(c); });
        EXPECT_GE(left, right);
    }
}

TEST_F(FileSelectorTest, TestSortByMtimeDesc) {
#ifdef _WIN32
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    const std::string tempRoot = Dic::FileUtil::SplicePath(tempPath,
        "msinsight_file_selector_mtime_" + std::to_string(GetCurrentProcessId()) + "_" +
            std::to_string(GetTickCount()));
    ASSERT_NE(CreateDirectoryA(tempRoot.c_str(), nullptr), 0);
#else
    char tmpl[] = "/tmp/msinsight_file_selector_mtime_XXXXXX";
    char *created = mkdtemp(tmpl);
    ASSERT_NE(created, nullptr);
    const std::string tempRoot = created;
#endif

    // Names intentionally reverse alphabetical vs intended mtime order.
    const std::string older = Dic::FileUtil::SplicePath(tempRoot, "a_older.json");
    const std::string newer = Dic::FileUtil::SplicePath(tempRoot, "z_newer.json");
    {
        std::ofstream ofs(older);
        ofs << "old";
    }
    {
        std::ofstream ofs(newer);
        ofs << "new";
    }

    // Force distinct mtimes (1s-resolution FS ignores short sleeps).
#ifdef _WIN32
    auto setMtimeSec = [](const std::string &path, int64_t unixSeconds) -> bool {
        HANDLE handle = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return false;
        }
        constexpr uint64_t kEpochDiff = 116444736000000000ULL;
        ULARGE_INTEGER ull;
        ull.QuadPart = static_cast<uint64_t>(unixSeconds) * 10000000ULL + kEpochDiff;
        FILETIME ft;
        ft.dwLowDateTime = ull.LowPart;
        ft.dwHighDateTime = ull.HighPart;
        const BOOL ok = SetFileTime(handle, nullptr, nullptr, &ft);
        CloseHandle(handle);
        return ok != 0;
    };
#else
    auto setMtimeSec = [](const std::string &path, time_t unixSeconds) -> bool {
        struct utimbuf times {};
        times.actime = unixSeconds;
        times.modtime = unixSeconds;
        return utime(path.c_str(), &times) == 0;
    };
#endif
    ASSERT_TRUE(setMtimeSec(older, 1700000000));
    ASSERT_TRUE(setMtimeSec(newer, 1700000100));

    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(tempRoot, childrenFolders, childrenFiles, exist, "mtime", "desc");
    EXPECT_TRUE(exist);
    ASSERT_EQ(childrenFiles.size(), 2U);
    EXPECT_EQ(childrenFiles[0]->name, "z_newer.json");
    EXPECT_EQ(childrenFiles[1]->name, "a_older.json");
    EXPECT_GT(childrenFiles[0]->mtime, childrenFiles[1]->mtime);

    childrenFolders.clear();
    childrenFiles.clear();
    FileSelector::GetFoldersAndFiles(tempRoot, childrenFolders, childrenFiles, exist, "mtime", "asc");
    ASSERT_EQ(childrenFiles.size(), 2U);
    EXPECT_EQ(childrenFiles[0]->name, "a_older.json");
    EXPECT_EQ(childrenFiles[1]->name, "z_newer.json");

    EXPECT_EQ(std::remove(older.c_str()), 0);
    EXPECT_EQ(std::remove(newer.c_str()), 0);
#ifdef _WIN32
    EXPECT_NE(RemoveDirectoryA(tempRoot.c_str()), 0);
#else
    EXPECT_EQ(rmdir(tempRoot.c_str()), 0);
#endif
}

TEST_F(FileSelectorTest, TestInvalidSortParamsFallbackToNameAsc) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> baselineFolders;
    std::vector<std::unique_ptr<File>> baselineFiles;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, baselineFolders, baselineFiles, exist, "name", "asc");
    std::vector<std::unique_ptr<Folder>> childrenFolders;
    std::vector<std::unique_ptr<File>> childrenFiles;
    FileSelector::GetFoldersAndFiles(path, childrenFolders, childrenFiles, exist, "bad-key", "bad-order");
    ASSERT_EQ(baselineFiles.size(), childrenFiles.size());
    for (size_t i = 0; i < baselineFiles.size(); ++i) {
        EXPECT_EQ(baselineFiles[i]->name, childrenFiles[i]->name);
    }
}

namespace {
std::string MakeTempRoot(const std::string &tag) {
#ifdef _WIN32
    char tempPath[MAX_PATH] = {0};
    EXPECT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    const std::string root = Dic::FileUtil::SplicePath(
        tempPath, tag + "_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount64()));
    if (GetFileAttributesA(root.c_str()) != INVALID_FILE_ATTRIBUTES) {
        // rare collision; unique tick already reduces risk
    }
    EXPECT_NE(CreateDirectoryA(root.c_str(), nullptr), 0);
    return root;
#else
    std::string pattern = "/tmp/" + tag + "_XXXXXX";
    std::vector<char> buf(pattern.begin(), pattern.end());
    buf.push_back('\0');
    char *created = mkdtemp(buf.data());
    EXPECT_NE(created, nullptr);
    return created == nullptr ? std::string() : std::string(created);
#endif
}

bool SetMtimeSec(const std::string &path, int64_t unixSeconds) {
#ifdef _WIN32
    HANDLE handle = CreateFileA(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    constexpr uint64_t kEpochDiff = 116444736000000000ULL;
    ULARGE_INTEGER ull;
    ull.QuadPart = static_cast<uint64_t>(unixSeconds) * 10000000ULL + kEpochDiff;
    FILETIME ft;
    ft.dwLowDateTime = ull.LowPart;
    ft.dwHighDateTime = ull.HighPart;
    const BOOL ok = SetFileTime(handle, nullptr, nullptr, &ft);
    CloseHandle(handle);
    return ok != 0;
#else
    struct utimbuf times {};
    times.actime = static_cast<time_t>(unixSeconds);
    times.modtime = static_cast<time_t>(unixSeconds);
    return utime(path.c_str(), &times) == 0;
#endif
}

void WriteFile(const std::string &path, const std::string &content) {
    std::ofstream ofs(path);
    ofs << content;
}

void RemoveTempRoot(
    const std::string &root, const std::vector<std::string> &files, const std::vector<std::string> &dirs = {}) {
    for (const auto &f : files) {
        std::remove(f.c_str());
    }
    for (const auto &d : dirs) {
#ifdef _WIN32
        RemoveDirectoryA(d.c_str());
#else
        rmdir(d.c_str());
#endif
    }
#ifdef _WIN32
    RemoveDirectoryA(root.c_str());
#else
    rmdir(root.c_str());
#endif
}
} // namespace

TEST_F(FileSelectorTest, TestDefaultArgsMatchNameAsc) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> aFolders;
    std::vector<std::unique_ptr<File>> aFiles;
    std::vector<std::unique_ptr<Folder>> bFolders;
    std::vector<std::unique_ptr<File>> bFiles;
    bool existA = true;
    bool existB = true;
    FileSelector::GetFoldersAndFiles(path, aFolders, aFiles, existA);
    FileSelector::GetFoldersAndFiles(path, bFolders, bFiles, existB, "name", "asc");
    EXPECT_EQ(existA, existB);
    EXPECT_TRUE(existA);
    ASSERT_EQ(aFiles.size(), bFiles.size());
    for (size_t i = 0; i < aFiles.size(); ++i) {
        EXPECT_EQ(aFiles[i]->name, bFiles[i]->name);
    }
}

TEST_F(FileSelectorTest, TestSameMtimeSecondaryNameAsc) {
    const std::string root = MakeTempRoot("msinsight_fs_tie");
    const std::string bPath = Dic::FileUtil::SplicePath(root, "b_prof.json");
    const std::string aPath = Dic::FileUtil::SplicePath(root, "a_prof.json");
    WriteFile(bPath, "b");
    WriteFile(aPath, "a");
    ASSERT_TRUE(SetMtimeSec(aPath, 1700000500));
    ASSERT_TRUE(SetMtimeSec(bPath, 1700000500));

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(files.size(), 2U);
    // equal mtime -> secondary name asc
    EXPECT_EQ(files[0]->name, "a_prof.json");
    EXPECT_EQ(files[1]->name, "b_prof.json");
    EXPECT_EQ(files[0]->mtime, files[1]->mtime);

    RemoveTempRoot(root, {aPath, bPath});
}

TEST_F(FileSelectorTest, TestFoldersBeforeFilesWithMtimeSort) {
    const std::string root = MakeTempRoot("msinsight_fs_group");
    const std::string dirPath = Dic::FileUtil::SplicePath(root, "zzz_dir");
    const std::string filePath = Dic::FileUtil::SplicePath(root, "aaa_file.json");
#ifdef _WIN32
    ASSERT_NE(CreateDirectoryA(dirPath.c_str(), nullptr), 0);
#else
    ASSERT_EQ(mkdir(dirPath.c_str(), 0755), 0);
#endif
    WriteFile(filePath, "f");
    // Make file newer than folder so mtime-only interleave would put file first.
    ASSERT_TRUE(SetMtimeSec(dirPath, 1700000000));
    ASSERT_TRUE(SetMtimeSec(filePath, 1700000900));

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(folders.size(), 1U);
    ASSERT_EQ(files.size(), 1U);
    EXPECT_EQ(folders[0]->name, "zzz_dir");
    EXPECT_EQ(files[0]->name, "aaa_file.json");
    // Grouping: folders vector is separate and returned before files by dealResource contract.
    EXPECT_GT(files[0]->mtime, folders[0]->mtime);

    RemoveTempRoot(root, {filePath}, {dirPath});
}

TEST_F(FileSelectorTest, TestMtimePopulatedForExistingFiles) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_FALSE(files.empty());
    for (const auto &f : files) {
        EXPECT_GT(f->mtime, 0) << f->name;
    }
}

TEST_F(FileSelectorTest, TestSortByMtimeLargeDirectory) {
    const std::string root = MakeTempRoot("msinsight_fs_large");
    constexpr int kCount = 120;
    std::vector<std::string> created;
    created.reserve(kCount);
    const int64_t baseTs = 1700000000;
    for (int i = 0; i < kCount; ++i) {
        // Names reverse to mtime order: prof_000 newest? No: higher i => newer mtime, name has zero pad.
        char name[64];
        std::snprintf(name, sizeof(name), "prof_%03d.json", i);
        const std::string path = Dic::FileUtil::SplicePath(root, name);
        WriteFile(path, std::to_string(i));
        ASSERT_TRUE(SetMtimeSec(path, baseTs + i));
        created.push_back(path);
    }

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(files.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(files.front()->name, "prof_119.json");
    EXPECT_EQ(files.back()->name, "prof_000.json");
    for (size_t i = 1; i < files.size(); ++i) {
        EXPECT_GE(files[i - 1]->mtime, files[i]->mtime);
    }

    folders.clear();
    files.clear();
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "asc");
    ASSERT_EQ(files.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(files.front()->name, "prof_000.json");
    EXPECT_EQ(files.back()->name, "prof_119.json");

    folders.clear();
    files.clear();
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "name", "asc");
    ASSERT_EQ(files.size(), static_cast<size_t>(kCount));
    EXPECT_EQ(files.front()->name, "prof_000.json");
    EXPECT_EQ(files.back()->name, "prof_119.json");

    RemoveTempRoot(root, created);
}

TEST_F(FileSelectorTest, TestSortFoldersByMtime) {
    const std::string root = MakeTempRoot("msinsight_fs_dirs");
    const std::string oldDir = Dic::FileUtil::SplicePath(root, "a_old_dir");
    const std::string newDir = Dic::FileUtil::SplicePath(root, "z_new_dir");
#ifdef _WIN32
    ASSERT_NE(CreateDirectoryA(oldDir.c_str(), nullptr), 0);
    ASSERT_NE(CreateDirectoryA(newDir.c_str(), nullptr), 0);
#else
    ASSERT_EQ(mkdir(oldDir.c_str(), 0755), 0);
    ASSERT_EQ(mkdir(newDir.c_str(), 0755), 0);
#endif
    ASSERT_TRUE(SetMtimeSec(oldDir, 1700000000));
    ASSERT_TRUE(SetMtimeSec(newDir, 1700000800));

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(folders.size(), 2U);
    EXPECT_EQ(folders[0]->name, "z_new_dir");
    EXPECT_EQ(folders[1]->name, "a_old_dir");

    RemoveTempRoot(root, {}, {oldDir, newDir});
}

TEST_F(FileSelectorTest, TestMixedCaseNameSortIgnoreCase) {
    const std::string root = MakeTempRoot("msinsight_fs_case");
    const std::string upper = Dic::FileUtil::SplicePath(root, "B_prof.json");
    const std::string lower = Dic::FileUtil::SplicePath(root, "a_prof.json");
    WriteFile(upper, "B");
    WriteFile(lower, "a");

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "name", "asc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(files.size(), 2U);
    EXPECT_EQ(files[0]->name, "a_prof.json");
    EXPECT_EQ(files[1]->name, "B_prof.json");

    folders.clear();
    files.clear();
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "name", "desc");
    ASSERT_EQ(files.size(), 2U);
    EXPECT_EQ(files[0]->name, "B_prof.json");
    EXPECT_EQ(files[1]->name, "a_prof.json");

    RemoveTempRoot(root, {upper, lower});
}

TEST_F(FileSelectorTest, TestEmptyDirectory) {
    const std::string root = MakeTempRoot("msinsight_fs_empty");
    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    EXPECT_TRUE(folders.empty());
    EXPECT_TRUE(files.empty());
    RemoveTempRoot(root, {});
}

TEST_F(FileSelectorTest, TestSameMtimeSecondaryNameWithAscOrder) {
    const std::string root = MakeTempRoot("msinsight_fs_tie_asc");
    const std::string bPath = Dic::FileUtil::SplicePath(root, "b_prof.json");
    const std::string aPath = Dic::FileUtil::SplicePath(root, "a_prof.json");
    WriteFile(bPath, "b");
    WriteFile(aPath, "a");
    ASSERT_TRUE(SetMtimeSec(aPath, 1700000600));
    ASSERT_TRUE(SetMtimeSec(bPath, 1700000600));

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "asc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(files.size(), 2U);
    // equal mtime -> secondary name asc regardless of order direction for ties
    EXPECT_EQ(files[0]->name, "a_prof.json");
    EXPECT_EQ(files[1]->name, "b_prof.json");
    RemoveTempRoot(root, {aPath, bPath});
}

TEST_F(FileSelectorTest, TestNameSortStillPopulatesMtime) {
    std::string path = Dic::FileUtil::SplicePath(testDataDir, "full_db");
    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(path, folders, files, exist, "name", "asc");
    ASSERT_TRUE(exist);
    ASSERT_FALSE(files.empty());
    for (const auto &f : files) {
        EXPECT_GT(f->mtime, 0) << f->name;
    }
}

#ifndef _WIN32
TEST_F(FileSelectorTest, TestSortByMtimeUsesSubsecondPrecision) {
    char tmpl[] = "/tmp/msinsight_nsec_XXXXXX";
    char *created = mkdtemp(tmpl);
    ASSERT_NE(created, nullptr);
    const std::string root = created;
    const std::string older = Dic::FileUtil::SplicePath(root, "z_later_name.json");
    const std::string newer = Dic::FileUtil::SplicePath(root, "a_earlier_name.json");
    WriteFile(older, "old");
    WriteFile(newer, "new");
    struct timespec olderTimes[2] = {{1700000000, 100000000}, {1700000000, 100000000}};
    struct timespec newerTimes[2] = {{1700000000, 900000000}, {1700000000, 900000000}};
    ASSERT_EQ(utimensat(AT_FDCWD, older.c_str(), olderTimes, 0), 0);
    ASSERT_EQ(utimensat(AT_FDCWD, newer.c_str(), newerTimes, 0), 0);

    std::vector<std::unique_ptr<Folder>> folders;
    std::vector<std::unique_ptr<File>> files;
    bool exist = true;
    FileSelector::GetFoldersAndFiles(root, folders, files, exist, "mtime", "desc");
    ASSERT_TRUE(exist);
    ASSERT_EQ(files.size(), 2U);
    EXPECT_EQ(files[0]->name, "a_earlier_name.json");
    EXPECT_EQ(files[1]->name, "z_later_name.json");
    EXPECT_EQ(files[0]->mtime - files[1]->mtime, 800);
    RemoveTempRoot(root, {older, newer});
}
#endif
