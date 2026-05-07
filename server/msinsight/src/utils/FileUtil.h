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

#ifndef DATA_INSIGHT_CORE_FILEUTIL_H
#define DATA_INSIGHT_CORE_FILEUTIL_H

#include <string>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <fstream>
#include <numeric>
#include "regex"
#include "ServerLog.h"
#include "FileDef.h"
#include "StringUtil.h"
#include "UtilErrorManager.h"

#if defined(_WIN32)
#include <filesystem>
#include <windows.h>
#include <shlwapi.h>
#include <io.h>
#include <tchar.h>
#include <aclapi.h>
#include <sddl.h>
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <sys/wait.h>
#include "dlfcn.h"
#endif
#ifdef __linux__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#ifdef __APPLE__
#include <filesystem>
#include <mach-o/dyld.h>
namespace fs = std::filesystem;
#endif
#ifdef _WIN32
#define FILE_SEPARATOR '\\'
#else
#define FILE_SEPARATOR '/'
#endif

namespace Dic {
const std::string DB_REG =
        R"(((msprof_[0-9]{1,16}|((ascend_pytorch_profiler)(_[0-9]{1,16}){0,1})|)"
        R"(((ascend_mindspore_profiler)(_[0-9]{1,16}){0,1})|cluster_analysis|leaks_dump_\d+)\.db$))";
enum PathCheckSense : int { // 根据使用场景去定义检查项，既灵活也为代码检视提供方便
    CHECK_DIR_READ = 1,
    CHECK_DIR_WRITE = 2,
    CHECK_FILE_READ = 4,
    CHECK_FILE_WRITE = 8,
};

class CheckResult {
public:
    CheckResult() = default;
    CheckResult(bool isSuccess, const std::string &errMsg) : isSuccess(isSuccess), errMsg(errMsg) {}
    explicit operator bool() const
    {
        return isSuccess;
    }
    bool operator!() const
    {
        return !isSuccess;
    }
    void Set(bool result, const std::string &msg = "")
    {
        isSuccess = result;
        errMsg = msg;
    }
    bool isSuccess = true;
    std::string  errMsg;
};

class FileUtil {
public:
    static inline bool CheckDirAccess(const std::string &path, const int &mode = 0)
    {
#ifdef _WIN32
        std::string normalizedPath = path;
        // Handle drive root paths: "D:" should be treated as "D:\"
        if (path.size() == 2 && isalpha(path[0]) && path[1] == ':') {
            normalizedPath = path + "\\";
        }
        std::wstring wPath = ConvertToLongPathW(normalizedPath);
        if (_waccess(wPath.c_str(), mode) == -1) {
            return false;
        }
#else
        if (access(path.c_str(), mode) == -1) {
            return false;
        }
#endif
        return true;
    }

    static inline std::string SplicePath(const std::string &path)
    {
        return path;
    }

    template<typename... Args>
    static inline std::string SplicePath(const std::string &first, Args... args)
    {
        std::string res(first);
#ifdef _WIN32
        if (!first.empty() && first[first.size() - 1] != '/' && first[first.size() - 1] != '\\') {
            return first + "\\" + SplicePath(args...);
        }
#else
        if (!first.empty() && first[first.size() - 1] != '/') {
            return first + "/" + SplicePath(args...);
        }
#endif
        return res + SplicePath(args...);
    }

    // Convert path to long path format for Windows (\\?\ prefix)
    static std::string ConvertToLongPath(const std::string &path);

    // Convert path to wide string long path format for Windows API calls
    static std::wstring ConvertToLongPathW(const std::string &path);

    static inline std::string GetRealPath(const std::string &path)
    {
#ifdef _WIN32
        // Use dynamic allocation for long path support (up to 32767 characters)
        std::wstring wPath = StringUtil::String2WString(path);
        wchar_t* wResolvedPath = _wfullpath(nullptr, wPath.c_str(), 0);
        if (wResolvedPath == nullptr) {
            return "";
        }
        std::string result = StringUtil::WString2String(wResolvedPath);
        free(wResolvedPath);
        return result;
#else
        char resolvedPath[PATH_MAX] = {0};
        if (realpath(path.c_str(), resolvedPath) == nullptr) {
            return "";
        }
        return std::string(resolvedPath);
#endif
    }

    static inline std::string GetFileName(const std::string &path)
    {
        if (path.empty()) {
            return "";
        }
        auto pos = path.find_last_of('/');
#ifdef _WIN32
        if (pos == std::string::npos) {
            pos = path.find_last_of('\\');
        } else if (path.find_last_of('\\') != std::string::npos) {
            pos = std::max(pos, path.find_last_of('\\'));
        }
#endif
        if (pos == std::string::npos) {
            return path;
        }
        return path.substr(pos + 1);
    }

#ifdef _WIN32
    static inline bool FindFolders(const std::string &path,
        std::vector<std::string> &folders, std::vector<std::string> &files, bool strict = true)
    {
        // Use wide character version for long path support
        intptr_t hFile = 0;
        const uint64_t fileCountLimit = 100000;
        struct _wfinddata_t fileInfo {};
        std::string currentPath = PathPreprocess(path);
        std::wstring searchPath = ConvertToLongPathW(path);
        if (searchPath.back() != L'\\' && searchPath.back() != L'/') {
            searchPath += L"\\*";
        } else {
            searchPath += L"*";
        }
        if ((hFile = _wfindfirst(searchPath.c_str(), &fileInfo)) == -1) {
            return false;
        }
        do {
            if ((fileInfo.attrib & (_A_HIDDEN | _A_SYSTEM)) != 0) {
                continue;
            }
            std::string fileName = StringUtil::WString2String(fileInfo.name);
            if (fileName == ".." || fileName == "." || fileName.empty()) {
                continue;
            }

            if (!CheckPathSecurity(SplicePath(currentPath, fileName))) {
                continue;
            }
            if ((fileInfo.attrib & _A_SUBDIR) != 0) {
                folders.emplace_back(fileName);
            } else {
                files.emplace_back(fileName);
            }
            if (folders.size() + files.size() > fileCountLimit) {
                Server::ServerLog::Warn("There are too many sub files in the folder");
                folders.clear();
                files.clear();
                break;
            }
        } while (_wfindnext(hFile, &fileInfo) == 0);
        _findclose(hFile);
        return true;
    }
#else
    static inline bool FindFolders(const std::string &path,
                                   std::vector<std::string> &folders,
                                   std::vector<std::string> &files,
                                   bool strict = true)
    {
        if (path.empty()) {
            Server::ServerLog::Error("path empty");
            Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_PATH_IS_EMPTY);
            return false;
        }
        DIR *pDir = nullptr;
        struct dirent *pDirent = nullptr;
        struct stat pathStat;
        std::string tmpPath(path);
        pDir = opendir(tmpPath.c_str());
        if (pDir == nullptr) {
            Server::ServerLog::Error("open dir failed");
            Dic::Common::SetCommonError(Dic::Common::ErrorCode::OPEN_DIR_FAILED);
            return false;
        }
        const uint64_t fileCountLimit = 100000;
        while ((pDirent = readdir(pDir)) != nullptr) {
            if (std::string(pDirent->d_name) == ".." || std::string(pDirent->d_name) == ".") {
                continue;
            }
            std::string fullPath = tmpPath + "/" + pDirent->d_name;
            if (stat(fullPath.c_str(), &pathStat) != 0) {
                continue;
            }
            if (strict && !CheckPathSecurity(fullPath)) {
                continue;
            }
            if (!strict && S_ISLNK(pathStat.st_mode)) {
                continue;
            }
            if (S_ISDIR(pathStat.st_mode)) {
                folders.emplace_back(pDirent->d_name);
            } else if (!strict || S_ISREG(pathStat.st_mode)) {
                files.emplace_back(pDirent->d_name);
            } else {
                Server::ServerLog::Info("Other type : [", pathStat.st_mode, "] : [", pDirent->d_name);
            }
            if (folders.size() + files.size() > fileCountLimit) {
                Server::ServerLog::Warn("There are too many sub files in the folder");
                folders.clear();
                files.clear();
                break;
            }
        }
        closedir(pDir);
        return true;
    }
#endif

    static inline bool IsFolder(const std::string &path)
    {
#ifdef _WIN32
        std::wstring wPath = ConvertToLongPathW(path);
        DWORD attributes = GetFileAttributesW(wPath.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
        struct stat st;
        if (stat(path.c_str(), &st) == -1) {
            return false;
        }
        return S_ISDIR(st.st_mode);
#endif
    }

    static inline bool RemoveFile(const std::string &path)
    {
        return std::remove(path.c_str()) == 0;
    }

    /**
     * 根据文件路径复制文件。
     * @param sourceFilePath 源文件路径
     * @param targetFilePath 目标文件路径（需保证目标文件父目录存在，无需目标文件本身存在。）
     * @return
     */
    static inline bool CopyFileByPath(const std::string &sourceFilePath, const std::string &targetFilePath)
    {
        // 检查 源文件路径 合法性，包括文件最小权限、软连接、长度、特殊字符
        if (!CheckPathSecurity(sourceFilePath, CHECK_FILE_READ)) {
            Server::ServerLog::Error("Source file path is invalid when copy file.");
            return false;
        }
        // 目标文件目录
        std::string targetDirPath = GetParentPath(targetFilePath);
        // 检查 目标文件目录 合法性、是否可写
        if (!CheckPathSecurity(targetDirPath, CHECK_DIR_WRITE)) {
            Server::ServerLog::Error("Target directory is invalid or not writable when copy file.");
            return false;
        }
#ifdef _WIN32
        std::wstring wSourcePath = ConvertToLongPathW(sourceFilePath);
        std::wstring wTargetPath = ConvertToLongPathW(targetFilePath);
        return CopyFileW(wSourcePath.c_str(), wTargetPath.c_str(), false);
#else
        try {
            return fs::copy_file(sourceFilePath, targetFilePath, fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error& e) {
            Server::ServerLog::Error("Could not copy file, error=" + std::string(e.what()));
            return false;
        }
#endif
    }

    static inline std::vector<std::string> GetDiskInfo()
    {
        std::vector<std::string> disk;
#ifdef _WIN32
        DWORD dwSize = MAX_PATH;
        char szLogicalDrives[MAX_PATH] = {0};
        DWORD dwResult = GetLogicalDriveStrings(dwSize - 1, szLogicalDrives);
        if (dwResult > 0 && dwResult <= MAX_PATH) {
            auto *szSingleDrive = szLogicalDrives;
            while (*szSingleDrive) {
                disk.emplace_back(szSingleDrive);
                szSingleDrive += strlen(szSingleDrive) + 1;
            }
        }
#else
        disk.emplace_back("/");
#endif
        return disk;
    }

    static inline std::map<std::string, std::vector<std::string>>
    SplitToRankList(std::vector<std::pair<std::string, std::string>> fileList)
    {
        std::map<std::string, std::vector<std::string>> rankListMap;
        for (const auto &item: fileList) {
            rankListMap[item.second].push_back(item.first);
        }
        return rankListMap;
    }

    static inline std::string GetRankIdFromFile(std::string timeLineFile)
    {
        std::string parent = GetParentPath(timeLineFile);
        std::string parentSec = GetParentPath(parent);
        std::vector<std::string> folders;
        std::vector<std::string> files;
        if (!FileUtil::FindFolders(parentSec, folders, files)) {
            return timeLineFile;
        }
        // 从profiler_info_文件获取
        for (const auto &file: files) {
            std::regex rankIdFileRegex(PROFILER_INFO_FILE_REG);
            if (std::regex_match(file, rankIdFileRegex)) {
                auto index = file.find_last_of('_');
                auto index2 = file.find_last_of('.');
                if (index == std::string::npos || index2 == std::string::npos) {
                    return file;
                }
                return file.substr(index + 1, index2 - index - 1);
            }
        }
        return timeLineFile;
    }

    static std::string GetRankIdFromPath(const std::string &filePath)
    {
        const int fileIdPosition = 3; // 上上层目录
        std::string path = FileUtil::GetRealPath(filePath);
        if (path.empty()) {
            return "";
        }
        auto pos = path.find_first_of('\\');
        while (pos != std::string::npos) {
            path.replace(pos, 1, "/");
            pos = path.find_first_of('\\');
        }
        auto list = StringUtil::Split(path, "/");
        if (list.size() < fileIdPosition) {
            return "";
        }
        return list.at(list.size() - fileIdPosition);
    }

    static std::string GetProfilerFileId(const std::string &filePath);

    static std::string GetSingleFileIdWithDb(const std::string &filePath);

    static std::string GetDbPath(const std::string &filePath);

    static std::string GetDbPath(const std::string &filePath, const std::string &fileId);

    static inline std::string GetParentPath(const std::string filePath)
    {
        if (filePath.empty()) {
            return "";
        }
#ifdef _WIN32
        size_t pos = filePath.find_last_of("\\");
        size_t pos2 = filePath.find_last_of("\\/");
        if (pos < pos2) {
            pos = pos2;
        }
#else
        size_t pos = filePath.find_last_of("\\/");
#endif
        if (pos != std::string::npos) {
            return filePath.substr(0, pos);
        }
        return "";
    }

    static inline std::string PathPreprocess(const std::string filePath)
    {
        std::string path(filePath);
        return path;
    }

    static long long GetFileSize(const char* fileName);

    static void CalculateDirSize(const std::string &path, long long &size, int depth);

    static void RecursionFindFilesByRegex(std::vector<std::string> &result, const std::string &path, int depth,
                                          const std::regex &fileRegex);
    static std::vector<std::string> FindAllFilesByRegex(const std::string &path, const std::regex &fileRegex);

    // 遍历目录，最大不超过5层，优先遍历ASCEND_PROFILER_OUTPUT/mindstudio_profiler_output目录，其存在则跳过其他文件夹
    static std::vector<std::string> FindFilesWithFilter(const std::string &path, const std::regex &fileRegex);

    static void CollectMatchdFiles(const std::regex &fileRegex, std::vector<std::string> &matchedFiles,
        const std::string &path, std::vector<std::string> &files);

    static inline bool IsWithinRecursionLimit(const std::vector<std::string> &files, int depth, std::string &error)
    {
        const int depthLimit = 8;
        const size_t fileSizeLimit = 100000;
        if (depth > depthLimit) {
            error = "Too many levels of file nesting";
            Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_TOO_DEEP);
            return false;
        }
        if (files.size() > fileSizeLimit) {
            error = "Too many files matching the requirements";
            Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_TOO_MANY);
            return false;
        }
        return true;
    }

    // 寻找目录下的第一份符合条件的文件
    static inline std::vector<std::string> FindFirstFileByRegex(const std::string &path, const std::regex &fileRegex)
    {
        std::vector<std::string> matchedFiles;
        if (!FileUtil::IsFolder(path)) {
            if (std::regex_match(FileUtil::GetFileName(path), fileRegex)) {
                matchedFiles.emplace_back(FileUtil::PathPreprocess(path));
            }
            return matchedFiles;
        }
        return FileUtil::FindFirstByRegex(path, 0, fileRegex);
    }

    inline static std::string StemFile(const std::string& filePath)
    {
        if (filePath.empty()) {
            return "";
        }
        std::string fileName = GetFileName(filePath);
        auto pos = fileName.find_first_of('.');
        return pos != std::string::npos ? fileName.substr(0, pos) : fileName;
    }

    static std::vector<std::string> FindNPUMonitorFiles(const std::string &path);

    /**
    * @brief 获取一个目录下的子目录，如果路径类型不为目录，则返回空
    */
    static std::vector<std::string> GetSubDirs(const std::string &filePath);

    static bool IsSubDir(const std::string& parent, const std::string& children);

    // 切分路径
    static std::vector<std::string> SplitFilePath(std::string &path);
    static bool IsSoftLink(const std::string &path);
    static bool IsFilePathExist(const std::string &filePath);
    static bool IsAbsolutePath(const std::string &path);
    static bool IsRegularFile(const std::string &filePath);
    static CheckResult CheckPathSecurity(const std::string &path, int mode = PathCheckSense::CHECK_DIR_READ);
    static void SetStrictMode(bool strictModeValue);
protected:
    static bool CheckPathComm(const std::string& path, CheckResult& result);
    static bool strictMode;
public:
    static bool CheckFilePathExist(const std::string& filePath);
    static bool CheckFilePathLength(const std::string& filePath);
    static uint32_t GetFilePathLengthLimit();
    static std::string GetAbsPath(const std::string &path);
    static std::string GetCurrPath();
    static std::string GetRootPath(const std::string& filePath);
    static std::shared_ptr<std::string> GetRelativePath(const std::string& targetFilePath,
                                                        const std::string& sourceFilePath);
    static bool ModifyFilePermissions(const std::string &filePath, const mode_t &mode);
    static bool ConvertToRealPath(std::string &errorMsg, std::vector<std::string> &path);
    static bool ConvertToRealPath(std::string &errorMsg, std::string &path);
    // 寻找目录下的第一份符合条件的Db或Json数据，Db: true Json: false，集群数据仅需找到一份文件来判断
    static bool FindIfDbTypeByRegex(const std::string &path, const std::regex &jsonRegex,
                                    const std::regex &dbRegex);
    static int FindDbOrJsonType(const std::string &path, int depth,
                                const std::regex &jsonRegex, const std::regex &dbRegex);
    static std::vector<std::string> FindFirstByRegex(const std::string &path, int depth, const std::regex &fileRegex);

    static bool CheckFileSize(const std::string &filePath, bool emptyAllow = false, size_t fileMaxSize = NORMAL_MAX_FILE_SIZE);

    /**
    * @brief 检查路径中是否包含非法字符
    */
    static bool CheckPathInvalidChar(const std::string &filePath);

    /**
    * @brief 检查文件属主
    */
    static bool CheckPathOwner(const std::string &filePath);

    /**
    * @brief 检查文件权限，后续替换现有实现
    */
    static bool CheckPathPermission(const std::string &filePath, fs::perms permission);

    static bool CheckWritableByOther(const std::string &filePath);

    static bool CheckWritableByOtherOrGroup(const std::string &filePath);
protected:
    constexpr static size_t NORMAL_MAX_FILE_SIZE = 20 *1024 * 1024 * 1024ULL;
    };
} // end of namespace Dic
#endif // DATA_INSIGHT_CORE_FILEUTIL_H
