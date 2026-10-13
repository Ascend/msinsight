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
#include "FileParser.h"

namespace Dic {
namespace Module {
std::string FileParser::GetError() { return error; }

void FileParser::SetParseEndCallBack(
    std::function<void(const std::string, const std::string, bool result, const std::string)> &callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    parseEndCallback = callback;
}

void FileParser::SetParseProgressCallBack(
    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> &callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    parseProgressCallback = callback;
}

void FileParser::Reset() {
    std::lock_guard<std::mutex> lock(callbackMutex);
    error.clear();
    parseEndCallback = nullptr;
    parseProgressCallback = nullptr;
    fileProgressMap.clear();
}

bool FileParser::HasCallbackFuncSet() const {
    std::lock_guard<std::mutex> lock(callbackMutex);
    return static_cast<bool>(parseEndCallback) && static_cast<bool>(parseProgressCallback);
}

void FileParser::SetFileProgress(const std::string &rankId, std::unique_ptr<FileProgress> progress) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    fileProgressMap[rankId] = std::move(progress);
}

void FileParser::AddParsedSizeAndNotify(const std::string &rankId, uint64_t bytes) {
    std::function<void(const std::string, uint64_t, uint64_t, int)> progressCallback;
    uint64_t parsedSize = 0;
    uint64_t totalSize = 0;
    int progress = 0;
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        auto it = fileProgressMap.find(rankId);
        if (it == fileProgressMap.end() || it->second == nullptr) {
            return;
        }
        it->second->AddToParsedSize(bytes);
        parsedSize = it->second->GetParsedSize();
        totalSize = it->second->GetTotalSize();
        progress = it->second->GetProgressPercentage();
        progressCallback = parseProgressCallback;
    }
    if (progressCallback) {
        progressCallback(rankId, parsedSize, totalSize, progress);
    }
}

void FileParser::InvokeEndCallback(const std::string &rankId, const std::string &fileId, bool result,
    const std::string &message) {
    std::function<void(const std::string, const std::string, bool, const std::string)> endCallback;
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        endCallback = parseEndCallback;
    }
    if (endCallback) {
        endCallback(rankId, fileId, result, message);
    }
}
} // end of namespace Module
} // end of namespace Dic
