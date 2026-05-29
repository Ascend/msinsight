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

#ifndef DATA_INSIGHT_CORE_MODULE_CORE_FILE_PARSER_H
#define DATA_INSIGHT_CORE_MODULE_CORE_FILE_PARSER_H

#include <string>
#include <functional>
#include <map>
#include <atomic>
#include <memory>
#include "ValidateUtil.h"

namespace Dic {
namespace Module {
enum class JsonFormat { JSON_ARRAY_FORMAT = 0, JSON_OBJECT_FORMAT };

struct FileProgress {
  public:
    FileProgress(uint64_t initialParsedSize, uint64_t initialTotalSize)
        : parsedSize(initialParsedSize), totalSize(initialTotalSize) {}

    uint64_t GetParsedSize() const { return parsedSize.load(); }

    uint64_t GetTotalSize() const { return totalSize.load(); }

    void AddToParsedSize(uint64_t value) { parsedSize.fetch_add(value); }

    int GetProgressPercentage() const {
        uint64_t parsed = parsedSize.load();
        uint64_t total = totalSize.load();
        if (total == 0) {
            return 0; // 避免除以0错误
        }

        double percentage = static_cast<double>(parsed) / static_cast<double>(total) * 100.0;
        int res = static_cast<int>(percentage);
        return res > maxPercentage ? maxPercentage : res;
    }

  private:
    std::atomic<std::uint64_t> parsedSize;
    std::atomic<std::uint64_t> totalSize;
    const int maxPercentage = 99;
};

class FileParser {
  public:
    FileParser() = default;
    virtual ~FileParser() = default;
    virtual bool Parse(const std::vector<std::string> &filePathArr, const std::string &rankId,
        const std::string &selectedFolder, const std::string &fileId) = 0;

    virtual void SetParseEndCallBack(
        std::function<void(const std::string, const std::string, bool result, const std::string)> &callback);
    virtual void SetParseProgressCallBack(
        std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> &callback);

    virtual std::string GetError();
    virtual void Reset();

    inline bool HasCallbackFuncSet() const { return parseEndCallback && parseProgressCallback; }

  protected:
    std::string error;
    std::function<void(const std::string, const std::string, bool result, const std::string)> parseEndCallback;
    std::function<void(const std::string, uint64_t parsedSize, uint64_t totalSize, int progress)> parseProgressCallback;
    std::map<std::string, std::unique_ptr<FileProgress>> fileProgressMap;
};
} // end of namespace Module
} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_MODULE_CORE_FILE_PARSER_H
