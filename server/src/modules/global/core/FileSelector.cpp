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
#include "pch.h"
#include "FileSelector.h"

namespace Dic {
namespace Module {
namespace Global {
using namespace Protocol;

bool FileSelector::CompareStrIgnoreCase(const std::string &s1, const std::string &s2) {
    std::string str1(s1);
    std::string str2(s2);
    std::transform(str1.begin(), str1.end(), str1.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(str2.begin(), str2.end(), str2.begin(), [](unsigned char c) { return std::tolower(c); });
    return str1 < str2;
}

void FileSelector::NormalizeSortParams(std::string &sortBy, std::string &order) {
    if (sortBy != kSortByName && sortBy != kSortByMtime) {
        sortBy = kSortByName;
    }
    if (order != kOrderAsc && order != kOrderDesc) {
        order = kOrderAsc;
    }
}

int64_t FileSelector::GetLastWriteTimeMs(const std::string &path) {
    if (path.empty()) {
        return 0;
    }
#ifdef _WIN32
    std::wstring wPath = FileUtil::ConvertToLongPathW(path);
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (GetFileAttributesExW(wPath.c_str(), GetFileExInfoStandard, &fileData) == 0) {
        return 0;
    }
    ULARGE_INTEGER ull;
    ull.LowPart = fileData.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fileData.ftLastWriteTime.dwHighDateTime;
    // FILETIME：自 1601-01-01 起的 100ns 间隔；下方为到 Unix 纪元的偏移
    constexpr uint64_t kEpochDiff = 116444736000000000ULL;
    if (ull.QuadPart < kEpochDiff) {
        return 0;
    }
    return static_cast<int64_t>((ull.QuadPart - kEpochDiff) / 10000ULL);
#else
    struct stat fileStat {};
    if (stat(path.c_str(), &fileStat) != 0) {
        return 0;
    }
#if defined(__APPLE__)
    const int64_t sec = static_cast<int64_t>(fileStat.st_mtimespec.tv_sec);
    const int64_t nsec = static_cast<int64_t>(fileStat.st_mtimespec.tv_nsec);
#else
    const int64_t sec = static_cast<int64_t>(fileStat.st_mtim.tv_sec);
    const int64_t nsec = static_cast<int64_t>(fileStat.st_mtim.tv_nsec);
#endif
    if (sec < 0) {
        return 0;
    }
    return sec * 1000 + nsec / 1000000;
#endif
}

void FileSelector::GetFoldersAndFiles(const std::string &path,
    std::vector<std::unique_ptr<Protocol::Folder>> &childrenFolders,
    std::vector<std::unique_ptr<Protocol::File>> &childrenFiles, bool &exist, const std::string &sortByParam,
    const std::string &orderParam) {
    std::string sortBy = sortByParam;
    std::string order = orderParam;
    NormalizeSortParams(sortBy, order);

    std::vector<std::string> folders;
    std::vector<std::string> files;
    // 处理中文路径
    std::string tempPath = path;
#ifdef _WIN32
    // windows环境下无需处理
#else
    if (tempPath.length() > 0 && tempPath[tempPath.length() - 1] == '/') {
        tempPath = tempPath.substr(0, tempPath.length() - 1);
    }
#endif
    // 防止目录遍历攻击
    std::string filepath = FileUtil::GetRealPath(tempPath);
    if (path.empty()) {
        folders = FileUtil::GetDiskInfo();
        exist = false;
    } else if (tempPath != filepath) {
        exist = false;
        return;
    } else if (tempPath == filepath) {
        FileUtil::FindFolders(path, folders, files, false);
    }

    childrenFolders.clear();
    childrenFiles.clear();

    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = FileUtil::SplicePath(path, folder);
        // 目录树要显示修改时间，因此无论是否按 mtime 排序都读取；磁盘根无稳定 mtime，置 0
        folderPtr->mtime = path.empty() ? 0 : GetLastWriteTimeMs(folderPtr->path);
        childrenFolders.emplace_back(std::move(folderPtr));
    }
    for (const auto &file : files) {
        std::string tmpPath = FileUtil::SplicePath(path, file);
        auto filePtr = std::make_unique<Protocol::File>(file, tmpPath);
        filePtr->mtime = GetLastWriteTimeMs(tmpPath);
        childrenFiles.emplace_back(std::move(filePtr));
    }

    const bool sortByMtime = (sortBy == kSortByMtime) && !path.empty();
    const bool orderDesc = (order == kOrderDesc);

    auto folderLess = [&](const std::unique_ptr<Folder> &left, const std::unique_ptr<Folder> &right) {
        if (sortByMtime) {
            if (left->mtime != right->mtime) {
                return orderDesc ? left->mtime > right->mtime : left->mtime < right->mtime;
            }
            return CompareStrIgnoreCase(left->name, right->name);
        }
        return orderDesc ? CompareStrIgnoreCase(right->name, left->name)
                         : CompareStrIgnoreCase(left->name, right->name);
    };
    auto fileLess = [&](const std::unique_ptr<File> &left, const std::unique_ptr<File> &right) {
        if (sortByMtime) {
            if (left->mtime != right->mtime) {
                return orderDesc ? left->mtime > right->mtime : left->mtime < right->mtime;
            }
            return CompareStrIgnoreCase(left->name, right->name);
        }
        return orderDesc ? CompareStrIgnoreCase(right->name, left->name)
                         : CompareStrIgnoreCase(left->name, right->name);
    };

    if (!childrenFolders.empty()) {
        std::stable_sort(childrenFolders.begin(), childrenFolders.end(), folderLess);
    }
    if (!childrenFiles.empty()) {
        std::stable_sort(childrenFiles.begin(), childrenFiles.end(), fileLess);
    }
}
} // end of namespace Global
} // end of namespace Module
} // end of namespace Dic
