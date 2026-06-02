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

#include "FuzzFileUtil.h"
#include <fstream>
#include <iostream>
#include "FuzzDefs.h"

const std::set<char> PathFuzzer::invalidPathChar = {'\0', '/'};
const std::vector<std::string> PathFuzzer::specialChars = {" ", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "{",
    "}", "[", "]", "<", ">", "?", "+", "-", "=", "~", "|", "\\", "中文文件", "日本語ファイル", "한국어 파일"};
void PathFuzzer::GenerateFilePathMutation(
    uint pathCount, std::vector<std::string> &fileList, std::vector<std::string> &dirList) {
    fileList.clear();
    dirList.clear();
    uint i = 0;
    while (static_cast<uint>(dirList.size() + fileList.size()) < pathCount || static_cast<uint>((i) / 2) > pathCount) {
        std::string filename = GenerateFileName(true, i++);
        int typeInt = RandomInt(0, 99);
        switch (typeInt % 10) {
        case 0:
            if (CreateRegularFileOrDir(filename, true)) {
                fileList.emplace_back(filename);
            }
            break;
        case 1:
            if (CreateRegularFileOrDir(filename, false)) {
                dirList.emplace_back(filename);
            }
            break;
        case 2:
            CreateBaseDirSymlink(filename);
            break;
        case 3:
            CreateCircularSymlink(filename);
            break;
        case 4:
            if (!CreateSpecialCharFileOrDir(filename, true, true).empty()) {
                fileList.emplace_back(filename);
            }
            break;
        case 5:
            if (!CreateSpecialCharFileOrDir(filename, true, false).empty()) {
                dirList.emplace_back(filename);
            }
            break;
        case 6:
            CreateSpecialCharFileOrDir(filename, false, false);
            break;
        case 7:
            CreateSpecialCharFileOrDir(filename, false, true);
            break;
        case 8:
            CreateInsecurityPermissionFileOrDir(filename, true);
            break;
        case 9:
            CreateInsecurityPermissionFileOrDir(filename, false);
            break;
        default:
            break;
        }
    }
}

std::string PathFuzzer::GenerateFileName(bool validPathCharOnly, uint index) {
    char *fuzzString = DTSetGetString(&g_Element[index], 9, NAME_MAX, "filename", "FuzzFileNameString");
    if (fuzzString == nullptr) {
        return "";
    }
    if (!validPathCharOnly) {
        return fuzzString;
    }
    std::string result;
    for (char *p = fuzzString; *p != '\0'; ++p) {
        if (invalidPathChar.find(*p) == invalidPathChar.end()) {
            result += *p;
        }
    }
    return result;
}

int GenerateFileMutation(const std::string &baseFilePath, char **mutationContent, int &mutationContentLength) {
    // 打开文件
    std::ifstream baseFile(baseFilePath, std::ios::in);
    if (!baseFile) {
        std::cout << "open base file failed: " << baseFilePath << std::endl;
        return 1;
    }

    // 移动文件指针到文件末尾，确定文件大小
    baseFile.seekg(0, std::ios::end);
    long fileSize = baseFile.tellg();
    baseFile.seekg(0, std::ios::beg);

    // 分配char数组
    std::string buffer;
    buffer.resize(fileSize);

    // 读取文件内容到char数组
    baseFile.read(&buffer[0], fileSize);

    // 检查是否成功读取
    if (!baseFile) {
        std::cout << "read base file failed: " << baseFilePath << std::endl;
        baseFile.close();
        return 1;
    }

    *mutationContent = DT_SetGetString(&g_Element[0], fileSize + 1, UINT32_MAX, &buffer[0]);
    mutationContentLength = DT_GET_MutatedValueLen(&g_Element[0]);

    // 释放资源
    baseFile.close();
    return 0;
}
