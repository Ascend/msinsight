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

#include <unordered_map>
#include <cstring>
#include <libgen.h>
#include "RegexUtil.h"
#include "FileUtil.h"

namespace Dic {
namespace {
// 非Windows平台不允许反斜杠
#ifdef _WIN32
// Windows long path support: paths up to 4096 characters with \\?\ prefix
constexpr size_t WINDOWS_LONG_PATH_MAX = 4096;

const std::unordered_map<std::string, std::string> INVALID_CHAR = {{"\n", "\\n"}, {"\f", "\\f"}, {"\r", "\\r"},
    {"\b", "\\b"}, {"\t", "\\t"}, {"\v", "\\v"}, {"\x7F", "\\x7F"}, {"\u007F", "\\u007F"}, {"\"", "\\\""},
    {"\'", "\\\'"}, {"%", "\\%"}, {">", "\\>"}, {"<", "\\<"}, {"|", "\\|"}, {"&", "\\&"}, {"$", "\\$"}, {";", "\\;"},
    {"`", "\\`"}};
#else
const std::unordered_map<std::string, std::string> INVALID_CHAR = {{"\n", "\\n"}, {"\f", "\\f"}, {"\r", "\\r"},
    {"\b", "\\b"}, {"\t", "\\t"}, {"\v", "\\v"}, {"\x7F", "\\x7F"}, {"\u007F", "\\u007F"}, {"\"", "\\\""},
    {"\'", "\\\'"}, {"%", "\\%"}, {">", "\\>"}, {"<", "\\<"}, {"|", "\\|"}, {"&", "\\&"}, {"$", "\\$"}, {";", "\\;"},
    {"`", "\\`"}, {"\\", "\\\\"}};
#endif
#ifdef _WIN32
const std::string_view MSVP_SLASH = "";
#else
const std::string_view MSVP_SLASH = "/";
#endif
}

bool FileUtil::strictMode = true;

#ifdef _WIN32
std::string FileUtil::ConvertToLongPath(const std::string &path) {
    if (path.empty()) {
        return path;
    }

    // Check if path already has the \\?\ prefix
    if (path.size() >= 4 && path.substr(0, 4) == "\\\\?\\") {
        return path;
    }

    // Check if path is a UNC path (starts with \\)
    if (path.size() >= 2 && path.substr(0, 2) == "\\\\") {
        // Convert UNC path from \\server\share to \\?\UNC\server\share
        return "\\\\?\\UNC\\" + path.substr(2);
    }

    // Convert regular absolute path to long path format
    // Check if it's an absolute path (starts with drive letter like C:)
    if (path.size() >= 2 && isalpha(path[0]) && path[1] == ':') {
        return "\\\\?\\" + path;
    }

    return path;
}

std::wstring FileUtil::ConvertToLongPathW(const std::string &path) {
    std::string longPath = ConvertToLongPath(path);
    return StringUtil::String2WString(longPath);
}
#endif

std::string FileUtil::GetCurrPath() {
    std::string strCurPath;
#ifdef _WIN32
    // Use wide character API for long path support
    std::vector<wchar_t> wCurPath(WINDOWS_LONG_PATH_MAX);
    DWORD len = ::GetModuleFileNameW(nullptr, wCurPath.data(), static_cast<DWORD>(wCurPath.size()));
    if (len > 0 && len < wCurPath.size()) {
        wCurPath[len] = L'\0';
        // Find the last backslash and null terminate after it
        wchar_t *lastSlash = wcsrchr(wCurPath.data(), L'\\');
        if (lastSlash != nullptr) {
            lastSlash[1] = L'\0';
        }
        strCurPath = StringUtil::WString2String(wCurPath.data());
    }
#else
#ifdef __APPLE__
    char curPath[1024];
    uint32_t size = sizeof(curPath);
    if (_NSGetExecutablePath(curPath, &size) == 0) {
        strCurPath = std::string(dirname(curPath));
    }
#else
    char curPath[1024];
    ssize_t len = readlink("/proc/self/exe", curPath, sizeof(curPath) - 1);
    if (len != -1) {
        curPath[len] = '\0';
        strCurPath = std::string(dirname(curPath));
    }
#endif
#endif
    return strCurPath;
}

bool FileUtil::IsAbsolutePath(const std::string &path) {
#ifdef _WIN32
    if (path.length() < 2) { // 2表示两个字符 d: 小于两个就一定不是绝对路径
        return false;
    }
    // 判断是否以盘符开头
    if (isalpha(path[0]) && path[1] == ':') {
        return true;
    }
#else
    if (!path.empty() && path[0] == '/') {
        return true;
    }
#endif
    return false;
}

// 这里需要修改，windows和linux下不一样，另外日志也不太能帮助定义，
std::string FileUtil::GetAbsPath(const std::string &path) {
    if (path.empty()) {
        return "";
    }
    if (IsAbsolutePath(path)) {
        return std::string(path);
    }
    std::string curPath = GetCurrPath();
    if (curPath.empty()) {
        Server::ServerLog::Error("Failed to retrieve the current path.");
        return "";
    }
    return curPath + std::string(MSVP_SLASH) + path;
}

std::vector<std::string> FileUtil::SplitFilePath(std::string &path) {
    if (path.empty()) {
        return {};
    }
#ifdef _WIN32
    return StringUtil::Split(path, "\\\\");
#else
    return StringUtil::Split(path, "/");
#endif
}

bool FileUtil::IsSoftLink(const std::string &path) {
#ifdef _WIN32
    std::wstring widePath = ConvertToLongPathW(path);
    DWORD attributes = GetFileAttributesW(widePath.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat fileStat;
    if (lstat(path.c_str(), &fileStat) != 0) {
        Server::ServerLog::Error("The file lstat failed.");
        return false;
    }
    return S_ISLNK(fileStat.st_mode);
#endif
}

bool FileUtil::IsRegularFile(const std::string &filePath) {
#ifdef _WIN32
    std::wstring widePath = ConvertToLongPathW(filePath);
    DWORD fileAttributes = GetFileAttributesW(widePath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0) {
        return false;
    }
    return S_ISREG(fileStat.st_mode);
#endif
}

CheckResult FileUtil::CheckPathSecurity(const std::string &path, int mode) {
    auto Enable = [mode](PathCheckSense sense) { return (mode & sense) == sense; };
    CheckResult result;
    if (!CheckPathComm(path, result)) {
        Server::ServerLog::Error("The file path is insecure, error msg=", result.errMsg);
        return result;
    }
    if (Enable(PathCheckSense::CHECK_FILE_READ) || Enable(PathCheckSense::CHECK_FILE_WRITE)) {
        if (!IsRegularFile(path)) {
            result.Set(false, "The path is not a regular file.");
            return result;
        }
        if (!CheckFileSize(path, true, NORMAL_MAX_FILE_SIZE)) {
            result.Set(false, "File size exceed limits(20Gb)");
            return result;
        }
    }
    if (Enable(PathCheckSense::CHECK_DIR_WRITE) || Enable(PathCheckSense::CHECK_FILE_WRITE)) {
        if (!CheckPathPermission(path, fs::perms::owner_write)) {
            result.Set(false, "The path not have write permission.");
            return result;
        }
    }
    return result;
}

bool FileUtil::IsFilePathExist(const std::string &filePath) {
#ifdef _WIN32
    std::wstring widePath = ConvertToLongPathW(filePath);
    DWORD fileAttributes = GetFileAttributesW(widePath.c_str());
    return fileAttributes != INVALID_FILE_ATTRIBUTES;
#else
    struct stat fileStat;
    return stat(filePath.c_str(), &fileStat) == 0;
#endif
}

bool FileUtil::CheckPathComm(const std::string &path, CheckResult &result) {
    if (path.empty()) {
        result.Set(false, "The path is empty");
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_PATH_IS_EMPTY);
        return false;
    }
    std::string dir = GetAbsPath(path);
    if (dir.empty()) {
        result.Set(false, "Failed to retrieve the absolute path.");
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_PATH_IS_EMPTY);
        return false;
    }
    if (!CheckFilePathLength(dir)) {
        result.Set(false, "The length of path exceeds the limit.");
        return false;
    }
    if (CheckPathInvalidChar(dir)) {
        result.Set(false, "The path contains invalid characters.");
        return false;
    }
    if (!CheckDirAccess(dir, 0)) {
        result.Set(false, "The path not exists");
        return false;
    }
    if (IsSoftLink(dir)) {
        result.Set(false, "The path is soft link.");
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_PATH_IS_SOFT_LINK);
        return false;
    }
    if (!CheckDirAccess(dir, R_OK)) {
        result.Set(false, "The path has no read access.");
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_NOT_READ_ACCESS);
        return false;
    }
    if (!CheckPathOwner(dir)) {
        result.Set(false, "The path's owner is not current user.");
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::PATH_OWNER_ERROR);
        return false;
    }
    if (!CheckWritableByOther(dir)) {
        result.Set(false, "The path could be written by other user.");
        return false;
    }

    return true;
}

void FileUtil::SetStrictMode(bool strictModeValue) { strictMode = strictModeValue; }

bool FileUtil::CheckFilePathExist(const std::string &filePath) {
    if (access(filePath.c_str(), R_OK) == -1) {
        Server::ServerLog::Error("Check file path exist cannot read file path");
        return false;
    }
    return true;
}

bool FileUtil::CheckFilePathLength(const std::string &filePath) {
#ifdef _WIN32
    if (filePath.size() >= WINDOWS_LONG_PATH_MAX) {
        Server::ServerLog::Error("The path length of % exceeds the maximum allowed length of % characters."
                                 "The file size is % MB",
            filePath, WINDOWS_LONG_PATH_MAX, filePath.size());
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::SUB_FILE_PATH_LENGTH_EXCEEDS);
        return false;
    }
#else
    if (filePath.size() >= PATH_MAX) {
        Server::ServerLog::Error("The path length of % exceeds the maximum allowed length of % characters."
                                 "The file size is % MB",
            filePath, PATH_MAX, filePath.size());
        Dic::Common::SetCommonError(Dic::Common::ErrorCode::SUB_FILE_PATH_LENGTH_EXCEEDS);
        return false;
    }
#endif
    return true;
}

uint32_t FileUtil::GetFilePathLengthLimit() {
#ifdef _WIN32
    return WINDOWS_LONG_PATH_MAX;
#else
    return PATH_MAX;
#endif
}

void FileUtil::CalculateDirSize(const std::string &path, long long int &size, int depth) {
    std::vector<std::string> matchedFiles;
    if (!FileUtil::IsFolder(path)) {
        return;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return;
    }
    std::string error;
    if (!IsWithinRecursionLimit(files, depth, error)) {
        Server::ServerLog::Error("Directory size calculation aborted. Reason: " + error);
        return;
    }
    for (std::string &folder : folders) {
        std::string spliceFile = SplicePath(path, folder);
        CalculateDirSize(spliceFile, size, depth + 1);
    }
    std::string gbkPath(path);
    for (std::string &file : files) {
        std::string spliceFile = SplicePath(gbkPath, file);
        if (std::strcmp(DATABASE_FILE_NAME.c_str(), file.c_str()) != 0 &&
            std::strcmp("cluster.db", file.c_str()) != 0) {
            size += GetFileSize(spliceFile.c_str());
        }
    }
}

std::string FileUtil::GetProfilerFileId(const std::string &filePath) {
    std::string fileId = FileUtil::GetFileName(filePath);
    std::string grandparentPath = FileUtil::GetParentPath(FileUtil::GetParentPath(filePath));
    if (fileId.empty() || grandparentPath.empty()) {
        return fileId;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(grandparentPath, folders, files)) {
        return fileId;
    }
    if (std::find(folders.begin(), folders.end(), ASCEND_PROFILER_OUTPUT) != folders.end()) {
        // 如果是ASCEND_PROFILER_OUTPUT，则优先匹配profiler_info_x.json，否则取_ascend_pt
        for (const auto &file : files) {
            if (std::regex_match(file, std::regex(PROFILER_INFO_FILE_REG))) {
                return file.substr(PROFILER_INFO_FILE_PREFIX.length(),
                    file.length() - PROFILER_INFO_FILE_PREFIX.length() - JSON_FILE_SUFFIX.length());
            }
        }
        fileId = FileUtil::GetFileName(grandparentPath);
    } else if (std::find(folders.begin(), folders.end(), MINDSTUDIO_PROFILER_OUTPUT) != folders.end()) {
        // 如果是mindstudio_profiler_output目录，则取Prof_xxx
        fileId = FileUtil::GetFileName(grandparentPath);
    } else if (std::regex_match(FileUtil::GetFileName(grandparentPath), std::regex(DEVICE_DIR_REG))) {
        // 如果是device_x目录，则fileId为x
        fileId = FileUtil::GetFileName(grandparentPath).substr(DEVICE_DIR_PREFIX.length());
    }

    return fileId;
}

std::string FileUtil::GetRootPath(const std::string &filePath) {
    std::string suffix;
#ifdef _WIN32
    size_t pos = filePath.find_first_of("\\");
    size_t pos2 = filePath.find_first_of("\\/");
    if (pos > pos2) {
        pos = pos2;
    }
    suffix = "\\";
#else
    size_t pos = filePath.find_first_of("\\/");
    suffix = "/";
#endif
    if (pos != std::string::npos) {
        return filePath.substr(0, pos) + suffix;
    }
    return "";
}

std::shared_ptr<std::string> FileUtil::GetRelativePath(
    const std::string &targetFilePath, const std::string &sourceFilePath) {
    // 注意：返回结果可能为空指针，需要进行判断
    std::shared_ptr<std::string> result = nullptr;
    // 对文件目录进行切割，获取两个路径列表
    std::vector<std::string> targetPath = StringUtil::Split(targetFilePath, "[\\\\/]");
    std::vector<std::string> sourcePath = StringUtil::Split(sourceFilePath, "[\\\\/]");
    // 去除列表中空字符串
    targetPath.erase(
        std::remove_if(targetPath.begin(), targetPath.end(), [](const std::string &str) { return str.empty(); }),
        targetPath.end());
    sourcePath.erase(
        std::remove_if(sourcePath.begin(), sourcePath.end(), [](const std::string &str) { return str.empty(); }),
        sourcePath.end());

    // 如果目标路径层级比源路径小，说明目标路径不包含源路径，返回空指针
    if (targetPath.size() < sourcePath.size()) {
        return result;
    }

    // 遍历每层源路径，判断是否和目标路径相等
    size_t i = 0;
    for (; i < sourcePath.size(); ++i) {
        if (targetPath[i] != sourcePath[i]) {
            break;
        }
    }

    if (i == sourcePath.size()) {
        targetPath.erase(targetPath.begin(), targetPath.begin() + i);
        result = std::make_shared<std::string>(std::accumulate(targetPath.begin(), targetPath.end(), std::string(),
            [&](const std::string &a, const std::string &b) -> std::string { return a.empty() ? b : a + "/" + b; }));
    }
    return result;
}

bool FileUtil::ModifyFilePermissions(const std::string &filePath, const mode_t &mode) {
    if (!FileUtil::IsFilePathExist(filePath)) {
        // 文件不存在，返回错误代码或抛出异常
        return false;
    }
    return chmod(filePath.c_str(), mode) == 0;
}

bool FileUtil::ConvertToRealPath(std::string &errorMsg, std::vector<std::string> &path) {
    for (auto it = path.begin(); it != path.end(); ++it) {
        if (!ConvertToRealPath(errorMsg, *it)) {
            return false;
        }
    }
    return true;
}

bool FileUtil::ConvertToRealPath(std::string &errorMsg, std::string &path) {
    std::string realPath = GetRealPath(path);
    if (realPath.empty()) {
        errorMsg = "The conversion of the " + path + "path to an absolute path has failed.";
        return false;
    }
    path = realPath;
    return true;
}

void FileUtil::RecursionFindFilesByRegex(
    std::vector<std::string> &result, const std::string &path, int depth, const std::regex &fileRegex) {
    std::string error;
    // 递归限制判断
    if (!IsWithinRecursionLimit(result, depth, error)) {
        return;
    }
    // 查找当前目录下所有文件和文件夹
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return;
    }
    // 遍历文件夹，进一步递归
    for (const auto &folder : folders) {
        std::string tmpPath = FileUtil::SplicePath(path, folder);
        RecursionFindFilesByRegex(result, tmpPath, depth + 1, fileRegex);
    }
    // 遍历文件，对每个文件进行判断是否满足正则表达式
    for (const auto &file : files) {
        std::string tmpPath = SplicePath(path, file);
        if (!std::regex_match(file, fileRegex)) {
            continue;
        }
        result.push_back(tmpPath);
    }
}
std::vector<std::string> FileUtil::FindAllFilesByRegex(const std::string &path, const std::regex &fileRegex) {
    std::vector<std::string> matchedFiles = {};
    if (!FileUtil::IsFolder(path)) {
        if (std::regex_match(FileUtil::GetFileName(path), fileRegex)) {
            matchedFiles.emplace_back(path);
        }
        return matchedFiles;
    }
    RecursionFindFilesByRegex(matchedFiles, path, 0, fileRegex);
    return matchedFiles;
}

bool FileUtil::FindIfDbTypeByRegex(const std::string &path, const std::regex &jsonRegex, const std::regex &dbRegex) {
    int dbFound = 2;
    return FileUtil::FindDbOrJsonType(path, 0, jsonRegex, dbRegex) == dbFound;
}

int FileUtil::FindDbOrJsonType(
    const std::string &path, int depth, const std::regex &jsonRegex, const std::regex &dbRegex) {
    int jsonFound = 1;
    int dbFound = 2;
    int maxDepth = 5;
    if (depth > maxDepth) {
        return jsonFound;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return jsonFound;
    }
    // db优先:如果text和db数据共存，则优先判断为db
    bool hasJson = false;
    for (const auto &file : files) {
        if (std::regex_match(file, dbRegex)) {
            return dbFound;
        }
        hasJson = hasJson || std::regex_match(file, jsonRegex);
    }
    // Ascend 目录优先
    bool ascendFolderExist = std::any_of(folders.begin(), folders.end(),
        [](const std::string &folder) { return folder.find(ASCEND_PROFILER_OUTPUT) != std::string::npos; });
    if (ascendFolderExist) {
        folders.erase(
            std::remove_if(folders.begin(), folders.end(),
                [](const std::string &folder) { return folder.find(ASCEND_PROFILER_OUTPUT) == std::string::npos; }),
            folders.end());
    }
    for (const auto &folder : folders) {
        std::string tmpPath = FileUtil::SplicePath(path, folder);
        int result = FindDbOrJsonType(tmpPath, depth + 1, jsonRegex, dbRegex);
        if (result == dbFound) {
            return dbFound;
        }
        if (result == jsonFound) {
            hasJson = true;
        }
    }
    return hasJson ? jsonFound : 0;
}

// 递归查找函数
std::vector<std::string> FileUtil::FindFirstByRegex(const std::string &path, int depth, const std::regex &fileRegex) {
    std::vector<std::string> matchedFiles;
    int maxDepth = 5;
    if (depth > maxDepth) {
        return matchedFiles;
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    if (!FileUtil::FindFolders(path, folders, files)) {
        return matchedFiles;
    }
    sort(files.begin(), files.end(), std::greater<std::string>());
    for (const auto &file : files) {
        if (std::regex_match(file, fileRegex)) {
            auto aimFile = FileUtil::SplicePath(path, file);
            matchedFiles.push_back(aimFile);
            return matchedFiles;
        }
    }
    for (const auto &folder : folders) {
        std::string tmpPath = FileUtil::SplicePath(path, folder);
        auto foundFiles = FindFirstByRegex(tmpPath, depth + 1, fileRegex);
        if (!foundFiles.empty()) {
            matchedFiles.insert(matchedFiles.end(), foundFiles.begin(), foundFiles.end());
            return matchedFiles;
        }
    }
    return matchedFiles;
}

bool FileUtil::CheckFileSize(const std::string &filePath, bool emptyAllow, size_t fileMaxSize) {
    constexpr size_t fileMinSize = 0;
#ifdef _WIN32
    std::wstring wFilePath = ConvertToLongPathW(filePath);
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (GetFileAttributesExW(wFilePath.c_str(), GetFileExInfoStandard, &fileData)) {
        // 获取文件大小
        uintmax_t fileSize = (static_cast<uintmax_t>(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow;
        if (fileSize <= fileMinSize && !emptyAllow) {
            Server::ServerLog::Error("This file % is an empty file.", filePath);
            return false;
        }
        if (fileSize > fileMaxSize) {
            Server::ServerLog::Error("The size limit for the file located at % has been exceeded.", filePath);
            return false;
        }
        return true;
    }
#else
    // 获取文件大小
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        if (fileStat.st_size < 0) {
            Server::ServerLog::Error("Size of this file is smaller than 0.");
            return false;
        }
        size_t fileSize = static_cast<size_t>(fileStat.st_size);
        if (fileSize == fileMinSize && !emptyAllow) {
            Server::ServerLog::Error("This file % is an empty file.", filePath);
            return false;
        }
        if (fileSize > fileMaxSize) {
            Server::ServerLog::Error("The size limit for the file located at % has been exceeded.", filePath);
            return false;
        }
        return true;
    }
#endif
    return false;
}

std::string FileUtil::GetDbPath(const std::string &filePath, const std::string &fileId) {
    std::string dbPath = GetDbPath(filePath);
    std::string tmpFileId = GetProfilerFileId(filePath);
    if (tmpFileId.length() < fileId.length() && fileId.find(tmpFileId) == 0) {
        // 修改db文件名为mindstudio_insight_data_xxx.db
        dbPath = dbPath.substr(0, dbPath.length() - DB_FILE_SUFFIX.length()) + fileId.substr(tmpFileId.length()) +
            DB_FILE_SUFFIX;
    }
    return dbPath;
}

long long FileUtil::GetFileSize(const char *fileName) {
    if (!fileName) {
        return 0;
    }

    if (strcmp(fileName, "") == 0) {
        return 0;
    }
#ifdef _WIN32
    std::wstring wFilePath = ConvertToLongPathW(std::string(fileName));
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    if (GetFileAttributesExW(wFilePath.c_str(), GetFileExInfoStandard, &fileData)) {
        // 获取文件大小
        uintmax_t fileSize = (static_cast<uintmax_t>(fileData.nFileSizeHigh) << 32) | fileData.nFileSizeLow;
        return fileSize;
    }
    return 0;
#else
    struct stat st;
    long long size = 0;
    if (stat(fileName, &st) == 0) {
        size = st.st_size;
    }
    return size;
#endif
}

std::string FileUtil::GetSingleFileIdWithDb(const std::string &filePath) {
    // 导入单个json csv bin文件时db文件名添加上对应文件除拓展名外的部分，使得单目录导入两个json或者csv或者bin后db文件不会相互覆盖
    std::string fileNameWithoutEx = FileUtil::StemFile(filePath);
    std::string dbFileName = StringUtil::StrJoin(fileNameWithoutEx, "_mindstudio_insight_data.db");
    std::string dir = FileUtil::GetParentPath(filePath);
    return FileUtil::SplicePath(dir, dbFileName);
}

std::string FileUtil::GetDbPath(const std::string &filePath) {
    if (StringUtil::EndWith(filePath, ".bin")) {
        return FileUtil::GetSingleFileIdWithDb(filePath);
    }
    std::string grandparentPath = FileUtil::GetParentPath(FileUtil::GetParentPath(filePath));
    if (grandparentPath.empty()) {
        return FileUtil::SplicePath(FileUtil::GetParentPath(filePath), DATABASE_FILE_NAME);
    }
    std::vector<std::string> folders = FileUtil::GetSubDirs(filePath);
    if (std::find(folders.begin(), folders.end(), ASCEND_PROFILER_OUTPUT) != folders.end()) {
        // 如果是ASCEND_PROFILER_OUTPUT，则放在ASCEND_PROFILER_OUTPUT下
        auto directory = FileUtil::SplicePath(grandparentPath, ASCEND_PROFILER_OUTPUT);
        return FileUtil::SplicePath(directory, DATABASE_FILE_NAME);
    } else if (std::find(folders.begin(), folders.end(), MINDSTUDIO_PROFILER_OUTPUT) != folders.end()) {
        // 如果是mindstudio_profiler_output目录，则放在mindstudio_profiler_output
        auto directory = FileUtil::SplicePath(grandparentPath, MINDSTUDIO_PROFILER_OUTPUT);
        return FileUtil::SplicePath(directory, DATABASE_FILE_NAME);
    } else if (std::regex_match(FileUtil::GetFileName(grandparentPath), std::regex(DEVICE_DIR_REG))) {
        // 如果是device_x目录，则放在device_x的上层目录
        return FileUtil::SplicePath(grandparentPath, DATABASE_FILE_NAME);
    }

    return FileUtil::SplicePath(FileUtil::GetParentPath(filePath), DATABASE_FILE_NAME);
}
bool FileUtil::CheckPathInvalidChar(const std::string &filePath) {
    for (auto &item : INVALID_CHAR) {
        if (filePath.find(item.first) != std::string::npos) {
            Server::ServerLog::Error("The path: % contains invalid character: %.", filePath, item.second);
            Dic::Common::SetCommonError(Dic::Common::ErrorCode::FILE_PATH_CONTAINS_INVALID_CHAR);
            return true;
        }
    }
    return false;
}

bool FileUtil::CheckWritableByOther(const std::string &filePath) {
#ifdef _WIN32
    return true;
#else
    // 后端启动时如果传入了--notStrict选项，导入文件时不要求权限和属主校验通过，仅在日志中提示
    struct stat fileStat {};
    if (stat(filePath.c_str(), &fileStat) != 0) {
        Server::ServerLog::Warn("Get file info failed when check owner");
        if (strictMode) {
            return false;
        }
        return true;
    }
    if (geteuid() == 0) {
        return true;
    }
    if ((fileStat.st_mode & S_IWOTH) != 0) {
        Server::ServerLog::Warn("The path could be written by other user.");
        if (strictMode) {
            return false;
        }
    }
    return true;
#endif
}

bool FileUtil::CheckWritableByOtherOrGroup(const std::string &filePath) {
#ifdef _WIN32
    return true;
#else
    struct stat fileStat {};
    if (stat(filePath.c_str(), &fileStat) != 0) {
        Server::ServerLog::Warn("Get file info failed when check owner");
        return false;
    }
    if (geteuid() == 0) {
        return true;
    }
    if (((fileStat.st_mode & S_IWOTH) != 0) || ((fileStat.st_mode & S_IWGRP) != 0)) {
        return false;
    }
    return true;
#endif
}

bool FileUtil::CheckPathOwner(const std::string &filePath) {
#ifdef _WIN32
    return true;
#else
    // 后端启动时如果传入了--notStrict选项，导入文件时不要求权限和属主校验通过，仅在日志中提示
    struct stat fileStat {};
    if (stat(filePath.c_str(), &fileStat) != 0) {
        Server::ServerLog::Warn("Get file info failed when check owner");
        if (strictMode) {
            return false;
        }
        return true;
    }
    uid_t currentUser = geteuid();
    // root用户属主校验默认通过
    if (currentUser == 0) {
        return true;
    }
    uid_t fileOwner = fileStat.st_uid;
    bool result = (fileOwner == currentUser);
    if (!result) {
        Server::ServerLog::Warn("The path's owner is not current user.");
        if (strictMode) {
            return false;
        }
    }
    return true;
#endif
}

bool FileUtil::CheckPathPermission(const std::string &filePath, fs::perms permission) {
#ifdef _WIN32
    return true;
#endif
    if (filePath.empty()) {
        return false;
    }
    if (!fs::exists(filePath)) {
        return false;
    }
    try {
        auto perms = fs::status(filePath).permissions();
        if ((perms & permission) != fs::perms::none) {
            return true;
        } else {
            Server::ServerLog::Warn("File % doesn't contain the permission %", filePath, static_cast<int>(permission));
            return false;
        }
    } catch (const fs::filesystem_error &e) {
        Server::ServerLog::Error("Get file permission failed, error:", e.what());
        return false;
    }
}

std::vector<std::string> FileUtil::GetSubDirs(const std::string &filePath) {
    if (!FileUtil::IsFolder(filePath)) {
        return {};
    }
    std::vector<std::string> folders;
    std::vector<std::string> files;
    FileUtil::FindFolders(filePath, folders, files);
    return folders;
}
bool FileUtil::IsSubDir(const std::string &parent, const std::string &children) {
    std::vector<std::string> parentPath = StringUtil::Split(parent, "[\\\\/]");
    std::vector<std::string> childrenPath = StringUtil::Split(children, "[\\\\/]");
    parentPath.erase(
        std::remove_if(parentPath.begin(), parentPath.end(), [](const std::string &str) { return str.empty(); }),
        parentPath.end());
    childrenPath.erase(
        std::remove_if(childrenPath.begin(), childrenPath.end(), [](const std::string &str) { return str.empty(); }),
        childrenPath.end());
    if (childrenPath.size() < parentPath.size()) {
        return false;
    }
    for (size_t i = 0; i < parentPath.size(); i++) {
        if (parentPath[i] != childrenPath[i]) {
            return false;
        }
    }
    return true;
}

static std::regex msprofRegex(R"(msprof_[0-9]{1,16}\.db$)");
std::vector<std::string> FileUtil::FindFilesWithFilter(const std::string &path, const std::regex &fileRegex) {
    std::vector<std::string> matchedFiles = {};
    if (!FileUtil::IsFolder(path)) {
        if (std::regex_match(FileUtil::GetFileName(path), fileRegex)) {
            matchedFiles.emplace_back(path);
        }
        return matchedFiles;
    }
    std::string error;
    std::function<void(const std::string &, int)> find = [&find, &matchedFiles, &fileRegex, &error](
                                                             const std::string &path, int depth) {
        if (!std::empty(error)) {
            return;
        }
        if (!IsWithinRecursionLimit(matchedFiles, depth, error)) {
            return;
        }
        std::vector<std::string> folders;
        std::vector<std::string> files;
        if (!FileUtil::FindFolders(path, folders, files)) {
            return;
        }
        // msprof db
        bool findMsprofDb = std::any_of(
            files.begin(), files.end(), [](const std::string &file) { return std::regex_match(file, msprofRegex); });
        if (!files.empty() && findMsprofDb) {
            return CollectMatchdFiles(fileRegex, matchedFiles, path, files);
        }

        auto dirs = {
            FileUtil::SplicePath(path, ASCEND_PROFILER_OUTPUT), FileUtil::SplicePath(path, MINDSTUDIO_PROFILER_OUTPUT)};
        for (const auto &dir : dirs) {
            if (FileUtil::IsFolder(dir)) {
                find(dir, depth + 1);
                return;
            }
        }
        for (const auto &folder : folders) {
            std::string tmpPath = FileUtil::SplicePath(path, folder);
            find(tmpPath, depth + 1);
        }
        CollectMatchdFiles(fileRegex, matchedFiles, path, files);
    };
    find(path, 0);
    if (!std::empty(error)) {
        Server::ServerLog::Warn(StringUtil::GetPrintAbleString(path), " warn is: ", error);
    }
    return matchedFiles;
}

void FileUtil::CollectMatchdFiles(const std::regex &fileRegex, std::vector<std::string> &matchedFiles,
    const std::string &path, std::vector<std::string> &files) {
    sort(files.begin(), files.end(), std::greater<std::string>());
    for (const auto &file : files) {
        std::string tmpPath = SplicePath(path, file);
        if (!std::regex_match(file, fileRegex)) {
            continue;
        }
        matchedFiles.push_back(tmpPath);
        if (!RegexUtil::RegexSearch(file, SLICE_STR).has_value()) {
            // 对于分片文件，需要找到所有的带有slice的文件；其他文件，则只找最新的一个
            break;
        }
    }
}

// 这个函数主要复用了FindFilesWithFilter的逻辑，不能直接使用的原因是FindFilesWithFilter对于多个匹配的文件只会找最新的一个
std::vector<std::string> FileUtil::FindNPUMonitorFiles(const std::string &path) {
    std::vector<std::string> matchedFiles;
    std::regex fileRegex(R"(msmonitor_\d+_\d+_(-1|\d+)\.db)");
    if (!FileUtil::IsFolder(path)) {
        if (std::regex_match(FileUtil::GetFileName(path), fileRegex)) {
            matchedFiles.emplace_back(path);
        }
        return matchedFiles;
    }

    std::string error;
    std::function<void(const std::string &, int)> find = [&find, &matchedFiles, &fileRegex, &error](
                                                             const std::string &path, int depth) {
        if (!std::empty(error)) {
            return;
        }
        if (!IsWithinRecursionLimit(matchedFiles, depth, error)) {
            return;
        }
        std::vector<std::string> folders;
        std::vector<std::string> files;
        if (!FileUtil::FindFolders(path, folders, files)) {
            return;
        }

        for (const auto &folder : folders) {
            std::string tmpPath = FileUtil::SplicePath(path, folder);
            find(tmpPath, depth + 1);
        }
        for (const auto &file : files) {
            if (std::regex_match(file, fileRegex)) {
                matchedFiles.emplace_back(SplicePath(path, file));
            }
        }
    };
    find(path, 0);
    if (!std::empty(error)) {
        Server::ServerLog::Warn("There is a warning message when finding npumonitor files, path is: ",
            StringUtil::GetPrintAbleString(path), " warning message is: ", error);
    }
    return matchedFiles;
}
} // Dic
