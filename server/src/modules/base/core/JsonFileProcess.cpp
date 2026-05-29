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
#include "FileParser.h"
#include "JsonFileProcess.h"

namespace Dic {
namespace Module {
std::vector<std::pair<int64_t, int64_t>> JsonFileProcess::SplitFile(
    const std::string &filePath, std::optional<std::pair<int64_t, int64_t>> position) {
    std::ifstream file = OpenReadFileSafely(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        Dic::Server::ServerLog::Error("Split file failed to open json file. ");
        return {};
    }

    // 如果解析文件的position使用的是默认参数，那么将起止位置设为0和文件结尾
    std::pair<int64_t, int64_t> pos;
    if (position.has_value()) {
        pos = std::make_pair(position.value().first, position.value().second);
    } else {
        int64_t start = 0;
        file.seekg(0, std::ifstream::end);
        int64_t end = file.tellg();
        pos = std::make_pair(start, end);
    }
    std::vector<std::pair<int64_t, int64_t>> result = GetSplitPosition(file, pos);
    file.close();
    return result;
}

std::vector<std::pair<int64_t, int64_t>> JsonFileProcess::GetSplitPosition(
    std::ifstream &file, std::pair<int64_t, int64_t> position) {
    file.seekg(0, std::ifstream::end); // 将当前位置移动到文件尾，以获取文件大小
    int64_t fileSize = file.tellg();
    int64_t contentStart = position.first;
    int64_t contentEnd = position.second;
    if (contentStart < 0 || contentEnd < 0 || contentStart >= contentEnd || contentEnd > fileSize) {
        Dic::Server::ServerLog::Error("Invalid position to split file, start position is % and end position is %.",
            position.first, position.second);
        return {};
    }

    std::vector<std::pair<int64_t, int64_t>> result = {};
    int64_t contentSize = contentEnd - contentStart;
    file.clear();
    file.seekg(contentStart, std::ios::beg); // 将当前位置移动到文件开头，正式开始处理
    // 首先判断Trace文件的格式是JSON Object Format还是JSON Array
    // Format，二者有差异，Object格式实际的数据应该从traceEvents之后开始
    Dic::Module::JsonFormat json = SeekRegexPosition(file, R"(\"traceEvents")")
        ? Dic::Module::JsonFormat::JSON_OBJECT_FORMAT
        : Dic::Module::JsonFormat::JSON_ARRAY_FORMAT;
    if (contentSize <= blockSize) {
        // 如果是Object类型，则有效数据从 ”traceEvents“: [  的"["之后开始
        ComputeSmallFilePosition(file, result, json, position);
        return result;
    }
    // 前面获取JsonFormat时，如果是Json Object Format，则已将当前位置移动到traceEvents之后，两种文件处理的逻辑一致
    bool endFlag = false;
    while (!endFlag) {
        if (!SeekCharPosition(file, '{')) {
            Dic::Server::ServerLog::Info("Failed to find json format start position.");
            break;
        }
        int64_t start = file.tellg();
        std::string endRegex;
        if (start + blockSize >= contentSize) {
            file.seekg(contentEnd - endBufferLength, std::ifstream::beg);
            endFlag = true;
        } else {
            file.seekg(blockSize, std::ifstream::cur);
        }
        if (!SeekPhEndPosition(file, endFlag, endBufferLength)) {
            Dic::Server::ServerLog::Error("Failed to find ph json format.");
            break;
        }
        int64_t end = file.tellg();
        result.emplace_back(start, end);
    }
    return result;
}

void JsonFileProcess::ComputeSmallFilePosition(std::ifstream &file, std::vector<std::pair<int64_t, int64_t>> &result,
    const JsonFormat &json, std::pair<int64_t, int64_t> position) {
    if (json == JsonFormat::JSON_OBJECT_FORMAT) {
        int64_t contentStart = position.first;
        int64_t contentEnd = position.second;
        int64_t contentSize = contentEnd - contentStart;
        if (!SeekRegexPosition(file, R"(\[\s*\{)")) {
            Server::ServerLog::Warn("Failed to find start position of json object format.");
            return;
        }
        int64_t start = file.tellg();
        // 如果解析的内容长度小于endBufferLength,直接将文件指针移动到内容起始位置
        if (contentSize < endBufferLength) {
            file.seekg(contentStart, std::ifstream::beg);
        } else {
            file.seekg(contentEnd - endBufferLength, std::ifstream ::beg);
        }
        const int bufferLength = contentSize < endBufferLength ? contentSize : endBufferLength;
        if (SeekPhEndPosition(file, true, bufferLength)) {
            int64_t end = file.tellg();
            if (start > INT64_MAX - 1 || start + 1 > end) {
                Server::ServerLog::Warn("Failed to find legal end position of json object format.");
                return;
            }
            result.emplace_back(start + 1, end); // 此处的1表示跳过R"(\[\s*\{)"中的"["
        }
    } else {
        result.emplace_back(0, 0);
    }
}

bool JsonFileProcess::SeekCharPosition(std::ifstream &file, char c) {
    file.clear();
    auto cur = file.tellg();
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(startBufferLength);
    file.read(buffer.get(), startBufferLength);
    int64_t readCount = file.gcount();
    if (readCount <= 0) {
        Dic::Server::ServerLog::Error("Seek char. Failed to read file.");
        return false;
    }
    file.clear();
    file.seekg(cur);
    std::string str(buffer.get(), readCount);
    size_t offset = str.find(c);
    if (offset == std::string::npos) {
        Dic::Server::ServerLog::Error("Failed to find separator.");
        return false;
    }
    file.seekg(static_cast<int64_t>(offset), std::ifstream::cur);
    return true;
}

bool JsonFileProcess::SeekRegexPosition(std::ifstream &file, const std::string &regex) {
    file.clear();
    auto cur = file.tellg();
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(endBufferLength);
    file.read(buffer.get(), endBufferLength);
    int64_t readCount = file.gcount();
    if (readCount <= 0) {
        Dic::Server::ServerLog::Error("Seek regex. Failed to read file.");
        return false;
    }
    file.clear();
    file.seekg(cur);
    std::string str(buffer.get(), readCount);
    auto result = Dic::RegexUtil::RegexSearch(str, regex);
    if (!result.has_value()) {
        Dic::Server::ServerLog::Warn("Can't find match regex:", regex);
        return false;
    }
    file.seekg(result.value().position(), std::ifstream::cur);
    return true;
}

bool JsonFileProcess::SeekPhEndPosition(std::ifstream &file, bool endFlag, int bufferLength) {
    file.clear();
    auto cur = file.tellg();
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(bufferLength);
    file.read(buffer.get(), bufferLength);
    int64_t readCount = file.gcount();
    if (readCount <= 0) {
        Dic::Server::ServerLog::Error("Seek ph end position. Failed to read file.");
        return false;
    }
    file.clear();
    file.seekg(cur);
    std::string str(buffer.get(), readCount);
    size_t offset = std::string::npos;
    if (endFlag) {
        offset = str.rfind("\"ph\"");
    } else {
        offset = str.find("\"ph\"");
    }
    if (offset == std::string::npos || offset > INT_MAX || str.size() > INT_MAX) {
        Dic::Server::ServerLog::Error("Failed to find ph.");
        return false;
    }
    int strLen = static_cast<int>(str.size());
    int iOffset = static_cast<int>(offset);
    int pos = -1;
    int leftCount = 0;
    for (int i = iOffset; i < strLen; ++i) {
        if (str[i] == '{') {
            leftCount++;
            continue;
        }
        if (str[i] == '}' && leftCount == 0) {
            pos = i;
            break;
        }
        if (str[i] == '}') {
            leftCount--;
        }
    }
    if (pos == -1) {
        Dic::Server::ServerLog::Error("Failed to find ph end position.");
        return false;
    }
    file.seekg(pos, std::ifstream::cur);
    return true;
}
} // end of namespace Module
} // end of namespace Dic
