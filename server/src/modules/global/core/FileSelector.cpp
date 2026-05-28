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

void FileSelector::GetFoldersAndFiles(const std::string &path,
    std::vector<std::unique_ptr<Protocol::Folder>> &childrenFolders,
    std::vector<std::unique_ptr<Protocol::File>> &childrenFiles, bool &exist) {
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
    if (!folders.empty()) {
        std::sort(folders.begin(), folders.end(), CompareStrIgnoreCase);
    }
    for (const auto &folder : folders) {
        auto folderPtr = std::make_unique<Folder>();
        folderPtr->name = folder;
        folderPtr->path = FileUtil::SplicePath(path, folder);
        childrenFolders.emplace_back(std::move(folderPtr));
    }
    if (!files.empty()) {
        std::sort(files.begin(), files.end(), CompareStrIgnoreCase);
    }
    for (const auto &file : files) {
        std::string tmpPath = FileUtil::SplicePath(path, file);
        childrenFiles.emplace_back(std::make_unique<Protocol::File>(file, tmpPath));
    }
}
} // end of namespace Global
} // end of namespace Module
} // end of namespace Dic
