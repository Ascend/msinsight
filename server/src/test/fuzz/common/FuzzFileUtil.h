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

#ifndef PROFILER_SERVER_FUZZFILEUTIL_H
#define PROFILER_SERVER_FUZZFILEUTIL_H

#include <string>
#include <random>
#include <set>
#include "FileUtil.h"

using namespace Dic;
/**
 * 根据提供基础文件文本内容，随机变异出新的文本内容
 *
 * @param baseFilePath 基础文件路径
 * @param mutationContent 指向变异后文本指针的指针
 * @param mutationContentLength 变异后文本内容的长度
 * @return 如果成功获取变异文本内容，返回0；否则返回1
 */
int GenerateFileMutation(const std::string &baseFilePath, char **mutationContent, int &mutationContentLength);

class PathFuzzer {
  public:
    /**
     * 在一个相对路径下创建一系列的异常文件/文件夹
     * @param pathCount : 创建子文件/子目录等的数量
     * @param fileList : 输出有效文件列表
     * @param dirList : 输出有效子目录列表
     */
    void GenerateFilePathMutation(
        uint pathCount, std::vector<std::string> &fileList, std::vector<std::string> &dirList);
    /**
     * 变异出一个随机字符串
     * @param validPathCharOnly : 是否过滤掉非法的路径字符
     * @return 文件名
     */
    static std::string GenerateFileName(bool validPathCharOnly, uint index);
    inline bool CreateRegularFileOrDir(const std::string &filename, const bool isFile) {
        if (filename.size() > NAME_MAX) {
            std::cerr << "Create symlink failed: symlinkName exceeds max length of file name." << std::endl;
            return false;
        }
        try {
            std::string fullPath = FileUtil::SplicePath(baseDir, filename);
            if (isFile) {
                std::ofstream file(fullPath);
                if (!file.is_open()) {
                    std::cerr << "File may already exist or failed to open: " << fullPath << std::endl;
                    return false;
                }
                file << "This is a test file.";
                file.close();
                return FileUtil::ModifyFilePermissions(fullPath, 0640);
            } else {
                if (!fs::create_directories(fullPath)) {
                    std::cerr << "Directory may already exist or failed to create: " << fullPath << std::endl;
                    return false;
                }
                return FileUtil::ModifyFilePermissions(fullPath, 0750);
            }
        } catch (const std::exception &e) {
            std::cerr << "Caught runtime_error: " << e.what() << std::endl;
            return false;
        }
    }

    inline bool CreateBaseDirSymlink(const std::string &symlinkName) const {
        if (symlinkName.size() > NAME_MAX) {
            std::cerr << "Create symlink failed: symlinkName exceeds max length of file name." << std::endl;
            return false;
        }
        try {
            fs::create_symlink(baseDir, FileUtil::SplicePath(baseDir, symlinkName));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Caught runtime_error: " << e.what() << std::endl;
            return false;
        }
    }

    inline bool CreateCircularSymlink(const std::string &symlinkName) const {
        try {
            fs::create_symlink(FileUtil::SplicePath(baseDir, symlinkName), FileUtil::SplicePath(baseDir, symlinkName));
            return true;
        } catch (std::exception &e) {
            std::cerr << "Caught runtime_error: " << e.what() << std::endl;
            return false;
        }
    }

    static inline int RandomInt(int min, int max) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(min, max);
        return distrib(gen);
    }

    static inline std::string GenerateRandomBytes(const size_t len) {
        std::string result;
        size_t i = 0;
        size_t loop = 0;
        size_t maxSizeType = std::numeric_limits<size_t>::max();
        size_t maxLoop = maxSizeType / 2 < len ? maxSizeType : 2 * len;
        while (i < len && loop++ < maxLoop) {
            char c = static_cast<char>(RandomInt(0, 255) % 256);
            if (invalidPathChar.find(c) == invalidPathChar.end()) {
                i++;
                result += c;
            }
        }
        return result;
    }

    inline std::string CreateSpecialCharFileOrDir(const std::string &filename, bool isUtf8, const bool isFile) {
        if (filename.size() > NAME_MAX) {
            std::cerr << "Create symlink failed: symlinkName exceeds max length of file name." << std::endl;
            return "";
        }
        std::string fullname;
        if (!isUtf8) {
            int randomBytesLen = RandomInt(1, NAME_MAX - filename.size());
            std::string randomBytes = GenerateRandomBytes(randomBytesLen);
            fullname = filename + randomBytes;
        } else {
            int randomSpecialStrIdx = RandomInt(0, specialChars.size() - 1);
            fullname = filename + specialChars[randomSpecialStrIdx];
        }
        if (CreateRegularFileOrDir(fullname, isFile)) {
            return fullname;
        } else {
            return "";
        }
    }

    // 创建权限异常文件/目录
    inline bool CreateInsecurityPermissionFileOrDir(const std::string &filename, bool isFile) {
        std::string fullPath = FileUtil::SplicePath(baseDir, filename);
        if (!CreateRegularFileOrDir(filename, isFile)) {
            return false;
        }

        chmod(fullPath.c_str(), 0777);
        return true;
    }

    inline bool ClearBaseDir() {
        try {
            if (fs::exists(baseDir)) {
                fs::remove_all(baseDir);
            }
            if (!fs::create_directory(baseDir)) {
                std::cerr << "无法创建目录: " << baseDir << std::endl;
                return false;
            }
            return true;
        } catch (const fs::filesystem_error &e) {
            std::cerr << "文件系统错误: " << e.what() << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "标准异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "未知错误" << std::endl;
        }
        return false;
    }

    ~PathFuzzer() { ClearBaseDir(); }

    const static std::set<char> invalidPathChar;
    const std::string baseDir = "./test_data/test_get_file_base_dir";
    const static std::vector<std::string> specialChars;
};

#endif // PROFILER_SERVER_FUZZFILEUTIL_H
