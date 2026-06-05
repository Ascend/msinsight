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
#include "FileUtil.h"
#include "IdBuilder.h"
#include "../TestSuit.h"
#include "JsonUtil.h"

using namespace Dic;

TEST(FileUtilTest, BasicAssertions) {
#ifdef _WIN32
    EXPECT_EQ(FileUtil::SplicePath("a", "b"), "a\\b");
    EXPECT_EQ(FileUtil::SplicePath("a", "b", "c"), "a\\b\\c");
#else
    EXPECT_EQ(FileUtil::SplicePath("a", "b"), "a/b");
#endif
}

#ifdef _WIN32
TEST(FileUtilTest, ConvertToLongPathOnWindows) {
    EXPECT_EQ(FileUtil::ConvertToLongPath("C:\\data\\trace_view.json"), "\\\\?\\C:\\data\\trace_view.json");
    EXPECT_EQ(FileUtil::ConvertToLongPath("\\\\?\\C:\\data\\trace_view.json"), "\\\\?\\C:\\data\\trace_view.json");
    EXPECT_EQ(FileUtil::ConvertToLongPath("\\\\server\\share\\trace_view.json"),
        "\\\\?\\UNC\\server\\share\\trace_view.json");
    EXPECT_EQ(FileUtil::ConvertToLongPath("relative\\trace_view.json"), "relative\\trace_view.json");
    EXPECT_EQ(FileUtil::ConvertToLongPath(""), "");
}
#endif

#ifdef _WIN32
TEST(FileUtilTest, ConvertToLongPathWOnWindows) {
    EXPECT_EQ(FileUtil::ConvertToLongPathW("C:\\data\\trace_view.json"), L"\\\\?\\C:\\data\\trace_view.json");
    EXPECT_EQ(FileUtil::ConvertToLongPathW("\\\\server\\share\\trace_view.json"),
        L"\\\\?\\UNC\\server\\share\\trace_view.json");
}
#endif

TEST(FileUtilTest, TestSplitToRankList) {
    std::vector<std::pair<std::string, std::string>> fileList;
    std::pair<std::string, std::string> pair1;
    pair1.first = "1";
    pair1.second = TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "trace_view.json");
    fileList.push_back(pair1);
    std::pair<std::string, std::string> pair2;
    pair1.first = "0";
    pair1.second = TestSuit::GetTestDataFile("test_rank_0", "ASCEND_PROFILER_OUTPUT", "trace_view.json");
    fileList.push_back(pair2);
    std::map<std::string, std::vector<std::string>> result = FileUtil::SplitToRankList(fileList);
    EXPECT_EQ(result.size(), 2);
}

TEST(FileUtilTest, TestGetRankIdFromFile) {
    std::string rank = FileUtil::GetRankIdFromFile(
        TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "trace_view.json"));
    EXPECT_EQ(rank, "1");
}

TEST(FileUtilTest, TestGetRankIdFromPath) {
    std::string rank = FileUtil::GetRankIdFromPath(
        TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "trace_view.json"));
    auto result = FileUtil::CheckPathSecurity(
        TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "trace_view.json"), CHECK_FILE_READ);
    EXPECT_EQ(rank, "test_rank_1");
    EXPECT_EQ(result.isSuccess, true);
}

TEST(FileUtilTest, TestGetDbPath) {
    std::string traceViewPath = TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "trace_view.json");
    std::string dbPath = FileUtil::GetDbPath(traceViewPath, "1");
    std::string expectedDbPath =
        TestSuit::GetTestDataFile("test_rank_1", "ASCEND_PROFILER_OUTPUT", "mindstudio_insight_data.db");
    EXPECT_EQ(dbPath, expectedDbPath);
}

TEST(FileUtilTest, GetFileId) { EXPECT_EQ(FileUtil::GetSingleFileIdWithDb("test"), "test_mindstudio_insight_data.db"); }

TEST(FileUtilTest, TestGetFileSizeNullFileName) {
    auto res = FileUtil::GetFileSize(nullptr);
    EXPECT_EQ(res, 0);
}

#ifdef _WIN32
TEST(FileUtilTest, GetFileSizeReadsWindowsFileSize) {
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string filePath =
        FileUtil::SplicePath(tempPath, "FileUtilGetFileSize_" + std::to_string(GetCurrentProcessId()) + ".tmp");
    std::ofstream file(filePath);
    file << "abc";
    file.close();

    EXPECT_EQ(FileUtil::GetFileSize(filePath.c_str()), 3);
    EXPECT_EQ(std::remove(filePath.c_str()), 0);
}
#endif

TEST(FileUtilTest, TestIsAbsolutePathEmtpyPath) { EXPECT_EQ(FileUtil::IsAbsolutePath(""), false); }

TEST(FileUtilTest, CheckDirAccessSuccessWhenFileExist) {
    std::ofstream file(".//example.txt");
    file.close();
    EXPECT_EQ(FileUtil::CheckDirAccess(".//example.txt"), true);
    EXPECT_EQ(std::remove(".//example.txt"), 0);
}

TEST(FileUtilTest, CheckDirAccessFailedWhenFileNotExist) { EXPECT_EQ(FileUtil::CheckDirAccess("./test1.text"), false); }

#ifdef _WIN32
TEST(FileUtilTest, CheckDirAccessSuccessWhenPathIsDriveLetterOnWindows) {
    char windowsDir[MAX_PATH] = {0};
    ASSERT_GT(GetWindowsDirectoryA(windowsDir, MAX_PATH), 0U);
    std::string driveLetter(windowsDir, 2);

    EXPECT_TRUE(FileUtil::CheckDirAccess(driveLetter));
    EXPECT_TRUE(FileUtil::CheckDirAccess(driveLetter + "\\"));
}
#endif

TEST(FileUtilTest, CheckFilePathLengthFailedWhenFilePathIsTooLong) {
#ifdef _WIN32
    std::string filePath(FileUtil::GetFilePathLengthLimit(), 'a');
#else
    std::string filePath(PATH_MAX, 'a');
#endif
    EXPECT_EQ(FileUtil::CheckFilePathLength(filePath), false);
}

TEST(FileUtilTest, CheckFilePathLengthSuccess) {
#ifdef _WIN32
    std::string filePath("test11");
#else
    std::string filePath("test111");
#endif
    EXPECT_EQ(FileUtil::CheckFilePathLength(filePath), true);
}

#ifdef _WIN32
TEST(FileUtilTest, GetFilePathLengthLimitUsesWindowsLongPathLimit) {
    EXPECT_GT(FileUtil::GetFilePathLengthLimit(), static_cast<uint32_t>(MAX_PATH));
    std::string filePath(FileUtil::GetFilePathLengthLimit(), 'a');
    EXPECT_FALSE(FileUtil::CheckFilePathLength(filePath));
}
#endif

#ifdef _WIN32
TEST(FileUtilTest, GetDiskInfoReturnsWindowsDrives) {
    auto disks = FileUtil::GetDiskInfo();
    ASSERT_FALSE(disks.empty());
    EXPECT_TRUE(std::any_of(disks.begin(), disks.end(), [](const std::string &disk) {
        return disk.size() >= 3 && disk[1] == ':' && (disk[2] == '\\' || disk[2] == '/');
    }));
}
#endif

TEST(FileUtilTest, CheckFilePathExistSuccessWhenFileExist) {
    std::ofstream file(".//example.txt");
    file.close();
    EXPECT_EQ(FileUtil::CheckFilePathExist(".//example.txt"), true);
    EXPECT_EQ(std::remove(".//example.txt"), 0);
}

TEST(FileUtilTest, CheckFilePathExistFailedWhenFileNotExist) {
    EXPECT_EQ(FileUtil::CheckFilePathExist(".//example_no_exist.txt"), false);
}

#ifdef _WIN32
TEST(FileUtilTest, IsFilePathExistAndIsRegularFileOnWindows) {
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string dirPath = FileUtil::SplicePath(tempPath,
        "FileUtilIsRegularFile_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount()));
    ASSERT_NE(CreateDirectoryA(dirPath.c_str(), nullptr), 0);
    std::string filePath = FileUtil::SplicePath(dirPath, "trace_view.json");
    std::ofstream file(filePath);
    file.close();

    EXPECT_TRUE(FileUtil::IsFilePathExist(filePath));
    EXPECT_TRUE(FileUtil::IsFilePathExist(dirPath));
    EXPECT_TRUE(FileUtil::IsRegularFile(filePath));
    EXPECT_FALSE(FileUtil::IsRegularFile(dirPath));
    EXPECT_FALSE(FileUtil::IsFilePathExist(FileUtil::SplicePath(dirPath, "missing.json")));
    EXPECT_FALSE(FileUtil::IsRegularFile(FileUtil::SplicePath(dirPath, "missing.json")));

    EXPECT_EQ(std::remove(filePath.c_str()), 0);
    EXPECT_NE(RemoveDirectoryA(dirPath.c_str()), 0);
}
#endif

TEST(FileUtilTest, IsAbsolutePathFailedWhenPathIsRelativePath) {
#ifdef _WIN32
    EXPECT_EQ(FileUtil::IsAbsolutePath("\\dbox\\example_no_exist.txt"), false);
    EXPECT_EQ(FileUtil::IsAbsolutePath("a"), false);
#else
    EXPECT_EQ(FileUtil::IsAbsolutePath("./home/test"), false);
#endif
}

TEST(FileUtilTest, IsAbsolutePathCheckSuccessWhenPathIsAbsPath) {
#ifdef _WIN32
    EXPECT_EQ(FileUtil::IsAbsolutePath("D:\\dbox\\example_no_exist.txt"), true);
#else
    EXPECT_EQ(FileUtil::IsAbsolutePath("/root/home/test"), true);
#endif
}

TEST(FileUtilTest, GetAbsPathFailedWhenPathIsEmpty) { EXPECT_EQ(FileUtil::GetAbsPath(""), ""); }

#ifdef _WIN32
TEST(FileUtilTest, GetCurrPathReturnsWindowsDirectoryPath) {
    std::string currPath = FileUtil::GetCurrPath();
    EXPECT_FALSE(currPath.empty());
    EXPECT_TRUE(currPath.back() == '\\' || currPath.back() == '/');
}
#endif

TEST(FileUtilTest, GetAbsPathSuccessWhenPathExist) {
    std::ofstream file(".//example.txt");
    file.close();
    EXPECT_NE(FileUtil::GetAbsPath(".//example.txt"), "");
    EXPECT_EQ(std::remove(".//example.txt"), 0);
}

#ifdef _WIN32
TEST(FileUtilTest, IsFolderChecksWindowsAttributes) {
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string dirPath = FileUtil::SplicePath(
        tempPath, "FileUtilIsFolder_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount()));
    ASSERT_NE(CreateDirectoryA(dirPath.c_str(), nullptr), 0);
    std::string filePath = FileUtil::SplicePath(dirPath, "trace_view.json");
    std::ofstream file(filePath);
    file.close();

    EXPECT_TRUE(FileUtil::IsFolder(dirPath));
    EXPECT_FALSE(FileUtil::IsFolder(filePath));
    EXPECT_FALSE(FileUtil::IsFolder(FileUtil::SplicePath(dirPath, "missing")));

    EXPECT_EQ(std::remove(filePath.c_str()), 0);
    EXPECT_NE(RemoveDirectoryA(dirPath.c_str()), 0);
}
#endif

TEST(FileUtilTest, IsSoftLinkCheckFailedWhenPathExistAndIsNotSoftlink) {
    std::ofstream file(".//example.txt");
    file.close();
    EXPECT_EQ(FileUtil::IsSoftLink(".//example.txt"), false);
    EXPECT_EQ(std::remove(".//example.txt"), 0);
}

TEST(FileUtilTest, IsSoftLinkCheckFailedWhenPathNotExist) {
    EXPECT_EQ(FileUtil::IsSoftLink(".//example_bot_exist.txt"), false);
}

TEST(FileUtilTest, IsSoftLinkCheckSuccessWhenPathIsSoftlink) {
    // 源文件路径
    const char *srcPath = ".//example.txt";
    // 链接文件路径
    const char *linkPath = ".//example_softlink.txt";
    std::ofstream file(srcPath);
    file.close();
#ifdef _WIN32
    // 创建软链接
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    EXPECT_NE(CreateSymbolicLink(linkPath, srcPath, flags), 0);
    EXPECT_EQ(FileUtil::IsSoftLink(linkPath), true);
#else
    // 创建软链接
    EXPECT_EQ(symlink(srcPath, linkPath), 0);
    EXPECT_EQ(FileUtil::IsSoftLink(".//example_softlink.txt"), true);
#endif
    EXPECT_NE(unlink(linkPath), -1);
    EXPECT_EQ(std::remove(srcPath), 0);
}

TEST(FileUtilTest, CheckPathValidFailedWhenPathNotExist) {
    EXPECT_FALSE(FileUtil::CheckPathSecurity(".//example_bot_exist.txt", CHECK_FILE_READ));
}

TEST(FileUtilTest, CheckPathValidFailedWhenPathIsEmpty) { EXPECT_FALSE(FileUtil::CheckPathSecurity("")); }

TEST(FileUtilTest, CheckPathValidFailedWhenFileExistedButPathIsTooLong) {
#ifdef _WIN32
    std::string filePath(FileUtil::GetFilePathLengthLimit(), 'a');
#else
    std::string filePath(PATH_MAX, 'a');
#endif
    EXPECT_FALSE(FileUtil::CheckPathSecurity(filePath));
}

TEST(FileUtilTest, CheckPathValidFailedWhenFileExistInvalidChar) {
    EXPECT_FALSE(FileUtil::CheckPathSecurity("te\\nst.text"));
}

#ifdef _WIN32
TEST(FileUtilTest, CheckPathInvalidCharHandlesWindowsRules) {
    EXPECT_FALSE(FileUtil::CheckPathInvalidChar("C:\\data\\trace_view.json"));
    EXPECT_TRUE(FileUtil::CheckPathInvalidChar("C:\\data\\trace|view.json"));
}
#endif

TEST(FileUtilTest, CheckPathValidFailedWhenFileIsSoftlink) {
    // 源文件路径
    const char *srcPath = ".//example.txt";
    // 链接文件路径
    const char *linkPath = ".//example_softlink.txt";
    std::ofstream file(srcPath);
    file.close();
#ifdef _WIN32
    // 创建软链接
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    EXPECT_NE(CreateSymbolicLink(linkPath, srcPath, flags), 0);
    EXPECT_FALSE(FileUtil::CheckPathSecurity("te\\nst.text"));
#else
    // 创建软链接
    EXPECT_EQ(symlink(srcPath, linkPath), 0);
    EXPECT_FALSE(FileUtil::CheckPathSecurity("te\\nst.text"));
#endif
    EXPECT_NE(unlink(linkPath), -1);
    EXPECT_EQ(std::remove(srcPath), 0);
}

TEST(FileUtilTest, CheckPathValidSuccessWhenFileExist) {
    std::string srcPath;
#ifdef _WIN32
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    srcPath = FileUtil::SplicePath(tempPath, "FileUtilCheckPath_" + std::to_string(GetCurrentProcessId()) + ".txt");
#else
    srcPath = ".//example.txt";
#endif
    // 源文件路径
    std::ofstream file(srcPath);
    ASSERT_TRUE(file.is_open());
    file.close();
    EXPECT_TRUE(FileUtil::CheckPathSecurity(srcPath, CHECK_FILE_READ));
    EXPECT_EQ(std::remove(srcPath.c_str()), 0);
}

#ifdef _WIN32
TEST(FileUtilTest, WindowsPermissionChecksReturnTrue) {
    EXPECT_TRUE(FileUtil::CheckPathOwner("missing.txt"));
    EXPECT_TRUE(FileUtil::CheckPathPermission("missing.txt", fs::perms::owner_write));
    EXPECT_TRUE(FileUtil::CheckWritableByOther("missing.txt"));
    EXPECT_TRUE(FileUtil::CheckWritableByOtherOrGroup("missing.txt"));
}
#endif

TEST(FileUtilTest, CheckPathValidSuccessWhenFileExistAndPathIsInChinese) {
#ifdef _WIN32
    wchar_t tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathW(MAX_PATH, tempPath), 0U);
    std::wstring wFilePath =
        std::wstring(tempPath) + L"FileUtil_\u6d4b\u8bd5_path_" + std::to_wstring(GetCurrentProcessId()) + L".txt";
    std::string filePath = StringUtil::WString2String(wFilePath);
    HANDLE hFile = CreateFileW(wFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT_NE(hFile, INVALID_HANDLE_VALUE);
    CloseHandle(hFile);
    EXPECT_TRUE(FileUtil::CheckPathSecurity(filePath, CHECK_FILE_READ));
    EXPECT_NE(DeleteFileW(wFilePath.c_str()), 0);
#else
    // 源文件路径
    const char *srcPath = ".//测试001.txt";
    std::ofstream file(srcPath);
    file.close();
    EXPECT_TRUE(FileUtil::CheckPathSecurity(srcPath));
    EXPECT_EQ(std::remove(srcPath), 0);
#endif
}

TEST(FileUtilTest, CheckFileSizeSuccessWhenFileIsEmptyAndPathIsInChinese) {
#ifdef _WIN32
    wchar_t tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathW(MAX_PATH, tempPath), 0U);
    std::wstring wFilePath =
        std::wstring(tempPath) + L"FileUtil_\u6d4b\u8bd5_size_" + std::to_wstring(GetCurrentProcessId()) + L".txt";
    std::string filePath = StringUtil::WString2String(wFilePath);
    HANDLE hFile = CreateFileW(wFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ASSERT_NE(hFile, INVALID_HANDLE_VALUE);
    const char *data = "abc";
    DWORD bytesWritten = 0;
    ASSERT_NE(WriteFile(hFile, data, static_cast<DWORD>(strlen(data)), &bytesWritten, NULL), 0);
    CloseHandle(hFile);
    EXPECT_EQ(bytesWritten, static_cast<DWORD>(strlen(data)));
    EXPECT_EQ(FileUtil::CheckFileSize(filePath), true);
    EXPECT_NE(DeleteFileW(wFilePath.c_str()), 0);
#else
    // 源文件路径
    const char *srcPath = ".//测试001.txt";
    std::ofstream file(srcPath);
    if (file.is_open()) {
        file << "测试1" << std::endl;
        file << "测试2" << std::endl;
        file.close();
    }
    EXPECT_TRUE(FileUtil::CheckPathSecurity(srcPath, CHECK_FILE_READ));
    EXPECT_EQ(std::remove(srcPath), 0);
#endif
}

TEST(FileUtilTest, CheckFileSizeFailedWhenFileExistButIsEmpty) {
    // 源文件路径
    const char *srcPath = ".//example.txt";
    std::ofstream file(srcPath);
    file.close();
    EXPECT_EQ(FileUtil::CheckFileSize(srcPath), false);
    EXPECT_EQ(std::remove(srcPath), 0);
}

TEST(FileUtilTest, GetRealPathSuccessWhenFileExistAndIsNotEmpty) {
    // 源文件路径
    const char *srcPath = ".//example.txt";
    std::ofstream file(srcPath);
    file.close();
    EXPECT_NE(FileUtil::GetRealPath(srcPath), "");
    EXPECT_EQ(std::remove(srcPath), 0);
}

TEST(FileUtilTest, GetRealPathFailedWhenFileNotExist) {
#ifdef _WIN32
    std::string filePath = "D:\\test\\test1\\example.txt";
    EXPECT_EQ(FileUtil::GetRealPath(filePath), filePath);
#else
    std::string filePath = "D://test/test1/example.txt";
    EXPECT_EQ(FileUtil::GetRealPath(filePath), "");
#endif
}

TEST(FileUtilTest, ConvertToRealPath) {
#ifdef _WIN32
    return;
#else
    std::vector<std::string> paths = {""};
    std::string errMsg;
    bool suc = FileUtil::ConvertToRealPath(errMsg, paths);
    EXPECT_EQ(suc, false);
    EXPECT_EQ(errMsg, "The conversion of the path to an absolute path has failed.");
    paths[0] = "/etc/hosts";
    errMsg.clear();
    EXPECT_EQ(FileUtil::ConvertToRealPath(errMsg, paths), true);
    EXPECT_EQ(errMsg.empty(), true);
#endif
}

#ifdef _WIN32
TEST(FileUtilTest, ConvertToRealPathUpdatesWindowsPaths) {
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string dirPath = FileUtil::SplicePath(tempPath,
        "FileUtilConvertToRealPath_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount()));
    ASSERT_NE(CreateDirectoryA(dirPath.c_str(), nullptr), 0);
    std::string filePath = FileUtil::SplicePath(dirPath, "trace_view.json");
    std::ofstream file(filePath);
    file.close();

    std::string errorMsg;
    std::string singlePath = filePath;
    EXPECT_TRUE(FileUtil::ConvertToRealPath(errorMsg, singlePath));
    EXPECT_TRUE(errorMsg.empty());
    EXPECT_TRUE(FileUtil::IsAbsolutePath(singlePath));

    std::vector<std::string> paths = {filePath};
    EXPECT_TRUE(FileUtil::ConvertToRealPath(errorMsg, paths));
    EXPECT_TRUE(errorMsg.empty());
    ASSERT_EQ(paths.size(), 1);
    EXPECT_TRUE(FileUtil::IsAbsolutePath(paths[0]));

    EXPECT_EQ(std::remove(filePath.c_str()), 0);
    EXPECT_NE(RemoveDirectoryA(dirPath.c_str()), 0);
}
#endif

TEST(FileUtilTest, GetRelativePath) {
    std::string path1 = "/etc/host/test";
    std::string path2 = "/etc/host";
    auto res = FileUtil::GetRelativePath(path1, path2);
    EXPECT_NE(res, nullptr);
    EXPECT_EQ(res->compare("test"), 0);
    res = FileUtil::GetRelativePath(path2, path1);
    EXPECT_EQ(res, nullptr);
}

#ifdef _WIN32
TEST(FileUtilTest, GetRelativePathHandlesWindowsSeparators) {
    std::string path1 = "C:\\etc\\host\\test";
    std::string path2 = "C:\\etc\\host";
    auto res = FileUtil::GetRelativePath(path1, path2);
    EXPECT_NE(res, nullptr);
    EXPECT_EQ(res->compare("test"), 0);
    res = FileUtil::GetRelativePath(path2, path1);
    EXPECT_EQ(res, nullptr);
}
#endif

#ifdef _WIN32
TEST(FileUtilTest, GetParentPathHandlesWindowsSeparators) {
    EXPECT_EQ(FileUtil::GetParentPath("C:\\data\\trace_view.json"), "C:\\data");
    EXPECT_EQ(FileUtil::GetParentPath("C:/data\\trace_view.json"), "C:/data");
    EXPECT_EQ(FileUtil::GetParentPath("trace_view.json"), "");
}
#endif

TEST(FileUtilTest, GetRootPath) {
#ifdef _WIN32
    std::string path = "C:\\etc\\hosts";
    EXPECT_EQ(FileUtil::GetRootPath(path), "C:\\");
#else
    std::string path = "/etc/hosts";
    EXPECT_EQ(FileUtil::GetRootPath(path), "/");
#endif
    path = "hosts";
    EXPECT_EQ(FileUtil::GetRootPath(path), "");
}

#ifdef _WIN32
TEST(FileUtilTest, GetRootPathHandlesWindowsMixedSeparators) {
    EXPECT_EQ(FileUtil::GetRootPath("C:/data/trace_view.json"), "C:\\");
}
#endif

#ifdef _WIN32
TEST(FileUtilTest, FindFoldersSkipsHiddenFilesOnWindows) {
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string dirPath = FileUtil::SplicePath(tempPath,
        "FileUtilFindFolders_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(GetTickCount()));
    ASSERT_NE(CreateDirectoryA(dirPath.c_str(), nullptr), 0);
    std::string childDir = FileUtil::SplicePath(dirPath, "rank_0");
    std::string visibleFile = FileUtil::SplicePath(dirPath, "trace_view.json");
    std::string hiddenFile = FileUtil::SplicePath(dirPath, "hidden_trace_view.json");

    ASSERT_NE(CreateDirectoryA(childDir.c_str(), nullptr), 0);
    std::ofstream visible(visibleFile);
    visible.close();
    std::ofstream hidden(hiddenFile);
    hidden.close();
    EXPECT_NE(SetFileAttributesW(FileUtil::ConvertToLongPathW(hiddenFile).c_str(), FILE_ATTRIBUTE_HIDDEN), 0);

    std::vector<std::string> folders;
    std::vector<std::string> files;
    EXPECT_TRUE(FileUtil::FindFolders(dirPath, folders, files));
    EXPECT_NE(std::find(folders.begin(), folders.end(), "rank_0"), folders.end());
    EXPECT_NE(std::find(files.begin(), files.end(), "trace_view.json"), files.end());
    EXPECT_EQ(std::find(files.begin(), files.end(), "hidden_trace_view.json"), files.end());

    SetFileAttributesW(FileUtil::ConvertToLongPathW(hiddenFile).c_str(), FILE_ATTRIBUTE_NORMAL);
    EXPECT_EQ(std::remove(hiddenFile.c_str()), 0);
    EXPECT_EQ(std::remove(visibleFile.c_str()), 0);
    EXPECT_NE(RemoveDirectoryA(childDir.c_str()), 0);
    EXPECT_NE(RemoveDirectoryA(dirPath.c_str()), 0);
}
#endif

TEST(FileUtilTest, FindIfDbTypeByRegex) {
    auto testDbDir = TestSuit::GetTestDataFile("full_db");
    const std::string DB_REG =
        R"((msprof_[0-9]{1,16}|((ascend_pytorch_profiler)(_[0-9]{1,16}){0,1})|cluster_analysis)\.db$)";
    const std::string traceViewReg = R"((((trace_view|msprof(_slice_[0-9]{1,2})?_[0-9]{1,14})\.json)|)"
                                     R"((operator_memory|operator_memory(_slice_[0-9]{1,2})?_[0-9]{1,14})\.csv)$)";
    bool suc = FileUtil::FindIfDbTypeByRegex(testDbDir, std::regex(traceViewReg), std::regex(DB_REG));
    EXPECT_EQ(suc, true);
}

TEST(FileUtilTest, CopyFileByPath) {
#ifdef _WIN32
    char tempPath[MAX_PATH] = {0};
    ASSERT_GT(GetTempPathA(MAX_PATH, tempPath), 0U);
    std::string sourcePath =
        FileUtil::SplicePath(tempPath, "CopyFileByPathTest_" + std::to_string(GetCurrentProcessId()) + ".tmp");
    std::string targetPath =
        FileUtil::SplicePath(tempPath, "CopyFileByPathTest_" + std::to_string(GetCurrentProcessId()) + ".copy");
#else
    std::string sourcePath = "CopyFileByPathTest.tmp";
    std::string targetPath = "./CopyFileByPathTest.copy";
#endif
    std::ofstream sourceFile(sourcePath);
    ASSERT_TRUE(sourceFile.is_open());
    sourceFile << "abc";
    sourceFile.close();
    EXPECT_TRUE(FileUtil::CopyFileByPath(sourcePath, targetPath));

    // remove file
    std::remove(sourcePath.c_str());
    std::remove(targetPath.c_str());
}

TEST(FileUtilTest, TestSplitFilePathSuccess) {
#ifdef _WIN32
    std::string dbPath1 = R"(D:\GUI_TEST_DATA\deepseek_32B\actor worker\ma-job_ascend_pt\ASCEND_PROFILER_OUTPUT\)";
#else
    std::string dbPath1 = "D:/GUI_TEST_DATA/deepseek_32B/actor worker/ma-job_ascend_pt/ASCEND_PROFILER_OUTPUT/";
#endif
    auto result1 = FileUtil::SplitFilePath(dbPath1);
    std::vector<std::string> expected1 = {
        "D:", "GUI_TEST_DATA", "deepseek_32B", "actor worker", "ma-job_ascend_pt", "ASCEND_PROFILER_OUTPUT"};
    EXPECT_EQ(result1, expected1);
}

TEST(FileUtilTest, SplicePath) {
#ifdef _WIN32
    EXPECT_EQ(FileUtil::SplicePath("home", "user", "test"), "home\\user\\test");
    EXPECT_EQ(FileUtil::SplicePath("C:\\data", "rank_0"), "C:\\data\\rank_0");
#else
    EXPECT_EQ("/home/user/test", FileUtil::SplicePath("/home", "user", "test"));
#endif
}

#ifdef _WIN32
TEST(FileUtilTest, GetFileNameHandlesWindowsSeparators) {
    EXPECT_EQ(FileUtil::GetFileName("C:\\data\\trace_view.json"), "trace_view.json");
    EXPECT_EQ(FileUtil::GetFileName("C:/data\\rank_0/trace_view.json"), "trace_view.json");
    EXPECT_EQ(FileUtil::GetFileName("trace_view.json"), "trace_view.json");
    EXPECT_EQ(FileUtil::GetFileName("C:\\data\\"), "");
}
#endif

TEST(FileUtilTest, StemFile) {
    EXPECT_EQ(FileUtil::StemFile("test.excel"), "test");
    EXPECT_EQ(FileUtil::StemFile("/home/user/test.tar"), "test");
    EXPECT_EQ(FileUtil::StemFile("test_excel"), "test_excel");
    EXPECT_EQ(FileUtil::StemFile("test.tar.gz"), "test");
}
