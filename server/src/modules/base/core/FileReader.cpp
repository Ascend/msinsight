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
#include "SafeFile.h"
#include "FileReader.h"

int64_t Dic::Module::FileReader::GetFileSize(const std::string &filePath) {
    return FileUtil::GetFileSize(filePath.c_str());
}

std::string Dic::Module::FileReader::ReadJsonArray(
    const std::string &filePath, int64_t startPosition, int64_t endPosition) {
    if (endPosition < startPosition) {
        Server::ServerLog::Warn("Read json array. Illegal position. Start: ", startPosition, " End: ", endPosition);
        return "";
    }
    std::ifstream file = OpenReadFileSafely(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Server::ServerLog::Error("Read json array. Failed to open file.");
        return "";
    }
    if (startPosition == 0 && endPosition == 0) {
        std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return str;
    }
    file.seekg(startPosition, std::ios::beg);
    int64_t suffixLen = 2; // [ ]
    int64_t len = endPosition - startPosition + 1 + suffixLen; // + [ ] + \0
    auto buffer = std::make_unique<char[]>(len);
    if (!file.read(buffer.get() + 1, len - suffixLen)) { // reserved '[' and ']'
        file.close();
        Server::ServerLog::Error("Read json array. Failed to read file. start:", startPosition, ", end:", endPosition);
        return "";
    }
    file.close();
    buffer[0] = '[';
    buffer[len - 1] = ']';
    return {buffer.get(), static_cast<uint64_t>(len)};
}
